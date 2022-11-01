#include "PlayMode.hpp"

#include "LitColorTextureProgram.hpp"

#include "DrawLines.hpp"
#include "Mesh.hpp"
#include "Load.hpp"
#include "gl_errors.hpp"
#include "data_path.hpp"

#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/string_cast.hpp>

#include <iomanip>
#include <sstream>


#define DEBUG

#ifdef DEBUG
#include <iostream>
#define LOG(ARGS) std::cout << ARGS << std::endl
#else
#define LOG(ARGS)
#endif

GLuint orbit_meshes_for_lit_color_texture_program = 0;
Load< MeshBuffer > main_meshes(LoadTagDefault, []() -> MeshBuffer const * {
	MeshBuffer const *ret = new MeshBuffer(data_path("orbit.pnct"));
	orbit_meshes_for_lit_color_texture_program = ret->make_vao_for_program(lit_color_texture_program->program);
	return ret;
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

//Helper to set up a drawable given a transform. Note that mesh_name comes from transform->name.
static void make_drawable(Scene &scene, Scene::Transform *transform) {
	assert(transform != nullptr);
	Mesh const &mesh = main_meshes->lookup(transform->name);

	scene.drawables.emplace_back(transform);
	Scene::Drawable &drawable = scene.drawables.back();

	drawable.pipeline = lit_color_texture_program_pipeline;

	drawable.pipeline.vao = orbit_meshes_for_lit_color_texture_program;
	drawable.pipeline.type = mesh.type;
	drawable.pipeline.start = mesh.start;
	drawable.pipeline.count = mesh.count;
}

PlayMode::PlayMode() : scene(*orbit_scene) {
	//get pointers to leg for convenience:
	// for (auto &transform : scene.transforms) {
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

	//get pointer to camera for convenience:
	if (scene.cameras.size() != 1) {
		throw std::runtime_error("Expecting scene to have exactly one camera, but it has "
			+ std::to_string(scene.cameras.size()));
	}
	camera = &scene.cameras.front();

	// first focus should be on the spaceship!
	entities.push_back(&spaceship);

	//start music loop playing:
	// (note: position will be over-ridden in update())
	// leg_tip_loop = Sound::loop_3D(*dusty_floor_sample, 1.0f, get_leg_tip_position(), 10.0f);

	//TODO: This is hacked together for dev demo purposes, mostly so we can test orbital simulation
	// The masses and distances are fudged for now. Things would normally be much futher away and move much
	// slower (if we're going for realism)
	{ //Load star
		scene.transforms.emplace_back();
		Scene::Transform *star_trans = &scene.transforms.back();
		star_trans->name = "Star";

		bodies.emplace_back(Body(275.0, 8.0e23f, std::numeric_limits< float >::infinity()));
		star = &bodies.back();
		entities.push_back(star);

		star->set_transform(star_trans);

		make_drawable(scene, star_trans);
		LOG("Loaded Star");
	}

	{ //Load planet
		scene.transforms.emplace_back();
		Scene::Transform *planet_trans = &scene.transforms.back();
		planet_trans->name = "Planet";

		bodies.emplace_back(Body(10.0, 6.0e18f, 1000.0f));
		Body *planet = &bodies.back();
		entities.push_back(planet);

		orbits.emplace_back(Orbit(star, 0.0f, 100000.0f, 0.0f, 0.0f, false));
		Orbit *planet_orbit = &orbits.back();

		planet->set_orbit(planet_orbit);
		planet->set_transform(planet_trans);

		star->add_satellite(planet);

		make_drawable(scene, planet_trans);
		LOG("Loaded Planet");

		//Load all things orbiting planet
		{ //Load moon of planet
			scene.transforms.emplace_back();
			Scene::Transform *moon_trans = &scene.transforms.back();
			moon_trans->name = "Moon";

			bodies.emplace_back(Body(1.0, 7.0e16f, 50.0));
			Body *moon = &bodies.back();
			entities.push_back(moon);

			orbits.emplace_back(Orbit(planet, 0.1f, 200.0f, glm::radians(30.0f), glm::radians(-120.0f), false));
			Orbit *moon_orbit = &orbits.back();

			moon->set_orbit(moon_orbit);
			moon->set_transform(moon_trans);

			planet->add_satellite(moon);

			make_drawable(scene, moon_trans);
			LOG("Loaded Moon");
		}
		{ //Load player
			scene.transforms.emplace_back();
			Scene::Transform *spaceship_trans = &scene.transforms.back();
			spaceship_trans->name = "Spaceship";

			// glm::vec3 rpos = glm::vec3(30.0f, 0.0f, 0.0f);
			// glm::vec3 rvel = glm::vec3(0.0f, 0.003654f, 0.0f);

			// orbits.emplace_back(Orbit(planet, planet->pos + rpos, planet->vel + rvel));

			orbits.emplace_back(Orbit(planet, 0.866f, 30.0f, glm::radians(120.0f), glm::radians(0.0f), false));
			Orbit *spaceship_orbit = &orbits.back();

			spaceship.init(spaceship_orbit, spaceship_trans, star, orbits);

			make_drawable(scene, spaceship_trans);
			LOG("Loaded Spaceship");
		}
	}

	// track order of focus points for camera
	for (const Entity *entity : entities) {
		camera_arms.insert({entity, CameraArm(entity)});
		camera_views.push_back(entity);
		camera_arms.at(entity).ScrollSensitivity = entity->radius;
	}
	// tune custom params as follows
	camera_arms.at(&spaceship).scroll_zoom = 40.f;
}

PlayMode::~PlayMode() {
}

bool PlayMode::handle_event(SDL_Event const &evt, glm::uvec2 const &window_size) {
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
		if (SDL_GetRelativeMouseMode() == SDL_FALSE) {
			SDL_SetRelativeMouseMode(SDL_TRUE);
			return true;
		}
	} else if (evt.type == SDL_MOUSEBUTTONUP) {
		// show the mouse cursor again once the mouse is released
		if (SDL_GetRelativeMouseMode() == SDL_TRUE) {
			SDL_SetRelativeMouseMode(SDL_FALSE);
			return true;
		}
	} else if (evt.type == SDL_MOUSEMOTION) {
		if (SDL_GetRelativeMouseMode() == SDL_TRUE) {
		 	mouse_motion_rel = glm::vec2(
				evt.motion.xrel / float(window_size.y),
				-evt.motion.yrel / float(window_size.y)
			);
			return true;
		}
	} else if(evt.type == SDL_MOUSEWHEEL) {
		auto &camarm = CurrentCameraArm();
		camarm.scroll_zoom = std::max(camarm.scroll_zoom - evt.wheel.y * camarm.ScrollSensitivity, 0.0f);
		// evt.wheel.x for horizontal scrolling
	}
	//TODO: down the line, we might want to record mouse motion if we want to support things like click-and-drag
	//(for orbital manuever planning tool)

	return false;
}

void PlayMode::CameraArm::update(Scene::Camera *cam, float mouse_x, float mouse_y) {
	const glm::vec3 &focus_point = entity->pos;
	float camera_arm_length = entity->radius * (10.0f + scroll_zoom);

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
	{ // update camera controls
		if (tab.downs > 0) {
			uint8_t dir = shift.pressed ? -1 : 1;
			camera_view_idx = (camera_view_idx + dir) % camera_arms.size();
		}
		CameraArm &camarm = CurrentCameraArm(); 
		camarm.update(camera,  mouse_motion_rel.x,  mouse_motion_rel.y);
	}

	if (plus.downs > 0 && minus.downs == 0) {
		dilation++;
	} else if (minus.downs > 0 && plus.downs == 0) {
		dilation--;
	}

	if (dilation == LEVEL_0){ // update rocket controls

		// accumulate angular acceleration
		if (left.downs > 0 && right.downs == 0) {
			spaceship.control_dtheta += 2.0f * (M_PI / 180.f);
		} else if (right.downs > 0 && left.downs == 0) {
			spaceship.control_dtheta += -2.0f * (M_PI / 180.f);
		} else {
			spaceship.control_dtheta *= 0.99; // slow decay
			if (std::fabs(spaceship.control_dtheta) < 0.01) // threshold to 0
				spaceship.control_dtheta = 0.f;
		}

		if (shift.pressed) {
			spaceship.thrust_percent = std::min(spaceship.thrust_percent + 0.1f , 100.0f);
		} else if (control.pressed) {
			spaceship.thrust_percent = std::max(spaceship.thrust_percent - 0.1f , 0.0f);
		}
	} else {
		spaceship.dtheta = 0.f;
		spaceship.thrust_percent = 0.f;
	}

	{ //update listener to camera position:
		glm::mat4x3 frame = camera->transform->make_local_to_parent();
		glm::vec3 frame_right = frame[0];
		glm::vec3 frame_at = frame[3];
		Sound::listener.set_position_right(frame_at, frame_right, 1.0f / 60.0f);
	}

	{ //basic orbital simulation demo
		star->update(elapsed);
		spaceship.update(elapsed, star, orbits);
	}

	//reset button press counters:
	for (auto &keybinding : keybindings) {
		Button &button = *keybinding.first;
		button.downs = 0;
	}
	mouse_motion_rel = glm::vec2(0, 0);
}

void PlayMode::draw(glm::uvec2 const &drawable_size) {
	//update camera aspect ratio for drawable:
	camera->aspect = float(drawable_size.x) / float(drawable_size.y);

	//set up light type and position for lit_color_texture_program:
	// TODO: consider using the Light(s) in the scene to do this
	glUseProgram(lit_color_texture_program->program);
	glUniform3fv(lit_color_texture_program->AMBIENT_COLOR_vec3, 1, glm::value_ptr(glm::vec3(0.2f, 0.2f,0.2f)));
	glUniform3fv(lit_color_texture_program->LIGHT_DIRECTION_vec3, 1, glm::value_ptr(glm::vec3(0.0f, 0.0f,-1.0f)));
	glUniform3fv(lit_color_texture_program->LIGHT_LOCATION_vec3, 1, glm::value_ptr(star->transform->position));
	glUniform3fv(lit_color_texture_program->LIGHT_ENERGY_vec3, 1, glm::value_ptr(glm::vec3(1.0f, 1.0f, 0.95f)));
	glUseProgram(0);

	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	glClearDepth(1.0f); //1.0 is actually the default value to clear the depth buffer to, but FYI you can change it.
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS); //this is the default depth comparison function, but FYI you can change it.

	scene.draw(*camera);

	{ //TODO: this is a demo of drawing the orbit
		glm::mat4 world_to_clip = camera->make_projection() * glm::mat4(camera->transform->make_world_to_local());
		DrawLines orbit_lines(world_to_clip);

		static constexpr glm::u8vec4 purple = glm::u8vec4(0xff, 0x00, 0xff, 0xff);
		static constexpr glm::u8vec4 blue = glm::u8vec4(0x00, 0x00, 0xff, 0xff);

		star->draw_orbits(orbit_lines, purple);
		spaceship.orbit->draw(orbit_lines, blue);
	}

	//Everything from this point on is part of the HUD overlay
	glDisable(GL_DEPTH_TEST);

	{ //DEBUG: draw spaceship (relative) position, (relative) velocity, heading, and acceleration vectors
		glm::mat4 world_to_clip = camera->make_projection() * glm::mat4(camera->transform->make_world_to_local());
		DrawLines vector_lines(world_to_clip);

		Orbit const &orbit = *spaceship.orbit;

		static constexpr glm::u8vec4 white = glm::u8vec4(0xff, 0xff, 0xff, 0xff); //rpos
		static constexpr glm::u8vec4 yellow = glm::u8vec4(0xff, 0xd3, 0x00, 0xff); //heading
		static constexpr glm::u8vec4 green = glm::u8vec4(0x00, 0xff, 0x20, 0xff); //rvel
		static constexpr glm::u8vec4 red = glm::u8vec4(0xff, 0x00, 0x00, 0xff); //acc
		// static float constexpr display_multiplier = 1000.0f;

		glm::vec3 heading = glm::vec3(
			std::cos(spaceship.theta),
			std::sin(spaceship.theta),
			0.0f
		) * 4.0f;

		vector_lines.draw(spaceship.pos, spaceship.pos + orbit.rpos, white);
		vector_lines.draw(spaceship.pos, spaceship.pos + heading, yellow);
		vector_lines.draw(spaceship.pos, spaceship.pos + orbit.rvel * 1000.0f, green);
		vector_lines.draw(spaceship.pos, spaceship.pos + spaceship.acc * 10000.0f, red);
	}

	{ //use DrawLines to overlay some text:
		float aspect = float(drawable_size.x) / float(drawable_size.y);
		DrawLines lines(glm::mat4(
			1.0f / aspect, 0.0f, 0.0f, 0.0f,
			0.0f, 1.0f, 0.0f, 0.0f,
			0.0f, 0.0f, 1.0f, 0.0f,
			0.0f, 0.0f, 0.0f, 1.0f
		));

		constexpr float H = 0.09f;

		std::stringstream stream;
		stream << std::fixed << std::setprecision(2)
			<< "thrust percent: " << spaceship.thrust_percent
			<< " fuel: " << spaceship.fuel
			<< " dilation: " << static_cast< float >(dilation);

		std::string text = stream.str();
		lines.draw_text(text,
			glm::vec3(-aspect + 0.1f * H, -1.0 + 0.1f * H, 0.0),
			glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
			glm::u8vec4(0x00, 0x00, 0x00, 0x00));
		float ofs = 2.0f / drawable_size.y;
		lines.draw_text(text,
			glm::vec3(-aspect + 0.1f * H + ofs, -1.0 + + 0.1f * H + ofs, 0.0),
			glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
			glm::u8vec4(0xff, 0xff, 0xff, 0x00));
	}
	GL_ERRORS();
}

// glm::vec3 PlayMode::get_leg_tip_position() {
	//the vertex position here was read from the model in blender:
// 	return lower_leg->make_local_to_world() * glm::vec4(-1.26137f, -11.861f, 0.0f, 1.0f);
// }
