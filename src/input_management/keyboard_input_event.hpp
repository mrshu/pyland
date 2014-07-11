#ifndef KEYBOARD_INPUT_EVENT_H
#define KEYBOARD_INPUT_EVENT_H



#include <set>
#include <functional>

#include "SDL2/SDL.h"

#include "input_event.hpp"



class InputManager;

///
/// Keyboard class for input event description.
///
struct KeyboardInputEvent : public InputEvent {
public:
    typedef std::function<void(KeyboardInputEvent)> Handler;
    
    KeyboardInputEvent(InputManager* manager, int scan_code, bool down, bool changed, bool repeated);
    int scan_code;
    int key_code;
    bool down;
    bool changed;
    bool repeated;

    // namespace Filter {
    //     static Handler no_repeat(const std::set<int> keys, const Handler handler)
    // }
    // static Handler if_key_in(const std::set<int> keys, const Handler handler) {
    //     return [&] (KeyboardInputEvent event) {
    //         if (keys.event.key_code) 
                
    //         return event;
    //     }
    // }
};



#endif
