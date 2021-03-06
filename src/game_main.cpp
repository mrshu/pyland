#define GLM_FORCE_RADIANS

#include <glog/logging.h>
#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/path.hpp>
#include <chrono>
#include <functional>
#include <glm/vec2.hpp>
#include <iostream>
#include <memory>
#include <random>
#include <ratio>
#include <string>
#include <sstream>
#include <utility>
#include <vector>

#include "game_init.hpp"
#include "button.hpp"
#include "callback_state.hpp"
#include "challenge_data.hpp"
#include "cutting_challenge.hpp"
#include "engine.hpp"
#include "event_manager.hpp"
#include "filters.hpp"
#include "final_challenge.hpp"
#include "game_window.hpp"
#include "gui_manager.hpp"
#include "gui_window.hpp"
#include "input_manager.hpp"
#include "interpreter.hpp"
#include "introduction_challenge.hpp"
#include "keyboard_input_event.hpp"
#include "lifeline.hpp"
#include "map_viewer.hpp"
#include "mouse_cursor.hpp"
#include "mouse_input_event.hpp"
#include "mouse_state.hpp"
#include "new_challenge.hpp"
#include "notification_bar.hpp"
#include "sprite.hpp"
#include "start_screen.hpp"

#ifdef USE_GLES
#include "typeface.hpp"
#include "text_font.hpp"
#include "text.hpp"
#endif

#include "game_main.hpp"

using namespace std;

static std::mt19937 random_generator;


