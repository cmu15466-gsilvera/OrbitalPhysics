#include "EmissiveShaderProgram.hpp"
#include "GL.hpp"
#include "LitColorTextureProgram.hpp"
#include "MenuMode.hpp"
#include "Utils.hpp"

#include "DrawLines.hpp"
#include "data_path.hpp"
#include "gl_errors.hpp"

#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/norm.hpp>
#include <glm/gtx/string_cast.hpp>

#include <iomanip>
#include <iterator>
#include <sstream>

#define DEBUG

#ifdef DEBUG
#include <iostream>
#define LOG(ARGS) std::cout << ARGS << std::endl
#else
#define LOG(ARGS)
#endif

MenuMode::MenuMode()
{
    window = HUD::loadSprite(data_path("window.png"));
	play_button = HUD::loadSprite(data_path("button.png"));
	exit_button = HUD::loadSprite(data_path("button.png"));
	
    { // load text
        start_menu_text.init(Text::AnchorType::CENTER);
		pause_menu_text.init(Text::AnchorType::CENTER);
    }
}

MenuMode::~MenuMode()
{
    free(play_button);
    free(exit_button);
	free(window);
}

bool MenuMode::handle_event(SDL_Event const &evt, glm::uvec2 const &window_size)
{
    if (evt.type == SDL_KEYDOWN)
    {
        bool was_key_down = false;
        for (auto &key_action : keybindings)
        {
            Button &button = *key_action.first;
            const std::vector<int> &keys = key_action.second;
            for (int keysym : keys)
            {
                if (evt.key.keysym.sym == keysym)
                {
                    button.pressed = true;
                    button.downs += 1;
                    was_key_down = true;
                }
            }
        }
        if (evt.key.keysym.sym == SDLK_ESCAPE)
        {
            SDL_SetRelativeMouseMode(SDL_FALSE);
            was_key_down = true;
        }
        return was_key_down;
    }
    else if (evt.type == SDL_KEYUP)
    {
        bool was_key_up = false;
        for (auto &key_action : keybindings)
        {
            Button &button = *key_action.first;
            const std::vector<int> &keys = key_action.second;
            for (int keysym : keys)
            {
                if (evt.key.keysym.sym == keysym)
                {
                    button.pressed = false;
                    was_key_up = true;
                }
            }
        }
        return was_key_up;
    }
    else if (evt.type == SDL_MOUSEBUTTONDOWN)
    {
        SDL_SetRelativeMouseMode(SDL_FALSE);
        return true;
    }
    else if (evt.type == SDL_MOUSEBUTTONUP)
    {
        // show the mouse cursor again once the mouse is released
        SDL_SetRelativeMouseMode(SDL_FALSE);
        return true;
    }
    else if (evt.type == SDL_MOUSEMOTION)
    {
        {
            // mouse_motion is (1, 1) in top right, (-1, -1) in bottom left
            mouse_motion = 2.f * glm::vec2(evt.motion.x / float(window_size.x) - 0.5f,
                                           -evt.motion.y / float(window_size.y) + 0.5f);
            return true;
        }
    }
    return false;
}


void MenuMode::update(float elapsed)
{
    // reset button press counters:
    for (auto &keybinding : keybindings)
    {
        Button &button = *keybinding.first;
        button.downs = 0;
    }
}

void MenuMode::draw(glm::uvec2 const &drawable_size)
{
    // set up light type and position for lit_color_texture_program:
    //  TODO: consider using the Light(s) in the scene to do this
    glUseProgram(lit_color_texture_program->program);
    glUseProgram(0);

    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClearDepth(1.0f); // 1.0 is actually the default value to clear the depth buffer to, but FYI you can change it.
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS); // this is the default depth comparison function, but FYI you can change it.

    GL_ERRORS();
}
