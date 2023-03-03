#pragma once
#include "Engine/Math/Vec3.hpp"
#include "Engine/Math/Vec2.hpp"
#include "Engine/Math/RandomNumberGenerator.hpp"
#include "Engine/Core/Rgba8.hpp"
#include "Engine/Core/AnimatedValue.hpp"

struct ParticleEmitterData;

struct Particle
{
public:
	void Update(float deltaSeconds, const ParticleEmitterData& data);
	bool IsAlive() const;
	float GetNormalizedAge() const;
	void Reset();

public:
	Vec3 m_position;
	float m_size = 0.f;
	Vec3 m_velocity;
	float m_rotation = 0.f;
	float m_color[4] = { 0.f };
	float m_lifeTime = 0.f;
	float m_age = 0.f;
	float m_orbitalAngle = 0.f;
	float m_orbitalRadius = 0.f;
	unsigned int m_particleID = 0;

private:
	Vec3 GetVelocityForCurrentAge(const ParticleEmitterData& data, float normalizedAge) const;
	float GetOrbitalVelocityForCurrentAge(const ParticleEmitterData& data, float normalizedAge) const;
	float GetOrbitalRadiusForCurrentAge(const ParticleEmitterData& data, float normalizedAge) const;
	float GetDragForCurrentAge(const ParticleEmitterData& data, float normalizedAge) const;
	Rgba8 GetColorForCurrentAge(const ParticleEmitterData& data, float normalizedAge) const;
	Vec3 GetResultantVelocityToAttractors(const ParticleEmitterData& data) const;
	float GetAngularVelocityForCurrentAge(const ParticleEmitterData& data, float normalizedAge) const;
};