GameMain::GameMain(int &argc, char **argv):
    embedWindow(800, 600, argc, argv, this),
    interpreter(boost::filesystem::absolute("python_embed/wrapper_functions.so").normalize()),
    gui_manager(),
    callbackstate(),
    map_viewer(&embedWindow, &gui_manager),
    buttontype(Engine::get_game_typeface()),        //TODO : REMOVE THIS HACKY EDIT - done for the demo tomorrow
    buttonfont(Engine::get_game_font()),
    tile_identifier_text(&embedWindow, Engine::get_game_font(), false)
{
    LOG(INFO) << "Constructing GameMain..." << endl;

    map_path = ("../maps/start_screen.tmx");

    switch (argc)
    {
    default:
        std::cout << "Usage: " << argv[0] << " [MAP] " << std::endl;
        return;
        // The lack of break statements is not an error!!!
    case 2:
        map_path = std::string(argv[1]);
    case 1:
        break;
    }

    /// CREATE GLOBAL OBJECTS

    //Create the game embedWindow to present to the users
    embedWindow.use_context();
    Engine::set_game_window(&embedWindow);

    //Create the input manager
    input_manager = embedWindow.get_input_manager();

    Engine::set_map_viewer(&map_viewer);

//    //Create the event manager
    em = EventManager::get_instance();
//
    sprite_window = std::make_shared<GUIWindow>();
    sprite_window->set_visible(false);
    gui_manager.set_root(sprite_window);

    notification_bar = new NotificationBar();

    Engine::set_notification_bar(notification_bar);
    //    SpriteSwitcher sprite_switcher;

    // quick fix so buttons in correct location in initial embedWindow before gui_resize_func callback
    original_window_size = embedWindow.get_size();
    sprite_window->set_width_pixels(original_window_size.first);
    sprite_window->set_height_pixels(original_window_size.second);

    gui_manager.parse_components();

    //The GUI resize function
    gui_resize_func = [&] (GameWindow* game_window)
    {
        LOG(INFO) << "GUI resizing";
        auto window_size = (*game_window).get_size();
        sprite_window->set_width_pixels(window_size.first);
        sprite_window->set_height_pixels(window_size.second);
        gui_manager.parse_components();
    };
    gui_resize_lifeline = embedWindow.register_resize_handler(gui_resize_func);

    //The callbacks
    // WARNING: Fragile reference capture
    map_resize_lifeline = embedWindow.register_resize_handler([&] (GameWindow *)
    {
        map_viewer.resize();
    });

    back_callback = input_manager->register_keyboard_handler(filter(
    {KEY_PRESS, KEY("ESCAPE")},
    [&] (KeyboardInputEvent)
    {
        Engine::get_challenge()->event_finish.trigger(0);;
    }
    ));

    up_callback = input_manager->register_keyboard_handler(filter(
    {KEY_HELD, REJECT(MODIFIER({"Left Shift", "Right Shift"})), KEY({"Up", "W"})},
    [&] (KeyboardInputEvent)
    {
        callbackstate.man_move(glm::ivec2( 0, 1));
    }
    ));

    down_callback = input_manager->register_keyboard_handler(filter(
    {KEY_HELD, REJECT(MODIFIER({"Left Shift", "Right Shift"})), KEY({"Down", "S"})},
    [&] (KeyboardInputEvent)
    {
        callbackstate.man_move(glm::ivec2( 0, -1));
    }
    ));

    right_callback = input_manager->register_keyboard_handler(filter(
    {KEY_HELD, REJECT(MODIFIER({"Left Shift", "Right Shift"})), KEY({"Right", "D"})},
    [&] (KeyboardInputEvent)
    {
        callbackstate.man_move(glm::ivec2( 1,  0));
    }
    ));

    left_callback = input_manager->register_keyboard_handler(filter(
    {KEY_HELD, REJECT(MODIFIER({"Left Shift", "Right Shift"})), KEY({"Left", "A"})},
    [&] (KeyboardInputEvent)
    {
        callbackstate.man_move(glm::ivec2(-1,  0));
    }
    ));

    monologue_callback = input_manager->register_keyboard_handler(filter(
    {KEY_PRESS, KEY("M")},
    [&] (KeyboardInputEvent)
    {
        callbackstate.monologue();
    }
    ));

    mouse_button_lifeline = input_manager->register_mouse_handler(filter(
    {MOUSE_RELEASE},
    [&] (MouseInputEvent event)
    {
        gui_manager.mouse_callback_function(event);
    }));

    zoom_in_callback = input_manager->register_keyboard_handler(filter(
    {KEY_HELD, KEY("=")},
    [&] (KeyboardInputEvent)
    {
        Engine::set_global_scale(Engine::get_global_scale() * 1.01f);
    }
    ));

    zoom_out_callback = input_manager->register_keyboard_handler(filter(
    {KEY_HELD, KEY("-")},
    [&] (KeyboardInputEvent)
    {
        Engine::set_global_scale(Engine::get_global_scale() / 1.01f);
    }
    ));

    zoom_zero_callback = input_manager->register_keyboard_handler(filter(
    {KEY_PRESS, MODIFIER({"Left Ctrl", "Right Ctrl"}), KEY("0")},
    [&] (KeyboardInputEvent)
    {
        Engine::set_global_scale(1.0f);
    }
    ));

    help_callback = input_manager->register_keyboard_handler(filter(
    {KEY_PRESS, MODIFIER({"Left Shift", "Right Shift"}), KEY("/")},
    [&] (KeyboardInputEvent)
    {
        auto id(Engine::get_map_viewer()->get_map_focus_object());
        auto active_player(ObjectManager::get_instance().get_object<Sprite>(id));

        Engine::print_dialogue(
            active_player->get_name(),
            active_player->get_instructions()
        );
    }
    ));

    for (unsigned int i=0; i<10; ++i)
    {
        digit_callbacks.push_back(
            input_manager->register_keyboard_handler(filter(
        {KEY_PRESS, KEY(std::to_string(i))},
        [&, i] (KeyboardInputEvent)
        {
            callbackstate.register_number_key(i);
        }
        ))
        );
    }

    switch_char = input_manager->register_mouse_handler(filter(
    {MOUSE_RELEASE},
    [&] (MouseInputEvent event)
    {
        LOG(INFO) << "mouse clicked on map at " << event.to.x << " " << event.to.y << " pixel";

        glm::vec2 tile_clicked(Engine::get_map_viewer()->pixel_to_tile(glm::ivec2(event.to.x, event.to.y)));
        LOG(INFO) << "interacting with tile " << tile_clicked.x << ", " << tile_clicked.y;

        auto sprites = Engine::get_sprites_at(tile_clicked);

        if (sprites.size() == 0)
        {
            LOG(INFO) << "No sprites to interact with";
        }
        else if (sprites.size() == 1)
        {
            callbackstate.register_number_id(sprites[0]);
        }
        else
        {
            LOG(WARNING) << "Not sure sprite object to switch to";
            callbackstate.register_number_id(sprites[0]);
        }
    }
    ));

    tile_identifier_text.move_ratio(1.0f, 0.0f);
    tile_identifier_text.resize(256, 64);
    tile_identifier_text.align_right();
    tile_identifier_text.vertical_align_bottom();
    tile_identifier_text.align_at_origin(true);
    tile_identifier_text.set_bloom_radius(5);
    tile_identifier_text.set_bloom_colour(0x00, 0x0, 0x00, 0xa0);
    tile_identifier_text.set_colour(0xff, 0xff, 0xff, 0xa8);
    tile_identifier_text.set_text("(?, ?)");

    func_char = [&] (GameWindow *)
    {
        LOG(INFO) << "text embedWindow resizing";
        Engine::text_updater();
    };

    text_lifeline_char = embedWindow.register_resize_handler(func_char);

    //Run the map
    run_game = true;

    //Setup challenge
    challenge_data = (new ChallengeData(
                          map_path,
                          &interpreter,
                          &gui_manager,
                          &embedWindow,
                          input_manager,
                          notification_bar,
                          0));

    cursor = new MouseCursor(&embedWindow);

    //Run the challenge - returns after challenge completes

    challenge_data->run_challenge = true;
    challenge = pick_challenge(challenge_data);
    Engine::set_challenge(challenge);
    challenge->start();

    last_clock = (std::chrono::steady_clock::now());

    //Run the challenge - returns after challenge completes
    VLOG(3) << "{";

    VLOG(3) << "}";

    embedWindow.execute_app();

    em->flush_and_disable();
    delete challenge;
    em->reenable();

    LOG(INFO) << "Constructed GameMain" << endl;

}

