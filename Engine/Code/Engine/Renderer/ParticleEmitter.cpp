#include <d3d11.h>
#include <algorithm>
#include "Engine/Renderer/ParticleEmitter.hpp"
#include "Engine/Renderer/Camera.hpp"
#include "Engine/Renderer/Renderer.hpp"
#include "Engine/Renderer/ConstantBuffer.hpp"
#include "Engine/Renderer/Particle.hpp"
#include "Engine/Renderer/SpriteAnimDefinition.hpp"
#include "Engine/Renderer/SpriteSheet.hpp"
#include "Engine/Renderer/ParticleSystem.hpp"
#include "Engine/Renderer/ParticlePool.hpp"
#include "Engine/Renderer/ParticlesManager.hpp"
#include "Engine/Math/RandomNumberGenerator.hpp"
#include "Engine/Math/MathUtils.hpp"
#include "Engine/Core/Vertex_PCU.hpp"
#include "Engine/Core/VertexUtils.hpp"
#include "Engine/Core/XmlUtils.hpp"
#include "Engine/Core/ErrorWarningAssert.hpp"
#include "Engine/Core/JobSystem.hpp"
#include "Engine/Core/Clock.hpp"
#include "Engine/Core/ProfileLogScope.hpp"
#include "Engine/Core/StringUtils.hpp"
#include "Engine/Core/EngineCommon.hpp"

constexpr int CPU_RENDER_CONSTANT_SLOT = 10;
constexpr int GPU_SORT_BUFFER_SLOT = 9;
constexpr int GPU_CONSTANT_BUFFER_SLOT = 8;
constexpr int GPU_ANIMATED_VALUE_BUFFER_SLOT = 7;
constexpr int GPU_SPAWN_PARTICLES_BUFFER_SLOT = 6;
constexpr int GPU_COUNT_BUFFER_SLOT = 5;
constexpr int GPU_BILLBOARD_BUFFER_SLOT = 4;
constexpr int GPU_SYNC_FRAME_COUNT = 60;

//the below constants values NEED to be the same as the constants defined in CommonCS.hlsl
constexpr unsigned int SPAWN_THREAD_COUNT = 128;
constexpr unsigned int SIMULATION_THREAD_COUNT = 128;
constexpr unsigned int FINISH_SIM_THREAD_COUNT = 128;
constexpr unsigned int SORT_THREAD_COUNT = 32;

constexpr unsigned int ANIM_CURVE_BIT_FLAG_VEL_OVERLIFE_X = 1;
constexpr unsigned int ANIM_CURVE_BIT_FLAG_VEL_OVERLIFE_Y = 2;
constexpr unsigned int ANIM_CURVE_BIT_FLAG_VEL_OVERLIFE_Z = 4;
constexpr unsigned int ANIM_CURVE_BIT_FLAG_SIZE_OVERLIFE_X = 8;
constexpr unsigned int ANIM_CURVE_BIT_FLAG_SIZE_OVERLIFE_Y = 16;
constexpr unsigned int ANIM_CURVE_BIT_FLAG_DRAG_OVERLIFE = 32;
constexpr unsigned int ANIM_CURVE_BIT_FLAG_ROT_OVERLIFE = 64;
constexpr unsigned int ANIM_CURVE_BIT_FLAG_COLOR_OVERLIFE = 128;
constexpr unsigned int ANIM_CURVE_BIT_FLAG_ORBIT_VEL_OVERLIFE = 256;
constexpr unsigned int ANIM_CURVE_BIT_FLAG_ORBIT_RADIUS_OVERLIFE = 512;

struct GPUSimConstants
{
	Vec3 gravity;								//0-12 bytes
	float deltaSeconds = 0.f;					//12-16 bytes
	float startColor[4] = { 0.f };				//16-32 bytes
	int gravityScale = 0;						//32-36 bytes
	unsigned int randInt = 0u;					//36-40 bytes
	IntVec2 spriteDimensions;					//40-48 bytes
	Vec4 pointAttractorOffsetAndStrength[8];	//48-176 bytes
	Vec4 orbitAxis;								//176-192 bytes
};

struct AnimatedValuesConstants
{
	Vec4 velOverLifetimeXYKeys[8] = { Vec4(-1.f, -1.f, -1.f, -1.f) };							//0-128 bytes
	Vec4 velOverLifetimeZAndSizeOverLifetimeXKeys[8] = { Vec4(-1.f, -1.f, -1.f, -1.f) };		//128-256 bytes
	Vec4 colorOverLifetimeRGB[8];																//256-384 bytes
	Vec4 colorOverLifetimeAlphaAndSizeOverLifetimeYKeys[8];										//384-512 bytes
	Vec4 dragOverLifetimeRotOverLifetime[8] = { Vec4(-1.f, -1.f, -1.f, -1.f) };					//512-640 bytes
	Vec4 orbitVelAndRadiusOverLifetime[8] = { Vec4(-1.f, -1.f, -1.f, -1.f) };					//640-768 bytes
	unsigned int curveTypeBitFlags = 0;															//768-772 bytes
	Vec3 padding;																				//772-784 bytes
};

struct SpawnDataConstants
{
	Vec2 lifetimeRange;						//0-8 bytes
	Vec2 startSpeedRange;					//8-16 bytes
	Vec2 startSizeRange;					//16-24 bytes
	Vec2 startRotationRange;				//24-32 bytes
	Vec3 offsetFromBase;					//32-44 bytes
	unsigned int shapeType = 0;				//44-48 bytes
	Vec3 boxDimensions;						//48-60 bytes
	unsigned int sphereEmitFrom = 0;		//60-64 bytes
	float sphereShapeRadius = 0.f;			//64-68 bytes
	Vec3 boxForward;						//68-80 bytes
	float coneShapeHalfAngle = 0.f;			//80-84 bytes
	Vec3 worldPosition;						//84-96 bytes
	unsigned int simSpace;					//96-100 bytes
	Vec3 coneForward;						//100-112 bytes
};

struct CountConstants
{
	int emitCount = 0;			//0-4 bytes
	int numParticles = 0;		//4-8 bytes
	Vec2 padding;				//8-16 bytes
};

struct BillboardConstants
{
	Vec4 cameraPosition;		//0-16 bytes
	Vec3 cameraUp;				//16-28 bytes
	unsigned int renderMode;	//28-32 bytes
};

struct SortConstants
{
	//for odd-even sort
	//Vec3 cameraPos;						//0-12 bytes
	//unsigned int iterationNum = 0u;		//12-16 bytes
	//unsigned int numParticles = 0u;		//16-20 bytes
	//IntVec3 padding;						//20-32 bytes

	//for bitonic sort
	unsigned int numItemsToSort = 0;		//0-4 bytes
	IntVec3 padding;						//4-16 bytes
};

