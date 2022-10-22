#pragma once

#include <array>
#include <string>
#include <vector>

#include <glm/glm.hpp>

//Forward declarations
struct Body;
struct Orbit;

//Stars, Planets, Moons, Asteroids, etc.
struct Body {
	Body(std::string const &name_, float r, float m, float sr) : name(name_), radius(r), mass(m), soi_radius(sr) {}

	void set_orbit(Orbit *orbit_);
	bool check_collision(glm::vec3 target_pos, float target_radius);

	Orbit *orbit;

	//Identifier
	std::string name;

	//Fixed values
	float radius; //collision radius, Megameters (1000 kilometers)
	float mass; //mass used for gravity calculation, Megagrams (1000 kilograms)
	float soi_radius; //sphere of influence radius, Megameters (1000 kilometers)
};

// Player
// struct Rocket {
// 	Rocket();

// 	glm::vec3 pos;
// 	glm::vec3 vec;
// 	glm::vec3 acc;

// 	float fuel;
// };

//Keplerian orbital mechanics
//See example: https://www.desmos.com/calculator/axhi5heeps
struct Orbit {
	//TODO: calculate c, a, phi using orbital position, orbital velocity, and origin body
	Orbit(Body *origin, glm::vec3 pos, glm::vec3 vel);

	//Test constructor
	Orbit(Body *origin_, float c_, float a_, float phi_) : origin(origin_), c(c_), a(a_), phi(phi_) {}

	//Given position and time, get updated position
	glm::vec3 get_pos(glm::vec3 pos, float elapsed);

	//Simulate the oribit (populate points)
	void predict();


	//Fixed values
	Body *origin;

	//Cache of orbit points for drawing
	std::array< glm::vec3, 360 > points;

	//Values defining orbit
	float c; //eccentricity
	float a; //semi-major axis
	float phi; //radial component of perapsis
};