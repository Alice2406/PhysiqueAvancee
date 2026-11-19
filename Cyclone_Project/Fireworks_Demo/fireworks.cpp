/*
 * The fireworks demo.
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
#include <vector>

#include "../Cyclone_Library/cyclone.h"
#include "ogl_headers.h"
#include "app.h"
#include "timing.h"
#include <iostream>

static cyclone::Random crandom;

// [EXERCICE 4] Generateur de poussee initiale pour les feux de type 7 pendant 0.75 s
class FireworkLaunchThrust : public cyclone::ParticleForceGenerator
{
public:
    cyclone::Vector3 thrustForce; // Force de poussee vers le haut (ex: (0, 22, 0))
    cyclone::real maxThrustTime;  // Duree de propulsion : 0.75 s
    cyclone::real elapsedSinceSpawn;
    bool active;

    FireworkLaunchThrust(const cyclone::Vector3& thrust = cyclone::Vector3(0.0f, 22.0f, 0.0f),
        cyclone::real duration = 0.75f)
        : thrustForce(thrust), maxThrustTime(duration), elapsedSinceSpawn(0.0f), active(false)
    {
    }

    void reset(cyclone::real duration = 0.75f)
    {
        maxThrustTime = duration;
        elapsedSinceSpawn = 0.0f;
        active = true;
    }

    void advanceTime(cyclone::real dt)
    {
        if (!active) return;
        elapsedSinceSpawn += dt;
        if (elapsedSinceSpawn >= maxThrustTime)
        {
            active = false;
        }
    }

    virtual void updateForce(cyclone::Particle* particle, cyclone::real duration)
    {
        if (!active || !particle->hasFiniteMass()) return;

        // Poussee vers le haut pendant les 0.75 premieres secondes uniquement
        if (elapsedSinceSpawn < maxThrustTime)
        {
            particle->addForce(thrustForce);
        }
    }
};

// [EXERCICE 5] Champ de force radial temporaire (0.2 s) declenche a l'explosion d'un feu de type 1
class FireworkRadialField : public cyclone::ParticleForceGenerator
{
public:
    cyclone::Vector3 center;        // Position d'explosion c
    cyclone::real radius;           // Rayon R > 0
    cyclone::real peakStrength;     // Force maximale K > 0 (atteinte a r = R/2)
    cyclone::real remainingTime;    // Duree de vie restante (0.2 s initialement)
    const cyclone::Particle* owner; // Particule emettrice (pour ne pas repousser elle-meme)

    FireworkRadialField(const cyclone::Vector3& c,
        cyclone::real R,
        cyclone::real K,
        cyclone::real lifetime = 0.2f,
        const cyclone::Particle* source = NULL)
        : center(c), radius(R), peakStrength(K), remainingTime(lifetime), owner(source)
    {
    }

    bool isExpired() const
    {
        return remainingTime <= 0.0f;
    }

    void updateLifetime(cyclone::real dt)
    {
        remainingTime -= dt;
    }

    virtual void updateForce(cyclone::Particle* particle, cyclone::real duration)
    {
        if (isExpired() || !particle->hasFiniteMass()) return;
        if (particle == owner) return; // Pousse les AUTRES feux actifs

        cyclone::Vector3 delta = particle->getPosition() - center;
        cyclone::real r = delta.magnitude();

        if (r < radius && radius > 0.0f)
        {
            // F_field = 4 * K * (1 - r / R) * ((p - c) / R) pour 0 <= r < R
            cyclone::real factor = (cyclone::real)4.0 * peakStrength *
                ((cyclone::real)1.0 - (r / radius)) / radius;
            particle->addForce(delta * factor);
        }
    }
};

/**
 * Fireworks are particles, with additional data for rendering and
 * evolution.
 */
class Firework : public cyclone::Particle
{
public:
    /** Fireworks have an integer type, used for firework rules. */
    unsigned type;

    /**
     * The age of a firework determines when it detonates. Age gradually
     * decreases, when it passes zero the firework delivers its payload.
     * Think of age as fuse-left.
     */
    cyclone::real age;

