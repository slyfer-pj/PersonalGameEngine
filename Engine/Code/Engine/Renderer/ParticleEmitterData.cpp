#include "Engine/Renderer/ParticleEmitterData.hpp"
#include "Engine/Renderer/Particle.hpp"
#include "Engine/Math/RandomNumberGenerator.hpp"

RandomNumberGenerator rng;

void ConeEmitter::GenerateParticle(Particle& particle)
{
	particle.m_velocity = rng.GetRandomDirectionInCone(m_coneForward, m_coneHalfAngle);
}

unsigned int ConeEmitter::GetShapeTypeNum() const
{
	return static_cast<unsigned int>(EmitterShape::CONE);
}

void SphereEmitter::GenerateParticle(Particle& particle)
{
	Vec3 randDirection = rng.GetRandomDirectionInSphere();
	particle.m_velocity = randDirection;
	if (m_fromSurface)
		particle.m_position += m_sphereRadius * randDirection;
	else
		particle.m_position += rng.GetRandomFloatInRange(0.f, m_sphereRadius) * randDirection;
}

unsigned int SphereEmitter::GetShapeTypeNum() const
{
	return static_cast<unsigned int>(EmitterShape::SPHERE);
}

void BoxEmitter::GenerateParticle(Particle& particle)
{
	particle.m_velocity = m_forward;
	Vec2 mins = Vec2::ZERO - Vec2(m_dimensions.x * 0.5f, m_dimensions.y * 0.5f);
	m_surface = AABB2(mins, mins + Vec2(m_dimensions.x, m_dimensions.y));
	Vec2 pointAtRandomUV = m_surface.GetPointAtUV(Vec2(rng.GetRandomFloatZeroToOne(), rng.GetRandomFloatZeroToOne()));
	particle.m_position += Vec3(pointAtRandomUV.x, pointAtRandomUV.y, -m_dimensions.z * 0.5f);
}

unsigned int BoxEmitter::GetShapeTypeNum() const
{
	return static_cast<unsigned int>(EmitterShape::BOX);
}

void ParticleEmitterData::LoadEmitterDataFromElement(const XmlElement& element)
{
	m_name = ParseXmlAttribute(element, "name", "Default");

	const tinyxml2::XMLElement* childElement = element.FirstChildElement("Base");
	if (childElement)
	{
		LoadBaseModuleDataFromElement(*childElement);
	}

	childElement = element.FirstChildElement("Emission");
	if (childElement)
	{
		LoadEmissionModuleDataFromElement(*childElement);
	}

	childElement = element.FirstChildElement("Shape");
	if (childElement)
	{
		LoadShapeModuleDataFromElement(*childElement);
	}

	childElement = element.FirstChildElement("SizeOverLifetime");
	if (childElement)
	{
		LoadSizeOverLifetimeDataFromElement(*childElement);
	}

	childElement = element.FirstChildElement("VelocityOverLifetime");
	if (childElement)
	{
		LoadVelocityOverLifetimeDataFromElement(*childElement);
	}

	childElement = element.FirstChildElement("OrbitalVelocityOverLifetime");
	if (childElement)
	{
		LoadOrbitalVelocityOverLifetimeDataFromElement(*childElement);
	}

	childElement = element.FirstChildElement("RotationOverLifetime");
	if (childElement)
	{
		LoadRotationOverLifetimeDataFromElement(*childElement);
	}

	childElement = element.FirstChildElement("ColorOverLifetime");
	if (childElement)
	{
		LoadColorOverLifetimeDataFromElement(*childElement);
	}

	childElement = element.FirstChildElement("Physics");
	if (childElement)
	{
		LoadPhysicsDataFromElement(*childElement);
	}

	childElement = element.FirstChildElement("Renderer");
	if (childElement)
	{
		LoadRendererDataFromElement(*childElement);
	}
}

void ParticleEmitterData::LoadDefaults()
{
	AddDefaultKeysForCurveOne(m_sizeOverLifetimeX, 1.f);
	AddDefaultKeysForCurveOne(m_sizeOverLifetimeY, 1.f);
	AddDefaultKeysForCurveOne(m_velocityOverLifetime_X, 1.f);
	AddDefaultKeysForCurveOne(m_velocityOverLifetime_Y, 1.f);
	AddDefaultKeysForCurveOne(m_velocityOverLifetime_Z, 1.f);
	AddDefaultKeysForCurveOne(m_dragOverLifetime, 1.f);
	AddDefaultKeysForCurveOne(m_rotationOverLifetime, 1.f);
	AddDefaultKeysForCurveOne(m_orbitalVelOverLifetime, 1.f);
	AddDefaultKeysForCurveOne(m_orbitalRadiusOverLifetime, 1.f);
	
	for (int i = int(m_colorOverLifetime.size()); i < 2; i++)
	{
		AnimatedValueKey<Rgba8> key(Rgba8::WHITE, 0.f);
		m_colorOverLifetime.push_back(key);
	}
}

