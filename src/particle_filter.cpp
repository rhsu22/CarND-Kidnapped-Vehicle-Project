/*
 * particle_filter.cpp
 *
 *  Created on: Dec 12, 2016
 *      Author: Tiffany Huang
 */

#include <random>
#include <algorithm>
#include <iostream>
#include <numeric>
#include <math.h>
#include <iostream>
#include <sstream>
#include <string>
#include <iterator>

#include "particle_filter.h"

using namespace std;

void ParticleFilter::init(double x, double y, double theta, double std[]) {
	// TODO: Set the number of particles. Initialize all particles to first position (based on estimates of
	//   x, y, theta and their uncertainties from GPS) and all weights to 1.
	// Add random Gaussian noise to each particle.
	// NOTE: Consult particle_filter.h for more information about this method (and others in this file).

	num_particles = 1000;
	std::default_random_engine gen;
	std::normal_distribution<double> dist_x(x, std[0]);
	std::normal_distribution<double> dist_y(y, std[1]);
	std::normal_distribution<double> dist_theta(theta, std[2]);
	for (int i = 0; i < num_particles; i++) {
		Particle p;
		p.x = dist_x(gen);
		p.y = dist_y(gen);
		p.theta = dist_theta(gen);
		p.weight = 1;
		particles.push_back(p);
		weights.push_back(1.0);
	}
	is_initialized = true;
}

void ParticleFilter::prediction(double delta_t, double std_pos[], double velocity, double yaw_rate) {
	// TODO: Add measurements to each particle and add random Gaussian noise.
	// NOTE: When adding noise you may find std::normal_distribution and std::default_random_engine useful.
	//  http://en.cppreference.com/w/cpp/numeric/random/normal_distribution
	//  http://www.cplusplus.com/reference/random/default_random_engine/

	std::default_random_engine gen;
	std::normal_distribution<double> dist_x(0, std_pos[0]);
	std::normal_distribution<double> dist_y(0, std_pos[1]);
	std::normal_distribution<double> dist_theta(0, std_pos[2]);

	if (yaw_rate == 0.) {
		for (auto& p : particles) {
			p.x += delta_t * velocity * cos(p.theta) + dist_x(gen);
			p.y += delta_t * velocity * sin(p.theta) + dist_y(gen);
			p.theta += dist_theta(gen);
		}
	}
	else {
		for (auto& p : particles) {
			p.x += velocity / yaw_rate * (sin(p.theta + yaw_rate * delta_t) - sin(p.theta)) + 1.5*dist_x(gen);
			p.y += velocity / yaw_rate * (cos(p.theta) - cos(p.theta + yaw_rate * delta_t)) + 1.5*dist_y(gen);
			p.theta += yaw_rate * delta_t + dist_theta(gen);
		}
	}
}

void ParticleFilter::dataAssociation(std::vector<LandmarkObs> predicted, std::vector<LandmarkObs>& observations) {
	// TODO: Find the predicted measurement that is closest to each observed measurement and assign the
	//   observed measurement to this particular landmark.
	// NOTE: this method will NOT be called by the grading code. But you will probably find it useful to
	//   implement this method and use it as a helper during the updateWeights phase.

}

double tfX(double px, double theta, double cx, double cy) {
	return cos(theta) * cx - sin(theta) * cy + px;
}

double tfY(double py, double theta, double cx, double cy) {
	return sin(theta) * cx + cos(theta) * cy + py;
}

