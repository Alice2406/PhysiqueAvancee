#include "pch.h"
#include "../Cyclone_Library/core.h"
#include "../Cyclone_Library/particle.h"

TEST(TestCaseName, magnitudetest) {
	cyclone::Vector3 a = {3,4,12};
	cyclone::real b = a.magnitude();

  EXPECT_NEAR(b, 13, 0.001);
}

TEST(TestCaseName, normalizetest) {
	cyclone::Vector3 a = { 0,3,4 };
	a.normalise();

	EXPECT_NEAR(a.x, 0, 0.001);
	EXPECT_NEAR(a.y, 0.6 , 0.001);
	EXPECT_NEAR(a.z, 0.8, 0.001);
}

TEST(TestCaseName, scalarproduct) {
	cyclone::Vector3 a = { 1,2,3 };
	cyclone::Vector3 b = { -2,5,4 };
	cyclone::real c;
	c = a.scalarProduct(b);

	EXPECT_NEAR(c, 20, 0.001);
}

TEST(TestCaseName, addScaledVector) {
	cyclone::Vector3 a = { 1,2,3 };
	cyclone::Vector3 b = { 2,-1,4 };
	cyclone::real scale = 0.5;
	a.addScaledVector(b, scale);

	EXPECT_NEAR(a.x, 2, 0.001);
	EXPECT_NEAR(a.y, 1.5, 0.001);
	EXPECT_NEAR(a.z, 5, 0.001);
}

TEST(TestCaseName, particuletest) {
	cyclone::Particle particule;
	particule.setPosition({ 0, 0, 0 });
	particule.setVelocity({ 2, 0, 0 });
	particule.setAcceleration({ 0, 0, 0 });
	particule.setDamping(1);
	particule.setMass(1);
	particule.integrate(0.5);
	cyclone::Vector3 newpos = particule.getPosition();
	cyclone::Vector3 newvel = particule.getVelocity();

	EXPECT_NEAR(newpos.x, 1, 0.001);
	EXPECT_NEAR(newpos.y, 0, 0.001);
	EXPECT_NEAR(newpos.z, 0, 0.001);
	EXPECT_NEAR(newvel.x, 2, 0.001);
	EXPECT_NEAR(newvel.y, 0, 0.001);
	EXPECT_NEAR(newvel.z, 0, 0.001);
}