struct SortData
{
	float distanceSq = 0.f;
	float index = 0.f;
};

struct CPURenderConstants
{
	IntVec2 spritesheetLayout;												//0-8 bytes
	Vec2 padding;															//8-16 bytes
	Vec4 sizeXYOverLifetime[8] = { Vec4(-1.f, -1.f, -1.f, -1.f) };			//16-144 bytes
	Vec4 orbitRadiusOverLifetime[8] = { Vec4(-1.f, -1.f, -1.f, -1.f) };		//144-276 bytes
	Vec3 orbitAxis;															//276-288 bytes
	unsigned int curveModeFlags = 0;										//288-292 bytes
};

struct IndirectDispatchData
{
	unsigned int threadGroupX = 1U;
	unsigned int threadGroupY = 1U;
	unsigned int threadGroupZ = 1U;
};

struct IndirectDrawData
{
	unsigned int vertexPerInstance = 0U;
	unsigned int instanceCount = 0U;
	unsigned int startVertexLocation = 0U;
	unsigned int startInstanceLocation = 0U;
};

ParticleEmitter::ParticleEmitter(ParticleSystem* parentParticleSystem, const XmlElement* emitterElement)
	:m_particleSystem(parentParticleSystem), m_renderer(m_particleSystem->m_renderer), m_jobSystem(m_particleSystem->m_jobSystem), m_gpuParticles(m_particleSystem->m_gpuParticles)
{
	if (emitterElement)
	{
		m_emitterData.LoadEmitterDataFromElement(*emitterElement);
	}
	else
	{
		m_emitterData.LoadDefaults();
	}
	CreateResources();
}

ParticleEmitter::~ParticleEmitter()
{
	ReleaseResources();
}

void ParticleEmitter::Update(float deltaSeconds, const Camera& camera)
{
	m_frameCount++;
	if (m_restart)
	{
		RestartEmitter();
	}

	SpawnParticle(deltaSeconds, camera);
	UpdateParticles(deltaSeconds, camera);
	UpdateDebugData();
}

void ParticleEmitter::BuildCPUMesh(const Camera& camera)
{
	Vec3 camPos;
	EulerAngles camOrientation;
	camera.GetTransform(camPos, camOrientation);
	Vec3 cameraUp = camera.GetUpVector();
	ParticlePool* pool = m_particleSystem->m_particlePool;
	const Particle* particleList = pool->GetParticleList();
	Vertex_PCU sampleVert;
	m_startIndexLocation = (unsigned int)pool->m_cpuMeshIndexData.size();
	m_startVertexLocation = (int)pool->m_cpuMeshVertexData.size();
	m_numIndices = 0;
	for (int i = 0; i < m_particles.size(); i++)
	{
		unsigned int particleIndex = m_particles[i];
		AddVertsForParticle(pool->m_cpuMeshVertexData, pool->m_cpuMeshIndexData, sampleVert, particleList[particleIndex], camPos, cameraUp);
	}
}

void ParticleEmitter::SpawnParticle(float deltaSeconds, const Camera& camera)
{
	//calculate num particles to emit
	int particlesToEmit = 0;
	if (m_emitterData.m_emissionMode == EmissionMode::CONSTANT)
	{
		float particlesToEmitThisFrame = m_emitterData.m_particlesEmittedPerSecond * deltaSeconds;
		particlesToEmit = RoundDownToInt(particlesToEmitThisFrame);
		float partialParticle = particlesToEmitThisFrame - (float)particlesToEmit;
		m_partialParticle += partialParticle;
		if (m_partialParticle > 1.f)
		{
			particlesToEmit++;
			m_partialParticle--;
		}
	}
	else
	{
		m_burstIntervalTimer += deltaSeconds;
		if (m_burstIntervalTimer > m_emitterData.m_burstInterval)
		{
			particlesToEmit = static_cast<int>(m_emitterData.m_numBurstParticles);
			m_burstIntervalTimer = 0.f;
		}
	}

	if (m_gpuParticles)
		SpawnParticlesOnGPU(particlesToEmit, deltaSeconds, camera);
	else
		SpawnParticlesOnCPU(particlesToEmit, deltaSeconds);
}

void ParticleEmitter::SpawnParticlesOnCPU(unsigned int particlesToEmit, float deltaSeconds)
{
	UNUSED(deltaSeconds);
	RandomNumberGenerator rng;

	int slotsLeft = m_emitterData.m_maxParticles - int(m_particles.size());
	particlesToEmit = slotsLeft < int(particlesToEmit) ? slotsLeft : particlesToEmit;

	//emit particles and set initial values
	for (unsigned int i = 0; i < particlesToEmit; i++)
	{
		unsigned int newParticleIndex = m_particleSystem->m_particlePool->GetAvailableParticleIndexFromPool();
		if (newParticleIndex != 0xffffffff)
		{
			Particle* particleList = m_particleSystem->m_particlePool->GetParticleList();
			Particle& particle = particleList[newParticleIndex];
			particle.m_position = m_emitterData.m_offsetFromWorldPos;
			if (m_emitterData.m_simulationSpace == SimulationSpace::WORLD)
			{
				particle.m_position += m_particleSystem->m_position;
			}
			m_emitterData.m_shape->GenerateParticle(particle);
			particle.m_particleID = m_currentParticleId++;
			particle.m_velocity *= rng.GetRandomFloatInRange(m_emitterData.m_startSpeed.m_min, m_emitterData.m_startSpeed.m_max);
			particle.m_rotation = rng.GetRandomFloatInRange(m_emitterData.m_startRotationDegrees.m_min, m_emitterData.m_startRotationDegrees.m_max);
			particle.m_lifeTime = rng.GetRandomFloatInRange(m_emitterData.m_particleLifetime.m_min, m_emitterData.m_particleLifetime.m_max);
			particle.m_size = rng.GetRandomFloatInRange(m_emitterData.m_startSize.m_min, m_emitterData.m_startSize.m_max);
			particle.m_orbitalAngle = rng.GetRandomFloatInRange(0.f, 360.f);
			m_emitterData.m_startColor.GetAsFloats(particle.m_color);
			m_particles.push_back(newParticleIndex);

			m_debugData.m_numParticlesSpawned++;
		}
	}
}