void ParticleEmitterData::LoadBaseModuleDataFromElement(const XmlElement& element)
{
	m_maxParticles = ParseXmlAttribute(element, "maxParticles", m_maxParticles);
	m_drawOrder = ParseXmlAttribute(element, "order", m_drawOrder);
	m_offsetFromWorldPos = ParseXmlAttribute(element, "offset", m_offsetFromWorldPos);
	m_particleLifetime = ParseXmlAttribute(element, "lifetime", FloatRange::ZERO);
	m_startSpeed = ParseXmlAttribute(element, "speed", FloatRange::ZERO);
	m_startSize = ParseXmlAttribute(element, "size", FloatRange::ZERO);
	m_startRotationDegrees = ParseXmlAttribute(element, "rotation", FloatRange::ZERO);
	m_startColor = ParseXmlAttribute(element, "startColor", m_startColor);
	m_gravityScale = ParseXmlAttribute(element, "gravity", m_gravityScale);
	std::string simSpace = ParseXmlAttribute(element, "simspace", "Local");
	m_simulationSpace = GetSimulationSpaceFromName(simSpace);
}

void ParticleEmitterData::LoadEmissionModuleDataFromElement(const XmlElement& element)
{
	std::string emissionMode = ParseXmlAttribute(element, "mode", "Constant");
	m_emissionMode = GetEmissionModeFromName(emissionMode);
	m_particlesEmittedPerSecond = ParseXmlAttribute(element, "emissionRate", m_particlesEmittedPerSecond);
	m_numBurstParticles = ParseXmlAttribute(element, "numBurstParticles", m_numBurstParticles);
	m_burstInterval = ParseXmlAttribute(element, "burstInterval", m_burstInterval);
}

void ParticleEmitterData::LoadShapeModuleDataFromElement(const XmlElement& element)
{
	std::string emitterType = ParseXmlAttribute(element, "shape", "");
	EmitterShape shape = GetEmitterShapeFromName(emitterType);

	float coneHalfAngle = ParseXmlAttribute(element, "coneHalfAngle", 0.f);
	Vec3 coneForward = ParseXmlAttribute(element, "coneForward", Vec3(0.f, 0.f, 1.f));
	float sphereRadius = ParseXmlAttribute(element, "sphereRadius", 0.f);
	bool fromSphereSurface = ParseXmlAttribute(element, "fromSurface", false);
	Vec3 boxForward = ParseXmlAttribute(element, "boxForward", Vec3(0.f, 0.f, 1.f));
	Vec3 boxDimensions = ParseXmlAttribute(element, "boxDimensions", Vec3::ONE);

	switch (shape)
	{
	case EmitterShape::CONE:
	default:
	{
		ConeEmitter* coneEmitter = new ConeEmitter();
		coneEmitter->m_coneHalfAngle = coneHalfAngle;
		coneEmitter->m_coneForward = coneForward;
		m_shape = coneEmitter;
		break;
	}
	case EmitterShape::SPHERE:
	{
		SphereEmitter* sphereEmitter = new SphereEmitter();
		sphereEmitter->m_sphereRadius = sphereRadius;
		sphereEmitter->m_fromSurface = fromSphereSurface;
		m_shape = sphereEmitter;
		break;
	}
	case EmitterShape::BOX:
	{
		BoxEmitter* boxEmitter = new BoxEmitter();
		boxEmitter->m_dimensions = boxDimensions;
		boxEmitter->m_forward = boxForward;
		m_shape = boxEmitter;
		break;
	}
	}
}

void ParticleEmitterData::LoadSizeOverLifetimeDataFromElement(const XmlElement& element)
{
	const XmlElement* sizeXKeys = element.FirstChildElement("X");
	m_sizeOverLifeXModifier = ParseXmlAttribute(*sizeXKeys, "modifier", m_sizeOverLifeXModifier);
	if (sizeXKeys)
	{
		ParseAnimCurveData(sizeXKeys, m_sizeOverLifetimeX);
	}

	const XmlElement* sizeYKeys = element.FirstChildElement("Y");
	m_sizeOverLifeYModifier = ParseXmlAttribute(*sizeYKeys, "modifier", m_sizeOverLifeYModifier);
	if (sizeYKeys)
	{
		ParseAnimCurveData(sizeYKeys, m_sizeOverLifetimeY);
	}
}

void ParticleEmitterData::LoadVelocityOverLifetimeDataFromElement(const XmlElement& element)
{
	m_volSpeedModifier = ParseXmlAttribute(element, "modifier", m_volSpeedModifier);
	const XmlElement* velXKeys = element.FirstChildElement("VelocityX");
	if (velXKeys)
	{
		ParseAnimCurveData(velXKeys, m_velocityOverLifetime_X);
	}

	const XmlElement* velYKeys = element.FirstChildElement("VelocityY");
	if (velYKeys)
	{
		ParseAnimCurveData(velXKeys, m_velocityOverLifetime_Y);
	}

	const XmlElement* velZKeys = element.FirstChildElement("VelocityZ");
	if (velZKeys)
	{
		ParseAnimCurveData(velZKeys, m_velocityOverLifetime_Z);
	}

	const XmlElement* dragKeys = element.FirstChildElement("Drag");
	m_dragModifier = ParseXmlAttribute(*dragKeys, "modifier", m_dragModifier);
	if (dragKeys)
	{
		ParseAnimCurveData(dragKeys, m_dragOverLifetime);
	}
}

