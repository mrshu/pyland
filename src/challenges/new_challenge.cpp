#include "api.hpp"
#include "base_challenge.hpp"
#include "challenge.hpp"
#include "challenge_data.hpp"
#include "challenge_helper.hpp"
#include "engine.hpp"
#include "entitythread.hpp"
#include "interpreter.hpp"
#include "make_unique.hpp"
#include "map.hpp"
#include "map_object.hpp"
#include "map_viewer.hpp"
#include "new_challenge.hpp"
#include "notification_bar.hpp"
#include "object_manager.hpp"
#include "sprite.hpp"

namespace py = boost::python;

NewChallenge::NewChallenge(ChallengeData* _challenge_data) : Challenge(_challenge_data) {

    //add the monkey to the game
    int monkey_id = ChallengeHelper::make_sprite(
        this,
        "sprite/monkey",
        "Alex",
        Walkability::BLOCKED,
        "east/still/1"
    );

    int player_id = ChallengeHelper::make_sprite(
        this,
        "sprite/1",
        "Ben",
        Walkability::BLOCKED,
        "east/still/1"
    );

    ChallengeHelper::make_interaction("trigger/objective/finish", [this] (int) {
        ChallengeHelper::set_completed_level(4);
        finish();
        return false;
    });

    //trigger/objective/button1
    //trigger/objective/button4
    //trigger/objective/door1
    //The following code is for door 1 and the buttons that activate it.
    int door_1_id = ChallengeHelper::make_object(this, "trigger/objective/door1", Walkability::BLOCKED, "4");

    ChallengeHelper::make_interaction("trigger/objective/button1", [this] (int) {

        return true;
    });

    ChallengeHelper::make_interaction("trigger/objective/button4", [this] (int) {

        return true;
    });

}


NewChallenge::~NewChallenge() {

}

void NewChallenge::start() {
Engine::print_dialogue ( "Tom",
        "Welcome to my new level\n"
    );
}

void NewChallenge::finish() {
   //Complete the challenge
   //TODO: Change this to use your challenge's id
   int challenge_id = 4;
   ChallengeHelper::set_completed_level(challenge_id);

   //Return to the start screen
   event_finish.trigger(0);
}