void ParticleEmitter::SpawnParticlesOnGPU(unsigned int particlesToEmit, float deltaSeconds, const Camera& camera)
{
	if (m_debugMode && !m_debugStepNow)
		return;

	//frame capture ends on Render()
	if (m_debugMode)
	{
		m_renderer->StartRenderDocFrameCapture();
	}

	UpdateAndBindGPUSimConstants(deltaSeconds, camera);

#if defined(_DEBUG)
	unsigned int deadListCount = 0;
	m_renderer->CopyGPUStructCount(m_particleCount, 0, m_deadParticleListUAV);
	m_renderer->CopyGPUToCPU(&deadListCount, sizeof(int), m_particleCount);
	GUARANTEE_RECOVERABLE(deadListCount == m_currentCountBuffer.deadListSize, "Mismatch in dead particle list count");
	/*if (deadListCount != m_currentCountBuffer.deadListSize)
	{
		m_renderer->StartRenderDocFrameCapture();
	}*/
#endif

	int slotsRemaining = (m_currentCountBuffer.maxParticles - m_currentCountBuffer.lastAliveParticleIndex) + m_currentCountBuffer.deadListSize;
	GUARANTEE_OR_DIE(slotsRemaining >= 0, "Slots can't be negative");
	if (particlesToEmit > (unsigned int)slotsRemaining)
		particlesToEmit = slotsRemaining;

	//store particle count in constant buffer
	CountConstants constant;
	constant.emitCount = particlesToEmit;
	constant.numParticles = m_currentTotalSpawnedParticles;
	m_renderer->CopyCPUToGPU(&constant, sizeof(CountConstants), m_gpuCountCBO);
	m_renderer->BindConstantBufferToComputeShader(GPU_COUNT_BUFFER_SLOT, m_gpuCountCBO);
	m_currentTotalSpawnedParticles += particlesToEmit;

	m_renderer->BindComputeShaderUAV(m_nullUAV, 0);

	//bind shader and resources
	m_renderer->BindComputeShader(m_spawnCS);
	m_renderer->BindComputeShaderUAV(m_particleDataUAV, 0);
	m_renderer->BindComputeShaderUAV(m_deadParticleListUAV, 1);
	m_renderer->BindComputeShaderUAV(m_countBufferUAV, 2);

	//dispatch compute 
	unsigned int spawnThreadGroupCount = static_cast<unsigned int>(ceilf((float)particlesToEmit / (float)SPAWN_THREAD_COUNT));
	spawnThreadGroupCount = Clamp(spawnThreadGroupCount, 1U, UINT32_MAX);
	m_renderer->DispatchComputeShader(spawnThreadGroupCount, 1, 1);

	m_renderer->BindComputeShader(nullptr);
	m_renderer->BindComputeShaderUAV(m_nullUAV, 0);
	m_renderer->BindComputeShaderUAV(m_nullUAV, 1);
	m_renderer->BindComputeShaderUAV(m_nullUAV, 2);

	m_debugData.m_numParticlesSpawned += particlesToEmit;
}

void ParticleEmitter::UpdateStatsFromGPU()
{
	m_debugData.m_aliveParticles = m_currentCountBuffer.listSize - m_currentCountBuffer.deadListSize;
	m_debugData.m_deadParticles = m_debugData.m_numParticlesSpawned - m_debugData.m_aliveParticles;
}

void ParticleEmitter::ReleaseResources()
{
	m_debugData = ParticleEmitterDebugData();
	m_partialParticle = 0.f;
	m_aliveParticles = 0;
	m_burstIntervalTimer = 99999.f;
	m_currentParticleId = 0;

	for (int i = 0; i < m_particles.size(); i++)
	{
		if (m_particles[i])
		{
			m_particleSystem->m_particlePool->ReturnParticleToPool(m_particles[i]);
		}
	}
	m_particles.clear();

	//release buffers
	DX_SAFE_RELEASE(m_particleVertexDataVSCopy);
	DX_SAFE_RELEASE(m_particleCount);
	DX_SAFE_RELEASE(m_sortedParticlesList);
	DX_SAFE_RELEASE(m_particleData);
	DX_SAFE_RELEASE(m_deadParticleIndexList);
	DX_SAFE_RELEASE(m_afterSpawnAliveParticleList);
	DX_SAFE_RELEASE(m_afterSimAliveParticleList);
	DX_SAFE_RELEASE(m_countBuffer);
	DX_SAFE_RELEASE(m_stagingCountBuffer);
	DX_SAFE_RELEASE(m_cpuParticleIndicies);
	DX_SAFE_RELEASE(m_dataToSort);

	//release UAVs and SRVs
	DX_SAFE_RELEASE(m_particleDataVSSRV);
	DX_SAFE_RELEASE(m_sortedParticleListUAV);
	DX_SAFE_RELEASE(m_particleDataUAV);
	DX_SAFE_RELEASE(m_deadParticleListUAV);
	DX_SAFE_RELEASE(m_afterSpawnAliveParticleListUAV);
	DX_SAFE_RELEASE(m_afterSimAliveParticleListUAV);
	DX_SAFE_RELEASE(m_countBufferUAV);
	DX_SAFE_RELEASE(m_cpuParticleIndexSRV);
	DX_SAFE_RELEASE(m_dataToSortUAV);
	DX_SAFE_RELEASE(m_sortedDataSRV);

	//release compute shaders
	DX_SAFE_RELEASE(m_setupSimCS);
	DX_SAFE_RELEASE(m_spawnCS);
	DX_SAFE_RELEASE(m_simulateCS);
	DX_SAFE_RELEASE(m_finishSimCS);
	DX_SAFE_RELEASE(m_sortCS);

	delete m_gpuSimCBO;
	m_gpuSimCBO = nullptr;
	delete m_gpuAnimatedValuesCBO;
	m_gpuAnimatedValuesCBO = nullptr;
	delete m_gpuSpawnParticlesCBO;
	m_gpuSpawnParticlesCBO = nullptr;
	delete m_gpuCountCBO;
	m_gpuCountCBO = nullptr;
	delete m_gpuBillboardCBO;
	m_gpuBillboardCBO = nullptr;
	delete m_gpuSortCBO;
	m_gpuSortCBO = nullptr;
	delete m_cpuParticlesCBO;
	m_cpuParticlesCBO = nullptr;
}

void ParticleEmitter::CreateResources()
{
	m_particleTexture = m_renderer->CreateOrGetTextureFromFile(m_emitterData.m_textureFilepath.c_str());

	if (m_gpuParticles)
	{
		InitializeGPUSimulationResource();
		InitializeGPUSortResources();

		//initialize this count at the start, since this buffer will only get updated after the first iteration
		//this creates a bug where the max particles on the first iteration will be 0
		m_currentCountBuffer.maxParticles = m_emitterData.m_maxParticles;
	}
	else
	{
		m_cpuParticleShader =  m_renderer->CreateOrGetShader("Data/Shaders/CPUParticles");
		m_renderer->CreateD3DUnorderedAccessBuffer(&m_cpuParticleIndicies, sizeof(unsigned int) * m_emitterData.m_maxParticles, sizeof(unsigned int), nullptr, true);
		m_renderer->CreateShaderResourceView(&m_cpuParticleIndexSRV, m_cpuParticleIndicies);
		m_gpuBillboardCBO = m_renderer->CreateConstantBuffer(sizeof(BillboardConstants));
		m_cpuParticlesCBO = m_renderer->CreateConstantBuffer(sizeof(CPURenderConstants));
		m_particles.reserve(m_emitterData.m_maxParticles);
	}
}

