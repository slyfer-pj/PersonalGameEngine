#pragma once
#include <vector>
#include "Engine/Core/Vertex_PCU.hpp"

struct Particle;
struct ID3D11Buffer;
struct ID3D11ShaderResourceView;

class ParticlePool
{
	friend class ParticlesManager;
private:
	ParticlePool(unsigned int numParticles, int id);
	~ParticlePool();

private:
	int m_id = 0;
	Particle* m_particleList = nullptr;
	unsigned int m_numMaxParticles = 0u;
	unsigned int m_nextFreeParticleIndex = 0u;
	std::vector<unsigned int> m_freedParticlesIndex;

public:
	unsigned int GetAvailableParticleIndexFromPool();
	void ReturnParticleToPool(unsigned int freedParticleIndex);
	unsigned int GetNumAvailableParticles() const;
	int GetID() const;
	Particle* GetParticleList() const;
	unsigned int GetNumMaxParticles() const;
	ID3D11Buffer* m_particlePoolData = nullptr;
	ID3D11ShaderResourceView* m_particlePoolSRV = nullptr;
	unsigned int GetCurrentListSize();

public:
	std::vector<Vertex_PCU> m_cpuMeshVertexData;
	std::vector<unsigned int> m_cpuMeshIndexData;
	unsigned int m_poolIndexBufferBaseOffset = 0u;
	int m_poolVertexBufferBaseOffset = 0u;
};