#include "PlayMode.hpp"
#include "EmissiveShaderProgram.hpp"
#include "GL.hpp"
#include "LitColorTextureProgram.hpp"
#include "FrameQuadProgram.hpp"
#include "BloomBlurProgram.hpp"
#include "OrbitalMechanics.hpp"
#include "Utils.hpp"

#include "DrawLines.hpp"
#include "Load.hpp"
#include "Scene.hpp"
#include "gl_errors.hpp"
#include "data_path.hpp"

#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/string_cast.hpp>
#include <glm/gtx/norm.hpp>

#include <iomanip>
#include <iterator>
#include <sstream>
#include <filesystem>
#include <string>

#define DEBUG

#ifdef DEBUG
#include <iostream>
#define LOG(ARGS) std::cout << ARGS << std::endl
#else
#define LOG(ARGS)
#endif

Load< Scene::RenderSet > main_meshes_emissives(LoadTagDefault, []() -> Scene::RenderSet const * {
	Scene::RenderSet *renderSet = new Scene::RenderSet();
	MeshBuffer const *ret = new MeshBuffer(data_path("assets/model/orbit.pnct"));
	renderSet->vao = ret->make_vao_for_program(emissive_program_pipeline.program);
	renderSet->meshes = ret;
	renderSet->pipeline = emissive_program_pipeline;

	return renderSet;
});


Load< Scene::RenderSet > main_meshes(LoadTagDefault, []() -> Scene::RenderSet const * {
	Scene::RenderSet *renderSet = new Scene::RenderSet();
	MeshBuffer const *ret = new MeshBuffer(data_path("assets/model/orbit.pnct"));
	renderSet->vao = ret->make_vao_for_program(lit_color_texture_program->program);
	renderSet->meshes = ret;
	renderSet->pipeline = lit_color_texture_program_pipeline;

	return renderSet;
});

Load< Scene > orbit_scene(LoadTagDefault, []() -> Scene const * {
	return new Scene(data_path("assets/model/orbit.scene"), [&](Scene &s, Scene::Transform *t, std::string const &m){
		//Drawables will be set up later, dummy function
	});
});

Load< Sound::Sample > bgm(LoadTagDefault, []() -> Sound::Sample const * {
	return new Sound::Sample(data_path("assets/sound/bgm.wav"));
});

void PlayMode::SetupFramebuffers(){
	 // configure (floating point) framebuffers
    // ---------------------------------------
    glGenFramebuffers(1, &hdrFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, hdrFBO);
    // create 2 floating point color buffers (1 for normal rendering, other for brightness threshold values)
    glGenTextures(2, colorBuffers);
    for (unsigned int i = 0; i < 2; i++)
    {
        glBindTexture(GL_TEXTURE_2D, colorBuffers[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16, window_dims.x, window_dims.y, 0, GL_RGBA, GL_FLOAT, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);  // we clamp to the edge as the blur filter would otherwise sample repeated texture values!
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        // attach texture to framebuffer
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i, GL_TEXTURE_2D, colorBuffers[i], 0);
    }
    // create and attach depth buffer (renderbuffer)
    glGenRenderbuffers(1, &rboDepth);
    glBindRenderbuffer(GL_RENDERBUFFER, rboDepth);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, window_dims.x, window_dims.y);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rboDepth);
    // tell OpenGL which color attachments we'll use (of this framebuffer) for rendering 
    attachments[0] = GL_COLOR_ATTACHMENT0;
    attachments[1] = GL_COLOR_ATTACHMENT1;
    glDrawBuffers(2, attachments);
    // finally check if framebuffer is complete
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        std::cout << "Framebuffer not complete!" << std::endl;
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // ping-pong-framebuffer for blurring
    glGenFramebuffers(2, pingpongFBO);
    glGenTextures(2, pingpongColorbuffers);
    for (unsigned int i = 0; i < 2; i++)
    {
        glBindFramebuffer(GL_FRAMEBUFFER, pingpongFBO[i]);
        glBindTexture(GL_TEXTURE_2D, pingpongColorbuffers[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, window_dims.x, window_dims.y, 0, GL_RGBA, GL_FLOAT, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE); // we clamp to the edge as the blur filter would otherwise sample repeated texture values!
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, pingpongColorbuffers[i], 0);
        // also check if framebuffers are complete (no need for depth buffer)
        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
            std::cout << "Framebuffer not complete!" << std::endl;
    }

	// finally ready to begin rendering
	framebuffer_ready = true;
}


PlayMode::PlayMode() : scene(*orbit_scene) {
	Utils::InitRand();

	//get pointer to camera for convenience:
	if (scene.cameras.size() != 1) {
		throw std::runtime_error("Expecting scene to have exactly one camera, but it has "
			+ std::to_string(scene.cameras.size()));
	}
	camera = &scene.cameras.front();

	throttle = HUD::loadSprite(data_path("assets/ui/ThrottleFuel.png"));
	throttleOverlay = HUD::loadSprite(data_path("assets/ui/ThrottleOverlay.png"));
	clock = HUD::loadSprite(data_path("assets/ui/Clock.png"));
	timecontroller = HUD::loadSprite(data_path("assets/ui/TimeController.png"));
	bar = HUD::loadSprite(data_path("assets/ui/sqr.png"));
	handle = HUD::loadSprite(data_path("assets/ui/handle.png"));
	target = HUD::loadSprite(data_path("assets/ui/reticle.png"));
	reticle = HUD::loadSprite(data_path("assets/ui/reticle.png"));
	lasercooldown = HUD::loadSprite(data_path("assets/ui/LaserCooldown.png"));

	//start music loop playing:
	bgm_loop = Sound::loop(*bgm, 0.5f, 0.0f);

	deserialize(data_path("levels/level_3.txt"));
	//NOTE: For testing purposes, feel free to change the above to level_2 or level_3

	{ //load text
		// UI_text.init(Text::AnchorType::LEFT);
		GameOverText.init(Text::AnchorType::CENTER);
        ThrottleHeader.init(Text::AnchorType::LEFT);
        ThrottleReading.init(Text::AnchorType::LEFT);
        SpeedupReading.init(Text::AnchorType::RIGHT);
		tutorial_text.init(Text::AnchorType::CENTER);
		CollisionHeader.init(Text::AnchorType::RIGHT);
		CollisionTimer.init(Text::AnchorType::RIGHT, true /* monospaced */);
		LaserText.init(Text::AnchorType::CENTER);
	}

}

