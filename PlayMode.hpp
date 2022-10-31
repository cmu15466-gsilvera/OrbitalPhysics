#include "Mode.hpp"

#include "OrbitalMechanics.hpp"

#include "Scene.hpp"
#include "Sound.hpp"

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
	Body *star; //All body updates cascade off of star update, should be done prior to spaceship update
	std::list< Body > bodies;
	std::list< Orbit > orbits;

	//camera:
	Scene::Camera *camera = nullptr;
	struct CameraArm { //Encapsulated so that we can have individually tracked views per body later on
		static float constexpr ScrollSensitivity = 30.0f;
		static float constexpr MouseSensitivity = 5.0f;

		//Controls position
		glm::vec3 camera_offset{0.0f, 10.0f, 10.0f};
		float scroll_zoom = 0.0f;

		// track locations and radii
		std::vector< std::pair< glm::vec3 *, float > > focus_points;
		size_t camera_view_idx = 0;
	};
	CameraArm camera_arm;

	void update_camera_view();

};
