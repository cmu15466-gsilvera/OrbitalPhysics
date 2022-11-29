#include "PlayMode.hpp"
#include "EmissiveShaderProgram.hpp"
#include "GL.hpp"
#include "LitColorTextureProgram.hpp"
#include "FrameQuadProgram.hpp"
#include "BloomBlurProgram.hpp"
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


#define DEBUG

#ifdef DEBUG
#include <iostream>
#define LOG(ARGS) std::cout << ARGS << std::endl
#else
#define LOG(ARGS)
#endif

Load< Scene::RenderSet > main_meshes_emissives(LoadTagDefault, []() -> Scene::RenderSet const * {
	Scene::RenderSet *renderSet = new Scene::RenderSet();
	MeshBuffer const *ret = new MeshBuffer(data_path("orbit.pnct"));
	renderSet->vao = ret->make_vao_for_program(emissive_program_pipeline.program);
	renderSet->meshes = ret;
	renderSet->pipeline = emissive_program_pipeline;

	return renderSet;
});


Load< Scene::RenderSet > main_meshes(LoadTagDefault, []() -> Scene::RenderSet const * {
	Scene::RenderSet *renderSet = new Scene::RenderSet();
	MeshBuffer const *ret = new MeshBuffer(data_path("orbit.pnct"));
	renderSet->vao = ret->make_vao_for_program(lit_color_texture_program->program);
	renderSet->meshes = ret;
	renderSet->pipeline = lit_color_texture_program_pipeline;

	return renderSet;
});

Load< Scene > orbit_scene(LoadTagDefault, []() -> Scene const * {
	return new Scene(data_path("orbit.scene"), [&](Scene &s, Scene::Transform *t, std::string const &m){
		//Drawables will be set up later, dummy function
	});
});

//TODO: probably need to load rocket sound assets and BGM here
Load< Sound::Sample > dusty_floor_sample(LoadTagDefault, []() -> Sound::Sample const * {
	return new Sound::Sample(data_path("dusty-floor.opus"));
});