void ParticleFilter::updateWeights(double sensor_range, double std_landmark[],
		const std::vector<LandmarkObs> &observations, const Map &map_landmarks) {
	// TODO: Update the weights of each particle using a mult-variate Gaussian distribution. You can read
	//   more about this distribution here: https://en.wikipedia.org/wiki/Multivariate_normal_distribution
	// NOTE: The observations are given in the VEHICLE'S coordinate system. Your particles are located
	//   according to the MAP'S coordinate system. You will need to transform between the two systems.
	//   Keep in mind that this transformation requires both rotation AND translation (but no scaling).
	//   The following is a good resource for the theory:
	//   https://www.willamette.edu/~gorr/classes/GeneralGraphics/Transforms/transforms2d.htm
	//   and the following is a good resource for the actual equation to implement (look at equation
	//   3.33
	//   http://planning.cs.uiuc.edu/node99.html

	// For each particle
	// Transform observations into world coordinates
	// For each observation, find nearest landmark
	// Update particle weight

	double std_x = std_landmark[0];
	double std_y = std_landmark[1];
	weights.clear();

	for (auto& particle : particles) {
		std::vector<int> associations;
		std::vector<double> sense_x;
		std::vector<double> sense_y;
		double weight = 1;

		for (auto& obs : observations) {
			double mx = tfX(particle.x, particle.theta, obs.x, obs.y);
			double my = tfY(particle.y, particle.theta, obs.x, obs.y);

			// Find nearest landmark
			double nearest_distance = std::numeric_limits<double>::max();
			const Map::single_landmark_s* nearest_landmark = nullptr;
			for (auto& lm : map_landmarks.landmark_list) {
				double distance = dist(mx, my, lm.x_f, lm.y_f);
				if (distance < nearest_distance) {
					nearest_landmark = &lm;
					nearest_distance = distance;
				}
			}

			// Skip landmarks that might be outside range of sensors
			double px = particle.x;
			double py = particle.y;
			if (dist(px,py,nearest_landmark->x_f,nearest_landmark->y_f) > sensor_range) {
				continue;
			}
			//if (abs(px - nearest_landmark->x_f) > sensor_range || abs(py - nearest_landmark->y_f) > sensor_range) {
			//	continue;
			//}
			associations.push_back(nearest_landmark->id_i);
			sense_x.push_back(mx);
			sense_y.push_back(my);

			// P(x,y)
			double exponent = -((mx - nearest_landmark->x_f) * (mx - nearest_landmark->x_f) / (2 * std_x * std_x)
												 +(my - nearest_landmark->y_f) * (my - nearest_landmark->y_f) / (2 * std_y * std_y));
			double scale = 2 * M_PI * std_x * std_y;
			weight *= exp(exponent) / scale;
		}

		// This means that no proper neighbors found
		if (weight == 1.) {
			weight = 0.;
		}
		weights.push_back(weight);
		particle.weight = weight;
		SetAssociations(particle, associations, sense_x, sense_y);
	}

}

void ParticleFilter::resample() {
	// TODO: Resample particles with replacement with probability proportional to their weight.
	// NOTE: You may find std::discrete_distribution helpful here.
	//   http://en.cppreference.com/w/cpp/numeric/random/discrete_distribution

	std::random_device rd;
  std::mt19937 gen(rd());
	std::discrete_distribution<> sampler(weights.begin(), weights.end());

	std::vector<Particle> resampled;
	for (int i = 0; i < num_particles; i++) {
		resampled.push_back(particles[sampler(gen)]);
	}
	particles = resampled;
}

Particle ParticleFilter::SetAssociations(Particle& particle, const std::vector<int>& associations,
                                     const std::vector<double>& sense_x, const std::vector<double>& sense_y)
{
    //particle: the particle to assign each listed association, and association's (x,y) world coordinates mapping to
    // associations: The landmark id that goes along with each listed association
    // sense_x: the associations x mapping already converted to world coordinates
    // sense_y: the associations y mapping already converted to world coordinates

    particle.associations= associations;
    particle.sense_x = sense_x;
    particle.sense_y = sense_y;

		return particle;
}

string ParticleFilter::getAssociations(Particle best)
{
	vector<int> v = best.associations;
	stringstream ss;
    copy( v.begin(), v.end(), ostream_iterator<int>(ss, " "));
    string s = ss.str();
    s = s.substr(0, s.length()-1);  // get rid of the trailing space
    return s;
}
string ParticleFilter::getSenseX(Particle best)
{
	vector<double> v = best.sense_x;
	stringstream ss;
    copy( v.begin(), v.end(), ostream_iterator<float>(ss, " "));
    string s = ss.str();
    s = s.substr(0, s.length()-1);  // get rid of the trailing space
    return s;
}
string ParticleFilter::getSenseY(Particle best)
{
	vector<double> v = best.sense_y;
	stringstream ss;
    copy( v.begin(), v.end(), ostream_iterator<float>(ss, " "));
    string s = ss.str();
    s = s.substr(0, s.length()-1);  // get rid of the trailing space
    return s;
}