PlayMode::~PlayMode() {
    HUD::freeSprites();
}

void PlayMode::serialize(std::string const &filename) {
	std::ofstream file(filename);
	if (!file.is_open()) {
		throw std::runtime_error("Failed to open save file '" + filename + "'.");
	}

	for (auto &body : bodies) {
		serialize_body(file, body);
		file << '\n';
	}

	serialize_asteroid(file);
	file << '\n';

	serialize_rocket(file);
	file << '\n';

	file.close();
}

void PlayMode::serialize_orbit(std::ofstream &file, Orbit const &orbit) {
	/**
	 * Format is:
	 * ---
	 * Orbit: origin_id;c,p,phi,theta,retrograde
	 * ---
	 */
	assert(orbit.origin != nullptr && orbit.origin->transform != nullptr);
	file << "Orbit: " << orbit.origin->id << ';';
	file << orbit.c << ',' << orbit.p << ',' << orbit.phi << ',' << orbit.theta << ',' << (orbit.incl != 0.0f) << '\n';
}

void PlayMode::serialize_body(std::ofstream &file, Body const &body) {
	/**
	 * Format is:
	 * ---
	 * Body:
	 * name_of_transform,id
	 * radius,mass,soi_radius
	 * Orbit: origin_id;c,p,phi,theta,retrograde
	 * ---
	 * For the star, which has no orbit, will have "Orbit: None" instead.
	 */
	assert(body.transform != nullptr);
	file << "Body:\n" << body.transform->name << ',' << body.id << '\n';
	file << body.radius << ',' << body.mass << ',' << body.soi_radius << '\n';
	if (body.orbit == nullptr) {
		file << "Orbit: None\n";
		return;
	}
	serialize_orbit(file, *body.orbit);
}

void PlayMode::serialize_asteroid(std::ofstream &file) {
	/**
	 * Format is:
	 * ---
	 * Asteroid:
	 * name_of_transform
	 * radius,mass
	 * Orbit: origin_id;c,p,phi,theta,retrograde
	 * ---
	 * Note that only the primary orbit is serialized. Continuations are not stored at the moment and recalculated on
	 * init.
	 */
	assert(asteroid.transform != nullptr);
	file << "Asteroid:\n" << asteroid.transform->name << '\n';
	file << asteroid.radius << ',' << asteroid.mass << '\n';

	assert(asteroid.orbits.size() > 0);
	serialize_orbit(file, asteroid.orbits.front());
}

void PlayMode::serialize_rocket(std::ofstream &file) {
	/**
	 * Format is:
	 * ---
	 * Rocket:
	 * theta,fuel,laser_timer
	 * Orbit: origin_id;c,p,phi,theta,retrograde
	 * ---
	 * Note that only the primary orbit is serialized. Continuations are not stored at the moment and recalculated on
	 * init.
	 */
	assert(spaceship.transform != nullptr);
	file << "Rocket:\n" << spaceship.transform->name << '\n';
	file << spaceship.theta << ',' << spaceship.fuel << ',' << spaceship.laser_timer << '\n';
	assert(spaceship.orbits.size() > 0);
	serialize_orbit(file, spaceship.orbits.front());
}

void PlayMode::deserialize(std::string const &filename) {
	std::ifstream file(filename);
	if (!file.is_open()) {
		throw std::runtime_error("Failed to open save file '" + filename + "'.");
	}

	//reset
	star = nullptr;
	bodies.clear();
	orbits.clear();
	entities.clear();
	id_to_body.clear();
	camera_arms.clear();
	scene.drawables.clear();
	dilation = LEVEL_0;

	entities.push_back(&spaceship);
	current_focus_entity = &spaceship; // first focus should be on spaceship

	entities.push_back(&asteroid);

	//deserialize
	std::string line;
	while (std::getline(file, line)) {
		if (line == "Body:") {
			deserialize_body(file);
		} else if (line == "Asteroid:") {
			deserialize_asteroid(file);
		} else if (line == "Rocket:") {
			deserialize_rocket(file);
		} else if (line == "") {
			continue;
		} else {
			throw std::runtime_error("Malformed save file: unknown entity type '" + line + "'.");
		}
	}

	// track order of focus points for camera
	for (const Entity *entity : entities) {
		camera_arms.insert({entity, CameraArm(entity)});
	}


	// tune custom params as follows
	CameraArm::camera_pan_offset = glm::normalize(CameraArm::camera_pan_offset);
	auto &camarm0 = camera_arms.at(&spaceship);
	camarm0.camera_arm_length = 160.0f;
	{ // set the spaceship as the first camera focus
		camera->transform->position = camarm0.get_target_point();
		camera->transform->rotation = glm::quatLookAt(glm::normalize(camarm0.get_focus_point() - camera->transform->position), glm::vec3(0, 0, 1));
	}
}

inline static void throw_on_err(std::istream &s, std::string const &errmsg) {
	if (!s) {
		throw std::runtime_error(errmsg);
	}
}

void PlayMode::deserialize_orbit(std::string const &line, std::list< Orbit > &olist) {
	//load origin_id;c,p,phi,theta,retrograde
	std::string token;
	std::string errmsg =
		"Malformed save file: orbit - '" + line + "' should be 'Orbit: {int};{float},{float},{float},{float},{0|1}'.";
	std::stringstream linestream(line.substr(7));

	try {

		throw_on_err(std::getline(linestream, token, ';'), errmsg);
		int origin_id = std::stoi(token);

		throw_on_err(std::getline(linestream, token, ','), errmsg);
		float c = std::stof(token);

		throw_on_err(std::getline(linestream, token, ','), errmsg);
		float p = std::stof(token);

		throw_on_err(std::getline(linestream, token, ','), errmsg);
		float phi = std::stof(token);

		throw_on_err(std::getline(linestream, token, ','), errmsg);
		float theta = std::stof(token);

		throw_on_err(std::getline(linestream, token, ','), errmsg);
		bool retrograde = std::stoi(token) != 0;

		auto entry = id_to_body.find(origin_id);
		if (entry == id_to_body.end()) {
			throw std::runtime_error("No such body with id: " + std::to_string(origin_id));
		}

		olist.emplace_back(entry->second, c, p, phi, theta, retrograde);
	}  catch (std::runtime_error &rethrow) {
		throw rethrow;
	} catch (std::exception &e) {
		std::cout << errmsg << std::endl;
		throw e;
	}
}

