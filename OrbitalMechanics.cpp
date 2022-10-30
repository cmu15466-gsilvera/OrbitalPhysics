#include "OrbitalMechanics.hpp"

#include <glm/gtx/norm.hpp>

#define DEBUG
#ifdef DEBUG
#include <iostream>
#include <glm/gtx/string_cast.hpp> // glm::to_string
#define LOG(ARGS) std::cout << ARGS << std::endl
#else
#define LOG(ARGS)
#endif

//Windows doesn't have M_PI apparently
#define M_PI 3.141529f

//Time acceleration
float dilation = 10000.0f;


void Body::set_orbit(Orbit *orbit_) {
	orbit = orbit_;
	if (orbit == nullptr) return;
	pos = orbit->get_pos();
	vel = orbit->get_vel();
}

void Body::update(float elapsed) {
	//TODO: if we want things to rotate, here would be where that gets done

	if (orbit != nullptr) {
		orbit->update(elapsed);
		pos = orbit->get_pos();
		vel = orbit->get_vel();
		orbit->predict();
	}

	assert(transform != nullptr);
	transform->position = pos;

	//Recursively update all satellites, their satellites, and so on
	for (Body *body : satellites) {
		assert(body != nullptr);
		body->update(elapsed);
	}
}

void Body::simulate(float time) {
	if (orbit != nullptr) {
		orbit->simulate(time);
	}

	//Recursively simulate all satellites, their satellites, and so on
	for (Body *body : satellites) {
		assert(body != nullptr);
		body->simulate(time);
	}
}

void Body::draw_orbits(DrawLines &lines, glm::u8vec4 const &color) {
	if (orbit != nullptr) orbit->draw(lines, color);

	for (Body *body : satellites) {
		assert(body != nullptr);
		body->draw_orbits(lines, color);
	}
}


void Rocket::init(Orbit *orbit_, Scene::Transform *transform_) {
	assert(transform_ != nullptr);

	orbit = orbit_;
	pos = orbit->get_pos();
	vel = orbit->get_vel();
	acc = glm::vec3(0.0f);

	transform = transform_;
	transform->position = pos;
}

void Rocket::update(float dthrust, float dtheta, float elapsed, Body *root, std::list< Orbit > &orbits) {

	{ // rocket controls & physics
	/// TODO: clean up the physics of this
	#define ZERO_SPACE_FRICTION true // we can change this if we want acceleration not to decay
	#if ZERO_SPACE_FRICTION
		rotacc = glm::vec3(0.f, 0.f, theta_thrust);
	#else
		if (theta_thrust != 0.f)
		{
			rotacc = glm::vec3(0.f, 0.f, theta_thrust);
		}
		rotacc *= 0.9; // slight decay in rotational acceleration
	#endif

		rotvel += elapsed * rotacc;
		rot += elapsed * rotvel;
		{ // bound rotation angles within -pi, pi
			auto bound_angles = [](float &angle){
				if (angle > M_PI)
					angle -= 2 * M_PI;
				if (angle < -M_PI)
					angle += 2 * M_PI;
			};
			bound_angles(rot.x);
			bound_angles(rot.y);
			bound_angles(rot.z);
		}
		theta = rot.z; // yaw (rotation alonx XY plane)
		transform->rotation = glm::quat(rot);
	}

	{ // orbital mechanics
		// TODO: use thrust/theta to update/reconfigure orbit?
		assert(transform != nullptr);
		orbit->update(elapsed);
		pos = orbit->get_pos();
		vel = orbit->get_vel();

		Body *origin = orbit->origin;

		glm::vec3 rpos = pos - origin->pos;
		glm::vec3 rvel = vel - origin->vel;

		if (!origin->in_soi(pos)) {
			Body *new_origin = origin->orbit->origin;
			assert(new_origin != nullptr);

			orbits.emplace_back(Orbit(new_origin, pos, vel, false));
			orbit = &orbits.back();
		}

		for (Body *satellite : origin->satellites) {
			if (satellite->in_soi(pos)) {
				orbits.emplace_back(Orbit(satellite, pos, vel, false));
				orbit = &orbits.back();
				break;
			}
		}

		orbit->sim_predict(root, orbits, 0);
		// orbit->predict();

		transform->position = pos;
	}
}

