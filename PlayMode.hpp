#include "GL.hpp"
#include "Mode.hpp"

#include "OrbitalMechanics.hpp"
#include "Skybox.hpp"

#include "Mesh.hpp"
#include "Scene.hpp"
#include "Sound.hpp"
#include "Text.hpp"
#include "HUD.hpp"

#include <glm/glm.hpp>

#include <list>
#include <fstream>
#include <vector>
#include <unordered_map>

struct PlayMode : Mode {
	PlayMode();
	virtual ~PlayMode();

	//functions called by main loop:
	virtual bool handle_event(SDL_Event const &, glm::uvec2 const &window_size) override;
	virtual void update(float elapsed) override;
	virtual void draw(glm::uvec2 const &drawable_size) override;

	void serialize(std::string const &filename);
	void serialize_orbit(std::ofstream &file, Orbit const &orbit);
	void serialize_body(std::ofstream &file, Body const &body);
	void serialize_rocket(std::ofstream &file);
	void serialize_asteroid(std::ofstream &file);

	void deserialize(std::string const &filename);
	void deserialize_orbit(std::string const &line, std::list< Orbit > &olist);
	void deserialize_body(std::ifstream &file);
	void deserialize_rocket(std::ifstream &file);
	void deserialize_asteroid(std::ifstream &file);

	//----- game state -----

	//input tracking:
	struct Button {
		uint8_t downs = 0;
		uint8_t pressed = 0;
	} left, right, down, up, tab, shift, control, plus, minus, space, menu, f5, f9;
	glm::vec2 mouse_motion_rel{0.f, 0.f};
	glm::vec2 mouse_motion{0.f, 0.f};
	bool can_pan_camera = false; // true when mouse down
	glm::uvec2 window_dims;

	GLuint hdrFBO = 0;
	GLuint rboDepth = 0;
	GLuint attachments[2];
	GLuint colorBuffers[2];
    unsigned int pingpongFBO[2];
    unsigned int pingpongColorbuffers[2];
	GLuint renderQuadVAO = 0;
	GLuint renderQuadVBO;
	void RenderFrameQuad();

	void SetupFramebuffers();
	bool framebuffer_ready = false; // needs to initialize

	HUD::Sprite *throttle;
	HUD::Sprite *throttleOverlay;
	HUD::Sprite *clock;
	HUD::Sprite *timecontroller;
	HUD::Sprite *handle;
	HUD::Sprite *bar;
	HUD::Sprite *target;
	HUD::Sprite *reticle;

	glm::vec2 target_xy;

	std::unordered_map< Button *, std::vector< int > > keybindings = {
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
		{ &space, {SDLK_SPACE} },
		{ &menu, {SDLK_ESCAPE} },
		{ &f5, {SDLK_F5} },
		{ &f9, {SDLK_F9} }
	};

	//local copy of the game scene (so code can change it during gameplay):
	Scene scene;

	// reticle for laser aiming
	const float reticle_radius_screen = 0.1f; // 10% of the window-size
	const float homing_threshold = 0.1f; // 1% of the window-size for homing threshold
	glm::vec2 reticle_aim{0.f, 0.f}; // (0, 0) in center, (1, 1) top right, (-1, -1) bottom left
	bool reticle_homing = false;

	// spaceship
	Rocket spaceship;

	bool bEnableEasyMode = true; // allow "negative thrust" to correct for over-orbit
	bool bCanThrustChangeDir = true; // whether or not we can thrust
	bool forward_thrust = true; // false => backwards thrust controls

	// asteroid
	Asteroid asteroid = Asteroid(0.5f, 0.2f);

	// other solar system bodies
	Body *star = nullptr; //All body updates cascade off of star update, should be done prior to spaceship update
	std::list< Entity* > entities; // bodies + rocket(s)
	std::list< Body > bodies;
	std::list< Orbit > orbits;
	std::unordered_map< int, Body * > id_to_body;
    Skybox skybox = Skybox();
	std::vector< HUD::Sprite* > sprites;

	//camera:
	Scene::Camera *camera = nullptr;
	struct CameraArm { //Encapsulated so that we can have individually tracked views per body later on

		CameraArm() = delete; // must pass in an Entity to focus on!
		CameraArm(const Entity *e) : entity(e) {}
		void update(Scene::Camera *camera, float mouse_x, float mouse_y);

		const Entity *entity = nullptr;

		static float constexpr ScrollSensitivity = 0.25f;
		static float constexpr MouseSensitivity = 5.0f;

		//Controls position
		glm::vec3 camera_offset{0.0f, 1.0f, 1.0f};
		static constexpr float init_scroll_zoom = 10.0f;
		float scroll_zoom = init_scroll_zoom;
		float camera_arm_length = 20.0f;
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
	Text ThrottleHeader;
	Text ThrottleReading;
	Text SpeedupReading;

	// bgm
	std::shared_ptr< Sound::PlayingSample > bgm_loop;
};