void PlayMode::deserialize_body(std::ifstream &file) {
	std::string line, token, errmsg;

	std::string name;
	int id;
	errmsg = "Malformed save file: body - '" + line + "' should be '{string},{int}'.";
	try { //load transform_name, id
		throw_on_err(std::getline(file, line),
			"Malformed save file: body - not enough lines.");
		std::stringstream linestream(line);

		throw_on_err(std::getline(linestream, token, ','), errmsg);
		name = token;

		throw_on_err(std::getline(linestream, token, ','), errmsg);
		id = std::stoi(token);
	} catch (std::runtime_error &rethrow) {
		throw rethrow;
	} catch (std::exception &e) {
		std::cout << errmsg << std::endl;
		throw e;
	}

	scene.transforms.emplace_back();
	Scene::Transform *trans = &scene.transforms.back();
	trans->name = name;

	float radius, mass, soi_radius, dayLengthInSeconds;
	errmsg = "Malformed save file: body - '" + line + "' should be '{float},{float},{float},{float}'.";
	try { //load radius, mass, soi_radius
		throw_on_err(std::getline(file, line),
			"Malformed save file: body - not enough lines.");
		std::stringstream linestream(line);

		throw_on_err(std::getline(linestream, token, ','), errmsg);
		radius = std::stof(token);

		throw_on_err(std::getline(linestream, token, ','), errmsg);
		mass = std::stof(token);

		throw_on_err(std::getline(linestream, token, ','), errmsg);
		soi_radius = std::stof(token);

		throw_on_err(std::getline(linestream, token, ','), errmsg);
		dayLengthInSeconds = std::stof(token);
	} catch (std::runtime_error &rethrow) {
		throw rethrow;
	} catch (std::exception &e) {
		std::cout << errmsg << std::endl;
		throw e;
	}

	bodies.emplace_back(id, radius, mass, soi_radius);
	Body &body = bodies.back();
    body.dayLengthInSeconds = dayLengthInSeconds;
	id_to_body.insert({id, &body});
	entities.push_back(&body);

	//check star
	throw_on_err(std::getline(file, line),
		"Malformed save file: body - not enough lines.");
	if (line == "Orbit: None") { //star
		//set transform
		body.set_transform(trans);

		//set star pointer
		star = &body;

		//make drawable
		trans->scale = glm::vec3(10.0f);
		auto drawable = Scene::make_drawable(scene, trans, main_meshes_emissives.value);
		drawable->set_uniforms = []() {
			glUniform4fv(emissive_program->COLOR_vec4, 1, glm::value_ptr(glm::vec4(1.0f, 0.83f, 0.0f, 1.0f)));
		};
	} else {//not star
		//load orbit
		deserialize_orbit(line, orbits);
		Orbit *orbit = &orbits.back();
		body.set_orbit(orbit);

		//set transform
		body.set_transform(trans);

		//add to origin satellites
		orbit->origin->add_satellite(&body);

		//make drawable
		Scene::make_drawable(scene, trans, main_meshes.value);
	}

	LOG("Loaded Body '" << name << "' (id: " << std::to_string(id) << ")");
}

void PlayMode::deserialize_asteroid(std::ifstream &file) {
	std::string line, token, errmsg;

	std::string name;
	errmsg = "Malformed save file: asteroid - '" + line + "' should be '{string}'.";
	try { //load transform_name
		throw_on_err(std::getline(file, line),
			"Malformed save file: asteroid - not enough lines.");
		std::stringstream linestream(line);

		throw_on_err(std::getline(linestream, token, ','), errmsg);
		name = token;
	} catch (std::runtime_error &rethrow) {
		throw rethrow;
	} catch (std::exception &e) {
		std::cout << errmsg << std::endl;
		throw e;
	}

	scene.transforms.emplace_back();
	Scene::Transform *trans = &scene.transforms.back();
	trans->name = name;

	float radius, mass;
	errmsg = "Malformed save file: asteroid - '" + line + "' should be '{float},{float}'.";
	try { //load radius, mass
		throw_on_err(std::getline(file, line),
			"Malformed save file: asteroid - not enough lines.");
		std::stringstream linestream(line);

		throw_on_err(std::getline(linestream, token, ','), errmsg);
		radius = std::stof(token);

		throw_on_err(std::getline(linestream, token, ','), errmsg);
		mass = std::stof(token);
	} catch (std::runtime_error &rethrow) {
		throw rethrow;
	} catch (std::exception &e) {
		std::cout << errmsg << std::endl;
		throw e;
	}

	asteroid = Asteroid(radius, mass);
	entities.push_back(&asteroid);

	{ //load orbit
		throw_on_err(std::getline(file, line),
			"Malformed save file: asteroid - not enough lines.");
		deserialize_orbit(line, asteroid.orbits);
	}

	//set transform
	asteroid.init(trans, star);

	//make drawable
	Scene::make_drawable(scene, trans, main_meshes.value);

	LOG("Loaded Asteroid '" << name);
}

void PlayMode::deserialize_rocket(std::ifstream &file) {
	std::string line, token, errmsg;

	spaceship = Rocket();
	entities.push_back(&spaceship);

	std::string name;
	errmsg = "Malformed save file: rocket - '" + line + "' should be '{string}'.";
	try { //load transform_name
		throw_on_err(std::getline(file, line),
			"Malformed save file: rocket - not enough lines.");
		std::stringstream linestream(line);

		throw_on_err(std::getline(linestream, token, ','), errmsg);
		name = token;
	} catch (std::runtime_error &rethrow) {
		throw rethrow;
	} catch (std::exception &e) {
		std::cout << errmsg << std::endl;
		throw e;
	}

	scene.transforms.emplace_back();
	Scene::Transform *trans = &scene.transforms.back();
	trans->name = name;

	errmsg = "Malformed save file: body - '" + line + "' should be '{float},{float},{float}'.";
	try { //load theta, fuel, laser_timer
		throw_on_err(std::getline(file, line),
			"Malformed save file: asteroid - not enough lines.");
		std::stringstream linestream(line);

		throw_on_err(std::getline(linestream, token, ','), errmsg);
		spaceship.theta = std::stof(token);

		throw_on_err(std::getline(linestream, token, ','), errmsg);
		spaceship.fuel = std::stof(token);

		throw_on_err(std::getline(linestream, token, ','), errmsg);
		spaceship.laser_timer = std::stof(token);
	} catch (std::runtime_error &rethrow) {
		throw rethrow;
	} catch (std::exception &e) {
		std::cout << errmsg << std::endl;
		throw e;
	}

	{ //load orbit
		throw_on_err(std::getline(file, line),
			"Malformed save file: body - not enough lines.");
		deserialize_orbit(line, spaceship.orbits);
	}

	//set transform
	spaceship.init(trans, star, &scene, asteroid);

	//make drawable
	Scene::make_drawable(scene, trans, main_meshes.value);

	LOG("Loaded Rocket '" << name);
}

