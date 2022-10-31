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

enum DilationLevel {
	LEVEL_0 = 1, //real-time, only permit movement under this level
	LEVEL_1 = 10,
	LEVEL_2 = 100,
	LEVEL_3 = 1000,
	LEVEL_4 = 10000
};

extern DilationLevel dilation;

DilationLevel operator++(DilationLevel &level, int);
DilationLevel operator--(DilationLevel &level, int);

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
	void simulate(float time);
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
	void update(float elapsed, Body *root, std::list< Orbit > &orbits);


	Orbit *orbit;
	Scene::Transform *transform;

	glm::vec3 pos;
	glm::vec3 vel;
	glm::vec3 acc;

	static float constexpr DryMass = 4.0f; // Megagram
	static float constexpr MaxThrust = 0.5f; // MegaNewtons
	static float constexpr MaxFuelConsumption = 0.0f; // Measured by mass, Megagram

	float dtheta = 0.0f; //change in theta indicated by user controls(yaw rotation)
	float theta = 0.0f; //rotation along XY plane, radians
	float thrust_percent = 0.0f; //forward thrust, expressed as a percentage of MaxThrust
	float h = 0.0f; //angular momentum
	float fuel = 8.0f; //measured by mass, Megagram
};

//Keplerian orbital mechanics
//See example: https://www.desmos.com/calculator/j0z5ksh8ed
//References:
//Orbital path: https://en.wikipedia.org/wiki/Kepler_orbit
//Orbital velocity: https://en.wikipedia.org/wiki/Vis-viva_equation
struct Orbit {
	Orbit(Body *origin, glm::vec3 pos, glm::vec3 vel, bool simulated);
	Orbit(Body *origin_, float c_, float p_, float phi_, float theta_, bool retrograde);

	float compute_dtheta() {
		//vis-viva equation: https://en.wikipedia.org/wiki/Vis-viva_equation
		dtheta = std::sqrt(mu * (2.0f / r - inv_a)) / r;
		return dtheta;
	}
	float compute_r() {
		//Kepler orbit equation: https://en.wikipedia.org/wiki/Kepler_orbit
		float denom = (1.0f + c * std::cos(theta));
		r = denom != 0.0f ? p / denom : p;
		return r;
	}
	void update(float elapsed);
	glm::vec3 get_pos();
	glm::vec3 get_vel();

	//Simulate and draw the orbit (populate points)
	void predict();
	void init_sim();
	void simulate(float time);
	void sim_predict(Body *root, std::list< Orbit > &orbits, int level);
	void draw(DrawLines &lines, glm::u8vec4 const &color);

	//Constants
	static float constexpr G = 6.67430e-23f; //Standard gravitational constant
	static size_t constexpr PredictDetail = 1440; //number of points to generate when predicting
	static float constexpr PredictAngle = 360.0f / static_cast< float >(PredictDetail); // dtheta between points
	static float constexpr TimeStep = 1.0f; //time step, seconds
	static glm::vec3 constexpr Invalid = glm::vec3(std::numeric_limits< float >::max()); // signifies point outside SOI
	static int constexpr MaxLevel = 1;
	//Fixed values
	Body *origin;

	//Future trajectory, populated by predict()
	std::array< glm::vec3, PredictDetail > points; //Cache of orbit points for drawing
	Orbit *continuation = nullptr; //Continuation in next SOI

	//Values defining orbit
	float c; //eccentricity (unitless)
	float p; //semi-latus rectum, in Megameters
	float a; //semi-major axis, in Megameters
	float phi; //radial component of periapsis, in radians
	float incl; //inclination, in radians

	//Cached for performance
	float mu; //standard gravitation parameter, mu = G * origin->mass;
	float mu_over_h; //=mu/h, h is magnitude of specific orbital angular momentum
	float inv_a; //=1/a
	glm::mat3 rot; //rotation matrix from orbital plane to world

	//Dynamics
	float r; //orbital distance, a.k.a distance from center of origin, in Megameters
	float theta; //true anomaly, a.k.a angle from periapsis, in radians
	float dtheta; //orbital angular velocity, in radians

	glm::vec3 rpos; //only used for degenerate case
	glm::vec3 rvel; //only used for degenerate case

	//Dynamics under simulation
	struct Simulation {
		float r;
		float theta;
		float dtheta;
		glm::vec3 pos;
		glm::vec3 vel;

		glm::vec3 rpos; //only used for degenerate case
		glm::vec3 rvel; //only used for degenerate case
	};

	Simulation sim;
};