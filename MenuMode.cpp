#include "MenuMode.hpp"
#include "EmissiveShaderProgram.hpp"
#include "GL.hpp"
#include "LitColorTextureProgram.hpp"
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
#include <ctime>

#define DEBUG

#ifdef DEBUG
#include <iostream>
#define LOG(ARGS) std::cout << ARGS << std::endl
#else
#define LOG(ARGS)
#endif

MenuMode::MenuMode()
{
	{ // initialize scene camera for skybox
		std::srand((unsigned int)std::time(nullptr));
		Scene::Transform *camera_trans = new Scene::Transform();
		camera = new Scene::Camera(camera_trans);
		auto random = [](){return std::rand() * 2.f - 1.0f;};
		random_direction = glm::vec3(random(), random(), random());
		random_direction = glm::normalize(random_direction);
	}

	window = HUD::loadSprite(data_path("assets/ui/window.png"));
	{ // play buttons
		glm::vec2 size0 = glm::vec2(0.15f, 0.09f);
		glm::vec2 size1 = 1.1f * size0;
		glm::vec2 location = glm::vec2(0.25f, 0.45f);

		const glm::u8vec4 button_0_col{0x33, 0x66, 0x11, 0x55};
		const glm::u8vec4 button_0_col_hover{0x33, 0x66, 0x11, 0x66};
		buttons.emplace_back(data_path("assets/ui/sqr.png"), button_0_col, button_0_col_hover, size0, size1, location, "Level 1");
		play_button0 = &buttons.back();

		const glm::u8vec4 button_1_col{0x55, 0x44, 0x11, 0x55};
		const glm::u8vec4 button_1_col_hover{0x55, 0x44, 0x11, 0x66};
		buttons.emplace_back(data_path("assets/ui/sqr.png"), button_1_col, button_1_col_hover, size0, size1, location - glm::vec2(0, size0.y * 1.5f), "Level 2");
		play_button1 = &buttons.back();

		const glm::u8vec4 button_2_col{0x77, 0x22, 0x11, 0x55};
		const glm::u8vec4 button_2_col_hover{0x77, 0x22, 0x11, 0x66};
		buttons.emplace_back(data_path("assets/ui/sqr.png"), button_2_col, button_2_col_hover, size0, size1, location - glm::vec2(0, size0.y * 3.0f), "Level 3");
		play_button2 = &buttons.back();
	}

	{ // exit button
		const glm::u8vec4 button_color{0x66, 0x33, 0x11, 0x55};
		const glm::u8vec4 button_color_hover{0x66, 0x33, 0x11, 0x66};
		glm::vec2 size0 = glm::vec2(0.15f, 0.09f);
		glm::vec2 size1 = 1.1f * size0;
		glm::vec2 location = glm::vec2(0.75f, 0.25f);

		buttons.emplace_back(data_path("assets/ui/sqr.png"), button_color, button_color_hover, size0, size1, location, "Exit");
		exit_button = &buttons.back();
	}

	{ // load text
		start_menu_text.init(Text::AnchorType::CENTER);
		menu_text_1.init(Text::AnchorType::CENTER);
	}
}

MenuMode::~MenuMode()
{
	free(window);
}

bool MenuMode::handle_event(SDL_Event const &evt, glm::uvec2 const &window_size)
{
	HUD::SCREEN_DIM = window_size;
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
		clicked = true;
		return true;
	}
	else if (evt.type == SDL_MOUSEBUTTONUP)
	{
		// show the mouse cursor again once the mouse is released
		SDL_SetRelativeMouseMode(SDL_FALSE);
		clicked = false;
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

bool MenuMode::ButtonSprite::is_hovered(glm::vec2 const &mouse) const
{
	glm::vec2 mouse_abs = ((mouse / 2.f) + 0.5f); // to screen percentage
	bool within_x = loc.x - size.x * 0.5f <= mouse_abs.x && mouse_abs.x <= loc.x + size.x * 0.5f;
	bool within_y = loc.y - size.y * 0.5f <= mouse_abs.y && mouse_abs.y <= loc.y + size.y * 0.5f;
	return within_x && within_y;
}

void MenuMode::ButtonSprite::draw(glm::vec2 const &drawable_size) const
{
	auto _color = bIsHovered ? color_hover : color;
	glm::vec2 _size = bIsHovered ? size_hover : size;
	glm::vec2 top_left_loc = glm::vec2{loc.x - _size.x * 0.5f, loc.y + _size.y * 0.5f} * drawable_size;
	if (text != nullptr && text->text_content.size() > 0)
	{
		glm::vec2 center_loc = top_left_loc + 0.5f * glm::vec2{_size.x, -1.2f * _size.y} * drawable_size;
		text->draw(1.f, drawable_size, _size.x, center_loc, 1.f, glm::u8vec4{0xff});
	}
	HUD::drawElement(_size * drawable_size, top_left_loc, sprite, _color);
}

void MenuMode::update(float elapsed)
{
	{ // update text UI
		std::stringstream stream;
		stream << "Project Athena";
		// stream << "a" << std::endl << "b";
		start_menu_text.set_text(stream.str());
	}

	{ // update text UI
		std::stringstream stream;
		stream << "Press or click (1) (2) or (3) to start a level";
		// stream << "a" << std::endl << "b";
		menu_text_1.set_text(stream.str());
	}

	{ // all hover checks for buttons
		for (auto &button : buttons)
		{
			button.bIsHovered = button.is_hovered(mouse_motion);
		}
	}

	{ // check to advance
		if (one.pressed || (play_button0->bIsHovered && clicked))
		{
			Mode::set_current(next_mode);
			next_mode->mode_level = 0;
		}
		else if (two.pressed || (play_button1->bIsHovered && clicked))
		{
			Mode::set_current(next_mode);
			next_mode->mode_level = 1;
		}
		else if (three.pressed || (play_button2->bIsHovered && clicked))
		{
			Mode::set_current(next_mode);
			next_mode->mode_level = 2;
		}

		if (back.pressed || (exit_button->bIsHovered && clicked))
		{
			// not sure if this is the best way to exit the game?
			this->finish = true;
			LOG("Goodbye!");
			return;
		}
	}

	{ // update camera for skybox
		camera->transform->rotation *= glm::quat(0.001f * random_direction);
	}

	// reset button press counters:
	for (auto &keybinding : keybindings)
	{
		Button &button = *keybinding.first;
		button.downs = 0;
		button.pressed = false;
	}
	clicked = false;
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

	{ // start text
		float x = drawable_size.x * 0.5f;
		float y = drawable_size.y * 0.9f; // top is 1.f bottom is 0.f
		float width = drawable_size.x * 0.6f;
		start_menu_text.draw(1.f, drawable_size, width, glm::vec2(x, y), 1.4f, glm::u8vec4{0xff});
	}

	{ // other text
		float x = drawable_size.x * 0.5f;
		float y = drawable_size.y * 0.6f; // top is 1.f bottom is 0.f
		float width = drawable_size.x * 0.5f;
		menu_text_1.draw(1.f, drawable_size, width, glm::vec2(x, y), 1.f, glm::u8vec4{0xff});
	}

	{ // menu buttons
		for (auto &button : buttons)
			button.draw(drawable_size);
	}

	{
		skybox.draw(camera);
	}

	GL_ERRORS();
}
