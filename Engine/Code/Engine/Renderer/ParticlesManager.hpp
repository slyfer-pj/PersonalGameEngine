#pragma once
#include <vector>
#include "Engine/Renderer/ParticlePool.hpp"
#include "Engine/Core/Job.hpp"

class ParticleSystem;
class Camera;
class Renderer;
class JobSystem;
class VertexBuffer;
class IndexBuffer;
struct Vec3;

typedef std::vector<ParticleSystem*> ParticleSystemList;

struct ParticlesManagerConfig
{
	int m_maxParticles = 0;
	int m_numPools = 0;
	Renderer* m_renderer = nullptr;
	JobSystem* m_jobSystem = nullptr;
};

struct ParticlesDebugData
{
	int m_maxParticles = 0;
	int m_particlesPerPool = 0;
	int m_numPools = 0;
	int m_aliveParticles = 0;
	int m_numCPUsystems = 0;
	int m_numGPUsystems = 0;
};

class ParticlesManager
{
public:
	ParticlesManager(const ParticlesManagerConfig& config);
	void Startup();
	void Shutdown();
	ParticleSystem* CreateParticleSystem(const char* dataFilepath, const Vec3& position, bool gpuParticles, bool defaultSystem = false);
	void UpdateParticleSystems(float deltaSeconds, const Camera& camera);
	void RenderParticleSystems(const Camera& camera);
	void KillParticleSystem(const ParticleSystem* systemToKill);
	void KillAllParticleSystems();
	ParticlesDebugData GetDebugData();
	ParticleSystem* ChangeParticleSystemType(ParticleSystem* particleSystemToChange);

private:
	ParticlesManagerConfig m_config;
	std::vector<ParticlePool*> m_particlePools;
	ParticleSystemList* m_allCPUParticleSystems = nullptr;
	ParticleSystemList m_allGPUParticleSystems;

private:
	int GetBestParticlePoolIndex();
};

class UpdateParticlesJob : public Job 
{
public:
	UpdateParticlesJob(ParticleSystemList& particleSystems, float deltaSeconds, const Camera& camera);

private:
	std::vector<ParticleSystem*> m_particleSystems;
	float m_deltaSeconds = 0.f;
	const Camera& m_camera;

private:
	void Execute() override;
	void OnFinished() override;
};