void PlayMode::SetupFramebuffers(){
	printf("%d, %d\n", window_dims.x, window_dims.y);
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
	//get pointers to leg for convenience:
	// for (auto &transform : scene.transforms) {glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	// 	if (transform.name == "Hip.FL") hip = &transform;
	// 	else if (transform.name == "UpperLeg.FL") upper_leg = &transform;
	// 	else if (transform.name == "LowerLeg.FL") lower_leg = &transform;
	// }
	// if (hip == nullptr) throw std::runtime_error("Hip not found.");
	// if (upper_leg == nullptr) throw std::runtime_error("Upper leg not found.");
	// if (lower_leg == nullptr) throw std::runtime_error("Lower leg not found.");

	// hip_base_rotation = hip->rotation;
	// upper_leg_base_rotation = upper_leg->rotation;
	// lower_leg_base_rotation = lower_leg->rotation;
	//
	Utils::InitRand();

	//get pointer to camera for convenience:
	if (scene.cameras.size() != 1) {
		throw std::runtime_error("Expecting scene to have exactly one camera, but it has "
			+ std::to_string(scene.cameras.size()));
	}
	camera = &scene.cameras.front();

	throttle = HUD::loadSprite(data_path("throttle.png"));
	window = HUD::loadSprite(data_path("window.png"));
	bar = HUD::loadSprite(data_path("sqr.png"));
	handle = HUD::loadSprite(data_path("handle.png"));
	target = HUD::loadSprite(data_path("reticle.png"));
	reticle = HUD::loadSprite(data_path("reticle.png"));
	// first focus should be on the spaceship!
	entities.push_back(&spaceship);

	// next on asteroid
	entities.push_back(&asteroid);

	//start music loop playing:
	// (note: position will be over-ridden in update())
	// leg_tip_loop = Sound::loop_3D(*dusty_floor_sample, 1.0f, get_leg_tip_position(), 10.0f);

	{ //Load star
		scene.transforms.emplace_back();
		Scene::Transform *star_trans = &scene.transforms.back();
		star_trans->name = "Star";

		//a slightly large red dwarf that's 0.4x the mass of the sun
		bodies.emplace_back(Body(275.0, 8.0e23f, std::numeric_limits< float >::infinity()));
		star = &bodies.back();
		entities.push_back(star);

		star->set_transform(star_trans);
		star_trans->scale = glm::vec3(10.0f);

		auto drawable = Scene::make_drawable(scene, star_trans, main_meshes_emissives.value);
		drawable->set_uniforms = []() { 
			glUniform4fv(emissive_program->COLOR_vec4, 1, glm::value_ptr(glm::vec4(1.0f, 0.83f, 0.0f, 1.0f))); 
		};

		LOG("Loaded Star");
	}

	{ //Load planet
		scene.transforms.emplace_back();
		Scene::Transform *planet_trans = &scene.transforms.back();
		planet_trans->name = "Planet";

		//a very large but not very dense rocky planet (same mass as Earth, about 1.66x the radius)
		bodies.emplace_back(Body(10.0f, 6.0e18f, 1000.0f));
		Body *planet = &bodies.back();
		entities.push_back(planet);

		orbits.emplace_back(Orbit(star, 0.0f, 100000.0f, 0.0f, 0.0f, false));
		Orbit *planet_orbit = &orbits.back();

		planet->set_orbit(planet_orbit);
		planet->set_transform(planet_trans);

		star->add_satellite(planet);

		Scene::make_drawable(scene, planet_trans, main_meshes.value);
		LOG("Loaded Planet");

		//Load all things orbiting planet
		{ //Load moon of planet
			scene.transforms.emplace_back();
			Scene::Transform *moon_trans = &scene.transforms.back();
			moon_trans->name = "Moon";

			//a Moon-sized moon with similar mass and rius
			bodies.emplace_back(Body(1.7f, 7.0e16f, 50.0f));
			Body *moon = &bodies.back();
			entities.push_back(moon);

			orbits.emplace_back(Orbit(planet, 0.1f, 200.0f, glm::radians(30.0f), glm::radians(-120.0f), false));
			Orbit *moon_orbit = &orbits.back();

			moon->set_orbit(moon_orbit);
			moon->set_transform(moon_trans);

			planet->add_satellite(moon);

			Scene::make_drawable(scene, moon_trans, main_meshes.value);
			LOG("Loaded Moon");
		}
		{ //Load player
			scene.transforms.emplace_back();
			Scene::Transform *spaceship_trans = &scene.transforms.back();
			spaceship_trans->name = "Spaceship";

			// glm::vec3 rpos = glm::vec3(30.0f, 0.0f, 0.0f);
			// glm::vec3 rvel = glm::vec3(0.0f, 0.003654f, 0.0f);

			// orbits.emplace_back(Orbit(planet, planet->pos + rpos, planet->vel + rvel));

			spaceship.orbits.emplace_front(
				Orbit(planet, 0.0f, 30.0f, glm::radians(120.0f), glm::radians(220.0f), false)
			);

			//use this is you're bad at the game
			// spaceship.orbits.emplace_front(
			// 	Orbit(planet, 0.507543f, 201.459f, 0.53f, glm::radians(56.0f), false)
			// );

			spaceship.init(spaceship_trans, star, &scene);

			Scene::make_drawable(scene, spaceship_trans, main_meshes.value);
			LOG("Loaded Spaceship");
		}
		{ //Load asteroid
			scene.transforms.emplace_back();
			Scene::Transform *asteroid_trans = &scene.transforms.back();
			asteroid_trans->name = "Asteroid";

			asteroid.orbits.emplace_front(
				Orbit(planet, 0.507543f, 201.459f, 0.53f, glm::radians(60.0f), false)
			);

			asteroid.init(asteroid_trans, star);

			Scene::make_drawable(scene, asteroid_trans, main_meshes.value);
			LOG("Loaded Asteroid");
		}
	}
    {
    }

	// track order of focus points for camera
	for (const Entity *entity : entities) {
		camera_arms.insert({entity, CameraArm(entity)});
		camera_views.push_back(entity);
	}
	// tune custom params as follows
	camera_arms.at(&spaceship).scroll_zoom = 5.f;

	{ // load text
		UI_text.init(Text::AnchorType::LEFT);
	}
}

PlayMode::~PlayMode() {
	free(throttle);
	free(window);
	free(handle);
	free(bar);
	free(target);
	free(reticle);
}

bool PlayMode::handle_event(SDL_Event const &evt, glm::uvec2 const &window_size) {
	auto prev_window_dims = window_dims;
	window_dims = window_size;
	if(hdrFBO == 0 || prev_window_dims != window_size){
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
			transition_to = next_mode; // switch to menu Mode
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
		camarm.scroll_zoom = std::max(camarm.scroll_zoom * (1.0f - evt.wheel.y * camarm.ScrollSensitivity), 5.f);
		// evt.wheel.x for horizontal scrolling
	}
	//TODO: down the line, we might want to record mouse motion if we want to support things like click-and-drag
	//(for orbital manuever planning tool)

	return false;
}

