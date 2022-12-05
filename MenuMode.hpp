#include "Mode.hpp"

#include "HUD.hpp"
#include "Text.hpp"
#include "Skybox.hpp"

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
	} one, two, three, back;

	glm::vec2 mouse_motion{0.f, 0.f};
	bool clicked = false;
	Skybox skybox = Skybox();
	Scene::Camera *camera = nullptr;
	glm::vec3 random_direction;

	HUD::ButtonSprite *play_button0, *play_button1, *play_button2, *tutorial_button, *exit_button;
	std::list< HUD::ButtonSprite  > buttons;

	glm::vec2 target_xy;

	std::unordered_map<Button *, std::vector<int>> keybindings = {
		// association between action/button and list of keybindings
		{&one, {SDLK_1}},
		{&two, {SDLK_2}},
		{&three, {SDLK_3}},
		{&back, {SDLK_ESCAPE}},
	};

	Text start_menu_text;
	Text menu_text_1;
};