void ParticleEmitter::RestartEmitter()
{
	ReleaseResources();
	CreateResources();

	m_restart = false;
}

void ParticleEmitter::SortParticlesBasedOnDistanceFromCamera(int minParticleListIndex, int maxParticleListIndex)
{
	//recursive quick sort algorithm
	if (minParticleListIndex < maxParticleListIndex)
	{
		//get a pivot index to divide the list and then sort each of the divided list individually
		int pivotElementIndex = PartitionParticleListForSort(minParticleListIndex, maxParticleListIndex);
		SortParticlesBasedOnDistanceFromCamera(minParticleListIndex, pivotElementIndex - 1);
		SortParticlesBasedOnDistanceFromCamera(pivotElementIndex + 1, maxParticleListIndex);
	}
}

int ParticleEmitter::PartitionParticleListForSort(int minParticleListIndex, int maxParticleListIndex)
{
	//assume the first element in the range is the farthest away
	int farthestParticleIndex = minParticleListIndex;
	//pick a pivot element (choosing the last in the range here) to sort the elements against
	int pivotElementIndex = maxParticleListIndex;

	float distanceToPivotParticleFromCam = m_particleDistFromCameraForSort[pivotElementIndex];

	//go from min range to max range - 1 (pivot is max range element)
	for (int i = minParticleListIndex; i <= maxParticleListIndex - 1; i++)
	{
		float particleDistFromCam = m_particleDistFromCameraForSort[i];

		//if i'th element value is greater than value of the value of pivot, swap this element with the current farthest element
		if (particleDistFromCam > distanceToPivotParticleFromCam)
		{
			std::swap(m_particles[farthestParticleIndex], m_particles[i]);
			std::swap(m_particleDistFromCameraForSort[farthestParticleIndex], m_particleDistFromCameraForSort[i]);
			farthestParticleIndex++;
		}
	}

	//now swap the next element to farthest element index with pivot ensuring that all element before pivot are farther than the pivot
	std::swap(m_particles[farthestParticleIndex], m_particles[pivotElementIndex]);
	std::swap(m_particleDistFromCameraForSort[farthestParticleIndex], m_particleDistFromCameraForSort[pivotElementIndex]);

	return farthestParticleIndex;
}

void ParticleEmitter::RunBitonicSort()
{
	SortConstants sortCB;
	sortCB.numItemsToSort = m_currentCountBuffer.listSize;
	m_renderer->CopyCPUToGPU(&sortCB, sizeof(SortConstants), m_gpuSortCBO);

	m_bitonicSort.run(m_emitterData.m_maxParticles, m_dataToSortUAV, m_gpuSortCBO->m_buffer);
}

//void ParticleEmitter::RunOddEvenSort()
//{
//	//bind resources and dispatch sort stage cs
//	SortConstants constants;
//	constants.cameraPos = camera.GetPosition();
//	constants.numParticles = m_currentCountBuffer.listSize;
//
//	m_renderer->BindConstantBufferToComputeShader(GPU_SORT_BUFFER_SLOT, m_gpuSortCBO);
//	m_renderer->BindComputeShader(m_sortCS);
//	m_renderer->BindComputeShaderSRV(m_particleDataVSSRV, 0);
//	m_renderer->BindComputeShaderUAV(m_sortedParticleListUAV, 0);
//
//	unsigned int sortGroupCount = static_cast<unsigned int>(ceilf((float)m_currentCountBuffer.listSize / (float)SORT_THREAD_COUNT));
//	for (unsigned int i = 0; i < m_currentCountBuffer.listSize; i++)
//	{
//		constants.iterationNum = i;
//		m_renderer->CopyCPUToGPU(&constants, sizeof(SortConstants), m_gpuSortCBO);
//		m_renderer->DispatchComputeShader(sortGroupCount, 1, 1);
//		m_renderer->CopyOverResourcesInGPU(m_particleVertexDataVSCopy, m_sortedParticlesList);
//	}
//}

void ParticleEmitter::RenderGPUParticles() const
{
	m_renderer->BindModelConstants();
	m_renderer->BindShader(m_gpuParticleShader);
	m_renderer->BindConstantBuffer(GPU_CONSTANT_BUFFER_SLOT, m_gpuSimCBO);
	m_renderer->BindConstantBuffer(GPU_ANIMATED_VALUE_BUFFER_SLOT, m_gpuAnimatedValuesCBO);
	m_renderer->BindConstantBuffer(GPU_BILLBOARD_BUFFER_SLOT, m_gpuBillboardCBO);
	m_renderer->GetDeviceContext()->IASetInputLayout(nullptr);
	m_renderer->GetDeviceContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	ID3D11ShaderResourceView* srvToBind[] = { m_particleDataVSSRV, m_sortedDataSRV };
	m_renderer->GetDeviceContext()->VSSetShaderResources(1, 2, srvToBind);
	m_renderer->BindTexture(m_particleTexture, 0, true);

	m_renderer->GetDeviceContext()->Draw(m_currentCountBuffer.listSize * 6, 0);
	m_renderer->BindShaderByName("Default");

	//frame capture began in SpawnParticlesOnGPU()
	if (m_debugMode)
	{
		m_renderer->EndRenderDocFrameCapture();
	}
}

void ParticleEmitter::RenderCPUParticles() const
{
	m_renderer->CopyCPUToGPU(m_particles.data(), sizeof(m_particles[0]) * m_particles.size(), m_cpuParticleIndicies);
	UpdateAndBindCPURenderConstants();
	m_renderer->BindModelConstants();
	m_renderer->BindShader(m_cpuParticleShader);
	m_renderer->BindConstantBuffer(CPU_RENDER_CONSTANT_SLOT, m_cpuParticlesCBO);
	m_renderer->BindConstantBuffer(GPU_BILLBOARD_BUFFER_SLOT, m_gpuBillboardCBO);
	m_renderer->GetDeviceContext()->IASetInputLayout(nullptr);
	m_renderer->GetDeviceContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	m_renderer->GetDeviceContext()->VSSetShaderResources(1, 1, &(m_particleSystem->m_particlePool->m_particlePoolSRV));
	m_renderer->GetDeviceContext()->VSSetShaderResources(2, 1, &m_cpuParticleIndexSRV);
	m_renderer->BindTexture(m_particleTexture, 0, true);
	m_renderer->GetDeviceContext()->Draw((int)m_particles.size() * 6, 0);
	m_renderer->BindShaderByName("Default");
}

