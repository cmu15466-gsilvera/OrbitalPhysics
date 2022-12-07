#pragma once

#include "DrawLines.hpp"
#include "Scene.hpp"
#include "Sound.hpp"

#define GLM_PRECISION_HIGHP_FLOAT
#define GLM_PRECISION_HIGHP_DOUBLE
#include <glm/glm.hpp>

#include <array>
#include <iomanip>
#include <limits>
#include <list>
#include <sstream>
#include <string>
#include <vector>
#include <deque>

#include "EmissiveShaderProgram.hpp"

extern double universal_time;

//Forward declarations
struct Body;
struct Orbit;

enum DilationLevel {
	LEVEL_0 = 1, //real-time, only permit movement under this level
	LEVEL_1 = 10,
	LEVEL_2 = 100,
	LEVEL_3 = 1000,
	LEVEL_4 = 10000,
	LEVEL_5 = 100000
};

extern DilationLevel dilation;

DilationLevel operator++(DilationLevel &level, int);
DilationLevel operator--(DilationLevel &level, int);
bool operator>(DilationLevel a, DilationLevel b);
glm::dvec3 DilationColor(const DilationLevel &level);
int DilationInt(const DilationLevel &level);

// Thing in the space
struct Entity {
	Entity(double r, double m) : radius(r), mass(m) {}
	glm::dvec3 pos{0.};
	glm::dvec3 vel{0.};
	glm::dvec3 acc{0.};
	double radius; //collision radius, Megameters (1000 kilometers)
	double mass; //mass used for gravity calculation, Gigagrams (1 million kilograms)
};

//Stars, Planets, Moons, etc.
struct Body : public Entity {
	Body(int _id, double r, double m, double sr) : Entity(r, m), id(_id), soi_radius(sr) {}

    double dayLengthInSeconds = 1.0;

	void set_orbit(Orbit *orbit_);
	void set_transform(Scene::Transform *transform_)  {
		transform = transform_;
		transform->position = pos;
	}
	void add_satellite(Body *body) {
		satellites.emplace_back(body);
	}
	bool check_collision(glm::dvec3 target_pos, double target_radius) {
		return glm::distance(pos, target_pos) <= radius + target_radius;
	}
	bool in_soi(glm::dvec3 target_pos) {
		return glm::distance(pos, target_pos) <= soi_radius;
	}
	void update(double elapsed);
	void init_sim();
	void simulate(double time);
	void draw_orbits(DrawLines &lines, glm::u8vec4 const &color, double scale);

	std::vector< Body * > satellites;
	Orbit *orbit = nullptr;
	Scene::Transform *transform = nullptr;

	//Fixed values
	int id;
	double soi_radius; //sphere of influence radius, Megameters (1000 kilometers)
};

// "Body" in space that can be consumed (good/fuel or bad/debris) 
// does not affect orbital mechanics (negligible)
struct Particle : public Body {
	Particle(double r_ = 0.1f) : Body(-1, 0.2, 0.0, 0.0) {} // using Id = -1 for pellets
	float value = 1.f;
};

struct Beam {
	Beam() = delete;
	Beam(glm::dvec3 &p, glm::dvec3 h) : pos(p), heading(h), start_pos(p) {};
	static constexpr glm::u8vec4 col = glm::u8vec4(0x00, 0xff, 0x00, 0xff); // green lasers
	static constexpr double vel = 299.792; // speed of light in megameters/sec
	static constexpr double MaxStrength = 0.1; // MegaNewtons.
	//NOTE: 100 kN is roughly weight of 2.5 elephants
	glm::dvec3 pos;
	const glm::dvec3 heading; // maybe we can make this change due to gravity of bodies?
	double dt = 0.;
	const glm::dvec3 start_pos;

	glm::dvec3 compute_delta_pos() const;
	bool collide(glm::dvec3 x) const;
	double get_mass(glm::dvec3 x) const;
	static double inverse_sq(glm::dvec3 const &x, glm::dvec3 const &start);
	void draw(DrawLines &DL) const;
};