bool PlayMode::handle_event(SDL_Event const &evt, glm::uvec2 const &window_size) {
	auto prev_window_dims = window_dims;
	window_dims = window_size;
	if(hdrFBO == 0 || prev_window_dims != window_size){
        HUD::SCREEN_DIM = window_size;
		SetupFramebuffers();
	}
	if (evt.type == SDL_KEYDOWN) {
		bool was_key_down = false;
		for (auto& key_action : keybindings) {
			Button &button = *key_action.first;
			const std::vector<int> &keys = key_action.second;
			for (int keysym : keys){
				if (evt.key.keysym.sym == keysym) {
					button.pressed = true;
					button.downs += 1;
					was_key_down = true;
				}
			}
		}
		if (evt.key.keysym.sym == SDLK_ESCAPE) {
			SDL_SetRelativeMouseMode(SDL_FALSE);
			was_key_down = true;
			bLevelLoaded = false; // reload level on menu
			bIsTutorial = false;
			game_status = GameStatus::PLAYING; // reset game status on menu
			Mode::set_current(next_mode); // switch to menu mode
		}
		return was_key_down;
	} else if (evt.type == SDL_KEYUP) {
		bool was_key_up = false;
		for (auto& key_action : keybindings) {
			Button &button = *key_action.first;
			const std::vector<int> &keys = key_action.second;
			for (int keysym : keys){
				if (evt.key.keysym.sym == keysym) {
					button.pressed = false;
					was_key_up = true;
				}
			}
		}
		return was_key_up;
	} else if (evt.type == SDL_MOUSEBUTTONDOWN) {
		can_pan_camera = true;
		SDL_SetRelativeMouseMode(SDL_TRUE);
		return true;
		// if (SDL_GetRelativeMouseMode() == SDL_FALSE) {
		// 	return true;
		// }
	} else if (evt.type == SDL_MOUSEBUTTONUP) {
		// show the mouse cursor again once the mouse is released
		can_pan_camera = false;
		SDL_SetRelativeMouseMode(SDL_TRUE);
		return true;
		// if (SDL_GetRelativeMouseMode() == SDL_TRUE) {
		// 	return true;
		// }
	} else if (evt.type == SDL_MOUSEMOTION) {
		{
			// mouse_motion is (1, 1) in top right, (-1, -1) in bottom left
			mouse_motion = 2.f * glm::vec2(evt.motion.x / float(window_size.x) - 0.5f, -evt.motion.y / float(window_size.y) + 0.5f);
			mouse_motion_rel = glm::vec2(
				evt.motion.xrel / float(window_size.x),
				-evt.motion.yrel / float(window_size.y)
			);
			return true;
		}
	} else if(evt.type == SDL_MOUSEWHEEL) {
		auto &camarm = CurrentCameraArm();
		float scroll_zoom = -evt.wheel.y * camarm.ScrollSensitivity;
		camarm.camera_arm_length += scroll_zoom * (camarm.camera_arm_length / camarm.init_radius_multiples);
		camarm.camera_arm_length = std::max(camarm.camera_arm_length, 5.f);
		// evt.wheel.x for horizontal scrolling
	}

	return false;
}

