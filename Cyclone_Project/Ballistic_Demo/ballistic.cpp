/*
 * The ballistic demo.
 *
 * Part of the Cyclone physics system.
 *
 * Copyright (c) Icosagon 2003. All Rights Reserved.
 *
 * This software is distributed under licence. Use of this software
 * implies agreement with all terms and conditions of the accompanying
 * software licence.
 */

#include <stdio.h>

#include "../Cyclone_Library/cyclone.h"
#include "ogl_headers.h"
#include "app.h"
#include "timing.h"

class BallisticCrosswind : public cyclone::ParticleForceGenerator
{
public:
    cyclone::real gustStrength;
    bool enabled;

    BallisticCrosswind(cyclone::real strength = 15.0f, bool isEnabled = true)
        : gustStrength(strength), enabled(isEnabled)
    {
    }

    virtual void updateForce(cyclone::Particle* particle, cyclone::real duration)
    {
        if (!enabled || !particle->hasFiniteMass()) return;

        cyclone::Vector3 pos = particle->getPosition();
        if (pos.z >= 20.0f && pos.z <= 80.0f)
        {
            particle->addForce(cyclone::Vector3(gustStrength, 0.0f, 0.0f));
        }
    }
};

/**
 * The main demo class definition.
 */
class BallisticDemo : public Application
{
    enum ShotType
    {
        UNUSED = 0,
        PISTOL,
        ARTILLERY,
        FIREBALL,
        LASER
    };

    /**
     * Holds a single ammunition round record.
     */
    struct AmmoRound
    {
        cyclone::Particle particle;
        ShotType type;
        unsigned startTime;
        float age; // [EXERCICE 1/2] Temps de vie restant en secondes

        /** Draws the round. */
        void render()
        {
            cyclone::Vector3 position;
            particle.getPosition(&position);

            glColor3f(0, 0, 0);
            glPushMatrix();
            glTranslatef(position.x, position.y, position.z);
            glutSolidSphere(0.3f, 5, 4);
            glPopMatrix();

            glColor3f(0.75, 0.75, 0.75);
            glPushMatrix();
            glTranslatef(position.x, 0, position.z);
            glScalef(1.0f, 0.1f, 1.0f);
            glutSolidSphere(0.6f, 5, 4);
            glPopMatrix();
        }
    };

    /**
     * Holds the maximum number of rounds that can be
     * fired.
     */
    const static unsigned ammoRounds = 16;

    /** Holds the particle data. */
    AmmoRound ammo[ammoRounds];

    /** Holds the current shot type. */
    ShotType currentShotType;

    /** Dispatches a round. */
    void fire();

    cyclone::ParticleForceRegistry registry;
    cyclone::ParticleGravity pistolGravity;
    cyclone::ParticleGravity artilleryGravity;
    cyclone::ParticleGravity fireballGravity;

    cyclone::ParticleDrag dragGenerator;
    bool dragEnabled;
    bool compareNoDampingMode;

    BallisticCrosswind crosswindGenerator;

    void clearShotForces(cyclone::Particle* particle)
    {
        registry.remove(particle, &pistolGravity);
        registry.remove(particle, &artilleryGravity);
        registry.remove(particle, &fireballGravity);
        registry.remove(particle, &dragGenerator);
        registry.remove(particle, &crosswindGenerator);
    }

public:
    /** Creates a new demo object. */
    BallisticDemo();

    /** Returns the window title for the demo. */
    virtual const char* getTitle();

    /** Update the particle positions. */
    virtual void update();

    /** Display the particle positions. */
    virtual void display();

    /** Handle a mouse click. */
    virtual void mouse(int button, int state, int x, int y);

    /** Handle a keypress. */
    virtual void key(unsigned char key);
};

BallisticDemo::BallisticDemo()
    : currentShotType(LASER),
    pistolGravity(cyclone::Vector3(0.0f, -1.0f, 0.0f)),
    artilleryGravity(cyclone::Vector3(0.0f, -20.0f, 0.0f)),
    fireballGravity(cyclone::Vector3(0.0f, 0.6f, 0.0f)),
    dragGenerator(0.05f, 0.01f),
    dragEnabled(false),
    compareNoDampingMode(false),
    crosswindGenerator(15.0f, false)
{
    // Make all shots unused
    for (AmmoRound* shot = ammo; shot < ammo + ammoRounds; shot++)
    {
        shot->type = UNUSED;
        shot->age = 0.0f;
    }
}

const char* BallisticDemo::getTitle()
{
    return "Cyclone > Ballistic Demo";
}