//Closest approach information
struct ClosestApproachInfo {
	Body *origin = nullptr;
	glm::dvec3 rocket_rpos; //rocket position (relative to body it orbits) at closest approach
	glm::dvec3 asteroid_rpos; //asteroid position (relative to body it orbits) at closest approach
	double time_diff = std::numeric_limits< double >::infinity(); //used for debug
	double dist = std::numeric_limits< double >::infinity();
};

//Asteroid
struct Asteroid : public Entity {
	Asteroid(double r_, double m_) : Entity(r_, m_) {}

	void init(Scene::Transform *transform_, Body *root);
	void update(double elapsed, std::deque< Beam > const &lasers);
	std::string get_time_remaining() {
		if (time_of_collision == std::numeric_limits< double >::infinity()) {
			return "∞";
		}
		std::stringstream stream;
		stream << std::setfill('0') << std::setw(9) << std::max(static_cast< int >(time_of_collision - universal_time), 0);
		return "T-" + stream.str();
	}

	Body *root = nullptr;
	std::list< Orbit > orbits;
	Scene::Transform *transform;

	bool crashed = false;
	double time_of_collision = 42.0; // dummy init value so we don't start on 0 and trigger win
};

//Player
struct Rocket : public Entity {
	Rocket() : Entity(0.2, 0.01) {}

	void init(Scene::Transform *transform_, Body *root, Scene *scene, Asteroid const &asteroid);

	void update(double elapsed, Asteroid const &asteroid);
	void update_lasers(double elapsed);
	void fire_laser();

	glm::dvec3 get_heading() const;

	Body *root;
	std::list< Orbit > orbits;
	Scene::Transform *transform;
	std::shared_ptr< Sound::PlayingSample > engine_loop;

	static double constexpr DryMass = 4.0; // Megagram
	static double constexpr MaxThrust = 0.05; // MegaNewtons
	static double constexpr MaxFuelConsumption = 0.00002; // Measured by mass, Megagram
	static double constexpr LaserCooldown = 1.0e4;

	static int constexpr MAX_BEAMS = 1000; // don't have more than this
	glm::dvec3 aim_dir;
	std::deque<Beam> lasers; // fast insertion/deletion at both ends

	double control_dtheta = 0.0; //change in theta indicated by user controls(yaw rotation)
	double dtheta = 0.0; //change in theta indicated by user controls(yaw rotation)
	double theta = 0.0; //rotation along XY plane, radians
	double thrust_percent = 0.0; //forward thrust, expressed as a percentage of MaxThrust
	double fuel = 8.0; //measured by mass, Megagram
	double maxFuel = 8.0; //measured by mass, Megagram

	double laser_timer = 0.0; //when 0, laser is fireable

	ClosestApproachInfo closest;

	bool crashed = false;

	double timeSinceLastParticle = 0.0;
	int lastParticle = 0;

	struct ThrustParticle {
		double lifeTime;
		glm::dvec3 velocity;
		double scale;
		double _t;
		glm::vec4 color;
		std::list<Scene::Transform>::iterator transform;
		ThrustParticle(std::list<Scene::Transform>::iterator trans_, double lifeTime_, glm::dvec3 v_, double scale) : lifeTime(lifeTime_), velocity(v_), scale(scale), transform(trans_) {
			_t = 0;
		}
	};
	std::vector<ThrustParticle> thrustParticles;
};

//Keplerian orbital mechanics
//See example: https://www.desmos.com/calculator/j0z5ksh8ed
//References:
//Orbital path: https://en.wikipedia.org/wiki/Kepler_orbit
//Orbital velocity: https://en.wikipedia.org/wiki/Vis-viva_equation
struct Orbit {
	Orbit(Body *origin, glm::dvec3 pos, glm::dvec3 vel, bool simulated, bool verbose = false);
	Orbit(Body *origin_, double c_, double p_, double phi_, double theta_, bool retrograde, bool verbose = false);

