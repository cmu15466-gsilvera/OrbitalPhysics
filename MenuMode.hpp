#include "Mode.hpp"

#include "HUD.hpp"
#include "Text.hpp"

#include <glm/glm.hpp>

#include <list>
#include <unordered_map>
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
    bool clicked = false;

    struct ButtonSprite
    {
        ButtonSprite(){};
        ~ButtonSprite()
        {
            if (sprite != nullptr)
                delete sprite;
        }
        ButtonSprite(std::string const &sprite_path, glm::u8vec4 const &c0, glm::u8vec4 const &c1, glm::vec2 const &s0,
                     glm::vec2 const &s1, glm::vec2 const &l, std::string const &str = "")
            : color(c0), color_hover(c1), size(s0), size_hover(s1), loc(l)
        {
            sprite = HUD::loadSprite(sprite_path);
            text = new Text();
            text->init(Text::AnchorType::CENTER);
            text->set_text(str);
        }
        glm::u8vec4 color;
        glm::u8vec4 color_hover;
        /// NOTE: location/size is relative to CENTER of sprite
        glm::vec2 size;
        glm::vec2 size_hover;
        glm::vec2 loc;
        Text *text;
        HUD::Sprite *sprite;
        bool bIsHovered = false;
        bool is_hovered(glm::vec2 const &) const;
        void draw(glm::vec2 const &) const;
    };

    HUD::Sprite *window;
    ButtonSprite play_button, exit_button;
    std::vector<ButtonSprite *> buttons;

    glm::vec2 target_xy;

    std::unordered_map<Button *, std::vector<int>> keybindings = {
        // association between action/button and list of keybindings
        {&enter, {SDLK_RETURN}},
        {&back, {SDLK_ESCAPE}},
    };

    Text start_menu_text;
    Text menu_text_1;
};
