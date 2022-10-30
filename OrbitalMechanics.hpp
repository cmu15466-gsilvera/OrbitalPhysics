#pragma once

#include "Scene.hpp"
#include "DrawLines.hpp"

#include <glm/glm.hpp>

#include <array>
#include <limits>
#include <list>
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
	void add_satellite(Body *body) {
		satellites.emplace_back(body);
	}
	bool check_collision(glm::vec3 target_pos, float target_radius) {
		return glm::distance(pos, target_pos) <= radius + target_radius;
	}
	bool in_soi(glm::vec3 target_pos) {
		return glm::distance(pos, target_pos) <= soi_radius;
	}
	void update(float elapsed);
	void draw_orbits(DrawLines &lines, glm::u8vec4 const &color);

	std::vector< Body * > satellites;
	Orbit *orbit = nullptr;
	Scene::Transform *transform = nullptr;

	//Fixed values
	float radius; //collision radius, Megameters (1000 kilometers)
	float mass; //mass used for gravity calculation, Gigagrams (1 million kilograms)
	float soi_radius; //sphere of influence radius, Megameters (1000 kilometers)

	//Variable values
	glm::vec3 pos = glm::vec3(0.0f);
	glm::vec3 vel = glm::vec3(0.0f);
};

// Player
struct Rocket {
	Rocket() {}

	void init(Orbit *orbit_, Scene::Transform *transform_);
	void update(float dthrust, float dtheta, float elapsed, std::list< Orbit > &orbits);
	void recalculate_orbits();

	Orbit *orbit;
	Scene::Transform *transform;

	glm::vec3 pos;
	glm::vec3 vel;
	glm::vec3 acc;

	// rotational euler mechanics
	glm::vec3 rot{0.f, 0.f, 0.f};
	glm::vec3 rotvel{0.f, 0.f, 0.f};
	glm::vec3 rotacc{0.f, 0.f, 0.f};

	static float constexpr DryMass = 4.0f; // Megagram

	bool stability_dampening = true; //controls SAS, dampens angular momentum
	float theta_thrust = 0.0f; // acceleration for theta (yaw rotation)
	float theta = 0.0f; //rotation along XY plane
	float thrust = 0.0f; //forward thrust
	float h = 0.0f; //angular momentum
	float fuel = 8.0f; //measured by mass, Megagram
};

//Keplerian orbital mechanics
//See example: https://www.desmos.com/calculator/j0z5ksh8ed
//References:
//Orbital path: https://en.wikipedia.org/wiki/Kepler_orbit
//Orbital velocity: https://en.wikipedia.org/wiki/Vis-viva_equation
struct Orbit {
	Orbit(Body *origin, glm::vec3 pos, glm::vec3 vel);
	Orbit(Body *origin_, float c_, float p_, float phi_, float theta_);

	float compute_dtheta() {
		//vis-viva equation: https://en.wikipedia.org/wiki/Vis-viva_equation
		dtheta = std::sqrt((G * origin->mass) * (2.0f / r - 1.0f / a)) / r;
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
	void draw(DrawLines &lines, glm::u8vec4 const &color);

	//Constants
	static float constexpr G = 6.67430e-23f; //Standard gravitational constant
	static size_t constexpr PredictDetail = 720; //number of points to generate when predicting
	static float constexpr PredictAngle = 360.0f / static_cast< float >(PredictDetail); // dtheta between points
	static float constexpr TimeStep = 1.0f; //time step, seconds
	static glm::vec3 constexpr Invalid = glm::vec3(std::numeric_limits< float >::max()); // signifies point outside SOI

	//Fixed values
	Body *origin;

	//Future trajectory, populated by predict()
	std::array< glm::vec3, PredictDetail > points; //Cache of orbit points for drawing
	Orbit *continuation; //Continuation in next SOI

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