// Create new orbit based on old one
void Rocket::recalculate_orbits() {
	//TODO
}

Orbit::Orbit(Body *origin_, glm::vec3 pos, glm::vec3 vel, bool simulated) : origin(origin_) {
	//Math references:
	//https://orbital-mechanics.space/classical-orbital-elements/orbital-elements-and-the-state-vector.html
	//https://scienceworld.wolfram.com/physics/SemilatusRectum.html

	// LOG("Entering new orbit around: " << origin_->transform->name);
	// LOG("\tentry pos: " << glm::to_string(pos));
	// LOG("\tentry vel: " << glm::to_string(vel));

	mu = G * origin->mass;
	glm::vec3 d, v;
	if (!simulated || origin->orbit == nullptr) {
		d = pos - origin->pos; //relative position
		v = vel - origin->vel; //relative velocity
	} else {
		Orbit *orbit = origin->orbit;
		d = pos - orbit->sim.pos; //relative position
		v = vel - orbit->sim.vel; //relative velocity
	}

	glm::vec3 h = glm::cross(d, v); //specific orbital angular momentum

	//TODO: handle retrograde orbits (180 degree inclination)
	assert(h.z >= 0 && "Need to implement retrograde orbits");

	glm::vec3 e = glm::cross(v, h) / mu - glm::normalize(d); //eccentricity vector, points to periapsis

	//Assuming no inclination
	assert(e.z == 0.0f);

	c = glm::l2Norm(e);
	p = glm::length2(h) / mu;
	if (c == 0.0f) { //true circular orbit doesn't really have a periapsis e will be zero-vector with no direction
		phi = 0.0f;
	} else {
		phi = glm::atan(e.y, e.x);
	}
	if (c != 1.0f) {
		a = p / (1.0f - c * c);
	} else {
		a = std::numeric_limits< float >::infinity();
	}

	// LOG("\tc: " << c << " p: " << p << " phi: " << phi << " a: " << a);

	theta = glm::atan(d.y, d.x) - phi;
	compute_r();
	compute_dtheta();

	// LOG("\tnew pos: " << glm::to_string(get_pos()));
	// LOG("\tnew vel: " << glm::to_string(get_vel()));

	init_sim();
}

Orbit::Orbit(Body *origin_, float c_, float p_, float phi_, float theta_)
		: origin(origin_), c(c_), p(p_), phi(phi_), theta(theta_) {

	if (c != 1.0f) {
		a = p / (1.0f - c * c);
	} else {
		a = std::numeric_limits< float >::infinity();
	}

	mu = G * origin->mass;

	LOG("Created orbit around: " << origin_->transform->name);
	LOG("\tc: " << c << " p: " << p << " phi: " << phi << " a: " << a);

	compute_r();
	compute_dtheta();

	init_sim();
}

void Orbit::update(float elapsed) {
	const float time_step = dilation / 100.0f;
	for (float t = 0.0f; t < elapsed * dilation; t += time_step) {
		theta += dtheta * time_step;
		compute_r();
		compute_dtheta();
	}

	init_sim();
}

glm::vec3 Orbit::get_pos() {
	float x = r * std::cos(theta + phi) + origin->pos.x;
	float y = r * std::sin(theta + phi) + origin->pos.y;

	return glm::vec3(x, y, 0.0f);
}

glm::vec3 Orbit::get_vel() {
	static float constexpr pi_over_2 = glm::radians(90.0f);

	float v_tan = dtheta * r;
	float dx = v_tan * std::cos(theta + phi + pi_over_2) + origin->vel.x;
	float dy = v_tan * std::sin(theta + phi + pi_over_2) + origin->vel.y;

	return glm::vec3(dx, dy, 0.0f);
}

void Orbit::predict() {
	float origin_x = origin->pos.x;
	float origin_y = origin->pos.y;
	float soi_radius = origin->soi_radius;

	const float theta_init = std::floor(theta / glm::radians(PredictAngle)) * glm::radians(PredictAngle);
	// LOG(theta_init);

	points[0] = get_pos();
	for (int i = 1; i < PredictDetail; i++) {
		float theta_ = theta_init + glm::radians(static_cast< float >(i) * PredictAngle);
		float r_ = p / (1 + c * std::cos(theta_));

		if (r_ < 0 || r_ > soi_radius) {
			points[i] = Invalid;
			break;
		}

		float x = r_ * std::cos(theta_ + phi) + origin_x;
		float y = r_ * std::sin(theta_ + phi) + origin_y;

		points[i].x = x;
		points[i].y = y;
		points[i].z = 0.0f;
	}
}

