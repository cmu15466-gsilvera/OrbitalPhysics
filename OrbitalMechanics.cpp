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
DilationLevel dilation = LEVEL_0;


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

void Rocket::update(float elapsed, Body *root, std::list< Orbit > &orbits) {
	bool moved = false;
	{ //rocket controls & physics
		//Going to assume stability assist via reaction wheels is always on and the controller is perfect to simplify
		// things. We can make the game harder later on by changing this to RCS based if need.
		theta += elapsed * dtheta; // yaw (rotation alonx XY plane)
		if (theta > M_PI)
			theta -= 2 * M_PI;
		if (theta < -M_PI)
			theta += 2 * M_PI;
		transform->rotation = glm::quat(glm::vec3(0.f, 0.f, theta));

		if (thrust_percent > 0.0f && fuel > 0.0f) {
			moved = true; //set flag to trigger orbit recalc later on

			//calculate acceleration: F = ma ==> acc = thrust * MaxThrust / (DryMass + fuel);
			//Note conversion from MegaNewtons/Megagram == meter/s^2 to Megameter/s^2
			float acc_magnitude = thrust_percent * MaxThrust / (DryMass + fuel * 1000.0f);
			acc = glm::vec3(
				acc_magnitude * std::cos(theta),
				acc_magnitude * std::sin(theta),
				0.0f
			);

			//update fuel consumption
			fuel = std::max(fuel - thrust_percent * MaxFuelConsumption, 0.0f);

			//update velocity
			vel += acc * elapsed;
		} else {
			acc = glm::vec3(0.0f);
		}
	}

	{ //orbital mechanics
		if (moved) {
			//recalculate orbit due to thrust
			Orbit *temp = orbit->continuation;
			*orbit = Orbit(orbit->origin, pos, vel, false);
			orbit->continuation = temp;
		}

		orbit->update(elapsed);
		assert(orbit->r > orbit->origin->radius);
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

		assert(transform != nullptr);
		transform->position = pos;
	}
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

	glm::vec3 hvec = glm::cross(d, v); //specific orbital angular momentum
	// LOG("\thvec: " << glm::to_string(hvec));

	//Remove inclination
	hvec.x = 0.0f;
	hvec.y = 0.0f;

	// incl = glm::acos(h.z / glm::l2Norm(h));
	incl = hvec.z >= 0.0f ? 0.0f : M_PI;
	float sign = hvec.z >= 0.0f ? 1.0f : -1.0f;

	glm::vec3 e = glm::cross(v, hvec) / mu - glm::normalize(d); //eccentricity vector, points to periapsis
	// LOG("\te: " << glm::to_string(e));
	//Remove inclination
	e.z = 0.0f;

	c = glm::l2Norm(e);

	float temp = glm::length2(hvec);
	p = temp / mu;
	mu_over_h = mu / std::sqrt(temp);

	if (c == 0.0f) { //true circular orbit doesn't really have a periapsis e will be zero-vector with no direction
		phi = 0.0f;
	} else {
		phi = glm::atan(e.y, e.x);
	}

	if (std::abs(c - 1.0f) > 1.0e-5f) {
		a = p / (1.0f - c * c);
	} else {
		a = std::numeric_limits< float >::infinity();
	}
	inv_a = 1.0f / a;

	// LOG("\tc: " << c << " p: " << p << " phi: " << phi << " a: " << a << " incl: " << incl);

	rot = glm::mat3(
		std::cos(-phi), -std::sin(-phi), 0.0f,
		std::sin(-phi), std::cos(-phi), 0.0f,
		0.0f, 0.0f, 1.0f
	) * glm::mat3(
		1.0f, 0.0f, 0.0f,
		0.0f, std::cos(incl), std::sin(incl),
		0.0f, -std::sin(incl), std::cos(incl)
	);

	theta = sign * (glm::atan(d.y, d.x) - phi);
	if (p > 1.0e-4f) {
		compute_r();
		compute_dtheta();
	} else {
		//degenerate orbit (freefall), do not use Kepler's equations
		r = glm::l2Norm(d);
		dtheta = 0.0f;
		p = 0.0f;
		rpos = d;
		rvel = v;
	}

	// LOG("\tr: " << r << " theta: " << theta << " dtheta: " << dtheta);

	// LOG("\tnew pos: " << glm::to_string(get_pos()));
	// LOG("\tnew vel: " << glm::to_string(get_vel()));

	init_sim();
}

Orbit::Orbit(Body *origin_, float c_, float p_, float phi_, float theta_, bool retrograde)
		: origin(origin_), c(c_), p(p_), phi(phi_), theta(theta_) {

	incl = retrograde ? M_PI : 0.0f;
	if (c != 1.0f) {
		a = p / (1.0f - c * c);
	} else {
		a = std::numeric_limits< float >::infinity();
	}
	inv_a = 1.0f / a;
	mu = G * origin->mass;
	mu_over_h = mu / std::sqrt(mu * p);
	rot = glm::mat3(
		std::cos(-phi), -std::sin(-phi), 0.0f,
		std::sin(-phi), std::cos(-phi), 0.0f,
		0.0f, 0.0f, 1.0f
	) * glm::mat3(
		1.0f, 0.0f, 0.0f,
		0.0f, std::cos(incl), std::sin(incl),
		0.0f, -std::sin(incl), std::cos(incl)
	);

	LOG("Created orbit around: " << origin_->transform->name);
	LOG("\tc: " << c << " p: " << p << " phi: " << phi << " a: " << a << " incl: " << incl);

	compute_r();
	compute_dtheta();

	init_sim();
}

