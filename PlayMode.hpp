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
	std::string quicksave_file = "quicksave.txt";

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

	const std::string params_file = "params.ini";
	void read_params();
	float ambient_light = 0.1f; // 1.0f is 100% ambient (easier to see), 0.f is 0% (more realistic)

	//input tracking:
	struct Button {
		uint8_t downs = 0;
		uint8_t pressed = 0;
	} left, right, down, up, tab, shift, control, tilde, plus, minus, space, menu, f5, f9, save, load, refresh;
	glm::vec2 mouse_motion_rel{0.f, 0.f};
	glm::vec2 mouse_motion{0.f, 0.f};
	bool can_pan_camera = false; // true when mouse down
	glm::uvec2 window_dims;
	HUD::ButtonSprite *menu_button = nullptr;

	Text fps_text;
	bool bShowFPS = true;
	std::vector<float> fps_data = {};
	float fps = 60.f;
	float time_since_fps = 0.f;

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
		{ &refresh, {SDLK_r} },
		{ &tilde, {SDLK_BACKQUOTE} },
		{ &shift, {SDLK_LSHIFT, SDLK_RSHIFT} },
		{ &control, {SDLK_LCTRL, SDLK_RCTRL} },
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
	std::list< Particle > fuel_pellets;
	std::list< Particle > debris_pellets;
	size_t fuel_particle_count = 30;
	size_t debris_particle_count = 10;
	int laser_closeness_for_particles = 40; // percent threshold that the laser needs to have to have effect on particles

	bool bEnableEasyMode = false; // allow "negative thrust" to correct for over-orbit
	bool bCanThrustChangeDir = true; // whether or not we can thrust
	bool forward_thrust = true; // false => backwards thrust controls

	static double constexpr MaxSimElapsed = 1.0 / 120.0;

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
	bool bLevelLaunched = false;
	bool bIsTutorial = false;
	struct TutorialState {
		TutorialState(const std::vector<PlayMode::Button *> &bs, glm::vec2 p, const std::string &t) : activations(bs), text(t), pos(p) {}
		std::vector<PlayMode::Button*> activations;
		std::string text;
		glm::vec2 pos;
		bool done = false;
	};
	float elapsed_s = 0.f;
	float text_anim_speed = 1.f;
	std::vector<TutorialState> tutorial_content = {
		// issued as FIFO queue
		{{&space}, {0.55f, 0.95f}, "Welcome to the tutorial!\n\nYour goal is to prevent a rogue asteroid collision (green orbit)\n\nthat will happen once the (top left) doomsday clock hits 0\n\n(Press *space* to continue.)"},
		{{&space}, {0.45f, 0.75f}, "Pan around the camera with click+drag and scroll to zoom\n\n(Press *space* to continue)"},
		{{&tab}, {0.45f, 0.35f}, "Change the camera focus by hovering over the \n\ncenter of another planet/entity and pressing *TAB*\n\n(Your target is locked when the reticle is red)"},
		{{&tilde}, {0.45f, 0.75f}, "Refocus to the spaceship by pressing *tilde*"},
		{{&space}, {0.4f, 0.4f}, "In the bottom-left is your rocket's thrust (green) and fuel levels (orange).\n\nMake sure to keep an eye on your fuel consumption!\n\n(Press *space* to continue)"},
		{{&space}, {0.55f, 0.40f}, "You have a high-power-long-reload laser on the\n\nrocket which you can use to push the asteroid\n\nBut it has a long reload time and short-range\n\nThe effectiveness is displayed below when you are locked onto a target\n\nYou usually need above 20%% to be successful\n\n(Press *space* to continue)"},
		{{&plus}, {0.4f, 0.4f}, "Since space is very large, you'll find that most\n\nof your time is spent doing nothing. You can \n\nspeed up that time by changing time dilation\n\nBy orders of magnitude. Press *E* to 10x time speedup\n\n"},
		{{&minus}, {0.4f, 0.4f}, "You can keep pressing *E* several times\n\nPress *Q* to revert time back to a slower rate (1/10x)"},
		{{&space}, {0.7f, 0.3f}, "Note that at any time you can press (1) for the menu\n\nBut you will lose all your progress. Press *space*"},
		{{&save, &load}, {0.7f, 0.3f}, "To keep your progress, you can press (2) to save\n\n and (3) to quick load your latest save\n\nTry it out!"},
		{{&space}, {0.45f, 0.75f}, "Now we'll go through the rocket controls. Press *space*"},
		{{&left}, {0.45f, 0.75f}, "Rotate the ship counter-clockwise by holding *A*"},
		{{&right}, {0.45f, 0.75f}, "Rotate clockwise by pressing *D*"},
		{{&up, &shift}, {0.45f, 0.75f}, "Apply forward thrust by pressing *W/shift*\n\nNotice that the angle you are rotated\n\naffects how your orbit is manipulated with thrust"},
		{{&down, &control}, {0.45f, 0.75f}, "Decrease thrust with *S/CTRL*"},
		{{&space}, {0.45f, 0.75f}, "The yellow lines on your (and asteroid's) orbit\n\nshow the approximated nearest approach position\n\n=> when you and the asteroid will be closest at\n\nthe current orbit trajectories. Press *space*"},
		{{&space}, {0.45f, 0.75f}, "If you see beige pellets, those are fuel packets that you\n\ncan shoot with your laser (>40%%) to regain fuel\n\nBut watch out for the red pellets which damage fuel\n\n(press *space*)"},
		{{&space, &refresh}, {0.45f, 0.75f}, "Finally, you can reload some game settings by pressing *r*\n\n(press *space* to continue)"},
		{{&space}, {0.45f, 0.75f}, "Your goal is to redirect the rogue asteroid labeled \n\nwith the red reticle. When you get close enough (>20%% laser effectiveness)\n\naim your laser and shoot it with the *space* key\n\nFinish this tutorial by pressing *space*"},
	};
	Text tutorial_text;
	glm::vec2 tutorial_locn;

	// logs to terminal
	const std::string readme_txt = "\n\nPlease consult the readme: README.txt for more detailed instructions!\n\n";

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
			return get_focus_point() + camera_arm_length * static_cast< float>(entity->radius) * camera_pan_offset;
		};

		inline const float get_camera_arm_dist() const { // for smooth transitions
			return camera_arm_length * static_cast< float>(entity->radius);
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
	Text asteroid_txt;

	// bgm
	std::shared_ptr< Sound::PlayingSample > bgm_loop;
};