	void init_dynamics() {
		assert(p > MinPForDegen);
		compute_r();
		compute_dtheta();
		rpos = get_rpos(theta, r);
		rvel = get_rvel(theta);
	}

	// Computing dynamics
	double compute_dtheta(double r_) {
		//vis-viva equation: https://en.wikipedia.org/wiki/Vis-viva_equation
		return std::sqrt(mu * (2.0 / r_ - inv_a)) / r_;
	}
	double compute_dtheta() {
		return dtheta = compute_dtheta(r);
	}
	double compute_r(double theta_) {
		//Kepler orbit equation: https://en.wikipedia.org/wiki/Kepler_orbit
		double denom = (1.0 + c * std::cos(theta_));
		return denom != 0.0 ? p / denom : p;
	}
	double compute_r() {
		return r = compute_r(theta);
	}
	void update(double elapsed);
	glm::dvec3 get_rpos(double theta_, double r_);
	glm::dvec3 get_rvel(double theta_);

	//Convenience functions
	glm::dvec3 get_pos() {
		return rpos + origin->pos;
	}
	glm::dvec3 get_vel() {
		return rvel + origin->vel;
	}

	//Simulate and draw the orbit (populate points)
	void predict();
	void init_sim();
	void simulate(double time);
	void sim_predict(
		Body *root, std::list< Orbit > &orbits, int level, std::list< Orbit >::iterator it, double start_time);
	bool will_soi_transit(double elapsed)  {
		return theta + 32.0f * dtheta * elapsed * static_cast< double >(dilation) >= soi_transit;
	}
	void find_closest_approach(Orbit const &other, size_t points_idx, size_t other_points_idx,
		ClosestApproachInfo &closest);
	double find_time_of_collision();
	void draw(DrawLines &lines, glm::u8vec4 const &color) const;

	//Constants
	static double constexpr G = 6.67430e-23; //Standard gravitational constant
	static double constexpr MinPForDegen = 1.0e-4;
	static size_t constexpr UpdateSteps = 100;
	static size_t constexpr PredictDetail = 900; //number of points to generate when predicting
	static double constexpr PredictAngle = glm::radians(360.0 / static_cast< double >(PredictDetail)); //change btwn pts
	static double constexpr TimeStep = 1.0; //time step, seconds
	static glm::dvec3 constexpr Invalid = glm::dvec3(std::numeric_limits< double >::max()); // signifies point outside SOI
	static int constexpr MaxLevel = 2;
	//Fixed values
	Body *origin;

	//Future trajectory, populated by predict()
	std::array< glm::dvec3, PredictDetail > points; //Cache of orbit points for drawing
	std::array< double, PredictDetail > point_times; //Used only for Rocket/Asteroid closest approach calc
	double soi_transit = std::numeric_limits< double >::infinity(); //theta value for SOI transit
	Orbit *continuation = nullptr; //Continuation in next SOI

	//Values defining orbit
	double c; //eccentricity (unitless)
	double p; //semi-latus rectum, in Megameters
	double a; //semi-major axis, in Megameters
	double phi; //radial component of periapsis, in radians
	double incl; //inclination, in radians

	//Cached for performance
	double mu; //standard gravitation parameter, mu = G * origin->mass;
	double mu_over_h; //=mu/h, h is magnitude of specific orbital angular momentum
	double inv_a; //=1/a
	glm::dmat3 rot; //rotation matrix from orbital plane to world

	//Dynamics
	double r; //orbital distance, a.k.a distance from center of origin, in Megameters
	double theta; //true anomaly, a.k.a angle from periapsis, in radians
	double dtheta; //orbital angular velocity, in radians

	glm::dvec3 rpos; //relative position (from origin)
	glm::dvec3 rvel; //relative velocity (from origin)

	//Dynamics under simulation
	struct Simulation {
		double r;
		double theta;
		double dtheta;
		glm::dvec3 pos;
		glm::dvec3 vel;

		glm::dvec3 rpos;
		glm::dvec3 rvel;
	};

	Simulation sim;
};
