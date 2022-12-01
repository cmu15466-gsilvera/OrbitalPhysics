#include "OrbitalMechanics.hpp"
#include "Load.hpp"
#include "Scene.hpp"
#include "data_path.hpp"
#include "Utils.hpp"
#include "glm/gtc/type_ptr.hpp"

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
#ifndef M_PI // but other OS's do
#define M_PI 3.141529f
#endif

#define PARTICLE_COUNT 100

Load< Scene::RenderSet > particles(LoadTagDefault, []() -> Scene::RenderSet const * {
	Scene::RenderSet *renderSet = new Scene::RenderSet();
	MeshBuffer const *ret = new MeshBuffer(data_path("assets/model/particles.pnct"));
	renderSet->vao = ret->make_vao_for_program(emissive_program->program);
	renderSet->meshes = ret;
	renderSet->pipeline = emissive_program_pipeline;

	return renderSet;
});

Load< Sound::Sample > laser_sfx(LoadTagDefault, []() -> Sound::Sample const * {
	return new Sound::Sample(data_path("assets/sound/laser.wav"));
});
Load< Sound::Sample > engine_sfx(LoadTagDefault, []() -> Sound::Sample const * {
	return new Sound::Sample(data_path("assets/sound/engine.wav"));
});


//universal time
static float universal_time = 0.0f;


//Time acceleration
DilationLevel dilation = LEVEL_0;
static DilationLevel constexpr MAX_SOI_TRANS_DILATION = LEVEL_3;

DilationLevel operator++(DilationLevel &level, int) {
	switch (level) {
	case LEVEL_0:
		return level = LEVEL_1;
	case LEVEL_1:
		return level = LEVEL_2;
	case LEVEL_2:
		return level = LEVEL_3;
	case LEVEL_3:
		return level = LEVEL_4;
	case LEVEL_4:
	case LEVEL_5:
	default:
		return level = LEVEL_5;
	}
}

DilationLevel operator--(DilationLevel &level, int) {
	switch (level) {
	case LEVEL_5:
		return level = LEVEL_4;
	case LEVEL_4:
		return level = LEVEL_3;
	case LEVEL_3:
		return level = LEVEL_2;
	case LEVEL_2:
		return level = LEVEL_1;
	case LEVEL_1:
	case LEVEL_0:
	default:
		return level = LEVEL_0;
	}
}

bool operator>(DilationLevel a, DilationLevel b) {
	return static_cast< int >(a) > static_cast< int >(b);
}

glm::vec3 DilationColor(const DilationLevel &level) {
	switch (level) {
	case LEVEL_0:
		return glm::vec3(1.0f); // white
	case LEVEL_1:
		return glm::vec3(1.0f, 1.0f, 0.0f); // yellow
	case LEVEL_2:
		return glm::vec3(1.0f, 0.64f, 0.0f); // orange
	case LEVEL_3:
	case LEVEL_4:
	case LEVEL_5:
	default:
		return glm::vec3(1.0f, 0.0f, 0.0f); // red
	}
}

std::string DilationSchematic(const DilationLevel &level) {
	// used as a visual indicator in the UI
	switch (level) {
	case LEVEL_0:
		return "[|.....]";
	case LEVEL_1:
		return "[||....]";
	case LEVEL_2:
		return "[|||...]";
	case LEVEL_3:
		return "[||||..]";
	case LEVEL_4:
		return "[|||||.]";
	case LEVEL_5:
	default:
		return "[||||||]";
	}
}

void Body::set_orbit(Orbit *orbit_) {
	orbit = orbit_;
	if (orbit == nullptr) return;
	pos = orbit->get_pos();
	vel = orbit->get_vel();
	orbit->predict();
}

void Body::update(float elapsed) {
	transform->rotation = transform->rotation * glm::quat(glm::vec3(0.f, 0.f, (2 * M_PI) * dilation * (elapsed / dayLengthInSeconds)));

	if (orbit != nullptr) {
		orbit->update(elapsed);
		pos = orbit->get_pos();
		vel = orbit->get_vel();
	}

	assert(transform != nullptr);
	transform->position = pos;

	//Recursively update all satellites, their satellites, and so on
	for (Body *body : satellites) {
		assert(body != nullptr);
		body->update(elapsed);
	}
}