void ParticleEmitter::InitializeGPUSortResources()
{
	m_bitonicSort.init(m_renderer);

	m_renderer->CreateD3DUnorderedAccessBuffer(&m_dataToSort, sizeof(SortData) * m_emitterData.m_maxParticles, sizeof(SortData), nullptr);
	m_renderer->CreateUnorderedAccessView(&m_dataToSortUAV, m_dataToSort, UAVType::REGULAR);
	m_renderer->CreateShaderResourceView(&m_sortedDataSRV, m_dataToSort);

	m_gpuSortCBO = m_renderer->CreateConstantBuffer(sizeof(SortConstants));
}

void ParticleEmitter::UpdateParticles(float deltaSeconds, const Camera& camera)
{
	if (m_gpuParticles)
		UpdateOnGPU(deltaSeconds, camera);
	else
		UpdateOnCPU(deltaSeconds, camera);
}

void ParticleEmitter::UpdateOnCPU(float deltaSeconds, const Camera& camera)
{
	Particle* particleList = m_particleSystem->m_particlePool->GetParticleList();
	if (m_emitterData.m_sortParticles)
	{
		m_particleDistFromCameraForSort.reserve(m_particles.size());
		m_particleDistFromCameraForSort.clear();
	}
	Vec3 camPos = camera.GetPosition();

	for (int i = 0; i < m_particles.size();)
	{
		unsigned int particleIndex = m_particles[i];
		Particle& particle = particleList[particleIndex];
		particle.Update(deltaSeconds, m_emitterData);
		if (!particle.IsAlive())
		{
			RemoveParticleFromList(i);
		}
		else
		{
			//when an element dies, we swap that element with the last element. so we increment i only when a particle doesn't die
			//and hence the new element in the dead place can be updated
			i++;

			if (m_emitterData.m_sortParticles)
			{
				float distSqFromCam = GetDistanceSquared3D(camPos, particleList[particleIndex].m_position);
				m_particleDistFromCameraForSort.push_back(distSqFromCam);
			}
		}
	}

	if (m_emitterData.m_sortParticles)
	{
		SortParticlesBasedOnDistanceFromCamera(0, int(m_particles.size() - 1));
	}

	m_currentFrameCamera = camera;
	m_debugData.m_aliveParticles = int(m_particles.size());
}

void ParticleEmitter::UpdateOnGPU(float deltaSeconds, const Camera& camera)
{
	UNUSED(camera);
	UNUSED(deltaSeconds);

	if (m_debugMode && !m_debugStepNow)
		return;

	//get total num of particles in list after spawn stage
	CountBuffer countBuffer;
	m_renderer->CopyOverResourcesInGPU(m_stagingCountBuffer, m_countBuffer);
	m_renderer->CopyGPUToCPU(&countBuffer, sizeof(CountBuffer), m_stagingCountBuffer);

	//bind resources and dispatch simulate cs
	m_renderer->BindComputeShader(m_simulateCS);
	m_renderer->BindComputeShaderUAV(m_particleDataUAV, 0);
	m_renderer->BindComputeShaderUAV(m_deadParticleListUAV, 1);
	m_renderer->BindComputeShaderUAV(m_countBufferUAV, 2);
	m_renderer->BindComputeShaderUAV(m_dataToSortUAV, 3);
	unsigned int simThreadGroupCount = static_cast<unsigned int>(ceilf((float)countBuffer.listSize / (float)SIMULATION_THREAD_COUNT));
	simThreadGroupCount = Clamp(simThreadGroupCount, 1U, UINT32_MAX);
	m_renderer->DispatchComputeShader(simThreadGroupCount, 1, 1);

	//clear uavs
	m_renderer->BindComputeShaderUAV(m_nullUAV, 0);
	m_renderer->BindComputeShaderUAV(m_nullUAV, 1);
	m_renderer->BindComputeShaderUAV(m_nullUAV, 2);
	m_renderer->BindComputeShaderUAV(m_nullUAV, 3);

	m_renderer->CopyOverResourcesInGPU(m_particleVertexDataVSCopy, m_particleData);

	//copy count buffer data for use in render and sort
	m_renderer->CopyOverResourcesInGPU(m_stagingCountBuffer, m_countBuffer);
	m_renderer->CopyGPUToCPU(&m_currentCountBuffer, sizeof(CountBuffer), m_stagingCountBuffer);

	//run sort
	if (m_emitterData.m_sortParticles)
	{
		RunBitonicSort();
	}

	//clear UAVs
	m_renderer->BindComputeShaderUAV(m_nullUAV, 0);
	m_debugStepNow = false;
}

void ParticleEmitter::AddVertsForParticle(std::vector<Vertex_PCU>& cpuMeshVertices, std::vector<unsigned int>& cpuMeshIndices, Vertex_PCU sampleVert, const Particle& particle, const Vec3& cameraPos, const Vec3& cameraUp)
{
	Vec3 upVector, rightVector, forwardVector;
	//get world position of particle such that we can subtract it from world cam position
	if (m_emitterData.m_renderMode == RenderMode::BILLBOARD)
	{
		Vec3 particleWorldPos = m_emitterData.m_simulationSpace == SimulationSpace::LOCAL ? GetModelMatrix().TransformPosition3D(particle.m_position) :
			particle.m_position;
		forwardVector = (cameraPos - particleWorldPos).GetNormalized();
		upVector = cameraUp;
		rightVector = CrossProduct3D(upVector, forwardVector).GetNormalized();
	}
	else
	{
		//horizontal billboard means the quad is parallel to the XY plane
		forwardVector = Vec3(0.f, 0.f, 1.f); //world Z
		upVector = Vec3(0.f, 1.f, 0.f); //world Y
		rightVector = CrossProduct3D(upVector, forwardVector);
	}
	
	RandomNumberGenerator rng;
	float sizeScaleX = m_emitterData.m_sizeOverLifetimeX.GetInterpolatedAnimValueFromCurve(particle.GetNormalizedAge(), rng, particle.m_particleID);
	float sizeScaleY = m_emitterData.m_sizeOverLifetimeY.GetInterpolatedAnimValueFromCurve(particle.GetNormalizedAge(), rng, particle.m_particleID);

	float particleQuadXHalfSize = particle.m_size * 0.5f * sizeScaleX * m_emitterData.m_sizeOverLifeXModifier;
	float particleQuadYHalfSize = particle.m_size * 0.5f * sizeScaleY * m_emitterData.m_sizeOverLifeYModifier;

	Mat44 rotationMatrix = Mat44::CreateRotationDegreesAboutAxis(forwardVector, particle.m_rotation);
	upVector = rotationMatrix.TransformVectorQuantity3D(upVector);
	rightVector = rotationMatrix.TransformVectorQuantity3D(rightVector);

	//draw the quad using up and right vector which have been gotten for the purpose of camera facing billboarding
	Vec3 topLeft = particle.m_position + upVector * particleQuadYHalfSize + (-rightVector) * particleQuadXHalfSize;
	Vec3 botLeft = particle.m_position + (-upVector) * particleQuadYHalfSize + (-rightVector) * particleQuadXHalfSize;
	Vec3 botRight = particle.m_position + (-upVector) * particleQuadYHalfSize + rightVector * particleQuadXHalfSize;
	Vec3 topRight = particle.m_position + upVector * particleQuadYHalfSize + rightVector * particleQuadXHalfSize;

	AABB2 uv = AABB2::ZERO_TO_ONE;
	if (m_emitterData.m_isSpriteSheetTexture)
	{
		SpriteSheet spriteSheet = SpriteSheet(*m_particleTexture, m_emitterData.m_spriteSheetGridLayout);
		SpriteAnimDefinition animDef = SpriteAnimDefinition(spriteSheet, 0, (m_emitterData.m_spriteSheetGridLayout.x * m_emitterData.m_spriteSheetGridLayout.y) - 1,
			particle.m_lifeTime, SpriteAnimPlaybackType::ONCE);
		SpriteDefinition spriteDef = animDef.GetSpriteDefAtTime(particle.m_age);
		uv = spriteDef.GetUVs();
	}

	Rgba8 particleColor;
	particleColor.SetFromFloats(particle.m_color);
	AddVertsForQuad3D(cpuMeshVertices, cpuMeshIndices, sampleVert, topLeft, botLeft, botRight, topRight, particleColor, uv);
	m_numIndices += 6;
}

