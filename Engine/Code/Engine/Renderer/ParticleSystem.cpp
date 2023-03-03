#include <algorithm>
#include "Engine/Renderer/ParticleSystem.hpp"
#include "Engine/Renderer/ParticleEmitter.hpp"
#include "Engine/Renderer/ParticleEmitterData.hpp"
#include "Engine/Core/XmlUtils.hpp"

ParticleSystem::ParticleSystem(ParticlesManager* manager, ParticlePool* particlePool, Renderer* renderer, JobSystem* jobSystem, const Vec3& position, const char* dataFilepath, bool gpuParticles, bool defaultSystem)
	:m_particlesManager(manager), m_particlePool(particlePool), m_renderer(renderer), m_jobSystem(jobSystem), m_position(position), m_gpuParticles(gpuParticles), m_filepath(dataFilepath)
{
	if (!defaultSystem)
	{
		//load particle system from xml file
		LoadFromFile(dataFilepath);
	}
	else
	{
		ParticleEmitter* newEmitter = new ParticleEmitter(this, nullptr);
		m_emitters.push_back(newEmitter);
	}
}

ParticleSystem::~ParticleSystem()
{
	for (int i = 0; i < m_emitters.size(); i++)
	{
		delete m_emitters[i];
	}

	m_emitters.clear();
}

void ParticleSystem::Update(float deltaSeconds, const Camera& camera)
{
	for (int i = 0; i < m_emitters.size(); i++)
	{
		m_emitters[i]->Update(deltaSeconds, camera);
	}
}

void ParticleSystem::Render()
{
	std::vector<ParticleEmitter*> sortedDrawOrderEmitters = m_emitters;
	std::sort(sortedDrawOrderEmitters.begin(), sortedDrawOrderEmitters.end(),
		[](const ParticleEmitter* a, const ParticleEmitter* b)
		{
			return a->GetEmitterData().m_drawOrder < b->GetEmitterData().m_drawOrder;
		}
	);

	for (int i = 0; i < sortedDrawOrderEmitters.size(); i++)
	{
		sortedDrawOrderEmitters[i]->Render();
	}
}

void ParticleSystem::LoadFromFile(const char* filepath)
{
	tinyxml2::XMLDocument doc;
	tinyxml2::XMLError status = doc.LoadFile(filepath);
	GUARANTEE_OR_DIE(status == tinyxml2::XML_SUCCESS, "Failed to particle system data file");
	XmlElement* rootElement = doc.RootElement();
	m_position = ParseXmlAttribute(*rootElement, "basePosition", m_position);

	XmlElement* childEmitterElement = rootElement->FirstChildElement();
	while (childEmitterElement)
	{
		ParticleEmitter* newEmitter = new ParticleEmitter(this, childEmitterElement);
		m_emitters.push_back(newEmitter);
		childEmitterElement = childEmitterElement->NextSiblingElement();
	}
}

Vec3 ParticleSystem::GetPosition() const
{
	return m_position;
}

void ParticleSystem::SetPosition(const Vec3& newPosition)
{
	m_position = newPosition;
}

std::vector<ParticleEmitterData> ParticleSystem::GetEmitterDataForAllEmitters() const
{
	std::vector<ParticleEmitterData> allData;
	for (int i = 0; i < m_emitters.size(); i++)
	{
		allData.push_back(m_emitters[i]->GetEmitterData());
	}

	return allData;
}

void ParticleSystem::UpdateEmitterData(const std::vector<ParticleEmitterData>& emitterData)
{
	for (int i = 0; i < m_emitters.size(); i++)
	{
		m_emitters[i]->UpdateEmitterData(emitterData[i]);
	}
}

void ParticleSystem::Restart()
{
	for (int i = 0; i < m_emitters.size(); i++)
	{
		m_emitters[i]->Restart();
	}
}

void ParticleSystem::ToggleDebugMode()
{
	for (int i = 0; i < m_emitters.size(); i++)
	{
		m_emitters[i]->ToggleDebugMode();
	}
}

void ParticleSystem::DebugGPUUpdateStepNow()
{
	for (int i = 0; i < m_emitters.size(); i++)
	{
		m_emitters[i]->DebugGPUUpdateStepNow();
	}
}
