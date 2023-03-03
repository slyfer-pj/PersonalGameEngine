#pragma once
#include <vector>
#include "Engine/Math/FloatRange.hpp"
#include "Engine/Math/IntVec2.hpp"
#include "Engine/Math/AABB3.hpp"
#include "Engine/Core/Rgba8.hpp"
#include "Engine/Core/AnimatedValue.hpp"
#include "Engine/Core/XmlUtils.hpp"
#include "Engine/Renderer/Renderer.hpp"

struct Particle;

enum class EmitterShape : unsigned int
{
	CONE = 0,
	SPHERE,
	BOX
};

enum class RenderMode : unsigned int 
{
	BILLBOARD = 0,
	HORIZONTAL_BILLBOARD
};

enum class SimulationSpace : unsigned int 
{
	LOCAL = 0,
	WORLD
};

enum class EmissionMode : unsigned int
{
	CONSTANT = 0,
	BURST
};

struct PointAttractor
{
	Vec3 m_offsetFromEmitter;
	float m_strength = 0.f;
};

struct ShapeEmitter
{
public:
	virtual void GenerateParticle(Particle& particle) = 0;
	virtual unsigned int GetShapeTypeNum() const = 0;
	virtual ~ShapeEmitter() = default;
};

struct ConeEmitter : public ShapeEmitter
{
public:
	void GenerateParticle(Particle& particle) override;
	unsigned int GetShapeTypeNum() const;

public:
	float m_coneHalfAngle = 30.f;
	Vec3 m_coneForward = Vec3(0.f, 0.f, 1.f);
};

struct SphereEmitter : public ShapeEmitter
{
public:
	void GenerateParticle(Particle& particle) override;
	unsigned int GetShapeTypeNum() const;

public:
	float m_sphereRadius = 5.f;
	bool m_fromSurface = false;
};

struct BoxEmitter : public ShapeEmitter
{
public:
	void GenerateParticle(Particle& particle) override;
	unsigned int GetShapeTypeNum() const;

public:
	Vec3 m_dimensions = Vec3(5.f, 5.f, 2.f);
	Vec3 m_forward = Vec3(0.f, 0.f, 1.f);

private:
	AABB2 m_surface;
};

struct ParticleEmitterData
{
public:
	std::string m_name = "Default";
	bool m_stopRender = false;
	int m_maxParticles = 100;
	int m_drawOrder = 0;
	Vec3 m_offsetFromWorldPos;
	FloatRange m_particleLifetime = FloatRange(2.f, 3.f);
	FloatRange m_startSpeed = FloatRange(3.f, 4.f);
	FloatRange m_startSize = FloatRange(0.25f, 0.5f);
	FloatRange m_startRotationDegrees = FloatRange(-30.f, 30.f);
	Rgba8 m_startColor = Rgba8::WHITE;
	int m_gravityScale = 0;
	SimulationSpace m_simulationSpace = SimulationSpace::LOCAL;
	EmissionMode m_emissionMode = EmissionMode::CONSTANT;
	float m_particlesEmittedPerSecond = 10.f;
	float m_numBurstParticles = 100.f;
	float m_burstInterval = 5.f;
	ShapeEmitter* m_shape = nullptr;
	float m_sizeOverLifeXModifier = 1.f;
	AnimatedCurve<float> m_sizeOverLifetimeX;
	float m_sizeOverLifeYModifier = 1.f;
	AnimatedCurve<float> m_sizeOverLifetimeY;
	float m_volSpeedModifier = 0.f;
	AnimatedCurve<float> m_velocityOverLifetime_X;
	AnimatedCurve<float> m_velocityOverLifetime_Y;
	AnimatedCurve<float> m_velocityOverLifetime_Z;
	float m_dragModifier = 0.f;
	AnimatedCurve<float> m_dragOverLifetime;
	float m_rotationModifier = 0.f;
	AnimatedCurve<float> m_rotationOverLifetime;
	float m_orbitalVelocityModifier = 0.f;
	AnimatedCurve<float> m_orbitalVelOverLifetime;
	float m_orbitalRadiusModifier = 0.f;
	AnimatedCurve<float> m_orbitalRadiusOverLifetime;
	Vec3 m_orbitalVelocityAxis = Vec3(0.f, 0.f, 1.f);
	std::vector<AnimatedValueKey<Rgba8>> m_colorOverLifetime;
	std::vector<PointAttractor> m_pointAttractors;
	RenderMode m_renderMode = RenderMode::BILLBOARD;
	std::string m_textureFilepath = "Default";
	bool m_isSpriteSheetTexture = false;
	IntVec2 m_spriteSheetGridLayout = IntVec2::ONE;
	BlendMode m_blendMode = BlendMode::ALPHA;
	bool m_sortParticles = false;
	AnimatedCurve<float> m_testCurve;

public:
	void LoadEmitterDataFromElement(const XmlElement& element);
	void LoadDefaults();

private:
	void LoadBaseModuleDataFromElement(const XmlElement& element);
	void LoadEmissionModuleDataFromElement(const XmlElement& element);
	void LoadShapeModuleDataFromElement(const XmlElement& element);
	void LoadSizeOverLifetimeDataFromElement(const XmlElement& element);
	void LoadVelocityOverLifetimeDataFromElement(const XmlElement& element);
	void LoadOrbitalVelocityOverLifetimeDataFromElement(const XmlElement& element);
	void LoadRotationOverLifetimeDataFromElement(const XmlElement& element);
	void LoadColorOverLifetimeDataFromElement(const XmlElement& element);
	void LoadPhysicsDataFromElement(const XmlElement& element);
	void LoadRendererDataFromElement(const XmlElement& element);

