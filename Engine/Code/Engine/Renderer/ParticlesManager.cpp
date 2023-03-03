#include <algorithm>
#include <d3d11.h>
#include "Engine/Renderer/ParticlesManager.hpp"
#include "Engine/Renderer/ParticleSystem.hpp"
#include "Engine/Renderer/Camera.hpp"
#include "Engine/Renderer/ParticleEmitter.hpp"
#include "Engine/Renderer/VertexBuffer.hpp"
#include "Engine/Renderer/IndexBuffer.hpp"
#include "Engine/Renderer/Particle.hpp"
#include "Engine/Core/JobSystem.hpp"
#include "Engine/Core/ProfileLogScope.hpp"

ParticlesManager::ParticlesManager(const ParticlesManagerConfig& config)
	:m_config(config)
{
}

void ParticlesManager::Startup()
{
	unsigned int particlesPerPool = m_config.m_maxParticles / m_config.m_numPools;
	m_allCPUParticleSystems = new ParticleSystemList[m_config.m_numPools];

	std::vector<ParticleSystem*> particleSystemPerPool;
	for (int i = 0; i < m_config.m_numPools; i++)
	{
		ParticlePool* newPool = new ParticlePool(particlesPerPool, i);
		m_config.m_renderer->CreateD3DUnorderedAccessBuffer(&(newPool->m_particlePoolData), sizeof(Particle) * particlesPerPool, sizeof(Particle), nullptr, true);
		m_config.m_renderer->CreateShaderResourceView(&(newPool->m_particlePoolSRV), newPool->m_particlePoolData);
		m_particlePools.push_back(newPool);
	}
}

void ParticlesManager::Shutdown()
{
	KillAllParticleSystems();

	for (int i = 0; i < m_config.m_numPools; i++)
	{
		DX_SAFE_RELEASE(m_particlePools[i]->m_particlePoolSRV);
		DX_SAFE_RELEASE(m_particlePools[i]->m_particlePoolData);
		delete m_particlePools[i];
	}
	m_particlePools.clear();
}

ParticleSystem* ParticlesManager::CreateParticleSystem(const char* dataFilepath, const Vec3& position, bool gpuParticles, bool defaultSystem)
{
	ParticleSystem* newParticleSystem = nullptr;
	if (gpuParticles)
	{
		newParticleSystem = new ParticleSystem(this, nullptr, m_config.m_renderer, m_config.m_jobSystem, position, dataFilepath, true, defaultSystem);
		m_allGPUParticleSystems.push_back(newParticleSystem);
	}
	else
	{
		int bestPoolIndex = GetBestParticlePoolIndex();
		newParticleSystem = new ParticleSystem(this, m_particlePools[bestPoolIndex], m_config.m_renderer, m_config.m_jobSystem, position, dataFilepath, false, defaultSystem);
		m_allCPUParticleSystems[bestPoolIndex].push_back(newParticleSystem);
	}
	return newParticleSystem;
}

void ParticlesManager::UpdateParticleSystems(float deltaSeconds, const Camera& camera)
{
	std::vector<Job*> queuedUpdateJobs;
	queuedUpdateJobs.reserve(m_config.m_numPools);

	//issue an update job for the gpu particles
	UpdateParticlesJob* gpuUpdateJob = new UpdateParticlesJob(m_allGPUParticleSystems, deltaSeconds, camera);
	m_config.m_jobSystem->QueueJobs(gpuUpdateJob);
	queuedUpdateJobs.push_back(gpuUpdateJob);

	//issue update jobs for all but the first pool
	for (int i = 1; i < m_config.m_numPools; i++)
	{
		m_particlePools[i]->m_cpuMeshVertexData.clear();
		m_particlePools[i]->m_cpuMeshIndexData.clear();
		UpdateParticlesJob* updateJob = new UpdateParticlesJob(m_allCPUParticleSystems[i], deltaSeconds, camera);
		m_config.m_jobSystem->QueueJobs(updateJob);
		queuedUpdateJobs.push_back(updateJob);
	}

	//do update for the first pool on the main thread
	ParticleSystemList systemsForPoolID0 = m_allCPUParticleSystems[0];
	m_particlePools[0]->m_cpuMeshVertexData.clear();
	m_particlePools[0]->m_cpuMeshIndexData.clear();
	for (int i = 0; i < systemsForPoolID0.size(); i++)
	{
		systemsForPoolID0[i]->Update(deltaSeconds, camera);
	}
	//m_jobDoneCounter++;

	//PROFILE_LOG_SCOPE(Synctime)
	{
		while (!queuedUpdateJobs.empty())
		{
			UpdateParticlesJob* finishedJob = dynamic_cast<UpdateParticlesJob*>(m_config.m_jobSystem->RetrieveFinishedJob());
			while (finishedJob != nullptr)
			{
				if (finishedJob)
				{
					for (auto iter = queuedUpdateJobs.begin(); iter != queuedUpdateJobs.end();)
					{
						if (finishedJob == *iter)
						{
							queuedUpdateJobs.erase(iter);
							break;
						}
						else
						{
							++iter;
						}
					}

					delete finishedJob;
				}

				finishedJob = dynamic_cast<UpdateParticlesJob*>(m_config.m_jobSystem->RetrieveFinishedJob());
			}
		}
	}
}