void ParticleEmitter::Render() const
{
	if (m_emitterData.m_stopRender)
		return;

	m_renderer->SetModelMatrix(m_emitterData.m_simulationSpace == SimulationSpace::LOCAL ? GetModelMatrix() : Mat44::IDENTITY);
	m_renderer->SetModelColor(m_emitterData.m_startColor);
	m_renderer->SetDepthStencilState(DepthTest::LESSEQUAL, false);
	m_renderer->SetBlendMode(m_emitterData.m_blendMode);

	if (m_gpuParticles)
	{
		RenderGPUParticles();
	}
	else
	{
		RenderCPUParticles();
	}
}

void ParticleEmitter::Restart()
{
	m_restart = true;
}

void ParticleEmitter::ChangeEmitterType(bool gpuEmitter)
{
	m_gpuParticles = gpuEmitter;
}

void ParticleEmitter::SetJobSystem(JobSystem* jobSystem)
{
	m_jobSystem = jobSystem;
}

void ParticleEmitter::InitializeGPUSimulationResource()
{
	//create all required buffers for the gpu pipeline
	m_renderer->CreateD3DUnorderedAccessBuffer(&m_particleVertexDataVSCopy, sizeof(Particle) * m_emitterData.m_maxParticles, sizeof(Particle), nullptr);
	m_renderer->CreateD3DStagingBuffer(&m_particleCount, sizeof(int), sizeof(int));
	m_renderer->CreateD3DUnorderedAccessBuffer(&m_sortedParticlesList, sizeof(Particle) * m_emitterData.m_maxParticles, sizeof(Particle), nullptr);

	//create all views for the buffers
	m_renderer->CreateShaderResourceView(&m_particleDataVSSRV, m_particleVertexDataVSCopy);
	m_renderer->CreateUnorderedAccessView(&m_sortedParticleListUAV, m_sortedParticlesList, UAVType::REGULAR);

	//new approach buffers
	m_renderer->CreateD3DUnorderedAccessBuffer(&m_particleData, sizeof(Particle) * m_emitterData.m_maxParticles, sizeof(Particle), nullptr);
	m_renderer->CreateD3DUnorderedAccessBuffer(&m_deadParticleIndexList, sizeof(unsigned int) * m_emitterData.m_maxParticles, sizeof(unsigned int), nullptr);
	m_renderer->CreateD3DUnorderedAccessBuffer(&m_afterSpawnAliveParticleList, sizeof(unsigned int) * m_emitterData.m_maxParticles, sizeof(unsigned int), nullptr);
	m_renderer->CreateD3DUnorderedAccessBuffer(&m_afterSimAliveParticleList, sizeof(unsigned int) * m_emitterData.m_maxParticles, sizeof(unsigned int), nullptr);
	CountBuffer countBuffer = { 0 };
	countBuffer.maxParticles = m_emitterData.m_maxParticles;
	m_renderer->CreateD3DUnorderedAccessBuffer(&m_countBuffer, sizeof(CountBuffer), sizeof(CountBuffer), &countBuffer);
	m_renderer->CreateD3DStagingBuffer(&m_stagingCountBuffer, sizeof(CountBuffer), sizeof(CountBuffer));

	m_renderer->CreateUnorderedAccessView(&m_particleDataUAV, m_particleData, UAVType::REGULAR);
	m_renderer->CreateUnorderedAccessView(&m_deadParticleListUAV, m_deadParticleIndexList, UAVType::APPEND);
	m_renderer->CreateUnorderedAccessView(&m_afterSpawnAliveParticleListUAV, m_afterSpawnAliveParticleList, UAVType::REGULAR);
	m_renderer->CreateUnorderedAccessView(&m_afterSimAliveParticleListUAV, m_afterSimAliveParticleList, UAVType::REGULAR);
	m_renderer->CreateUnorderedAccessView(&m_countBufferUAV, m_countBuffer, UAVType::REGULAR);

	m_renderer->BindComputeShaderUAV(m_deadParticleListUAV, 0, 0);

	//create and compile compute shaders
	m_renderer->CreateAndCompileComputeShader("Data/Shaders/SetupSimCS.hlsl", &m_setupSimCS);
	m_renderer->CreateAndCompileComputeShader("Data/Shaders/SpawnCS.hlsl", &m_spawnCS);
	m_renderer->CreateAndCompileComputeShader("Data/Shaders/SimulateCS.hlsl", &m_simulateCS);
	m_renderer->CreateAndCompileComputeShader("Data/Shaders/OddEvenSortCS.hlsl", &m_sortCS);
	m_gpuParticleShader = m_renderer->CreateOrGetShader("Data/Shaders/Particles");

	//create constant buffer
	m_gpuSimCBO = m_renderer->CreateConstantBuffer(sizeof(GPUSimConstants));
	m_gpuAnimatedValuesCBO = m_renderer->CreateConstantBuffer(sizeof(AnimatedValuesConstants));
	m_gpuSpawnParticlesCBO = m_renderer->CreateConstantBuffer(sizeof(SpawnDataConstants));
	m_gpuCountCBO = m_renderer->CreateConstantBuffer(sizeof(CountConstants));
	m_gpuBillboardCBO = m_renderer->CreateConstantBuffer(sizeof(BillboardConstants));
}

