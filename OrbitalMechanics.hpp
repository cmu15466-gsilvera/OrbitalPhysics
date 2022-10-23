#pragma once

#include "Scene.hpp"
#include "DrawLines.hpp"

#include <glm/glm.hpp>

#include <array>
#include <limits>
#include <string>
#include <vector>


//Forward declarations
struct Body;
struct Orbit;

//Stars, Planets, Moons, Asteroids, etc.
struct Body {
	Body(float r, float m, float sr) : radius(r), mass(m), soi_radius(sr) {}

	void set_orbit(Orbit *orbit_);
	void set_transform(Scene::Transform *transform_)  {
		transform = transform_;
		transform->position = pos;
	}
	bool check_collision(glm::vec3 target_pos, float target_radius) {
		return glm::distance(pos, target_pos) <= radius + target_radius;
	}
	bool in_soi(glm::vec3 target_pos) {
		return glm::distance(pos, target_pos) <= soi_radius;
	}
	void update(float elapsed);

	Orbit *orbit = nullptr;
	Scene::Transform *transform = nullptr;

	//Fixed values
	float radius; //collision radius, Megameters (1000 kilometers)
	float mass; //mass used for gravity calculation, Gigagrams (1 million kilograms)
	float soi_radius; //sphere of influence radius, Megameters (1000 kilometers)
	//Note that the above unit choices are made so that G is still 6.6e-11 like in Nm^2/kg^2

	//Variable values
	glm::vec3 pos = glm::vec3(0.0f);
	glm::vec3 vel = glm::vec3(0.0f);
};

// Player
// struct Rocket {
// 	Rocket(Orbit *orbit_, Scene::Transform *transform_);

// 	void update(float dtheta, float elapsed);
// 	void update_orbit();

// 	Orbit *orbit = nullptr;
// 	Scene::Transform *transform = nullptr;

// 	glm::vec3 pos;
// 	glm::vec3 vec;
// 	glm::vec3 acc;

// 	static float constexpr DryMass = 4.0f; // Megagram

// 	bool stability_dampening = true; //controls SAS, dampens angular momentum
// 	float theta = 0.0f; //rotation along XY plane
// 	float h = 0.0f; //angular momentum
// 	float fuel = 8.0f; //measured by mass, Megagram
// };

//Keplerian orbital mechanics
//See example: https://www.desmos.com/calculator/axhi5heeps
//References:
//Orbital path: https://en.wikipedia.org/wiki/Kepler_orbit
//Orbital velocity: https://en.wikipedia.org/wiki/Vis-viva_equation
struct Orbit {
	Orbit(Body *origin, glm::vec3 pos, glm::vec3 vel);
	Orbit(Body *origin_, float c_, float p_, float phi_, float theta_);

	float compute_dtheta() {
		//vis-viva equation: https://en.wikipedia.org/wiki/Vis-viva_equation
		dtheta = (G * origin->mass / r) * (2.0f / r - 1.0f / a);
		return dtheta;
	}
	float compute_r() {
		//Kepler orbit equation: https://en.wikipedia.org/wiki/Kepler_orbit
		r = p / (1.0f + c * std::cos(theta));
		return r;
	}
	glm::vec3 update(float elapsed);
	glm::vec3 get_pos();
	glm::vec3 get_vel();

	//Simulate and draw the oribit (populate points)
	void predict();
	void draw(DrawLines &lines, glm::u8vec4 color);

	//Constants
	static float constexpr G = 6.67430e-11f; // Standard gravitational constant
	static int constexpr PredictDetail = 360; // number of points to generate when predicting
	static float constexpr PredictAngle = 360.0f / static_cast< float >(PredictDetail); // dtheta between points
	static float constexpr TimeStep = 0.001f; // time step, seconds
	static glm::vec3 constexpr Invalid = glm::vec3(std::numeric_limits< float >::max()); // signifies point outside SOI

	//Fixed values
	Body *origin;

	//Cache of orbit points for drawing
	std::array< glm::vec3, PredictDetail > points;

	//Values defining orbit
	float c; //eccentricity
	float p; //semi-latus rectum
	float phi; //radial component of periapsis

	//Useful parameters
	float a; //semi-major axis

	//Dynamics
	float r; //orbital distance, a.k.a distance from center of origin
	float theta; //true anomaly, a.k.a angle from periapsis
	float dtheta; //orbital angular velocity
};