GameMain::~GameMain()
{

    LOG(INFO) << "Destructing GameMain..." << endl;
    delete notification_bar;
    delete challenge_data;
    delete cursor;
    LOG(INFO) << "Destructed GameMain..." << endl;
}

void GameMain::game_loop(bool showMouse)
{
    if (!challenge_data->game_window->check_close() && challenge_data->run_challenge)
    {
        //callbackstate.man_move(glm::ivec2( 0, 1));
        last_clock = std::chrono::steady_clock::now();

        VLOG(3) << "} SB | IM {";
        GameWindow::update();

        VLOG(3) << "} IM | EM {";

        do
        {
            EventManager::get_instance()->process_events();
        }
        while (
            std::chrono::steady_clock::now() - last_clock
            < std::chrono::nanoseconds(1000000000 / 60)
        );

        VLOG(3) << "} EM | RM {";
        Engine::get_map_viewer()->render();
        VLOG(3) << "} RM | TD {";
        Engine::text_displayer();
        challenge_data->notification_bar->text_displayer();

        // This is not an input event, because the map can move with
        // the mouse staying still.
        {
            std::pair<int,int> pixels = input_manager->get_mouse_pixels();
            glm::ivec2 tile(Engine::get_map_viewer()->pixel_to_tile( {pixels.first, pixels.second}));
            if (tile != tile_identifier_old_tile)
            {
                tile_identifier_old_tile = tile;
                std::stringstream position;
                position << "(" << tile.x << ", " << tile.y << ")";

                tile_identifier_text.set_text(position.str());
            }
        }
        tile_identifier_text.display();

        //Only show SDL cursor on rapsberry pi, not required on desktop
        #ifdef USE_GLES
            //Display when mouse is over the SDL widget
            if (showMouse) {cursor->display();};
        #endif

        VLOG(3) << "} TD | SB {";
        challenge_data->game_window->swap_buffers();
    }
    else
    {
        em->flush_and_disable();
        delete challenge;
        em->reenable();

        challenge_data->run_challenge = true;
        challenge = pick_challenge(challenge_data);
        Engine::set_challenge(challenge);
        challenge->start();
        callbackstate.stop();
        //Update tool bar here
        //embedWindow.get_cur_game_init()->getMainWin()->updateToolBar();

    }
    return;
}

Challenge* GameMain::pick_challenge(ChallengeData* challenge_data)
{
    int next_challenge(challenge_data->next_challenge);
    Challenge *challenge(nullptr);
    std::string map_name = "";
    switch(next_challenge)
    {
    case 0:
        map_name ="../maps/start_screen.tmx";
        challenge_data->map_name = map_name;
        challenge = new StartScreen(challenge_data);
        break;
    case 1:
        map_name = "../maps/introduction.tmx";
        challenge_data->map_name = map_name;
        challenge = new IntroductionChallenge(challenge_data);
        break;
    case 2:
        map_name = "../maps/cutting_challenge.tmx";
        challenge_data->map_name = map_name;
        challenge = new CuttingChallenge(challenge_data);
        break;
    case 3:
        map_name = "../maps/new_challenge.tmx";
        challenge_data->map_name = map_name;
        challenge = new NewChallenge(challenge_data);
        break;
    default:
        break;
    }
    return challenge;
}

GameWindow* GameMain::getGameWindow()
{
    return &embedWindow;
}

CallbackState GameMain::getCallbackState(){
    return callbackstate;
}

std::chrono::steady_clock::time_point GameMain::get_start_time(){
    return start_time;
}

