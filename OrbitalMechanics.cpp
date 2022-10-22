#include "OrbitalMechanics.hpp"

#include <glm/gtx/polar_coordinates.hpp>


#define DEBUG
#ifdef DEBUG
#include <iostream>
#define LOG(ARGS) std::cout << ARGS << std::endl;
#else
#define LOG(ARGS)
#endif


void Body::set_orbit(Orbit *orbit_) {
	throw std::runtime_error("Not yet implemented");
}

bool Body::check_collision(glm::vec3 target_pos, float target_radius) {
	throw std::runtime_error("Not yet implemented");
}

//calculate c, a, phi using orbital position, orbital velocity, and origin body
Orbit::Orbit(Body *origin, glm::vec3 pos, glm::vec3 vel) {
	throw std::runtime_error("Not yet implemented");
}

//Given position and time, get updated position
glm::vec3 Orbit::get_pos(glm::vec3 pos, float elapsed) {
	throw std::runtime_error("Not yet implemented");
}

//Simulate the oribit (populate points)
void Orbit::predict() {
	glm::vec3 origin_pos = glm::vec3(0.0f, 0.0f, 0.0f);

	float numerator = a * (1 - c * c);

	for (int i = 0; i < 360; i++) {
		float theta = glm::radians(static_cast< float >(i));
		float r = numerator / (1 + c * std::cos(theta + phi));

		float x = r * std::cos(theta);
		float y = r * std::sin(theta);

		points[i].x = x;
		points[i].y = y;
	}
}