void Body::init_sim() {
	if (orbit != nullptr) orbit->init_sim();
	for (Body *satellite : satellites) {
		satellite->init_sim();
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

void Body::draw_orbits(DrawLines &lines, glm::u8vec4 const &color, float scale) {
	if (orbit != nullptr && scale >= orbit->p) orbit->draw(lines, color);

	for (Body *body : satellites) {
		assert(body != nullptr);
		body->draw_orbits(lines, color, scale);
	}
}

void Beam::draw(DrawLines &DL) const {
	// drawing one timestep "ago" since the time dilation makes it really fast
	// ==> so that we always see the start of the beam at the rocket
	// "mass" equates to beam strength which dissipates over time (opacity)
	const glm::vec3 start = pos - compute_delta_pos();
	const glm::vec3 &end = pos;

	/// NOTE: we are drawing many lines per dilation to account for opacity shift
	const glm::vec3 delta = (end - start) / static_cast<float>(dilation);
	for (int i = 0; i < dilation; i++) {
		glm::vec3 istart = start + static_cast<float>(i) * delta;
		float mass = get_mass(istart + delta);
		if (mass < 1.0e-2f) break;
		DL.draw(istart, istart + delta, glm::u8vec4{col.x, col.y, col.z, mass * col.w});
	}
}

glm::vec3 Beam::compute_delta_pos() const {
	// used to compute the delta to where this beam will be in 1 timestep (dt)
	return vel * heading * dt * static_cast< float >(dilation);
}

bool Beam::collide(glm::vec3 x) const {
	glm::vec3 prev_pos = pos - compute_delta_pos();
	return glm::l2Norm(pos - x) + glm::l2Norm(prev_pos - x) - glm::l2Norm(pos - prev_pos) < 0.1f;
}

float Beam::get_mass(glm::vec3 x) const {
	float denom = 1.0f + glm::l2Norm(x - start_pos) * 0.05f;
	return 1.0f / (denom * denom);
}

void Rocket::init(Scene::Transform *transform_, Body *root_, Scene *scene, Asteroid const &asteroid) {
	assert(transform_ != nullptr);

	root = root_;
	Orbit &orbit = orbits.front();
	pos = orbit.get_pos();
	vel = orbit.get_vel();
	acc = glm::vec3(0.0f);
	orbit.sim_predict(root, orbits, 0, orbits.begin(), universal_time);
	orbit.find_closest_approach(asteroid.orbits.front(), 0, 0, closest);

	transform = transform_;
	transform->position = pos;
	transform->scale = glm::vec3(radius);

	thrustParticles.reserve(PARTICLE_COUNT);
	for(int i = 0; i < PARTICLE_COUNT; i++){
		scene->transforms.emplace_back();
		auto it = scene->transforms.end();
		it--;
		it->name = "Particle";
		auto drawable = Scene::make_drawable(*scene, &(*it), particles);
		thrustParticles.push_back(ThrustParticle(it, 5.0f, glm::vec3(0), 0.04f));
		ThrustParticle *currentParticle = &thrustParticles[thrustParticles.size() - 1];
		drawable->set_uniforms = [currentParticle]() {
			glUniform4fv(emissive_program->COLOR_vec4, 1, glm::value_ptr(currentParticle->color));
		};
		it->enabled = false;
	}

	engine_loop = Sound::loop(*engine_sfx, thrust_percent / 100.0f, 0.0f);
}

glm::vec3 Rocket::get_heading() const {
	return {std::cos(theta), std::sin(theta), 0.0f};
}

void Rocket::fire_laser() {
	if (laser_timer > 0.0f) return;

	lasers.emplace_back(Beam(pos, aim_dir));
	Sound::play(*laser_sfx, 0.5f, 0.0f);

	laser_timer = LaserCooldown;
}

void Rocket::update_lasers(float elapsed) {
	for (Beam &b : lasers) {
		b.dt = elapsed;
		b.pos += b.compute_delta_pos();
	}

	// delete beams once we have too many
	int num_to_remove = std::max(0, static_cast<int>(lasers.size()) - MAX_BEAMS);
	for (int i = num_to_remove; i > 0; i--) {
		lasers.pop_front();
	}
}

void Rocket::update(float elapsed, Asteroid const &asteroid) {
	bool moved = false;

	engine_loop->set_volume(thrust_percent / 100.0f);
	if (laser_timer > 0.0f) {
		laser_timer = std::max(laser_timer - elapsed * dilation, 0.0f);
	}

	{ //update thrust particles
		if (thrust_percent > 0){
			float rate = glm::mix(0.0f, 250.0f, std::min((thrust_percent / 10.0f), 1.0f));
			while (timeSinceLastParticle > (1.0f / rate)) {
				auto particle = &thrustParticles[lastParticle];
				auto trans = thrustParticles[lastParticle].transform;
				trans->position = transform->make_local_to_world() * glm::vec4(
					-3.5f, Utils::RandBetween(-0.5f, 0.5f), Utils::RandBetween(-0.5f, 0.5f), 1
				);
				trans->scale = glm::vec3(0.1f,0.1f,0.1f);
				particle->_t = 0;
				timeSinceLastParticle -= (1.0f / rate);
				glm::vec3 velocity = transform->make_local_to_world() * glm::vec4(
					Utils::RandBetween(-10.5f, -5.0f), 0, 0, 0.0f
				);
				particle->velocity = velocity;
				particle->color = glm::vec4(1, 0, 0, 1);
				particle->lifeTime = Utils::RandBetween(0.34f, 0.36f);
				trans->enabled = true;
				lastParticle = (1 + lastParticle) % PARTICLE_COUNT;
			}
			timeSinceLastParticle += elapsed;
		}

		for (auto &particle : thrustParticles) {
			if (!particle.transform->enabled) continue;
			particle._t += elapsed;
			if (particle._t >= particle.lifeTime) {
				particle.transform->enabled = false;
			}
			particle.transform->position += particle.velocity * elapsed;
			float a = (particle.lifeTime - particle._t) / particle.lifeTime;
			particle.transform->scale = glm::vec3(particle.scale * a);
			particle.color = glm::vec4(1, (1 - a), 0, 1);
		}
	}

	{ //rocket controls & physics
		//Going to assume stability assist via reaction wheels is always on and the controller is perfect to simplify
		// things. We can make the game harder later on by changing this to RCS based if need.
		dtheta = control_dtheta;
		theta += elapsed * dtheta; // yaw (rotation alonx XY plane)
		if (theta > M_PI)
			theta -= 2 * M_PI;
		if (theta < -M_PI)
			theta += 2 * M_PI;
		transform->rotation = glm::quat(glm::vec3(0.f, 0.f, theta));

		if (thrust_percent != 0.0f && fuel > 0.0f) {
			moved = true; //set flag to trigger orbit recalc later on

			//calculate acceleration: F = ma ==> acc = thrust * MaxThrust / (DryMass + fuel);
			//Note conversion from MegaNewtons/Megagram == meter/s^2 to Megameter/s^2
			float acc_magnitude = thrust_percent * MaxThrust / ((DryMass + fuel) * 1000.0f);
			acc = glm::vec3(
				acc_magnitude * std::cos(theta),
				acc_magnitude * std::sin(theta),
				0.0f
			);

			//update fuel consumption
			fuel = std::max(fuel - std::fabs(thrust_percent) * MaxFuelConsumption, 0.0f);

			//update velocity
			vel += acc * elapsed;
		} else {
			acc = glm::vec3(0.0f);
		}
	}

	{ //orbital mechanics
		Orbit &orbit = orbits.front();
		if (moved) {
			//recalculate orbit due to thrust
			Orbit *temp = orbit.continuation;
			orbit = Orbit(orbit.origin, pos, vel, false);
			orbit.continuation = temp;
			orbit.sim_predict(root, orbits, 0, orbits.begin(), universal_time);
			closest.dist = std::numeric_limits< float >::infinity();
			orbit.find_closest_approach(asteroid.orbits.front(), 0, 0, closest);
		}

		while (orbit.will_soi_transit(elapsed) && dilation > MAX_SOI_TRANS_DILATION) {
			dilation--;
		}

		orbit.update(elapsed);
		assert(orbit.r > orbit.origin->radius);
		pos = orbit.get_pos();
		vel = orbit.get_vel();

		Body *origin = orbit.origin;

		if (!origin->in_soi(pos)) {
			assert(origin->orbit != nullptr);
			Body *new_origin = origin->orbit->origin;

			orbit = Orbit(new_origin, pos, vel, false);
			orbit.sim_predict(root, orbits, 0, orbits.begin(), universal_time);
			closest.dist = std::numeric_limits< float >::infinity();
			orbit.find_closest_approach(asteroid.orbits.front(), 0, 0, closest);
		}

		for (Body *satellite : origin->satellites) {
			if (satellite->in_soi(pos)) {
				orbit = Orbit(satellite, pos, vel, false);
				orbit.sim_predict(root, orbits, 0, orbits.begin(), universal_time);
				closest.dist = std::numeric_limits< float >::infinity();
				orbit.find_closest_approach(asteroid.orbits.front(), 0, 0, closest);
				break;
			}
		}

		assert(transform != nullptr);
		transform->position = pos;
	}

	universal_time += elapsed * dilation;
}


void Asteroid::init(Scene::Transform *transform_, Body *root_) {
	assert(transform_ != nullptr);

	root = root_;
	Orbit &orbit = orbits.front();
	pos = orbit.get_pos();
	vel = orbit.get_vel();
	acc = glm::vec3(0.0f);
	orbit.sim_predict(root, orbits, 0, orbits.begin(), universal_time);

	transform = transform_;
	transform->position = pos;
	transform->scale = glm::vec3(radius);
}

void Asteroid::update(float elapsed, std::deque< Beam > const &lasers) {
	bool moved = false;
	{ //TODO: this is a placeholder for when rocket and asteroid interact

		//simplification: only consider first laser in contact with asteroid
		const Beam *beam = nullptr;
		for (auto &laser : lasers) {
			if (laser.collide(pos)) {
				beam = &laser;
				break;
			}
		}
		if (beam != nullptr) {
			float dvel_magnitude =
				beam->get_mass(pos) * Beam::MaxStrength / (mass * 1000.0f);

			// LOG("|dvel|: " << dvel_magnitude);
			// LOG("|accum|: " << dvel_mag_accum);
			// LOG("|vel|: " << glm::l2Norm(vel));
			if (dvel_magnitude > 5.0e-3f * glm::l2Norm(vel)) {
				moved = true;

				//update velocity
				vel += dvel_magnitude * beam->heading;

				//If we want to simulate the asteroid being burned away (since lasers move things by ablation)
				// we can slightly reduce the mass here. Effect should be negligible though.
			}
		}
	}

	{ //orbital mechanics
		Orbit &orbit = orbits.front();
		if (moved) {
			//recalculate orbit due to thrust
			Orbit *temp = orbit.continuation;
			orbit = Orbit(orbit.origin, pos, vel, false);
			orbit.continuation = temp;
			orbit.sim_predict(root, orbits, 0, orbits.begin(), universal_time);
		}

		while (orbit.will_soi_transit(elapsed) && dilation > MAX_SOI_TRANS_DILATION) {
			dilation--;
		}

		orbit.update(elapsed);
		assert(orbit.r > orbit.origin->radius);
		pos = orbit.get_pos();
		vel = orbit.get_vel();

		Body *origin = orbit.origin;

		if (!origin->in_soi(pos)) {
			assert(origin->orbit != nullptr);
			Body *new_origin = origin->orbit->origin;

			orbit = Orbit(new_origin, pos, vel, false);
			orbit.sim_predict(root, orbits, 0, orbits.begin(), universal_time);
		}

		for (Body *satellite : origin->satellites) {
			if (satellite->in_soi(pos)) {
				orbit = Orbit(satellite, pos, vel, false);
				orbit.sim_predict(root, orbits, 0, orbits.begin(), universal_time);
				break;
			}
		}

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
	float d_norm = glm::l2Norm(d);
	if (p / d_norm > MinPForDegen) {
		init_dynamics();
	} else {
		//degenerate orbit (freefall), do not use Kepler's equations
		r = d_norm;
		dtheta = 0.0f;
		p = 0.0f;
		rpos = d;
		rvel = v;
	}

	// LOG("\tr: " << r << " theta: " << theta << " dtheta: " << dtheta);

	// LOG("\tnew pos: " << glm::to_string(get_pos()));
	// LOG("\tnew vel: " << glm::to_string(get_vel()));
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

	init_dynamics();
}

glm::vec3 Orbit::get_rpos(float theta_, float r_) {
	return rot * glm::vec3(
		r_ * std::cos(theta_),
		r_ * std::sin(theta_),
		0.0f
	);
}

glm::vec3 Orbit::get_rvel(float theta_) {
	return rot * glm::vec3(
		-mu_over_h * std::sin(theta_),
		mu_over_h * (c + std::cos(theta_)),
		0.0f
	);
}

void Orbit::update(float elapsed) {
	const float time_step = elapsed * static_cast< float >(dilation) / static_cast< float >(UpdateSteps);
	if (p == 0.0f) { //degenerate case
		glm::vec3 drvel;
		for (size_t i = 0; i < UpdateSteps; i++) {
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
	} else { //standard case
		for (size_t i = 0; i < UpdateSteps; i++) {
			theta += dtheta * time_step;
			compute_r();
			compute_dtheta();
		}

		rpos = get_rpos(theta, r);
		rvel = get_rvel(theta);
	}
}

void Orbit::predict() {
	//Use this only for bodies, not for player
	assert(c < 1.0f);

	for (size_t i = 0; i < PredictDetail; i++) {
		float theta_ = static_cast< float >(i) * PredictAngle;
		float r_ = compute_r(theta_);

		points[i] = get_rpos(theta_, r_);
	}
}

void Orbit::init_sim() {
	sim.r = r;
	sim.theta = theta;
	sim.dtheta = dtheta;
	sim.pos = get_pos();
	sim.vel = get_vel();

	sim.rpos = rpos;
	sim.rvel = rvel;
}

void Orbit::sim_predict(
		Body *root, std::list< Orbit > &orbits, int level, std::list< Orbit >::iterator it, float start_time) {
	root->init_sim();
	init_sim();

	float current_time = start_time;
	points[0] = sim.rpos;
	point_times[0] =  current_time;
	++it;

	if (p == 0.0f) { //degenerate case
		points[1] = glm::vec3(0.0f);
		points[2] = Invalid;
		return;
	}

	float aligned = std::ceil(sim.theta / PredictAngle) * PredictAngle;
	float step = (aligned - sim.theta) / sim.dtheta;
	for (size_t i = 1; i < PredictDetail; i++) {
		root->simulate(step);
		simulate(step);
		current_time += step;
		point_times[i] = current_time;
		step = PredictAngle / sim.dtheta;

		if (sim.r > origin->soi_radius) {
			points[i] = Invalid;
			soi_transit = sim.theta;

			if (level >= MaxLevel) return;

			// SOI transfer to origin of origin
			if (it == orbits.end()) {
				assert(origin->orbit != nullptr);
				orbits.emplace_back(Orbit(origin->orbit->origin, sim.pos, sim.vel, true));
				it = --orbits.end();
				continuation = &orbits.back();
			} else {
				continuation = &(*it);
				Orbit *temp = continuation->continuation;
				*continuation = Orbit(origin->orbit->origin, sim.pos, sim.vel, true);
				continuation->continuation = temp;
			}
			continuation->sim_predict(root, orbits, level+1, it, current_time);
			return;
		}

		for (Body *satellite : origin->satellites) {
			assert(satellite != nullptr && satellite->orbit != nullptr);
			if (glm::distance(sim.pos, satellite->orbit->sim.pos) < satellite->soi_radius) {
				points[i] = Invalid;
				soi_transit = sim.theta;

				if (level >= MaxLevel) return;

				// SOI transfer to satellite of origin
				if (it == orbits.end()) {
					orbits.emplace_back(Orbit(satellite, sim.pos, sim.vel, true));
					it = --orbits.end();
					continuation = &orbits.back();
				} else {
					continuation = &(*it);
					Orbit *temp = continuation->continuation;
					*continuation = Orbit(satellite, sim.pos, sim.vel, true);
					continuation->continuation = temp;
				}
				continuation->sim_predict(root, orbits, level+1, it, current_time);
				return;
			}
		}

		points[i] = sim.rpos;
	}

	if (continuation != nullptr) {
		soi_transit = std::numeric_limits< float >::infinity();
		continuation = nullptr;
	}
}

void Orbit::simulate(float time) {
	const float time_step = time / static_cast< float >(UpdateSteps);
	if (p == 0.0f) { //degenerate case
		glm::vec3 drvel;
		for (size_t i = 0; i < UpdateSteps; i++) {
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
	} else { //standard case
		for (size_t i = 0; i < UpdateSteps; i++) {
			sim.theta += sim.dtheta * time_step;
			sim.r = compute_r(sim.theta);
			sim.dtheta = compute_dtheta(sim.r);
		}
		sim.rpos = get_rpos(sim.theta, sim.r);
		sim.rvel = get_rvel(sim.theta);
	}

	sim.pos = origin->orbit != nullptr ? sim.rpos + origin->orbit->sim.pos : sim.rpos;
	sim.vel = origin->orbit != nullptr ? sim.rvel + origin->orbit->sim.vel : sim.rvel;
}

void Orbit::find_closest_approach(
		Orbit const &other, size_t points_idx, size_t other_points_idx, ClosestApproachInfo &closest) {
	// LOG("start:" << origin->transform->name << " " << other.origin->transform->name << " " << closest.dist);

	//NOTE: only call for rocket
	size_t ni = points.size();
	size_t nj = other.points.size();

	size_t i = points_idx;
	size_t j = other_points_idx;
	while (i < ni && j < nj) {
		glm::vec3 pos_i = points[i];
		glm::vec3 pos_j = other.points[j];

		if (pos_i == Orbit::Invalid || pos_j == Orbit::Invalid) {
			break;
		}

		if (std::abs(point_times[i] - other.point_times[j]) < 1e8f && origin == other.origin) {
			//find distance and update closest if needed
			float dist = glm::distance(pos_i, pos_j);
			if (dist < closest.dist) {
				closest.origin = origin;
				closest.rocket_rpos = pos_i;
				closest.asteroid_rpos = pos_j;
				closest.dist = dist;
				closest.time_diff = point_times[i] - other.point_times[j];
			}
		}

		//step whichever orbit is further behind
		if (point_times[i] < other.point_times[j]) {
			i++;
		} else {
			j++;
		}
	}

	// LOG("end: " << origin->transform->name << " " << other.origin->transform->name << " " << closest.dist);

	if (i < ni && j < nj && continuation != nullptr && other.continuation != nullptr) {
		continuation->find_closest_approach(*other.continuation, 0, 0, closest);
	} else if (i < ni && continuation != nullptr) {
		continuation->find_closest_approach(other, 0, j, closest);
	} else if (j < nj && other.continuation != nullptr) {
		find_closest_approach(*other.continuation, i, 0, closest);
	}
}

void Orbit::draw(DrawLines &lines, glm::u8vec4 const &color) {
	size_t n = points.size();

	glm::vec3 const &origin_pos = origin->pos;

	// LOG(n << " points");
	for (size_t i = 0; i < n; i++) {
		// LOG("drawing " << glm::to_string(points[i-1])  << " to " << glm::to_string(points[i]));
		glm::vec3 next = points[(i + 1) % n];

		if (next == Orbit::Invalid || glm::l2Norm(next) > 1e8f) break;

		float alpha = color.w * std::max(0.2f, static_cast<float>(n - i)) / n;
		lines.draw(points[i] + origin_pos, next + origin_pos, glm::u8vec4(color.x, color.y, color.z, alpha));
	}

	if (continuation != nullptr) continuation->draw(lines, color);
}