	template <typename T>
	void ParseKeyDataAndAddToKeyArray(const XmlElement* firstElement, std::vector<AnimatedValueKey<T>>& keyArray);
	template<typename T>
	void ParseAnimCurveData(const XmlElement* parent, AnimatedCurve<T>& curve);
	template <typename T>
	void AddDefaultKeysForCurveOne(AnimatedCurve<T>& curve, const T& defaultValue);

};

template <typename T>
void ParticleEmitterData::ParseKeyDataAndAddToKeyArray(const XmlElement* firstElement, std::vector<AnimatedValueKey<T>>& keyArray)
{
	const XmlElement* elementKey = firstElement;
	while (elementKey)
	{
		AnimatedValueKey<T> key;
		float value = ParseXmlAttribute(*elementKey, "value", 0.f);
		float time = ParseXmlAttribute(*elementKey, "time", 0.f);
		key.SetValue(value);
		key.SetTime(time);
		keyArray.push_back(key);
		elementKey = elementKey->NextSiblingElement();
	}

	for (int i = (int)keyArray.size(); i < 2; i++)
	{
		AnimatedValueKey<T> key;
		key.SetValue(1.f);
		key.SetTime(i * 1.f);
		keyArray.push_back(key);
	}
}

template<typename T>
void ParticleEmitterData::ParseAnimCurveData(const XmlElement* parent, AnimatedCurve<T>& curve)
{
	const XmlElement* curveOneElement = parent->FirstChildElement("CurveOne");
	const XmlElement* elementKey = curveOneElement ? curveOneElement->FirstChildElement() : parent->FirstChildElement();
	ParseKeyDataAndAddToKeyArray(elementKey, curve.m_curveOneKeys);

	const XmlElement* curveTwoElement = parent->FirstChildElement("CurveTwo");
	if (curveTwoElement)
	{
		curve.SetToRandomBetweenCurves(curveTwoElement);
		const XmlElement* curveTwoElementKey = curveTwoElement->FirstChildElement();
		ParseKeyDataAndAddToKeyArray(curveTwoElementKey, curve.m_curveTwoKeys);
	}
}

template <typename T>
void ParticleEmitterData::AddDefaultKeysForCurveOne(AnimatedCurve<T>& curve, const T& defaultValue)
{
	for (int i = (int)curve.m_curveOneKeys.size(); i < 2; i++)
	{
		AnimatedValueKey<T> key;
		key.SetValue(defaultValue);
		key.SetTime(i * 1.f);
		curve.m_curveOneKeys.push_back(key);
	}
}

EmitterShape GetEmitterShapeFromName(const std::string& name);
RenderMode GetRenderModeFromName(const std::string& name);
SimulationSpace GetSimulationSpaceFromName(const std::string& name);
EmissionMode GetEmissionModeFromName(const std::string& name);