    /**
     * Updates the firework by the given duration of time. Returns true
     * if the firework has reached the end of its life and needs to be
     * removed.
     */
    bool update(cyclone::real duration)
    {
        // Update our physical state
        integrate(duration);

        // We work backwards from our age to zero.
        age -= duration;
        return (age < 0) || (position.y < 0);
    }
};

/**
 * Firework rules control the length of a firework's fuse and the
 * particles it should evolve into.
 */
struct FireworkRule
{
    /** The type of firework that is managed by this rule. */
    unsigned type;

    /** The minimum length of the fuse. */
    cyclone::real minAge;

    /** The maximum legnth of the fuse. */
    cyclone::real maxAge;

    /** The minimum relative velocity of this firework. */
    cyclone::Vector3 minVelocity;

    /** The maximum relative velocity of this firework. */
    cyclone::Vector3 maxVelocity;

    /** The damping of this firework type. */
    cyclone::real damping;

    /**
     * The payload is the new firework type to create when this
     * firework's fuse is over.
     */
    struct Payload
    {
        /** The type of the new particle to create. */
        unsigned type;

        /** The number of particles in this payload. */
        unsigned count;

        /** Sets the payload properties in one go. */
        void set(unsigned type, unsigned count)
        {
            Payload::type = type;
            Payload::count = count;
        }
    };

    /** The number of payloads for this firework type. */
    unsigned payloadCount;

    /** The set of payloads. */
    Payload* payloads;

    FireworkRule()
        :
        payloadCount(0),
        payloads(NULL)
    {
    }

    void init(unsigned payloadCount)
    {
        FireworkRule::payloadCount = payloadCount;
        payloads = new Payload[payloadCount];
    }

    ~FireworkRule()
    {
        if (payloads != NULL) delete[] payloads;
    }

    /**
     * Set all the rule parameters in one go.
     */
    void setParameters(unsigned type, cyclone::real minAge, cyclone::real maxAge,
        const cyclone::Vector3& minVelocity, const cyclone::Vector3& maxVelocity,
        cyclone::real damping)
    {
        FireworkRule::type = type;
        FireworkRule::minAge = minAge;
        FireworkRule::maxAge = maxAge;
        FireworkRule::minVelocity = minVelocity;
        FireworkRule::maxVelocity = maxVelocity;
        FireworkRule::damping = damping;
    }

    /**
     * Creates a new firework of this type and writes it into the given
     * instance. The optional parent firework is used to base position
     * and velocity on.
     */
    void create(Firework* firework, const Firework* parent = NULL) const
    {
        firework->type = type;
        firework->age = minAge + (maxAge - minAge) * crandom.randomReal();
        firework->setMass(1.0f);
        firework->setDamping(damping);
        firework->setAcceleration(cyclone::Vector3::GRAVITY);

        cyclone::Vector3 vel;
        auto rx = crandom.randomReal();
        auto ry = crandom.randomReal();
        auto rz = crandom.randomReal();

        // [CORRECTION DE BUG] Ajout des parentheses autour de (maxVelocity.y - minVelocity.y)
        vel = { minVelocity.x + (maxVelocity.x - minVelocity.x) * rx,
                minVelocity.y + (maxVelocity.y - minVelocity.y) * ry,
                minVelocity.z + (maxVelocity.z - minVelocity.z) * rz };

        if (parent != NULL)
        {
            firework->setPosition(parent->getPosition());
            firework->setVelocity(parent->getVelocity() + vel);
        }
        else
        {
            firework->setPosition(0, 0, 0);
            firework->setVelocity(vel);
        }
        firework->clearAccumulator();
    }
};

/**
 * The main demo class definition.
 */
class FireworksDemo : public Application
{
    /**
     * Holds the maximum number of fireworks that can be in use.
     */
    const static unsigned maxFireworks = 1024;

    /** Holds the firework data. */
    Firework fireworks[maxFireworks];

    /** Holds the index of the next firework slot to use. */
    unsigned nextFirework;

    /** And the number of rules. */
    const static unsigned ruleCount = 9;

    /** Holds the set of rules. */
    FireworkRule rules[ruleCount];