void ParticleEmitterData::LoadOrbitalVelocityOverLifetimeDataFromElement(const XmlElement& element)
{
	m_orbitalVelocityAxis = ParseXmlAttribute(element, "forward", m_orbitalVelocityAxis);
	const XmlElement* velKeys = element.FirstChildElement("Velocity");
	m_orbitalVelocityModifier = ParseXmlAttribute(*velKeys, "modifier", m_orbitalVelocityModifier);
	if (velKeys)
	{
		ParseAnimCurveData(velKeys, m_orbitalVelOverLifetime);
	}

	const XmlElement* radiusKeys = element.FirstChildElement("Radius");
	m_orbitalRadiusModifier = ParseXmlAttribute(*radiusKeys, "modifier", m_orbitalRadiusModifier);
	if (velKeys)
	{
		ParseAnimCurveData(radiusKeys, m_orbitalRadiusOverLifetime);
	}
}

void ParticleEmitterData::LoadRotationOverLifetimeDataFromElement(const XmlElement& element)
{
	m_rotationModifier = ParseXmlAttribute(element, "modifier", m_rotationModifier);
	ParseAnimCurveData(&element, m_rotationOverLifetime);
}

void ParticleEmitterData::LoadColorOverLifetimeDataFromElement(const XmlElement& element)
{
	const XmlElement* elementKey = element.FirstChildElement();
	while (elementKey)
	{
		AnimatedValueKey<Rgba8> key;
		Rgba8 value = ParseXmlAttribute(*elementKey, "value", Rgba8::WHITE);
		float time = ParseXmlAttribute(*elementKey, "time", 0.f);
		key.SetValue(value);
		key.SetTime(time);
		m_colorOverLifetime.push_back(key);
		elementKey = elementKey->NextSiblingElement();
	}

	//there have to be a minimum of 2 keys
	for (int i = int(m_colorOverLifetime.size()); i < 2; i++)
	{
		AnimatedValueKey<Rgba8> key(Rgba8::WHITE, 0.f);
		m_colorOverLifetime.push_back(key);
	}
}

void ParticleEmitterData::LoadPhysicsDataFromElement(const XmlElement& element)
{
	const XmlElement* elementKey = element.FirstChildElement();
	while (elementKey)
	{
		PointAttractor attractor;
		attractor.m_offsetFromEmitter = ParseXmlAttribute(*elementKey, "offset", attractor.m_offsetFromEmitter);
		attractor.m_strength = ParseXmlAttribute(*elementKey, "strength", attractor.m_strength);
		m_pointAttractors.push_back(attractor);
		elementKey = elementKey->NextSiblingElement();
	}
}

void ParticleEmitterData::LoadRendererDataFromElement(const XmlElement& element)
{
	std::string renderMode = ParseXmlAttribute(element, "mode", "Billboard");
	m_renderMode = GetRenderModeFromName(renderMode);
	m_textureFilepath = ParseXmlAttribute(element, "texture", m_textureFilepath);
	m_isSpriteSheetTexture = ParseXmlAttribute(element, "isSpriteSheet", m_isSpriteSheetTexture);
	m_spriteSheetGridLayout = ParseXmlAttribute(element, "dimensions", IntVec2::ONE);
	std::string blendMode = ParseXmlAttribute(element, "blend", "Alpha");
	if (blendMode == "Alpha")
		m_blendMode = BlendMode::ALPHA;
	else if (blendMode == "Additive")
		m_blendMode = BlendMode::ADDITIVE;
	else
		m_blendMode = BlendMode::OPAQUE;
	m_sortParticles = ParseXmlAttribute(element, "sortParticles", m_sortParticles);
}

EmitterShape GetEmitterShapeFromName(const std::string& name)
{
	if (name == "Sphere")
		return EmitterShape::SPHERE;
	else if (name == "Box")
		return EmitterShape::BOX;
	else
		return EmitterShape::CONE;
}

RenderMode GetRenderModeFromName(const std::string& name)
{
	if (name == "HorizontalBillboard")
		return RenderMode::HORIZONTAL_BILLBOARD;
	else
		return RenderMode::BILLBOARD;
}

SimulationSpace GetSimulationSpaceFromName(const std::string& name)
{
	if (name == "World")
		return SimulationSpace::WORLD;
	else
		return SimulationSpace::LOCAL;
}

EmissionMode GetEmissionModeFromName(const std::string& name)
{
	if (name == "Burst")
		return EmissionMode::BURST;
	else
		return EmissionMode::CONSTANT;
}

