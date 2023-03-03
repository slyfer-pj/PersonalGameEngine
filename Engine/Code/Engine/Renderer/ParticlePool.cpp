#include "Engine/Renderer/ParticlePool.hpp"
#include "Engine/Renderer/Particle.hpp"

ParticlePool::ParticlePool(unsigned int numParticles, int id)
	:m_numMaxParticles(numParticles),m_id(id)
{
	m_particleList = new Particle[m_numMaxParticles];
	m_cpuMeshVertexData.reserve(m_numMaxParticles * (size_t)4);
	m_cpuMeshIndexData.reserve(m_numMaxParticles * (size_t)6);
}

ParticlePool::~ParticlePool()
{
	delete[] m_particleList;
	m_particleList = nullptr;

	m_freedParticlesIndex.clear();
}

unsigned int ParticlePool::GetAvailableParticleIndexFromPool()
{
	//returns -1 if there is no empty particles slots left
	unsigned int newParticleIndex = 0xffffffff;
	if (m_freedParticlesIndex.size() > 0)
	{
		newParticleIndex = *(m_freedParticlesIndex.end() - 1);
		m_freedParticlesIndex.pop_back();
	}
	else if (m_nextFreeParticleIndex < m_numMaxParticles)
	{
		newParticleIndex = m_nextFreeParticleIndex;
		m_nextFreeParticleIndex++;
	}

	return newParticleIndex;
}

void ParticlePool::ReturnParticleToPool(unsigned int freedParticleIndex)
{
	m_particleList[freedParticleIndex].Reset();
	m_freedParticlesIndex.push_back(freedParticleIndex);
}

unsigned int ParticlePool::GetNumAvailableParticles() const
{
	return int(m_freedParticlesIndex.size()) + (m_numMaxParticles - m_nextFreeParticleIndex);
}

int ParticlePool::GetID() const
{
	return m_id;
}

Particle* ParticlePool::GetParticleList() const
{
	return m_particleList;
}

unsigned int ParticlePool::GetNumMaxParticles() const
{
	return m_numMaxParticles;
}

unsigned int ParticlePool::GetCurrentListSize()
{
	return m_nextFreeParticleIndex;
}
