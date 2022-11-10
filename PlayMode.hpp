#include "Mode.hpp"

#include "OrbitalMechanics.hpp"

#include "Mesh.hpp"
#include "Scene.hpp"
#include "Sound.hpp"
#include "Text.hpp"
#include "HUD.hpp"

#include <glm/glm.hpp>

#include <list>
#include <vector>

struct PlayMode : Mode {
	PlayMode();
	virtual ~PlayMode();

	//functions called by main loop:
	virtual bool handle_event(SDL_Event const &, glm::uvec2 const &window_size) override;
	virtual void update(float elapsed) override;
	virtual void draw(glm::uvec2 const &drawable_size) override;

	//----- game state -----

	//input tracking:
	struct Button {
		uint8_t downs = 0;
		uint8_t pressed = 0;
	} left, right, down, up, tab, shift, control, plus, minus;
	glm::vec2 mouse_motion_rel{0.f, 0.f};

	HUD::Sprite *throttle;
	HUD::Sprite *handle;
	HUD::Sprite *bar;

	std::unordered_map<Button*, std::vector<int>> keybindings = {
		// association between action/button and list of keybindings
        { &left, {SDLK_LEFT, SDLK_a} },
        { &right, {SDLK_RIGHT, SDLK_d} },
        { &up, {SDLK_UP, SDLK_w} },
        { &down, {SDLK_DOWN, SDLK_s} },
        { &tab, {SDLK_TAB} },
        { &shift, {SDLK_LSHIFT} }, // and rshift?
        { &control, {SDLK_LCTRL} },
        { &plus, {SDLK_e, SDLK_PLUS} },
		{ &minus, {SDLK_q, SDLK_MINUS} },
		/// TODO: add SDL_ESCAPE for quit?
    };

	//local copy of the game scene (so code can change it during gameplay):
	Scene scene;

	//hexapod leg to wobble:
	// Scene::Transform *hip = nullptr;
	// Scene::Transform *upper_leg = nullptr;
	// Scene::Transform *lower_leg = nullptr;
	// glm::quat hip_base_rotation;
	// glm::quat upper_leg_base_rotation;
	// glm::quat lower_leg_base_rotation;
	// float wobble = 0.0f;

	// glm::vec3 get_leg_tip_position();

	//music coming from the tip of the leg (as a demonstration):
	// std::shared_ptr< Sound::PlayingSample > leg_tip_loop;

	Rocket spaceship;
	Asteroid asteroid = Asteroid(1.0f, 0.2f); //TODO: reduce asteroid radius and scale down model
	Body *star; //All body updates cascade off of star update, should be done prior to spaceship update
	std::list< Entity* > entities; // bodies + rocket(s)
	std::list< Body > bodies;
	std::list< Orbit > orbits;

	//camera:
	Scene::Camera *camera = nullptr;
	struct CameraArm { //Encapsulated so that we can have individually tracked views per body later on

		CameraArm() = delete; // must pass in an Entity to focus on!
		CameraArm(const Entity *e) : entity(e) {}
		void update(Scene::Camera *camera, float mouse_x, float mouse_y);

		const Entity *entity = nullptr;

		static float constexpr ScrollSensitivity = 30.0f;
		static float constexpr MouseSensitivity = 5.0f;

		//Controls position
		glm::vec3 camera_offset{0.0f, 1.0f, 1.0f};
		float scroll_zoom = 10.0f;
	};

	std::unordered_map< const Entity*, CameraArm > camera_arms;
	std::vector< const Entity* > camera_views;
	size_t camera_view_idx = 0;

	CameraArm &CurrentCameraArm() {
		if (camera_view_idx > camera_views.size() || camera_view_idx > camera_arms.size())
			throw std::runtime_error("camera view index " + std::to_string(camera_view_idx) + " oob");
		const Entity* entity = camera_views[camera_view_idx];
		if (camera_arms.find(entity) == camera_arms.end())
			throw std::runtime_error("camera entity " + std::to_string(camera_view_idx) + " not in arm map");
		return camera_arms.at(entity);
	}

	// text UI
	Text UI_text;

};
