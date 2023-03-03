#pragma once
#include <vector>
#include "Engine/Math/Vec3.hpp"

class ParticleEmitter;
class Camera;
class Renderer;
class JobSystem;
struct ParticleEmitterData;
class ParticlePool;
class ParticlesManager;

class ParticleSystem
{
	friend class ParticlesManager;
	friend class UpdateParticlesJob;
private:
	ParticleSystem(ParticlesManager* manager, ParticlePool* particlePool, Renderer* renderer, JobSystem* jobSystem, const Vec3& position, const char* dataFilepath, bool gpuParticles, bool defaultSystem);
	//ParticleSystem(ParticlePool* particlePool, Renderer* renderer, JobSystem* jobSystem, const Vec3& position, const ParticleEmitterData& emitterData, bool gpuParticles);
	~ParticleSystem();
	void Update(float deltaSeconds, const Camera& camera);
	void Render();
	void LoadFromFile(const char* filepath);

public:
	ParticlesManager* m_particlesManager = nullptr;
	std::vector<ParticleEmitter*> m_emitters;
	Renderer* m_renderer = nullptr;
	JobSystem* m_jobSystem = nullptr;
	Vec3 m_position;
	bool m_gpuParticles = false;
	ParticlePool* m_particlePool = nullptr;
	std::string m_filepath;

public:
	Vec3 GetPosition() const;
	void SetPosition(const Vec3& newPosition);
	std::vector<ParticleEmitterData> GetEmitterDataForAllEmitters() const;
	void UpdateEmitterData(const std::vector<ParticleEmitterData>& emitterData);
	void Restart();
	void ToggleDebugMode();
	void DebugGPUUpdateStepNow();
};