    // [EXERCICE 4 & 5] Registre de forces, poussee par slot pour type 7, et champs radiaux actifs
    cyclone::ParticleForceRegistry registry;
    FireworkLaunchThrust slotThrust[maxFireworks];
    std::vector<FireworkRadialField*> activeRadialFields;

    /** Dispatches a firework from the origin. */
    void create(unsigned type, const Firework* parent = NULL);

    /** Dispatches the given number of fireworks from the given parent. */
    void create(unsigned type, unsigned number, const Firework* parent);

    /** Creates the rules. */
    void initFireworkRules();

    // [EXERCICE 5] Cree un champ radial temporaire (0.2 s) autour d'une detonation de type 1
    void spawnRadialField(const cyclone::Vector3& center, const cyclone::Particle* source);

    // [EXERCICE 5] Met a jour la duree de vie des champs radiaux et supprime ceux expires
    void updateRadialFields(cyclone::real duration);

public:
    /** Creates a new demo object. */
    FireworksDemo();
    ~FireworksDemo();

    /** Sets up the graphic rendering. */
    virtual void initGraphics();

    /** Returns the window title for the demo. */
    virtual const char* getTitle();

    /** Update the particle positions. */
    virtual void update();

    /** Display the particle positions. */
    virtual void display();

    /** Handle a keypress. */
    virtual void key(unsigned char key);
};

// Method definitions
FireworksDemo::FireworksDemo()
    :
    nextFirework(0)
{
    // Make all shots unused
    for (Firework* firework = fireworks;
        firework < fireworks + maxFireworks;
        firework++)
    {
        firework->type = 0;
        unsigned idx = (unsigned)(firework - fireworks);
        slotThrust[idx].active = false;
    }

    // Create the firework types
    initFireworkRules();
}

FireworksDemo::~FireworksDemo()
{
    // [EXERCICE 5] Liberation propre des champs radiaux alloues dynamiquement
    for (size_t i = 0; i < activeRadialFields.size(); ++i)
    {
        delete activeRadialFields[i];
    }
    activeRadialFields.clear();
}

void FireworksDemo::initFireworkRules()
{
    // Go through the firework types and create their rules.
    rules[0].init(2);
    rules[0].setParameters(
        1, // type
        0.5f, 1.4f, // age range
        cyclone::Vector3(-5, -5, -5), // min velocity
        cyclone::Vector3(5, 15, 5), // max velocity
        0.1 // damping
    );
    rules[0].payloads[0].set(3, 5);
    rules[0].payloads[1].set(5, 5);

    rules[1].init(1);
    rules[1].setParameters(
        2, // type
        0.5f, 1.4f, // age range
        cyclone::Vector3(-5, -5, -5), // min velocity
        cyclone::Vector3(5, 15, 5), // max velocity
        0.1 // damping
    );
    rules[1].payloads[0].set(4, 2);

    rules[2].init(0);
    rules[2].setParameters(
        3, // type
        0.5f, 1.4f, // age range
        cyclone::Vector3(-5, -5, -5), // min velocity
        cyclone::Vector3(5, 15, 5), // max velocity
        0.1 // damping
    );

    rules[3].init(0);
    rules[3].setParameters(
        4, // type
        0.5f, 1.4f, // age range
        cyclone::Vector3(-5, -5, -5), // min velocity
        cyclone::Vector3(5, 15, 5), // max velocity
        0.1 // damping
    );

    rules[4].init(1);
    rules[4].setParameters(
        5, // type
        0.5f, 1.4f, // age range
        cyclone::Vector3(-5, -5, -5), // min velocity
        cyclone::Vector3(5, 15, 5), // max velocity
        0.1 // damping
    );
    rules[4].payloads[0].set(3, 5);

    rules[5].init(0);
    rules[5].setParameters(6, // type
        0.5f, 1.4f, // age range
        cyclone::Vector3(-5, -5, -5), // min velocity
        cyclone::Vector3(5, 5, 5), // max velocity
        0.1 // damping
    );

    rules[6].init(1);
    rules[6].setParameters(7, // type
        0.5f, 1.4f, // age range
        cyclone::Vector3(-5, -5, -5), // min velocity
        cyclone::Vector3(5, 15, 5), // max velocity
        0.1 // damping
    );
    rules[6].payloads[0].set(8, 10);

    rules[7].init(0);
    rules[7].setParameters(8, // type
        0.5f, 1.4f, // age range
        cyclone::Vector3(-5, -5, -5), // min velocity
        cyclone::Vector3(5, 15, 5), // max velocity
        0.1 // damping
    );

    rules[8].init(0);
    rules[8].setParameters(9, // type
        0.5f, 1.4f, // age range
        cyclone::Vector3(-5, -5, -5), // min velocity
        cyclone::Vector3(5, 15, 5), // max velocity
        0.1 // damping
    );
}