void ParticlesManager::RenderParticleSystems(const Camera& camera)
{
	m_config.m_renderer->BindShaderByName("Default");
	std::vector<ParticleSystem*> allParticleSystems;
	for (int i = 0; i < m_config.m_numPools; i++)
	{
		m_config.m_renderer->CopyCPUToGPU(m_particlePools[i]->GetParticleList(), sizeof(Particle) * m_particlePools[i]->GetCurrentListSize(), m_particlePools[i]->m_particlePoolData);
		ParticleSystemList poolSystems = m_allCPUParticleSystems[i];
		allParticleSystems.insert(allParticleSystems.end(), poolSystems.begin(), poolSystems.end());
	}

	allParticleSystems.insert(allParticleSystems.end(), m_allGPUParticleSystems.begin(), m_allGPUParticleSystems.end());

	//sort particle systems based on distance from camera in descending order
	Vec3 camPos = camera.GetPosition();
	std::sort(allParticleSystems.begin(), allParticleSystems.end(),
		[camPos](const ParticleSystem* a, const ParticleSystem* b)
		{
			float aDistFromCam = GetDistanceSquared3D(camPos, a->GetPosition());
			float bDistFromCam = GetDistanceSquared3D(camPos, b->GetPosition());
			return aDistFromCam > bDistFromCam;
		}
	);

	//render particle systems
	for (int i = 0; i < allParticleSystems.size(); i++)
	{
		allParticleSystems[i]->Render();
	}
}

void ParticlesManager::KillParticleSystem(const ParticleSystem* systemToKill)
{
	if (systemToKill->m_gpuParticles)
	{
		for (int i = 0; i < m_allGPUParticleSystems.size(); i++)
		{
			if (m_allGPUParticleSystems[i] == systemToKill)
			{
				delete m_allGPUParticleSystems[i];
				m_allGPUParticleSystems.erase(m_allGPUParticleSystems.begin() + i);
				return;
			}
		}
	}
	else
	{
		int poolId = systemToKill->m_particlePool->GetID();
		ParticleSystemList& particleSystems = m_allCPUParticleSystems[poolId];
		for (int i = 0; i < particleSystems.size(); i++)
		{
			if (particleSystems[i] == systemToKill)
			{
				delete particleSystems[i];
				particleSystems.erase(particleSystems.begin() + i);
				return;
			}
		}
	}
}

void ParticlesManager::KillAllParticleSystems()
{
	for (int i = 0; i < m_config.m_numPools; i++)
	{
		ParticleSystemList& particleSystems = m_allCPUParticleSystems[i];
		for (int j = 0; j < particleSystems.size(); j++)
		{
			delete particleSystems[j];
		}
		particleSystems.clear();
	}

	for (int i = 0; i < m_allGPUParticleSystems.size(); i++)
	{
		delete m_allGPUParticleSystems[i];
	}
	m_allGPUParticleSystems.clear();
}

ParticlesDebugData ParticlesManager::GetDebugData() 
{
	ParticlesDebugData data;
	data.m_maxParticles = m_config.m_maxParticles;
	data.m_numPools = m_config.m_numPools;
	data.m_particlesPerPool = m_config.m_maxParticles / m_config.m_numPools;
	
	for (int i = 0; i < m_config.m_numPools; i++)
	{
		ParticleSystemList& particleSystems = m_allCPUParticleSystems[i];
		for (int j = 0; j < particleSystems.size(); j++)
		{
			data.m_numCPUsystems++;
			for (int k = 0; k < particleSystems[j]->m_emitters.size(); k++)
			{
				data.m_aliveParticles += particleSystems[j]->m_emitters[k]->GetDebugData().m_aliveParticles;
			}
		}
	}

	for (int i = 0; i < m_allGPUParticleSystems.size(); i++)
	{
		data.m_numGPUsystems++;
		for (int k = 0; k < m_allGPUParticleSystems[i]->m_emitters.size(); k++)
		{
			data.m_aliveParticles += m_allGPUParticleSystems[i]->m_emitters[k]->GetDebugData().m_aliveParticles;
		}
	}

	return data;
}

ParticleSystem* ParticlesManager::ChangeParticleSystemType(ParticleSystem* particleSystemToChange)
{
	ParticleSystem* changedSystem = nullptr;
	Vec3 position = particleSystemToChange->GetPosition();
	std::string dataFilepath = particleSystemToChange->m_filepath;
	bool toGPUParticles = !particleSystemToChange->m_gpuParticles;
	KillParticleSystem(particleSystemToChange);

	changedSystem = CreateParticleSystem(dataFilepath.c_str(), position, toGPUParticles);
	return changedSystem;
}

int ParticlesManager::GetBestParticlePoolIndex()
{
	constexpr int minParticlesDiff = 100;
	int indexOfBestPool = 0;
	int highestAvailableParticles = m_particlePools[0]->GetNumAvailableParticles();
	for (int i = 1; i < m_particlePools.size(); i++)
	{
		int availableParticles = m_particlePools[i]->GetNumAvailableParticles();
		if (availableParticles > highestAvailableParticles + minParticlesDiff)
		{
			indexOfBestPool = i;
			highestAvailableParticles = availableParticles;
		}
		else if ((m_allCPUParticleSystems[i]).size() < (m_allCPUParticleSystems[indexOfBestPool]).size())
		{
			indexOfBestPool = i;
			highestAvailableParticles = availableParticles;
		}
	}

	return indexOfBestPool;
}

UpdateParticlesJob::UpdateParticlesJob(ParticleSystemList& particleSystems, float deltaSeconds, const Camera& camera)
	:m_particleSystems(particleSystems), m_deltaSeconds(deltaSeconds), m_camera(camera)
{
}

void UpdateParticlesJob::Execute()
{
	//PROFILE_LOG_SCOPE(UpdateJob)
	{
		for (int i = 0; i < m_particleSystems.size(); i++)
		{
			m_particleSystems[i]->Update(m_deltaSeconds, m_camera);
		}
	}
}

void UpdateParticlesJob::OnFinished()
{
}
