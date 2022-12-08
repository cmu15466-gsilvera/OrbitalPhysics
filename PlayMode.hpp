#include "GL.hpp"
#include "Mode.hpp"

#include "OrbitalMechanics.hpp"
#include "Skybox.hpp"
#include "FancyPlanet.hpp"

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
	void exit_to_menu();

	//------ serialization -------
	const std::string quicksave_file = "quicksave.txt";

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
	} left, right, down, up, tab, shift, control, tilde, plus, minus, space, menu, f5, f9, save, load;
	glm::vec2 mouse_motion_rel{0.f, 0.f};
	glm::vec2 mouse_motion{0.f, 0.f};
	bool can_pan_camera = false; // true when mouse down
	glm::uvec2 window_dims;
	HUD::ButtonSprite *menu_button = nullptr;

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
	HUD::Sprite *buttons;
	HUD::Sprite *lasercooldown;
	const Entity *target_lock = nullptr;

	glm::vec2 target_xy;

	std::unordered_map< Button *, std::vector< int > > keybindings = {
		// association between action/button and list of keybindings
		{ &left, {SDLK_LEFT, SDLK_a} },
		{ &right, {SDLK_RIGHT, SDLK_d} },
		{ &up, {SDLK_UP, SDLK_w} },
		{ &down, {SDLK_DOWN, SDLK_s} },
		{ &tab, {SDLK_TAB} },
		{ &save, {SDLK_2} },
		{ &load, {SDLK_3} },
		{ &tilde, {SDLK_BACKQUOTE} },
		{ &shift, {SDLK_LSHIFT} }, // and rshift?
		{ &control, {SDLK_LCTRL} },
		{ &plus, {SDLK_e, SDLK_PLUS} },
		{ &minus, {SDLK_q, SDLK_MINUS} },
		{ &space, {SDLK_SPACE} },
		{ &menu, {SDLK_ESCAPE, SDLK_1} },
		{ &f5, {SDLK_F5} },
		{ &f9, {SDLK_F9} }
	};

	//local copy of the game scene (so code can change it during gameplay):
	Scene scene;

	// reticle for laser aiming
	const float reticle_radius_screen = 0.1f; // 10% of the window-size
	const float homing_threshold = 0.1f; // 1% of the window-size for homing threshold
	glm::vec2 reticle_aim{0.f, 0.f}; // (0, 0) in center, (1, 1) top right, (-1, -1) bottom left
	glm::dvec3 world_target{0., 0., 0.}; // where (in world space) is the reticle target
	bool reticle_homing = false;

	// spaceship
	Rocket spaceship;

	// spaceship fuel
	std::vector<Particle*> fuel_pellets;
	int laser_closeness_for_particles = 40; // percent threshold that the laser needs to have to have effect on particles

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
	std::list< FancyPlanet > fancyPlanets;
    Skybox skybox = Skybox();
	std::vector< HUD::Sprite* > sprites;

	// game stuff
	enum GameStatus : uint8_t {
		PLAYING=0,
		WIN,
		LOSE,
	};
	GameStatus game_status = GameStatus::PLAYING;
	Text GameOverText;
	float anim = 0.f;

	bool bLevelLoaded = false;
	bool bIsTutorial = false;
	struct TutorialState {
		TutorialState(const std::vector<PlayMode::Button *> &bs, const std::string &t) : activations(bs), text(t) {}
		std::vector<PlayMode::Button*> activations;
		std::string text;
		bool done = false;
	};
	float tut_anim = 0.f;
	std::vector<TutorialState> tutorial_content = {
		// issued as LIFO queue
		{{&minus}, "Your goal is to redirect the rogue asteroid labeled \n\nwith the red reticle. When you get close enough\n\naim your laser and shoot it with the space key\n\nFinish this tutorial by pressing the (-) minus key"},
		{{&down, &control}, "There you go, now decrease/negating thrust with S/CTRL"},
		{{&up, &shift}, "Epic. Now apply forward thrust by pressing W/shift"},
		{{&right}, "Now rotate clockwise by pressing D"},
		{{&space}, "Nice job! Press space to move on."},
		{{&left}, "Good. Now rotate the ship counter-clockwise by holding A"},
		{{&tilde}, "Nice! Now refocus on the spaceship by pressing tilde"},
		{{&tab}, "Change the camera focus by hovering over the \n\ncenter of another planet/entity and pressing TAB"},
		{{&space}, "Welcome to the tutorial!\n\nLook around with click+drag and scroll to zoom\n\nPress space to begin the rest of the tutorial"},
	};
	Text tutorial_text;

	//camera:
	Scene::Camera *camera = nullptr;
	struct CameraArm { //Encapsulated so that we can have individually tracked views per body later on

		CameraArm() = delete; // must pass in an Entity to focus on!
		CameraArm(const Entity *e) : entity(e) {}

		const Entity *entity = nullptr;

		static float constexpr ScrollSensitivity = 3.5f;

		//Controls position
		static constexpr float init_radius_multiples = 20.0f;
		float camera_arm_length = init_radius_multiples;

		inline static glm::vec3 camera_pan_offset{1.0f, 1.0f, 1.0f}; // must not be zeros
		inline const glm::vec3 get_focus_point() const { // for smooth transitions
			return entity->pos;
		};

		inline const glm::vec3 get_target_point() const { // for smooth transitions
			return get_focus_point() + static_cast< float>(camera_arm_length * entity->radius) * camera_pan_offset;
		};
	};

	std::unordered_map< const Entity*, CameraArm > camera_arms;
	glm::vec3 camera_transition{0.f};
	float camera_transition_interp = 0.f;
	glm::quat camera_start_rot, camera_end_rot;
	const Entity *current_focus_entity = nullptr;

	CameraArm &CurrentCameraArm() {
		if (current_focus_entity == nullptr)
			throw std::runtime_error("No current focus entity for camera!");
		if (camera_arms.find(current_focus_entity) == camera_arms.end())
			throw std::runtime_error("Current camera entity not in map!");
		return camera_arms.at(current_focus_entity);
	}

	// text UI
	Text ThrottleHeader;
	Text ThrottleReading;
	Text SpeedupReading;
	Text CollisionHeader;
	Text CollisionTimer;
	Text LaserText;

	// bgm
	std::shared_ptr< Sound::PlayingSample > bgm_loop;
};