void Orbit::update(float elapsed) {
	const float time_step = static_cast<float>(dilation) / 100.0f;
	glm::vec3 drvel;
	for (float t = 0.0f; t < elapsed * dilation; t += time_step) {
		if (p != 0.0f) {
			theta += dtheta * time_step;
			compute_r();
			compute_dtheta();
		} else {
			float f = -mu / (r * r);
			drvel = time_step * f * (rot * glm::vec3(
				std::cos(theta),
				std::sin(theta),
				0.0f
			));
			rpos += (rvel + 0.5f * drvel) * time_step;
			rvel += drvel;
			r = glm::l2Norm(rpos);
			theta = glm::atan(rpos.y, rpos.x);
		}
	}

	init_sim();
}

glm::vec3 Orbit::get_pos() {
	if (p != 0.0f) {
		return rot * glm::vec3(
			r * std::cos(theta),
			r * std::sin(theta),
			0.0f
		) + origin->pos;
	} else {
		return rpos + origin->pos;
	}
}

glm::vec3 Orbit::get_vel() {
	if (p != 0.0f) {
		return rot * glm::vec3(
			-mu_over_h * std::sin(theta),
			mu_over_h * (c + std::cos(theta)),
			0.0f
		) + origin->vel;
	} else {
		return rvel + origin->vel;
	}
}

void Orbit::predict() {
	glm::vec3 &origin_pos = origin->pos;
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

		points[i] = rot * glm::vec3(
			r_ * std::cos(theta_),
			r_ * std::sin(theta_),
			0.0f
		) + origin_pos;
	}
}

void Orbit::init_sim() {
	sim.r = r;
	sim.theta = theta;
	sim.dtheta = dtheta;
	sim.pos = get_pos();
	sim.vel = get_vel();

	sim.rpos = rpos; //only used for degenerate case
	sim.rvel = rvel; //only used for degenerate case
}

void Orbit::sim_predict(Body *root, std::list< Orbit > &orbits, int level) {
	glm::vec3 &origin_pos = origin->pos;

	points[0] = sim.pos;
	float aligned = std::ceil(sim.theta / glm::radians(PredictAngle)) * glm::radians(PredictAngle);
	float step = p != 0.0f ? (aligned - sim.theta) / sim.dtheta : 10.0f;
	for (int i = 1; i < PredictDetail; i++) {
		root->simulate(step);
		simulate(step);
		step = p != 0.0f ? glm::radians(PredictAngle) / sim.dtheta : 10.0f;

		if (sim.r > origin->soi_radius) {
			points[i] = Invalid;

			if (level >= MaxLevel) return;

			// SOI transfer to origin of origin
			if (continuation == nullptr) {
				assert(origin->orbit != nullptr);
				orbits.emplace_back(Orbit(origin->orbit->origin, sim.pos, sim.vel, true));
				continuation = &orbits.back();
			} else {
				Orbit *temp = continuation->continuation;
				*continuation = Orbit(origin->orbit->origin, sim.pos, sim.vel, true);
				continuation->continuation = temp;
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
					Orbit *temp = continuation->continuation;
					*continuation = Orbit(satellite, sim.pos, sim.vel, true);
					continuation->continuation = temp;
				}
				continuation->sim_predict(root, orbits, level+1);
				return;
			}
		}

		points[i] = rot * glm::vec3(
			sim.r * std::cos(sim.theta),
			sim.r * std::sin(sim.theta),
			0.0f
		) + origin_pos;
	}

	if (continuation != nullptr) {
		//TODO: cleanup continuation since there's no longer an SOI transition
		continuation = nullptr;
	}
}

void Orbit::simulate(float time) {
	static float constexpr steps = 100.0f;
	const float time_step = time / steps ;
	glm::vec3 drvel;
	for (float t = 0.0f; t < time; t += time_step) {
		if (p != 0.0f) {
			sim.theta += sim.dtheta * time_step;
			sim.r = p / (1.0f + c * std::cos(sim.theta));
			sim.dtheta = std::sqrt(mu * (2.0f / sim.r - inv_a)) / sim.r;
		} else {
			float f = -mu / (sim.r * sim.r);
			drvel = time_step * f * (rot * glm::vec3(
				std::cos(sim.theta),
				std::sin(sim.theta),
				0.0f
			));
			sim.rpos += (sim.rvel + 0.5f * drvel) * time_step;
			sim.rvel += drvel;
			sim.r = glm::l2Norm(sim.rpos);
			sim.theta = glm::atan(sim.rpos.y, sim.rpos.x);
		}
	}

	if (p != 0.0f) {
		sim.pos = rot * glm::vec3(
			sim.r * std::cos(sim.theta),
			sim.r * std::sin(sim.theta),
			0.0f
		);

		if (origin->orbit != nullptr) sim.pos += origin->orbit->sim.pos;

		sim.vel = rot * glm::vec3(
			-mu_over_h * std::sin(sim.theta),
			mu_over_h * (c + std::cos(sim.theta)),
			0.0f
		);

		if (origin->orbit != nullptr) sim.vel += origin->orbit->sim.vel;
	} else {
		sim.pos = origin->orbit != nullptr ? sim.rpos + origin->orbit->sim.pos : sim.rpos;
		sim.vel = origin->orbit != nullptr ? sim.rvel + origin->orbit->sim.vel : sim.rvel;
	}
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