void ParticleEmitter::UpdateAndBindGPUSimConstants(float deltaSeconds, const Camera& camera)
{
	//update and bind spawn constants
	SpawnDataConstants spawnConstants;
	spawnConstants.lifetimeRange = Vec2(m_emitterData.m_particleLifetime.m_min, m_emitterData.m_particleLifetime.m_max);
	spawnConstants.startSpeedRange = Vec2(m_emitterData.m_startSpeed.m_min, m_emitterData.m_startSpeed.m_max);
	spawnConstants.startSizeRange = Vec2(m_emitterData.m_startSize.m_min, m_emitterData.m_startSize.m_max);
	spawnConstants.startRotationRange = Vec2(m_emitterData.m_startRotationDegrees.m_min, m_emitterData.m_startRotationDegrees.m_max);
	spawnConstants.offsetFromBase = Vec3(m_emitterData.m_offsetFromWorldPos);
	spawnConstants.worldPosition = m_particleSystem->m_position;
	spawnConstants.simSpace = static_cast<unsigned int>(m_emitterData.m_simulationSpace);
	spawnConstants.shapeType = m_emitterData.m_shape->GetShapeTypeNum();
	ConeEmitter* cone = dynamic_cast<ConeEmitter*>(m_emitterData.m_shape);
	if (cone)
	{
		spawnConstants.coneShapeHalfAngle = cone->m_coneHalfAngle;
		spawnConstants.coneForward = Vec3(cone->m_coneForward.x < 0.00001f ? 0.00001f : cone->m_coneForward.x, cone->m_coneForward.y, cone->m_coneForward.z);
	}
	SphereEmitter* sphere = dynamic_cast<SphereEmitter*>(m_emitterData.m_shape);
	if (sphere)
	{
		spawnConstants.sphereShapeRadius = sphere->m_sphereRadius;
		spawnConstants.sphereEmitFrom = sphere->m_fromSurface ? 1 : 0;
	}
	BoxEmitter* box = dynamic_cast<BoxEmitter*>(m_emitterData.m_shape);
	if (box)
	{
		spawnConstants.boxDimensions = box->m_dimensions;
		spawnConstants.boxForward = box->m_forward;
	}
	m_renderer->CopyCPUToGPU(&spawnConstants, sizeof(SpawnDataConstants), m_gpuSpawnParticlesCBO);
	m_renderer->BindConstantBufferToComputeShader(GPU_SPAWN_PARTICLES_BUFFER_SLOT, m_gpuSpawnParticlesCBO);

	//simulation constant data
	RandomNumberGenerator rng;
	GPUSimConstants simConstants;
	simConstants.gravity = Vec3(0.f, 0.f, -10.f);
	simConstants.deltaSeconds = deltaSeconds;
	m_emitterData.m_startColor.GetAsFloats(simConstants.startColor);
	simConstants.gravityScale = m_emitterData.m_gravityScale;
	//simConstants.randInt = m_frameCount;
	simConstants.randInt = rng.GetRandomIntInRange(0, 100000000, m_frameCount);
	simConstants.spriteDimensions = m_emitterData.m_isSpriteSheetTexture ? m_emitterData.m_spriteSheetGridLayout : IntVec2::ONE;
	for (int i = 0; i < m_emitterData.m_pointAttractors.size(); i++)
	{
		PointAttractor attractor = m_emitterData.m_pointAttractors[i];
		simConstants.pointAttractorOffsetAndStrength[i] = Vec4(attractor.m_offsetFromEmitter.x, attractor.m_offsetFromEmitter.y,
			attractor.m_offsetFromEmitter.z, attractor.m_strength);
	}
	simConstants.orbitAxis = Vec4(m_emitterData.m_orbitalVelocityAxis, 0.f);
	m_renderer->CopyCPUToGPU(&simConstants, sizeof(GPUSimConstants), m_gpuSimCBO);
	m_renderer->BindConstantBufferToComputeShader(GPU_CONSTANT_BUFFER_SLOT, m_gpuSimCBO);

	//animated properties constant data
	AnimatedValuesConstants animValueConstants;

	//encode all "over lifetime" properties in the constant buffer
	//velOverLifetimeXYKeys = vec4(velX value, velX time, velY value, velY time)
	//velOverLifetimeZAndSizeOverLifetimeKeys = vec4(velZ value, velZ time, sizeX value, sizeX time);

	//velocity over life values
	CopyCurveDataToAnimConstants(m_emitterData.m_velocityOverLifetime_X, animValueConstants.velOverLifetimeXYKeys, 0,
		animValueConstants.curveTypeBitFlags, ANIM_CURVE_BIT_FLAG_VEL_OVERLIFE_X, m_emitterData.m_volSpeedModifier);
	CopyCurveDataToAnimConstants(m_emitterData.m_velocityOverLifetime_Y, animValueConstants.velOverLifetimeXYKeys, 2,
		animValueConstants.curveTypeBitFlags, ANIM_CURVE_BIT_FLAG_VEL_OVERLIFE_Y, m_emitterData.m_volSpeedModifier);
	CopyCurveDataToAnimConstants(m_emitterData.m_velocityOverLifetime_Z, animValueConstants.velOverLifetimeZAndSizeOverLifetimeXKeys, 0,
		animValueConstants.curveTypeBitFlags, ANIM_CURVE_BIT_FLAG_VEL_OVERLIFE_Z, m_emitterData.m_volSpeedModifier);

	//drag over life values
	CopyCurveDataToAnimConstants(m_emitterData.m_dragOverLifetime, animValueConstants.dragOverLifetimeRotOverLifetime, 0,
		animValueConstants.curveTypeBitFlags, ANIM_CURVE_BIT_FLAG_DRAG_OVERLIFE, m_emitterData.m_dragModifier);

	//size over life values
	CopyCurveDataToAnimConstants(m_emitterData.m_sizeOverLifetimeX, animValueConstants.velOverLifetimeZAndSizeOverLifetimeXKeys, 2,
		animValueConstants.curveTypeBitFlags, ANIM_CURVE_BIT_FLAG_SIZE_OVERLIFE_X, m_emitterData.m_sizeOverLifeXModifier);
	CopyCurveDataToAnimConstants(m_emitterData.m_sizeOverLifetimeY, animValueConstants.colorOverLifetimeAlphaAndSizeOverLifetimeYKeys,  2,
		animValueConstants.curveTypeBitFlags, ANIM_CURVE_BIT_FLAG_SIZE_OVERLIFE_Y, m_emitterData.m_sizeOverLifeYModifier);

	//rotation over life values
	CopyCurveDataToAnimConstants(m_emitterData.m_rotationOverLifetime, animValueConstants.dragOverLifetimeRotOverLifetime, 2,
		animValueConstants.curveTypeBitFlags, ANIM_CURVE_BIT_FLAG_ROT_OVERLIFE, m_emitterData.m_rotationModifier);

	//orbital vel over life values
	CopyCurveDataToAnimConstants(m_emitterData.m_orbitalVelOverLifetime, animValueConstants.orbitVelAndRadiusOverLifetime, 0,
		animValueConstants.curveTypeBitFlags, ANIM_CURVE_BIT_FLAG_ORBIT_VEL_OVERLIFE, m_emitterData.m_orbitalVelocityModifier);

	//orbital radius over life values
	CopyCurveDataToAnimConstants(m_emitterData.m_orbitalRadiusOverLifetime, animValueConstants.orbitVelAndRadiusOverLifetime, 2,
		animValueConstants.curveTypeBitFlags, ANIM_CURVE_BIT_FLAG_ORBIT_RADIUS_OVERLIFE, m_emitterData.m_orbitalRadiusModifier);

	//color over lifetime values
	std::vector<AnimatedValueKey<Rgba8>> colorKeys = m_emitterData.m_colorOverLifetime;
	for (int i = 0; i < colorKeys.size(); i++)
	{
		float floatColorVals[4];
		Rgba8 color = colorKeys[i].GetValue();
		color.GetAsFloats(floatColorVals);
		animValueConstants.colorOverLifetimeRGB[i].x = floatColorVals[0];
		animValueConstants.colorOverLifetimeRGB[i].y = floatColorVals[1];
		animValueConstants.colorOverLifetimeRGB[i].z = floatColorVals[2];
		animValueConstants.colorOverLifetimeRGB[i].w = colorKeys[i].GetTime();

		animValueConstants.colorOverLifetimeAlphaAndSizeOverLifetimeYKeys[i].x = floatColorVals[3];
		animValueConstants.colorOverLifetimeAlphaAndSizeOverLifetimeYKeys[i].y = colorKeys[i].GetTime();
	}
	m_renderer->CopyCPUToGPU(&animValueConstants, sizeof(AnimatedValuesConstants), m_gpuAnimatedValuesCBO);
	m_renderer->BindConstantBufferToComputeShader(GPU_ANIMATED_VALUE_BUFFER_SLOT, m_gpuAnimatedValuesCBO);

	//billboard constant data
	BillboardConstants billboardConstants;
	Vec3 camPos = camera.GetPosition();
	Vec3 camUp = camera.GetUpVector();
	billboardConstants.cameraPosition =  Vec4(camPos.x, camPos.y, camPos.z, 0.f);
	billboardConstants.cameraUp = Vec3(camUp.x, camUp.y, camUp.z);
	billboardConstants.renderMode = static_cast<unsigned int>(m_emitterData.m_renderMode);
	m_renderer->CopyCPUToGPU(&billboardConstants, sizeof(BillboardConstants), m_gpuBillboardCBO);
	m_renderer->BindConstantBufferToComputeShader(4, m_gpuBillboardCBO);
}