void PlayMode::CameraArm::update(Scene::Camera *cam, float mouse_x, float mouse_y) {
	const glm::vec3 &focus_point = entity->pos;
	camera_arm_length = std::max(entity->radius, 1.0f) * scroll_zoom;

	// camera arm length depends on radius
	{
		glm::mat4x3 frame = cam->transform->make_local_to_parent();
		glm::vec3 right_vec = frame[0];
		glm::vec3 up_vec = frame[1];

		this->camera_offset -= (mouse_x * right_vec + mouse_y * up_vec) * camera_arm_length * MouseSensitivity;
		this->camera_offset = camera_arm_length * glm::normalize(camera_offset);

		glm::vec3 new_pos = focus_point + camera_offset;
		glm::vec3 dir = glm::normalize(focus_point - new_pos);

		//TODO: fix the spinning when go directly over and up is parallel to dir
		// Doing this probably requires not using the transformation matrix's vectors for finding right and up vecs
		cam->transform->position = new_pos;
		cam->transform->rotation = glm::quatLookAt(dir, glm::vec3(0, 0, 1));
	}
}

void PlayMode::update(float elapsed) {
	{ //update dilation
		if (plus.downs > 0 && minus.downs == 0) {
			dilation++;
		} else if (minus.downs > 0 && plus.downs == 0) {
			dilation--;
		}
	}


	{ // update rocket controls
		{ // reset dilation on controls
			if (up.downs || down.downs ||  shift.downs || control.downs)
				dilation = LEVEL_0; // reset time so user inputs are used
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

		if (shift.pressed || up.pressed) {
			spaceship.thrust_percent = std::min(spaceship.thrust_percent + 1.0f , 100.0f);
		} else if (control.pressed || down.pressed) {
			spaceship.thrust_percent = std::max(spaceship.thrust_percent - 10.0f , 0.0f);
		}
	}

	if (dilation != LEVEL_0) {
		spaceship.dtheta = 0.f;
		spaceship.thrust_percent = 0.f;
	}

	{ //update text UI
		std::stringstream stream;
		stream << std::fixed << std::setprecision(1) << "thrust: " << spaceship.thrust_percent << "%" << '\n'
			   << '\n' << "fuel: " << spaceship.fuel << '\n'
			   << '\n' << "time: " << DilationSchematic(dilation) << " ("  << dilation << "x)";
		// stream << "a" << std::endl << "b";
		UI_text.set_text(stream.str());
	}

	{ //update listener to camera position:
		glm::mat4x3 frame = camera->transform->make_local_to_parent();
		glm::vec3 frame_right = frame[0];
		glm::vec3 frame_at = frame[3];
		Sound::listener.set_position_right(frame_at, frame_right, 1.0f / 60.0f);
	}

	{ //orbital simulation
		star->update(elapsed);
		asteroid.update(elapsed, spaceship.lasers);
		spaceship.update(elapsed);
	}

	{ // asteroid target
		glm::mat4 world_to_screen = camera->make_projection() * glm::mat4(camera->transform->make_world_to_local());
		glm::vec3 pos3d = glm::vec3(world_to_screen * glm::vec4(asteroid.pos, 1.0f)); // [-1..1]^2
		target_xy = glm::vec2{(pos3d.x / pos3d.z), (pos3d.y / pos3d.z)} * 0.5f + 0.5f; // -> [0,1]^2
	}

	{ // reticle tracker
		// make the reticle follow the mouse
		reticle_aim = mouse_motion;

		// unless it is "close" to another body, in which case track to that one
		glm::mat4 world_to_screen = camera->make_projection() * glm::mat4(camera->transform->make_world_to_local());
		float min_dist = std::numeric_limits<float>::max(); // infinity
		glm::vec2 homing_reticle_pos = reticle_aim;
		glm::vec3 homing_target{0.f, 0.f, 0.f};
		for (const Entity *entity : entities) {
			if (entity == (&spaceship) || !camera->in_view(entity->pos))
				continue; // don't shoot laser at self or offscreen objects
			glm::vec3 pos3d = glm::vec3(world_to_screen * glm::vec4(entity->pos, 1.0f));
			glm::vec2 pos2d{(pos3d.x / pos3d.z), (pos3d.y / pos3d.z)};
			float ss_dist = glm::length(reticle_aim - pos2d); // screen-space distance (for comparisons)
			if (ss_dist < homing_threshold && ss_dist < min_dist) {
				min_dist = ss_dist;
				homing_reticle_pos = pos2d;
				homing_target = entity->pos;
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

	{ //laser
		if (space.pressed){
			spaceship.lasers.emplace_back(Beam(spaceship.pos, spaceship.aim_dir));
		}

		spaceship.update_lasers(elapsed);
	}

	{ //update camera controls (after spaceship update for smooth motion)
		if (tab.downs > 0) {
			uint8_t dir = shift.pressed ? -1 : 1;
			camera_view_idx = (camera_view_idx + dir) % camera_arms.size();
		}
		CameraArm &camarm = CurrentCameraArm();

		if (can_pan_camera) {
			camarm.update(camera,  mouse_motion_rel.x,  mouse_motion_rel.y);
		}
		else {
			camarm.update(camera,  0.f,  0.f);
		}
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
	// TODO: consider using the Light(s) in the scene to do this
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

	{ //TODO: this is a demo of drawing the orbit
		glm::mat4 world_to_clip = camera->make_projection() * glm::mat4(camera->transform->make_world_to_local());
		DrawLines orbit_lines(world_to_clip);

		static constexpr glm::u8vec4 grey = glm::u8vec4(0x80, 0x80, 0x80, 0xff);
		static constexpr glm::u8vec4 blue = glm::u8vec4(0x00, 0x00, 0xff, 0xff);
		static constexpr glm::u8vec4 green = glm::u8vec4(0x00, 0xff, 0x00, 0xff);

		star->draw_orbits(orbit_lines, grey, CurrentCameraArm().camera_arm_length);
		spaceship.orbits.front().draw(orbit_lines, blue);
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
		float radius = radscale * CurrentCameraArm().scroll_zoom / CameraArm::init_scroll_zoom;
		if (radius > 1.f / radscale) {
			glm::vec3 heading = spaceship.get_heading();
			vector_lines.draw(spaceship.pos + heading * (0.5f * radius), spaceship.pos + heading * (1.5f * radius), white);

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
					auto seg_start = glm::vec3(center.x + circ_verts[(i) % num_verts].x, center.y + circ_verts[i % num_verts].y, center.z);
					auto seg_end = glm::vec3(center.x + circ_verts[(i + 1) % num_verts].x, center.y + circ_verts[(i + 1) % num_verts].y, center.z);
					vector_lines.draw(seg_start, seg_end, color);
				}
			};
			draw_circle(spaceship.pos, radius * glm::vec2(1.f, 1.f), white);
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

	{ // draw mouse cursor reticle
		static constexpr glm::u8vec4 red = glm::u8vec4(0xcc, 0x00, 0x00, 0x45); // target locked
		static constexpr glm::u8vec4 yellow = glm::u8vec4(0xcc, 0xd3, 0x00, 0x45);
		glm::u8vec4 color = reticle_homing ? red : yellow;
		glm::vec2 reticle_size= glm::vec2(200, 200);
		glm::vec2 reticle_pos{reticle_aim.x * drawable_size.x - 0.5f * reticle_size.x, reticle_aim.y * drawable_size.y + 0.5f * reticle_size.y};
		HUD::drawElement(reticle_size, reticle_pos, target, drawable_size, color);
	}

	{ //use DrawLines to overlay some text:
		float x = drawable_size.x * 0.03f;
		float y = drawable_size.y * (1.0f - 0.1f); // top is 1.f bottom is 0.f
		float width = drawable_size.x * 0.2f;
		UI_text.draw(1.f, drawable_size, width, glm::vec2(x, y), 1.f, DilationColor(dilation));
	}

	HUD::drawElement(glm::vec2(100, 300), glm::vec2(130, 320), throttle, drawable_size);
	HUD::drawElement(drawable_size, glm::vec2(0, drawable_size.y), window, drawable_size);
	float thrust_amnt = std::fabs(spaceship.thrust_percent) / 100.0f;
	glm::u8vec4 color{0xff};
	if (spaceship.thrust_percent > 0) {
		color = glm::u8vec4{0, 0xff, 0, 0xff}; // green
	} else {
		color = glm::u8vec4{0xff, 0, 0, 0xff}; // red
	}
	HUD::drawElement(glm::vec2(80, (250 * thrust_amnt)), glm::vec2(140, 32 + (250 * thrust_amnt)), bar, drawable_size, color);
	HUD::drawElement(glm::vec2(120, 30), glm::vec2(120, 60 + (250 * thrust_amnt)), handle, drawable_size);


	if (camera->in_view(asteroid.pos)) { // draw asteroid target
		glm::vec2 target_size = glm::vec2{150, 150};
		glm::vec2 target_pos{target_xy.x * drawable_size.x - 0.5f * target_size.x, target_xy.y * drawable_size.y + 0.5f * target_size.y};
		// draw_circle(reticle_pos, glm::vec2{reticle_radius_screen, reticle_radius_screen}, reticle_homing ? red : yellow);
		const auto orange = glm::u8vec4{241, 90, 34, 0x45};
		HUD::drawElement(target_size, target_pos, reticle, drawable_size, orange);
	}

	GL_ERRORS();
}

// glm::vec3 PlayMode::get_leg_tip_position() {
	//the vertex position here was read from the model in blender:
// 	return lower_leg->make_local_to_world() * glm::vec4(-1.26137f, -11.861f, 0.0f, 1.0f);
// }