void FireworksDemo::spawnRadialField(const cyclone::Vector3& center, const cyclone::Particle* source)
{
    // [EXERCICE 5] Rayon R = 10.0f, force pic K = 50.0f, duree = 0.2f s
    FireworkRadialField* field = new FireworkRadialField(center, 10.0f, 50.0f, 0.2f, source);
    activeRadialFields.push_back(field);

    // Enregistre ce champ sur toutes les autres particules actives
    for (unsigned i = 0; i < maxFireworks; ++i)
    {
        if (fireworks[i].type > 0 && &fireworks[i] != source)
        {
            registry.add(&fireworks[i], field);
        }
    }
}

void FireworksDemo::updateRadialFields(cyclone::real duration)
{
    // [EXERCICE 5] Met a jour les durees de vie et retire les champs expires
    for (size_t i = 0; i < activeRadialFields.size(); )
    {
        FireworkRadialField* field = activeRadialFields[i];
        field->updateLifetime(duration);

        if (field->isExpired())
        {
            for (unsigned j = 0; j < maxFireworks; ++j)
            {
                registry.remove(&fireworks[j], field);
            }
            delete field;
            activeRadialFields.erase(activeRadialFields.begin() + i);
        }
        else
        {
            ++i;
        }
    }
}

void FireworksDemo::initGraphics()
{
    // Call the superclass
    Application::initGraphics();

    // But override the clear color
    glClearColor(0.0f, 0.0f, 0.1f, 1.0f);
}

const char* FireworksDemo::getTitle()
{
    return "Cyclone > Fireworks Demo";
}

void FireworksDemo::create(unsigned type, const Firework* parent)
{
    // Get the rule needed to create this firework
    FireworkRule* rule = rules + (type - 1);

    unsigned slotIndex = nextFirework;
    Firework* targetFirework = fireworks + slotIndex;

    // [EXERCICE 4 & 5] Nettoyage des anciennes registrations si le slot est reutilise
    registry.remove(targetFirework, &slotThrust[slotIndex]);
    for (size_t i = 0; i < activeRadialFields.size(); ++i)
    {
        registry.remove(targetFirework, activeRadialFields[i]);
    }
    slotThrust[slotIndex].active = false;

    // Create the firework
    rule->create(targetFirework, parent);

    // [EXERCICE 4] Si le nouveau feu est de type 7, activer la poussee pendant 0.75 s
    if (type == 7)
    {
        slotThrust[slotIndex].reset(0.75f);
        registry.add(targetFirework, &slotThrust[slotIndex]);
    }

    // [EXERCICE 5] Enregistrer les champs radiaux actifs sur cette particule si elle est active
    for (size_t i = 0; i < activeRadialFields.size(); ++i)
    {
        if (targetFirework != activeRadialFields[i]->owner)
        {
            registry.add(targetFirework, activeRadialFields[i]);
        }
    }

    // Increment the index for the next firework
    nextFirework = (nextFirework + 1) % maxFireworks;
}

void FireworksDemo::create(unsigned type, unsigned number, const Firework* parent)
{
    for (unsigned i = 0; i < number; i++)
    {
        create(type, parent);
    }
}

