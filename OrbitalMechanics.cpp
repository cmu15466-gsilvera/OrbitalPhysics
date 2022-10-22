#include "OrbitalMechanics.hpp"

#include <glm/gtx/norm.hpp>

#define DEBUG
#ifdef DEBUG
#include <iostream>
#define LOG(ARGS) std::cout << ARGS << std::endl;
#else
#define LOG(ARGS)
#endif


Orbit::Orbit(Body *origin_, glm::vec3 pos, glm::vec3 vel) : origin(origin_) {
	//Math references:
	//https://orbital-mechanics.space/classical-orbital-elements/orbital-elements-and-the-state-vector.html
	//https://scienceworld.wolfram.com/physics/SemilatusRectum.html

	float mu = G * origin->mass;
	glm::vec3 d = pos - origin_->pos; // relative position
	glm::vec3 v = vel - origin_->vel; // relative velocity
	glm::vec3 h = glm::cross(d, v); // specific orbital angular momentum
	glm::vec3 e = glm::cross(v, h) / mu - glm::normalize(d); // eccentricity vector, points to periapsis

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

	compute_r();
	compute_dtheta();
}

Orbit::Orbit(Body *origin_, float c_, float p_, float phi_, float theta_)
		: origin(origin_), c(c_), p(p_), phi(phi_), theta(theta_) {

	if (c != 1.0f) {
		a = p / (1.0f - c * c);
	} else {
		a = std::numeric_limits< float >::infinity();
	}

	compute_r();
	compute_dtheta();
}

glm::vec3 Orbit::update(float elapsed) {
	for (float t = 0.0f; t < elapsed; t += TimeStep) {
		theta += dtheta * TimeStep;
		compute_r();
		compute_dtheta();
	}

	return get_pos();
}

glm::vec3 Orbit::get_pos() {
	float x = r * std::cos(theta + phi) + origin->pos.x;
	float y = r * std::sin(theta + phi) + origin->pos.y;

	return glm::vec3(x, y, 0.0f);
}

void Orbit::predict() {
	float origin_x = origin->pos.x;
	float origin_y = origin->pos.y;
	float soi_radius = origin->soi_radius;

	for (int i = 0; i < PredictDetail; i++) {
		float theta_ = glm::radians(static_cast< float >(i) * PredictAngle);
		float r_ = p / (1 + c * std::cos(theta_));

		if (r_ < 0 || r_ > soi_radius) {
			points[i] = Invalid;
			continue;
		}

		float x = r_ * std::cos(theta_ + phi) + origin_x;
		float y = r_ * std::sin(theta_ + phi) + origin_y;

		points[i].x = x;
		points[i].y = y;
	}
}