void PlayMode::update(float elapsed) {
	if (!bLevelLoaded) {
		if (mode_level < 3)
			deserialize(data_path("levels/level_" + std::to_string(mode_level + 1) + ".txt"));
		else {
			bIsTutorial = true;
			deserialize(data_path("levels/level_1.txt"));
			LOG("Starting tutorial...");
		}

		bLevelLoaded = true;
	}

	bool playing = (game_status == GameStatus::PLAYING);

	if (playing && bIsTutorial) {
		tut_anim = elapsed;
		bool remaining = false;
		for (auto &state : tutorial_content) {
			if (!state.done) {
				remaining = true;
				tutorial_text.set_text(state.text);
				for (auto *button : state.activations) {
					if (button->downs > 0) {
						state.done = true;
						tutorial_text.reset_time();
						break;
					}
				}
			}
		}
		if (!remaining) {
			bIsTutorial = false;
		}
	}

	if (playing) { //handle quicksave/quickload
		if (f5.downs > 0) {
			serialize(data_path("quicksave.txt"));
		} else if (f9.downs > 0 && std::filesystem::exists(data_path("quicksave.txt"))) {
			deserialize(data_path("quicksave.txt"));
		}
	}

	if (playing) { //update dilation
		if (plus.downs > 0 && minus.downs == 0) {
			dilation++;
            if(dilationInt < 5){
                dilationInt++;
            }
		} else if (minus.downs > 0 && plus.downs == 0) {
			dilation--;
            if(dilationInt > 0){
                dilationInt--;
            }
		}
	}


	if (playing) { // update rocket controls
		{ // reset dilation on controls
			if (up.downs || down.downs ||  shift.downs || control.downs) {
				dilationInt = 0;
				dilation = LEVEL_0; // reset time so user inputs are used
			}
		}

		static float constexpr dtheta_update_amount = glm::radians(20.0f);
		if (left.downs > 0 && right.downs == 0) {
			spaceship.control_dtheta = std::min(20.f * dtheta_update_amount, spaceship.control_dtheta + dtheta_update_amount);
		} else if (right.downs > 0 && left.downs == 0) {
			spaceship.control_dtheta = std::max(-20.f * dtheta_update_amount, spaceship.control_dtheta - dtheta_update_amount);;
		} else {
			spaceship.control_dtheta *= 0.9f; // slow decay
			if (std::fabs(spaceship.control_dtheta) < 0.01) // threshold to 0
				spaceship.control_dtheta = 0.f;
		}

		{ // thrust
			bool increase_thrust = shift.pressed || up.pressed;
			bool decrease_thrust = control.pressed || down.pressed;

			if (spaceship.thrust_percent == 0.f) {
				if (increase_thrust) {
					if (bCanThrustChangeDir && forward_thrust == false){
						forward_thrust = true;
					}
				}
				else if (decrease_thrust && bEnableEasyMode) {
					if (bCanThrustChangeDir && forward_thrust == true){
						forward_thrust = false;
					}
				}
			}
			bCanThrustChangeDir = (!increase_thrust && !decrease_thrust);

			const float MaxThrust = 100.f;
			const float SlowDelta = 1.f;
			const float FastDelta = 10.f;
			if (forward_thrust) {
				if (increase_thrust) {
					spaceship.thrust_percent = std::min(spaceship.thrust_percent + SlowDelta , MaxThrust);
				} else if (decrease_thrust) {
					spaceship.thrust_percent = std::max(spaceship.thrust_percent - FastDelta, 0.0f);
				}
			}
			else {
				if (increase_thrust) {
					spaceship.thrust_percent = std::min(spaceship.thrust_percent + FastDelta , 0.0f);
				} else if (decrease_thrust) {
					spaceship.thrust_percent = std::max(spaceship.thrust_percent - SlowDelta , -MaxThrust);
				}
			}
		}
	}

	if (dilation != LEVEL_0) {
		spaceship.dtheta = 0.f;
		spaceship.thrust_percent = 0.f;
	}

	// { //update text UI
	// 	std::stringstream stream;
	// 	stream << std::fixed << std::setprecision(1) << "thrust: " << spaceship.thrust_percent << "%" << '\n'
	// 		   << '\n' << "fuel: " << spaceship.fuel << '\n'
	// 		   << '\n' << "time: " << DilationSchematic(dilation) << " ("  << dilation << "x)";
	// 	UI_text.set_text(stream.str());
	// }

    if (playing) {
		ThrottleHeader.set_text("Throttle");
        if(spaceship.thrust_percent < 100.0f){
		    ThrottleReading.set_text(std::to_string(static_cast<int>(spaceship.thrust_percent)) + "%");
        }else{
		    ThrottleReading.set_text("MAX");
        }
		SpeedupReading.set_text(std::to_string(dilation));
		CollisionHeader.set_text("Time to Impact");
		CollisionTimer.set_text(asteroid.get_time_remaining());
		LaserText.set_text(spaceship.laser_timer == 0.0f ? "Ready to Fire" : "Recharging");
    }

	{ //update listener to camera position:
		glm::mat4x3 frame = camera->transform->make_local_to_parent();
		glm::vec3 frame_right = frame[0];
		glm::vec3 frame_at = frame[3];
		Sound::listener.set_position_right(frame_at, frame_right, 1.0f / 60.0f);
	}

	if (playing) { //orbital simulation
		star->update(elapsed);
		asteroid.update(elapsed, spaceship.lasers);
		spaceship.update(elapsed, asteroid);
	}

	if (playing) { // collision logic
		if (asteroid.crashed) {
			target_lock = &asteroid;
			tab.downs = 1; // to trigger the camera transition
			game_status = GameStatus::LOSE;
			dilation = LEVEL_0;
		} else if (asteroid.time_of_collision == std::numeric_limits< float >::infinity()) {
			target_lock = &spaceship;
			tab.downs = 1; // to trigger the camera transition
			game_status = GameStatus::WIN;
			dilation = LEVEL_0;
		}
	}

	if (!playing) {
		anim = 0.3f * elapsed;
	}

	if (playing && !bIsTutorial) { //laser
		if (space.pressed) {
			spaceship.fire_laser();
		}

		spaceship.update_lasers(elapsed);
	}

	{ //update camera controls (after spaceship update for smooth motion)
		auto update_camera_pan = [&](){ // compute camera offset according to mouse
			if (can_pan_camera) {
				glm::mat4x3 frame = camera->transform->make_local_to_parent();
				glm::vec3 right_vec = frame[0];
				glm::vec3 up_vec = frame[1];
				const float s = 5.0f; // scale mouse
				CameraArm::camera_pan_offset -= (mouse_motion_rel.x * right_vec + mouse_motion_rel.y * up_vec) * s;
				CameraArm::camera_pan_offset = glm::normalize(CameraArm::camera_pan_offset);
			};
		};
		update_camera_pan();


		auto &camarm = CurrentCameraArm();
		glm::vec3 focus_pt = camarm.get_focus_point(); // center of the body of mass
		glm::vec3 target_pt = camarm.get_target_point(); // where the camera actually goes (not in the center)
		if (tilde.downs > 0) { // automatically go back to spaceship
			tab.downs = 1;
			target_lock = &spaceship;
		}
		if (tab.downs > 0 && target_lock != nullptr) { // to switch camera views
			current_focus_entity = target_lock;
			// start the camera transition (new current camera arm)
			auto &camarm2 = camera_arms.at(current_focus_entity);
			target_pt = camarm2.get_target_point();
			focus_pt = camarm2.get_focus_point();
			camera_transition = target_pt - camera->transform->position;
			camera_start_rot = camera->transform->rotation;
			camera_end_rot = glm::quatLookAt(glm::normalize(focus_pt - target_pt), glm::vec3(0, 0, 1));
		}

		bool done_interp = false;
		if (camera_transition != glm::vec3{0.f}) {
			float t = 1.0f - glm::length(camera->transform->position - target_pt) / glm::length(camera_transition);
			{ // smoothly interpolate position (with bell-like curve)
				auto bell_curve = [](float x){return 1.f * std::exp(-std::pow(x - 0.5f, 6.0f) / 0.01f); };
				camera->transform->position += camera_transition * elapsed * bell_curve(t);
			}
			{ // smoothly interpolate rotation (linearly)
				camera->transform->rotation = glm::mix(camera_start_rot, camera_end_rot, t);
			}
			if (camera_transition_interp > t) // see where the peak occurs (hit t \approx 1)
				done_interp = true;
			camera_transition_interp = t; // track previous interpolation
		} else {
			camera_transition_interp = 0;
			done_interp = true;
		}

		if (done_interp) {
			//TODO: fix the spinning when go directly over and up is parallel to dir
			// Doing this probably requires not using the transformation matrix's vectors for finding right and up vecs
			camera->transform->position = target_pt;
			camera->transform->rotation = glm::quatLookAt(glm::normalize(focus_pt - camera->transform->position), glm::vec3(0, 0, 1));
			camera_transition = glm::vec3{0.f};
			camera_transition_interp = 0.f;
		}
	}

	if (playing) { //asteroid target (NOTE: this relies on camera pos being updated)
		glm::mat4 world_to_screen = camera->make_projection() * glm::mat4(camera->transform->make_world_to_local());
		glm::vec3 pos3d = glm::vec3(world_to_screen * glm::vec4(asteroid.pos, 1.0f)); // [-1..1]^2
		target_xy = glm::vec2{(pos3d.x / pos3d.z), (pos3d.y / pos3d.z)} * 0.5f + 0.5f; // -> [0,1]^2
	}

	if (playing) { //reticle tracker
		// make the reticle follow the mouse
		reticle_aim = mouse_motion;

		// unless it is "close" to another body, in which case track to that one
		glm::mat4 world_to_screen = camera->make_projection() * glm::mat4(camera->transform->make_world_to_local());
		float min_dist = std::numeric_limits<float>::max(); // infinity
		glm::vec2 homing_reticle_pos = reticle_aim;
		glm::vec3 homing_target{0.f, 0.f, 0.f};
		target_lock = nullptr;
		for (const Entity *entity : entities) {
			if (!camera->in_view(entity->pos))
				continue; // can't focus on offscreen elements
			glm::vec3 pos3d = glm::vec3(world_to_screen * glm::vec4(entity->pos, 1.0f));
			glm::vec2 pos2d{(pos3d.x / pos3d.z), (pos3d.y / pos3d.z)};
			float ss_dist = glm::length(reticle_aim - pos2d); // screen-space distance (for comparisons)
			if (ss_dist < homing_threshold && ss_dist < min_dist) {
				min_dist = ss_dist;
				homing_reticle_pos = pos2d;
				homing_target = entity->pos;
				target_lock = entity;
			}
		}
		reticle_homing = (homing_reticle_pos != reticle_aim); // whether or not we locked onto a target
		reticle_aim = homing_reticle_pos * 0.5f + 0.5f; // [-1:1]^2 -> [0:1]^2

		// now make sure the rocket laser launcher system follows this direction
		glm::vec3 world_target{0.f, 0.f, 0.f};
		if (reticle_homing) {
			world_target = homing_target; // already know what position the target is
		}
		else {
			// perform a ray trace from camera onto the plane z=0
			glm::mat4x3 frame = camera->transform->make_local_to_parent();
			glm::vec3 cam_right = frame[0];
			glm::vec3 cam_up = frame[1];
			glm::vec3 cam_forward = -frame[2];
			const float fovY = camera->fovy;
			const float fovX = camera->fovy * camera->aspect;
			const float dX = fovX * mouse_motion.x / 2.f;
			const float dY = fovY * mouse_motion.y / 2.f;
			glm::vec3 ray = cam_forward + cam_right * dX + cam_up * dY;

			// extend the ray: origin + t * ray = pt such that pt.z == 0
			// ==> origin.z + t * ray.z == 0 ==> t = -origin.z / ray.z;
			auto origin = camera->transform->position;
			float t = -origin.z / ray.z;
			world_target = (origin + t * ray);
		}

		// update rocket blaster aim
		spaceship.aim_dir = glm::normalize(world_target - spaceship.pos);
		spaceship.aim_dir.z = 0.f;
	}

	//reset button press counters:
	for (auto &keybinding : keybindings) {
		Button &button = *keybinding.first;
		button.downs = 0;
	}
	mouse_motion_rel = glm::vec2(0, 0);
}

