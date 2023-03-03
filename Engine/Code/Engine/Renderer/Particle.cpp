#include "Engine/Renderer/Particle.hpp"
#include "Engine/Renderer/ParticleEmitter.hpp"
#include "Engine/Math/MathUtils.hpp"

RandomNumberGenerator g_rng;

void Particle::Update(float deltaSeconds, const ParticleEmitterData& data)
{
	m_age += deltaSeconds;
	float normalizedAge = GetNormalizedAge();
	Vec3 velOverTime = GetVelocityForCurrentAge(data, normalizedAge);
	Vec3 velTowardsPointAttractor = GetResultantVelocityToAttractors(data);
	Vec3 gravitationalForce = (Vec3(0.f, 0.f, -10.f) * static_cast<float>(data.m_gravityScale));
	Vec3 dragForce = GetDragForCurrentAge(data, normalizedAge) * -1.f * m_velocity;
	Vec3 acceleration = gravitationalForce + dragForce + velOverTime + velTowardsPointAttractor;
	m_rotation += GetAngularVelocityForCurrentAge(data, normalizedAge) * deltaSeconds;
	m_orbitalAngle += GetOrbitalVelocityForCurrentAge(data, normalizedAge) * deltaSeconds;
	m_orbitalRadius += GetOrbitalRadiusForCurrentAge(data, normalizedAge) * deltaSeconds;
	m_velocity += acceleration * deltaSeconds;
	m_position += m_velocity * deltaSeconds;
	Rgba8 color = GetColorForCurrentAge(data, normalizedAge);
	color.GetAsFloats(m_color);
}

bool Particle::IsAlive() const
{
	return m_age < m_lifeTime;
}

float Particle::GetNormalizedAge() const
{
	return m_age / m_lifeTime;
}

void Particle::Reset()
{
	m_position = Vec3::ZERO;
	m_size = 0.f;
	m_velocity = Vec3::ZERO;
	m_rotation = 0.f;
	m_lifeTime = 0.f;
	m_age = 0.f;
	for (int i = 0; i < 4; i++)
	{
		m_color[i] = 0.f;
	}
	m_orbitalAngle = 0.f;
	m_orbitalRadius = 0.f;
}

Vec3 Particle::GetVelocityForCurrentAge(const ParticleEmitterData& data, float normalizedAge) const
{
	float xVelOverTime = data.m_velocityOverLifetime_X.GetInterpolatedAnimValueFromCurve(normalizedAge, g_rng, m_particleID);
	float yVelOverTime = data.m_velocityOverLifetime_Y.GetInterpolatedAnimValueFromCurve(normalizedAge, g_rng, m_particleID);
	float zVelOverTime = data.m_velocityOverLifetime_Z.GetInterpolatedAnimValueFromCurve(normalizedAge, g_rng, m_particleID);

	return Vec3(xVelOverTime, yVelOverTime, zVelOverTime) * data.m_volSpeedModifier;
}

float Particle::GetOrbitalVelocityForCurrentAge(const ParticleEmitterData& data, float normalizedAge) const
{
	float orbitalVelOverTime = data.m_orbitalVelOverLifetime.GetInterpolatedAnimValueFromCurve(normalizedAge, g_rng, m_particleID);
	return orbitalVelOverTime * data.m_orbitalVelocityModifier;
}

float Particle::GetOrbitalRadiusForCurrentAge(const ParticleEmitterData& data, float normalizedAge) const
{
	float orbitalRadiusOverTime = data.m_orbitalRadiusOverLifetime.GetInterpolatedAnimValueFromCurve(normalizedAge, g_rng, m_particleID);
	return orbitalRadiusOverTime * data.m_orbitalRadiusModifier;
}

float Particle::GetDragForCurrentAge(const ParticleEmitterData& data, float normalizedAge) const
{
	float currentDrag = data.m_dragOverLifetime.GetInterpolatedAnimValueFromCurve(normalizedAge, g_rng, m_particleID);
	return currentDrag * data.m_dragModifier;
}

Rgba8 Particle::GetColorForCurrentAge(const ParticleEmitterData& data, float normalizedAge) const
{
	int prevFrame, nextFrame;
	const std::vector<AnimatedValueKey<Rgba8>>& colorkeys = data.m_colorOverLifetime;
	GetAnimationFramesGivenNormalizedAge(normalizedAge, prevFrame, nextFrame, colorkeys);
	Rgba8 currentColor = Interpolate(colorkeys[prevFrame].GetValue(), colorkeys[nextFrame].GetValue(),
		GetFractionWithin(normalizedAge, colorkeys[prevFrame].GetTime(), colorkeys[nextFrame].GetTime()));

	return currentColor;
}

Vec3 Particle::GetResultantVelocityToAttractors(const ParticleEmitterData& data) const
{
	Vec3 resultantVelocity;
	for (int i = 0; i < data.m_pointAttractors.size(); i++)
	{
		Vec3 particleToAttractor = data.m_pointAttractors[i].m_offsetFromEmitter - m_position;
		float distanceToAttractor = particleToAttractor.GetLength();
		if (distanceToAttractor <= 0.f)
			distanceToAttractor = 1.f;
		float strengthBasedOnDistance = 0.f;
		strengthBasedOnDistance = data.m_pointAttractors[i].m_strength / ((distanceToAttractor + 1.f) *  (distanceToAttractor + 1.f));

		resultantVelocity += ((particleToAttractor / distanceToAttractor) * strengthBasedOnDistance);
	}

	return resultantVelocity;
}

float Particle::GetAngularVelocityForCurrentAge(const ParticleEmitterData& data, float normalizedAge) const
{
	float angularVelScale = data.m_rotationOverLifetime.GetInterpolatedAnimValueFromCurve(normalizedAge, g_rng, m_particleID);
	return angularVelScale * data.m_rotationModifier;
}