void FireworksDemo::update()
{
    // Find the duration of the last frame in seconds
    float duration = (float)TimingData::get().lastFrameDuration * 0.001f;
    if (duration <= 0.0f) return;

    // [EXERCICE 4] Avancer le chronometre de poussee des feux de type 7
    for (unsigned i = 0; i < maxFireworks; ++i)
    {
        if (fireworks[i].type == 7 && slotThrust[i].active)
        {
            slotThrust[i].advanceTime(duration);
        }
    }

    // [EXERCICE 4 & 5] Evaluer les generateurs de forces AVANT l'integration
    registry.updateForces(duration);

    for (Firework* firework = fireworks;
        firework < fireworks + maxFireworks;
        firework++)
    {
        // Check if we need to process this firework.
        if (firework->type > 0)
        {
            // Does it need removing?
            if (firework->update(duration))
            {
                // Find the appropriate rule
                FireworkRule* rule = rules + (firework->type - 1);

                // [EXERCICE 5] A la detonation d'un feu de type 1, creer un champ radial temporaire (0.2 s)
                if (firework->type == 1)
                {
                    spawnRadialField(firework->getPosition(), firework);
                }

                // [EXERCICE 4 & 5] Retirer les forces liees a ce slot avant de le desactiver
                unsigned slotIdx = (unsigned)(firework - fireworks);
                registry.remove(firework, &slotThrust[slotIdx]);
                for (size_t k = 0; k < activeRadialFields.size(); ++k)
                {
                    registry.remove(firework, activeRadialFields[k]);
                }
                slotThrust[slotIdx].active = false;

                // Delete the current firework (this doesn't affect its
                // position and velocity for passing to the create function,
                // just whether or not it is processed for rendering or
                // physics.
                firework->type = 0;

                // Add the payload
                for (unsigned i = 0; i < rule->payloadCount; i++)
                {
                    FireworkRule::Payload* payload = rule->payloads + i;
                    create(payload->type, payload->count, firework);
                }
            }
        }
    }

    // [EXERCICE 5] Mettre a jour la duree de vie des champs radiaux en fin de pas
    updateRadialFields(duration);

    Application::update();
}

void FireworksDemo::display()
{
    const static cyclone::real size = 0.1f;

    // Clear the viewport and set the camera direction
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glLoadIdentity();
    gluLookAt(0.0, 4.0, 10.0, 0.0, 4.0, 0.0, 0.0, 1.0, 0.0);

    // Render each firework in turn
    glBegin(GL_QUADS);
    for (Firework* firework = fireworks;
        firework < fireworks + maxFireworks;
        firework++)
    {
        // Check if we need to process this firework.
        if (firework->type > 0)
        {
            switch (firework->type)
            {
            case 1: glColor3f(1, 0, 0); break;
            case 2: glColor3f(1, 0.5f, 0); break;
            case 3: glColor3f(1, 1, 0); break;
            case 4: glColor3f(0, 1, 0); break;
            case 5: glColor3f(0, 1, 1); break;
            case 6: glColor3f(0.4f, 0.4f, 1); break;
            case 7: glColor3f(1, 0, 1); break;
            case 8: glColor3f(1, 1, 1); break;
            case 9: glColor3f(1, 0.5f, 0.5f); break;
            };

            const cyclone::Vector3& pos = firework->getPosition();
            glVertex3f(pos.x - size, pos.y - size, pos.z);
            glVertex3f(pos.x + size, pos.y - size, pos.z);
            glVertex3f(pos.x + size, pos.y + size, pos.z);
            glVertex3f(pos.x - size, pos.y + size, pos.z);

            // Render the firework's reflection
            glVertex3f(pos.x - size, -pos.y - size, pos.z);
            glVertex3f(pos.x + size, -pos.y - size, pos.z);
            glVertex3f(pos.x + size, -pos.y + size, pos.z);
            glVertex3f(pos.x - size, -pos.y + size, pos.z);
        }
    }
    glEnd();
}

void FireworksDemo::key(unsigned char key)
{
    switch (key)
    {
    case '1': create(1, 1, NULL); break;
    case '2': create(2, 1, NULL); break;
    case '3': create(3, 1, NULL); break;
    case '4': create(4, 1, NULL); break;
    case '5': create(5, 1, NULL); break;
    case '6': create(6, 1, NULL); break;
    case '7': create(7, 1, NULL); break;
    case '8': create(8, 1, NULL); break;
    case '9': create(9, 1, NULL); break;
    }
}

/**
 * Called by the common demo framework to create an application
 * object (with new) and return a pointer.
 */
Application* getApplication()
{
    return new FireworksDemo();
}