void PlayMode::RenderFrameQuad(){
	if (renderQuadVAO == 0)
    {
        float quadVertices[] = {
            // positions        // texture Coords
            -1.0f,  1.0f, 0.0f, 0.0f, 1.0f,
            -1.0f, -1.0f, 0.0f, 0.0f, 0.0f,
             1.0f,  1.0f, 0.0f, 1.0f, 1.0f,
             1.0f, -1.0f, 0.0f, 1.0f, 0.0f,
        };
        // setup plane VAO
        glGenVertexArrays(1, &renderQuadVAO);
        glGenBuffers(1, &renderQuadVBO);
        glBindVertexArray(renderQuadVAO);
        glBindBuffer(GL_ARRAY_BUFFER, renderQuadVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), &quadVertices, GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
    }

	GL_ERRORS();
    glBindVertexArray(renderQuadVAO);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindVertexArray(0);
}

void PlayMode::draw(glm::uvec2 const &drawable_size) {
	//update camera aspect ratio for drawable:
	camera->aspect = float(drawable_size.x) / float(drawable_size.y);

	//set up light type and position for lit_color_texture_program:
	glUseProgram(lit_color_texture_program->program);
	glUniform3fv(lit_color_texture_program->AMBIENT_COLOR_vec3, 1, glm::value_ptr(glm::vec3(0.1f, 0.1f,0.1f)));
	glUniform3fv(lit_color_texture_program->LIGHT_DIRECTION_vec3, 1, glm::value_ptr(glm::vec3(0.0f, 0.0f,-1.0f)));
	glUniform3fv(lit_color_texture_program->LIGHT_LOCATION_vec3, 1, glm::value_ptr(star->transform->position));
	glUniform3fv(lit_color_texture_program->LIGHT_ENERGY_vec3, 1, glm::value_ptr(glm::vec3(1.0f, 1.0f, 0.95f)));
	glUseProgram(0);

	glBindFramebuffer(GL_FRAMEBUFFER, hdrFBO);
	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	glClearDepth(1.0f); //1.0 is actually the default value to clear the depth buffer to, but FYI you can change it.
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS); //this is the default depth comparison function, but FYI you can change it.
	GL_ERRORS();

	scene.draw(*camera);

    for(auto it = fancyPlanets.begin(); it != fancyPlanets.end(); it++){
        it->draw(camera);
    }

    // skybox comes last always
    skybox.draw(camera);

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	if (hdrFBO == 0)
		return;

	// 2. blur bright fragments with two-pass Gaussian Blur
	// --------------------------------------------------
	bool horizontal = true, first_iteration = true;
	unsigned int amount = 100;
	glUseProgram(bloom_blur_program->program);
	for (unsigned int i = 0; i < amount; i++)
	{
		glBindFramebuffer(GL_FRAMEBUFFER, pingpongFBO[horizontal]);
		glUniform1i(bloom_blur_program->HORIZONTAL_bool, horizontal);
		glBindTexture(GL_TEXTURE_2D, first_iteration ? colorBuffers[1] : pingpongColorbuffers[!horizontal]);  // bind texture of other framebuffer (or scene if first iteration)
		RenderFrameQuad();
		horizontal = !horizontal;
		if (first_iteration)
			first_iteration = false;
	}
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glUseProgram(frame_quad_program->program);
	glUniform1i(frame_quad_program->HDR_tex, 0);
	glUniform1i(frame_quad_program->BLOOM_tex, 1);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, colorBuffers[0]);
	glActiveTexture(GL_TEXTURE0 + 1);
	glBindTexture(GL_TEXTURE_2D, pingpongColorbuffers[!horizontal]);
	glUniform1i(frame_quad_program->BLOOM_bool, true);
	glUniform1f(frame_quad_program->EXPOSURE_float, 1.0f);
	RenderFrameQuad();

	//Everything from this point on is part of the HUD overlay
	glDisable(GL_DEPTH_TEST);

	{
		glm::mat4 world_to_clip = camera->make_projection() * glm::mat4(camera->transform->make_world_to_local());
		DrawLines orbit_lines(world_to_clip);

		static constexpr glm::u8vec4 grey = glm::u8vec4(0x80, 0x80, 0x80, 0xff);
		static constexpr glm::u8vec4 cyan = glm::u8vec4(0x00, 0xff, 0xff, 0xff);
		static constexpr glm::u8vec4 green = glm::u8vec4(0x00, 0xff, 0x00, 0xff);

		star->draw_orbits(orbit_lines, grey, CurrentCameraArm().camera_arm_length);
		spaceship.orbits.front().draw(orbit_lines, cyan);
		asteroid.orbits.front().draw(orbit_lines, green);
	}


	if (true) { //DEBUG: draw spaceship (relative) position, (relative) velocity, heading, and acceleration vectors
		glm::mat4 world_to_clip = camera->make_projection() * glm::mat4(camera->transform->make_world_to_local());
		DrawLines vector_lines(world_to_clip);

		// Orbit const &orbit = spaceship.orbits.front();

		static constexpr glm::u8vec4 white = glm::u8vec4(0xff, 0xff, 0xff, 0xff);
		// static constexpr glm::u8vec4 yellow = glm::u8vec4(0xff, 0xd3, 0x00, 0xff); //heading
		// static constexpr glm::u8vec4 green = glm::u8vec4(0x00, 0xff, 0x20, 0xff); //rvel
		// static constexpr glm::u8vec4 red = glm::u8vec4(0xff, 0x00, 0x00, 0xff); //acc
		// static float constexpr display_multiplier = 1000.0f;

		const float radscale = 0.5f;
		float radius = radscale * CurrentCameraArm().camera_arm_length / CameraArm::init_radius_multiples;
		float circle_radius = 0.4f * radius;
		if (radius > 1.f / radscale) {
			glm::vec3 heading = spaceship.get_heading();
			vector_lines.draw(
				spaceship.pos + heading * (0.5f * circle_radius),
				spaceship.pos + heading * (1.5f * circle_radius),
				white);

			auto draw_circle = [&vector_lines](glm::vec3 const &center, glm::vec2 const &radius, glm::u8vec4 const &color,
											const int num_verts = 50) {
				// draw a circle by drawing a bunch of lines

				std::vector<glm::vec2> circ_verts;
				circ_verts.reserve(num_verts);
				for (int i = 0; i < num_verts; i++)
				{
					circ_verts.emplace_back(glm::vec2{(radius.x * glm::cos(i * 2 * M_PI / num_verts)), (radius.y * glm::sin(i * 2 * M_PI / num_verts))});
				}

				for (int i = 0; i < num_verts; i++)
				{
					if (i % 2 == 0) continue;
					auto seg_start = glm::vec3(center.x + circ_verts[(i) % num_verts].x, center.y + circ_verts[i % num_verts].y, center.z);
					auto seg_end = glm::vec3(center.x + circ_verts[(i + 1) % num_verts].x, center.y + circ_verts[(i + 1) % num_verts].y, center.z);
					vector_lines.draw(seg_start, seg_end, color);
				}
			};
			draw_circle(spaceship.pos, circle_radius * glm::vec2(1.f, 1.f), white);
		}

		if (spaceship.closest.dist < 1.0e10f) { //draw closest approach
			glm::vec3 spaceship_point = spaceship.closest.rocket_rpos + spaceship.closest.origin->pos;
			glm::vec3 asteroid_point = spaceship.closest.asteroid_rpos + spaceship.closest.origin->pos;
			vector_lines.draw(spaceship_point, spaceship_point + glm::vec3(0.0f, 0.0f, 5.0f), white);
			vector_lines.draw(asteroid_point, asteroid_point - glm::vec3(0.0f, 0.0f, 5.0f), white);
		}


		// vector_lines.draw(spaceship.pos, spaceship.pos + orbit.rpos, white);
		// vector_lines.draw(spaceship.pos, spaceship.pos + orbit.rvel * 1000.0f, green);
		// vector_lines.draw(spaceship.pos, spaceship.pos + spaceship.acc * 10000.0f, red);

		// vector_lines.draw(asteroid.pos, asteroid.pos + glm::vec3(-1.0f, 0.0f, 0.0f), red);
	}

	{ // draw spaceship laser beams (screenspace)
		glm::mat4 world_to_clip = camera->make_projection() * glm::mat4(camera->transform->make_world_to_local());
		DrawLines line_drawer(world_to_clip);

		for (const Beam &L : spaceship.lasers){
			L.draw(line_drawer);
		}
	}

	if (game_status == GameStatus::PLAYING) { // draw mouse cursor reticle
		static constexpr glm::u8vec4 red = glm::u8vec4(0xcc, 0x00, 0x00, 0x65); // target locked
		static constexpr glm::u8vec4 yellow = glm::u8vec4(0xcc, 0xd3, 0x00, 0x75);
		glm::u8vec4 color = reticle_homing ? red : yellow;
		glm::vec2 reticle_size= glm::vec2(50, 50);
		glm::vec2 reticle_pos{reticle_aim.x * drawable_size.x - 0.5f * reticle_size.x, reticle_aim.y * drawable_size.y + 0.5f * reticle_size.y};
		HUD::drawElement(reticle_size, reticle_pos, target, color);
	}

	if (game_status == GameStatus::PLAYING && camera->in_view(asteroid.pos)) { //draw asteroid target
		glm::vec2 target_size = glm::vec2{60, 60};
		glm::vec2 target_pos{target_xy.x * drawable_size.x - 0.5f * target_size.x, target_xy.y * drawable_size.y + 0.5f * target_size.y};
		// draw_circle(reticle_pos, glm::vec2{reticle_radius_screen, reticle_radius_screen}, reticle_homing ? red : yellow);
		const auto orange = glm::u8vec4{241, 90, 34, 0x45};
		HUD::drawElement(target_size, target_pos, reticle, orange);
	}

	/* { //use DrawLines to overlay some text: */
	/* 	float x = drawable_size.x * 0.03f; */
	/* 	float y = drawable_size.y * (1.0f - 0.1f); // top is 1.f bottom is 0.f */
	/* 	float width = drawable_size.x * 0.2f; */
	/* 	UI_text.draw(1.f, drawable_size, width, glm::vec2(x, y), 1.f, DilationColor(dilation)); */
	/* } */

	float thrust_amnt = std::fabs(spaceship.thrust_percent) / 100.0f;
	float fuel_amt = (spaceship.fuel / spaceship.maxFuel);
	HUD::drawElement(glm::vec2(0, throttle->height), throttle);
	HUD::drawElement(HUD::fromAnchor(HUD::Anchor::TOPLEFT, glm::vec2(0, 0)), clock);
	HUD::drawElement(HUD::fromAnchor(HUD::Anchor::CENTERRIGHT, glm::vec2(-timecontroller->width + 10, timecontroller->height / 2)), timecontroller);
	auto throttle_color = spaceship.thrust_percent >= 0 ? glm::vec4(83, 178, 0, 255) : glm::vec4(178, 83, 0, 255);
	HUD::drawElement(glm::vec2(390.0f * thrust_amnt, 107.0f), HUD::fromAnchor(HUD::Anchor::BOTTOMLEFT, glm::vec2(20, 192)), bar, throttle_color);
	HUD::drawElement(glm::vec2(390.0f * 0.60f, 111.0f), HUD::fromAnchor(HUD::Anchor::BOTTOMLEFT, glm::vec2(81, 192)), throttleOverlay);
	HUD::drawElement(glm::vec2(390.0f * fuel_amt, 61.0f), HUD::fromAnchor(HUD::Anchor::BOTTOMLEFT, glm::vec2(20, 79)), bar, glm::vec4(221, 131, 0.0, 255));
	ThrottleHeader.draw(1.f, drawable_size, 200, glm::vec2(22, 290), 0.5f, glm::vec4(1.0));
	ThrottleReading.draw(1.f, drawable_size, 200, glm::vec2(22, 220), 1.3f, glm::vec4(1.0));
	SpeedupReading.draw(1.f, drawable_size, 200, HUD::fromAnchor(HUD::Anchor::CENTERRIGHT, glm::vec2(-5, 142)), 1.3f, DilationColor(dilation));
    for (int i = 0; i < dilationInt + 1; i++){
	    HUD::drawElement(glm::vec2(70, 23), HUD::fromAnchor(HUD::Anchor::CENTERRIGHT, glm::vec2(-75, -145 + (46 * i))), bar, glm::vec4(DilationColor(dilation) * 255.0f, 255.0));
    }
	CollisionHeader.draw(1.f, drawable_size, 200, HUD::fromAnchor(HUD::Anchor::TOPLEFT, glm::vec2(450, -60)), 0.3f, glm::vec4(1.0f));
	CollisionTimer.draw(1.f, drawable_size, 200, HUD::fromAnchor(HUD::Anchor::TOPLEFT, glm::vec2(450, -100)), 0.75f, glm::vec4(0.1f, 1.0f, 0.1f, 1.0f));

	float cooldown = (spaceship.LaserCooldown - spaceship.laser_timer) / spaceship.LaserCooldown;
	HUD::drawElement(glm::vec2((drawable_size.x - lasercooldown->width) / 2, lasercooldown->height), lasercooldown);
	HUD::drawElement(glm::vec2(371.0f * cooldown, 22.0f), glm::vec2((drawable_size.x - 370) / 2, 26), bar, glm::vec4(0x00, 0xff, 0x00, 0xe0));
	LaserText.draw(1.f, drawable_size, 400, HUD::fromAnchor(HUD::Anchor::BOTTOMCENTER, glm::vec2(0, 10)), 0.3f, glm::vec4(1.0f));

	if (game_status != GameStatus::PLAYING) {
		std::string message = game_status == GameStatus::WIN ? "Mission Accomplished!" : "Mission Failed!";
		auto color = game_status == GameStatus::WIN ? glm::u8vec4{0x0, 0xff, 0x0, 0xff} : glm::u8vec4{0xff, 0x0, 0x0, 0xff};
		GameOverText.set_text(message);
		GameOverText.draw(anim, drawable_size, 200, 0.5f * glm::vec2(drawable_size), 2.0f, color);
	}

	if (bIsTutorial) { // draw tutorial text
		auto color = glm::u8vec4{0xff};
		tutorial_text.draw(tut_anim, drawable_size, 0.3f * drawable_size.x, glm::vec2{0.45f * drawable_size.x, 0.75f * drawable_size.y}, 1.0f, color);
	}

	/* glm::u8vec4 color{0xff}; */
	/* if (spaceship.thrust_percent > 0) { */
	/* 	color = glm::u8vec4{0, 0xff, 0, 0xff}; // green */
	/* } else { */
	/* 	color = glm::u8vec4{0xff, 0, 0, 0xff}; // red */
	/* } */
	/* HUD::drawElement(glm::vec2(80, (250 * thrust_amnt)), glm::vec2(140, 32 + (250 * thrust_amnt)), bar, color); */
	/* HUD::drawElement(glm::vec2(120, 30), glm::vec2(120, 60 + (250 * thrust_amnt)), handle); */


	GL_ERRORS();
}