void Orbit::init_sim() {
	sim.r = r;
	sim.theta = theta;
	sim.dtheta = dtheta;
	sim.pos = get_pos();
	sim.vel = get_vel();
}

void Orbit::sim_predict(Body *root, std::list< Orbit > &orbits, int level) {
	float origin_x = origin->pos.x;
	float origin_y = origin->pos.y;

	points[0] = sim.pos;
	float aligned = std::ceil(sim.theta / glm::radians(PredictAngle)) * glm::radians(PredictAngle);
	float step = (aligned - sim.theta) / sim.dtheta;
	for (int i = 1; i < PredictDetail; i++) {
		root->simulate(step);
		simulate(step);
		step = glm::radians(PredictAngle) / sim.dtheta * (1 + level);

		if (sim.r > origin->soi_radius) {
			points[i] = Invalid;

			if (level >= MaxLevel) return;

			// SOI transfer to origin of origin
			if (continuation == nullptr) {
				assert(origin->orbit != nullptr);
				orbits.emplace_back(Orbit(origin->orbit->origin, sim.pos, sim.vel, true));
				continuation = &orbits.back();
			} else {
				*continuation = Orbit(origin->orbit->origin, sim.pos, sim.vel, true);
			}
			continuation->sim_predict(root, orbits, level+1);
			return;
		}

		for (Body *satellite : origin->satellites) {
			assert(satellite != nullptr && satellite->orbit != nullptr);
			if (glm::distance(sim.pos, satellite->orbit->sim.pos) < satellite->soi_radius) {
				points[i] = Invalid;

				if (level >= MaxLevel) return;

				// SOI transfer to satellite of origin
				if (continuation == nullptr) {
					orbits.emplace_back(Orbit(satellite, sim.pos, sim.vel, true));
					continuation = &orbits.back();
				} else {
				*continuation = Orbit(satellite, sim.pos, sim.vel, true);
			}
				continuation->sim_predict(root, orbits, level+1);
				return;
			}
		}

		points[i].x = sim.r * std::cos(sim.theta + phi) + origin_x;
		points[i].y = sim.r * std::sin(sim.theta + phi) + origin_y;
		points[i].z = 0.0f;
	}
}

void Orbit::simulate(float time) {
	static float constexpr pi_over_2 = glm::radians(90.0f);
	static float constexpr steps = 100.0f;
	const float time_step = time / steps ;
	for (float t = 0.0f; t < time; t += time_step) {
		sim.theta += sim.dtheta * time_step;
		sim.r = p / (1.0f + c * std::cos(sim.theta));
		sim.dtheta = std::sqrt(mu * (2.0f / sim.r - 1.0f / a)) / sim.r;
	}

	sim.pos.x = sim.r * std::cos(sim.theta + phi);
	sim.pos.y = sim.r * std::sin(sim.theta + phi);
	sim.pos.z = 0.0f;

	if (origin->orbit != nullptr) sim.pos += origin->orbit->sim.pos;

	sim.vel.x = sim.dtheta * sim.r * std::cos(sim.theta + phi + pi_over_2);
	sim.vel.y = sim.dtheta * sim.r * std::sin(sim.theta + phi + pi_over_2);
	sim.vel.z = 0.0f;

	if (origin->orbit != nullptr) sim.vel += origin->orbit->sim.vel;
}

void Orbit::draw(DrawLines &lines, glm::u8vec4 const &color) {
	size_t n = points.size();

	// LOG(n << " points");
	for (size_t i = 0; i < n; i++) {
		// LOG("drawing " << glm::to_string(points[i-1])  << " to " << glm::to_string(points[i]));
		glm::vec3 next = points[(i + 1) % n];
		if (next == Orbit::Invalid) break;
		lines.draw(points[i], next, color);
	}

	if (continuation != nullptr) continuation->draw(lines, color);
}