void BallisticDemo::fire()
{
    AmmoRound* shot;
    for (shot = ammo; shot < ammo + ammoRounds; shot++)
    {
        if (shot->type == UNUSED) break;
    }

    if (shot >= ammo + ammoRounds) return;

    clearShotForces(&shot->particle);
    shot->particle.setAcceleration(0.0f, 0.0f, 0.0f);
    shot->particle.clearAccumulator();

    cyclone::real pistolDamp = compareNoDampingMode ? 1.0f : 0.99f;
    cyclone::real artilleryDamp = compareNoDampingMode ? 1.0f : 0.99f;
    cyclone::real fireballDamp = compareNoDampingMode ? 1.0f : 0.9f;
    cyclone::real laserDamp = compareNoDampingMode ? 1.0f : 0.99f;

    switch (currentShotType)
    {
    case PISTOL:
        shot->particle.setMass(2.0f);
        shot->particle.setVelocity(0.0f, 0.0f, 35.0f);
        shot->particle.setDamping(pistolDamp);
        registry.add(&shot->particle, &pistolGravity);
        break;

    case ARTILLERY:
        shot->particle.setMass(200.0f);
        shot->particle.setVelocity(0.0f, 30.0f, 40.0f);
        shot->particle.setDamping(artilleryDamp);
        registry.add(&shot->particle, &artilleryGravity);
        break;

    case FIREBALL:
        shot->particle.setMass(1.0f);
        shot->particle.setVelocity(0.0f, 0.0f, 10.0f);
        shot->particle.setDamping(fireballDamp);
        registry.add(&shot->particle, &fireballGravity);
        break;

    case LASER:
        shot->particle.setMass(0.1f);
        shot->particle.setVelocity(0.0f, 0.0f, 100.0f);
        shot->particle.setDamping(laserDamp);
        break;
    }

    if (dragEnabled)
    {
        registry.add(&shot->particle, &dragGenerator);
    }

    registry.add(&shot->particle, &crosswindGenerator);

    shot->particle.setPosition(0.0f, 1.5f, 0.0f);
    shot->startTime = TimingData::get().lastFrameTimestamp;
    shot->age = 5.0f;
    shot->type = currentShotType;
}

void BallisticDemo::update()
{
    float duration = (float)TimingData::get().lastFrameDuration * 0.001f;
    if (duration <= 0.0f) return;

    registry.updateForces(duration);

    for (AmmoRound* shot = ammo; shot < ammo + ammoRounds; shot++)
    {
        if (shot->type != UNUSED)
        {
            shot->particle.integrate(duration);
            shot->age -= duration;

            if (shot->particle.getPosition().y < 0.0f ||
                shot->age <= 0.0f ||
                shot->particle.getPosition().z > 200.0f)
            {
                shot->type = UNUSED;
                clearShotForces(&shot->particle);
            }
        }
    }

    Application::update();
}

void BallisticDemo::display()
{
    // Clear the viewport and set the camera direction
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glLoadIdentity();
    gluLookAt(-25.0, 8.0, 5.0, 0.0, 5.0, 22.0, 0.0, 1.0, 0.0);

    // Draw a sphere at the firing point, and add a shadow projected
    // onto the ground plane.
    glColor3f(0.0f, 0.0f, 0.0f);
    glPushMatrix();
    glTranslatef(0.0f, 1.5f, 0.0f);
    glutSolidSphere(0.1f, 5, 5);
    glTranslatef(0.0f, -1.5f, 0.0f);
    glColor3f(0.75f, 0.75f, 0.75f);
    glScalef(1.0f, 0.1f, 1.0f);
    glutSolidSphere(0.1f, 5, 5);
    glPopMatrix();

    // Draw some scale lines
    glColor3f(0.75f, 0.75f, 0.75f);
    glBegin(GL_LINES);
    for (unsigned i = 0; i < 200; i += 10)
    {
        glVertex3f(-5.0f, 0.0f, i);
        glVertex3f(5.0f, 0.0f, i);
    }
    glEnd();

    // Render each particle in turn
    for (AmmoRound* shot = ammo; shot < ammo + ammoRounds; shot++)
    {
        if (shot->type != UNUSED)
        {
            shot->render();
        }
    }

    // Render the description
    glColor3f(0.0f, 0.0f, 0.0f);
    renderText(10.0f, 58.0f, "Click: Fire | 1-4: Ammo | d: Drag | m: Damping=1 | w: Wind | +/-: Wind force");

    char statusBuf[256];
    snprintf(statusBuf, sizeof(statusBuf),
        "Drag: %s | Damping=1 Mode: %s | Crosswind [20..80]: %s (%.1f N)",
        dragEnabled ? "ON" : "OFF",
        compareNoDampingMode ? "ON" : "OFF",
        crosswindGenerator.enabled ? "ON" : "OFF",
        crosswindGenerator.gustStrength);
    renderText(10.0f, 34.0f, statusBuf);

    // Render the name of the current shot type
    switch (currentShotType)
    {
    case PISTOL: renderText(10.0f, 10.0f, "Current Ammo: Pistol"); break;
    case ARTILLERY: renderText(10.0f, 10.0f, "Current Ammo: Artillery"); break;
    case FIREBALL: renderText(10.0f, 10.0f, "Current Ammo: Fireball"); break;
    case LASER: renderText(10.0f, 10.0f, "Current Ammo: Laser"); break;
    }
}

void BallisticDemo::mouse(int button, int state, int x, int y)
{
    // Fire the current weapon.
    if (state == GLUT_DOWN) fire();
}

void BallisticDemo::key(unsigned char key)
{
    switch (key)
    {
    case '1': currentShotType = PISTOL; break;
    case '2': currentShotType = ARTILLERY; break;
    case '3': currentShotType = FIREBALL; break;
    case '4': currentShotType = LASER; break;
    case 'd':
    case 'D':
        dragEnabled = !dragEnabled;
        break;
    case 'm':
    case 'M':
        compareNoDampingMode = !compareNoDampingMode;
        break;
    case 'w':
    case 'W':
        crosswindGenerator.enabled = !crosswindGenerator.enabled;
        break;
    case '+':
    case '=':
        crosswindGenerator.gustStrength += 5.0f;
        break;
    case '-':
    case '_':
        crosswindGenerator.gustStrength -= 5.0f;
        break;
    }
}

/**
 * Called by the common demo framework to create an application
 * object (with new) and return a pointer.
 */
Application* getApplication()
{
    return new BallisticDemo();
}