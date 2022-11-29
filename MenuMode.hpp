#include "Mode.hpp"

#include "HUD.hpp"
#include "Text.hpp"

#include <glm/glm.hpp>

#include <list>
#include <vector>

struct MenuMode : Mode
{
    MenuMode();
    virtual ~MenuMode();

    // functions called by main loop:
    virtual bool handle_event(SDL_Event const &, glm::uvec2 const &window_size) override;
    virtual void update(float elapsed) override;
    virtual void draw(glm::uvec2 const &drawable_size) override;

    //----- game state -----

    // input tracking:
    struct Button
    {
        uint8_t downs = 0;
        uint8_t pressed = 0;
    } enter, back;
    glm::vec2 mouse_motion{0.f, 0.f};

    HUD::Sprite *window;
    HUD::Sprite *play_button;
    HUD::Sprite *exit_button;

    glm::vec2 target_xy;

    std::unordered_map<Button *, std::vector<int>> keybindings = {
        // association between action/button and list of keybindings
        {&enter, {SDLK_RETURN}},
        {&back, {SDLK_ESCAPE}},
    };

    Text start_menu_text;
    Text pause_menu_text;
};