void ParticleEmitter::UpdateAndBindCPURenderConstants() const
{
	CPURenderConstants constants;
	constants.spritesheetLayout = m_emitterData.m_isSpriteSheetTexture ? m_emitterData.m_spriteSheetGridLayout : IntVec2::ONE;

	//size over lifetime
	CopyCurveDataToAnimConstants(m_emitterData.m_sizeOverLifetimeX, constants.sizeXYOverLifetime, 0,
		constants.curveModeFlags, ANIM_CURVE_BIT_FLAG_SIZE_OVERLIFE_X, m_emitterData.m_sizeOverLifeXModifier);
	CopyCurveDataToAnimConstants(m_emitterData.m_sizeOverLifetimeY, constants.sizeXYOverLifetime, 2,
		constants.curveModeFlags, ANIM_CURVE_BIT_FLAG_SIZE_OVERLIFE_Y, m_emitterData.m_sizeOverLifeYModifier);

	//orbit radius over lifetime
	CopyCurveDataToAnimConstants(m_emitterData.m_orbitalRadiusOverLifetime, constants.orbitRadiusOverLifetime, 0,
		constants.curveModeFlags, ANIM_CURVE_BIT_FLAG_ORBIT_RADIUS_OVERLIFE, m_emitterData.m_orbitalRadiusModifier);

	constants.orbitAxis = m_emitterData.m_orbitalVelocityAxis;
	m_renderer->CopyCPUToGPU(&constants, sizeof(CPURenderConstants), m_cpuParticlesCBO);

	//use the gpu cbo for the billboard constants as well
	BillboardConstants billboardConstants;
	Vec3 camPos = m_currentFrameCamera.GetPosition();
	Vec3 camUp = m_currentFrameCamera.GetUpVector();
	billboardConstants.cameraPosition = Vec4(camPos.x, camPos.y, camPos.z, 0.f);
	billboardConstants.cameraUp = Vec3(camUp.x, camUp.y, camUp.z);
	billboardConstants.renderMode = static_cast<unsigned int>(m_emitterData.m_renderMode);
	m_renderer->CopyCPUToGPU(&billboardConstants, sizeof(BillboardConstants), m_gpuBillboardCBO);
}

void ParticleEmitter::DebugPrintParticleData(const Particle& particle)
{
	DebuggerPrintf(Stringf("Particle: Pos = (%.2f, %.2f, %.2f) Vel = (%.2f, %.2f, %.2f)\n", particle.m_position.x, particle.m_position.y, particle.m_position.z,
		particle.m_velocity.x, particle.m_velocity.y, particle.m_velocity.z).c_str());
}

void ParticleEmitter::UpdateDebugData()
{
	m_debugData.m_particleListSize = m_emitterData.m_maxParticles;
	m_debugData.m_isGPUSim = m_gpuParticles;

	if (m_gpuParticles)
	{
		UpdateStatsFromGPU();
	}
}

void ParticleEmitter::RemoveParticleFromList(unsigned int listIndex)
{
	unsigned int deadParticleIndex = m_particles[listIndex];
	m_particles[listIndex] = m_particles[int(m_particles.size() - 1)];
	m_particles.pop_back();
	m_particleSystem->m_particlePool->ReturnParticleToPool(deadParticleIndex);
}

ParticleEmitterDebugData ParticleEmitter::GetDebugData() const
{
	return m_debugData;
}

ParticleEmitterData ParticleEmitter::GetEmitterData() const
{
	return m_emitterData;
}

Mat44 ParticleEmitter::GetModelMatrix() const
{
	return Mat44::CreateTranslation3D(m_particleSystem->m_position);
}

void ParticleEmitter::ToggleDebugMode()
{
	m_debugMode = !m_debugMode;
}

void ParticleEmitter::DebugGPUUpdateStepNow()
{
	m_debugStepNow = true;
}

void ParticleEmitter::UpdateEmitterData(const ParticleEmitterData& updatedData)
{
	m_emitterData = updatedData;
	m_particleTexture = m_renderer->CreateOrGetTextureFromFile(m_emitterData.m_textureFilepath.c_str());
}
