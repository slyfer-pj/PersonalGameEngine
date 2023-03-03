#pragma once
#include "Engine/Core/Vertex_PCU.hpp"
#include "Engine/Renderer/ParticleEmitterData.hpp"
#include "Engine/Core/Job.hpp"
#include "Engine/Math/IntRange.hpp"
#include "Engine/Math/Mat44.hpp"
#include "ThirdParty/AMDParallelSort/SortLib.h"
#include <vector>

constexpr float PARTICLE_QUAD_SIZE = 0.2f;
//constexpr unsigned int MAX_PARTICLES = 100000;

class Camera;
class Renderer;
class ConstantBuffer;
class JobSystem;
class Shader;
class Texture;
class ParticleSystem;

struct Particle;
struct ParticleEmitterData;
struct ID3D11Buffer;
struct ID3D11ShaderResourceView;
struct ID3D11UnorderedAccessView;
struct ID3D11ComputeShader;

struct ParticleEmitterDebugData
{
	int m_numParticlesSpawned = 0;
	int m_particleListSize = 0;
	int m_aliveParticles = 0;
	int m_deadParticles = 0;
	bool m_isGPUSim = false;
};

struct CountBuffer
{
	unsigned int maxParticles = 0u;
	unsigned int listSize = 0u;
	unsigned int deadListSize = 0u;
	unsigned int lastAliveParticleIndex = 0u;
};

class ParticleEmitter
{
public:
	ParticleEmitter(ParticleSystem* parentParticleSystem, const XmlElement* emitterElement);
	~ParticleEmitter();

	void Update(float deltaSeconds, const Camera& camera);
	void UpdateEmitterData(const ParticleEmitterData& updatedData);
	void AddVertsForParticle(std::vector<Vertex_PCU>& cpuMeshVertices, std::vector<unsigned int>& cpuMeshIndices, Vertex_PCU sampleVert,
		const Particle& particle, const Vec3& cameraPos, const Vec3& cameraUp);
	void Render() const;
	void Restart();
	void ChangeEmitterType(bool gpuEmitter);
	void SetJobSystem(JobSystem* jobSystem);

	ParticleEmitterDebugData GetDebugData() const;
	ParticleEmitterData GetEmitterData() const;
	Mat44 GetModelMatrix() const;

	void ToggleDebugMode();
	void DebugGPUUpdateStepNow();

private:
	void CreateResources();
	void InitializeGPUSimulationResource();
	void InitializeGPUSortResources();

	void BuildCPUMesh(const Camera& camera);
	void SpawnParticle(float deltaSeconds, const Camera& camera);
	void SpawnParticlesOnCPU(unsigned int particlesToEmit, float deltaSeconds);
	void SpawnParticlesOnGPU(unsigned int particlesToEmit, float deltaSeconds, const Camera& camera);

	void UpdateParticles(float deltaSeconds, const Camera& camera);
	void UpdateOnCPU(float deltaSeconds, const Camera& camera);
	void UpdateOnGPU(float deltaSeconds, const Camera& camera);
	void UpdateAndBindGPUSimConstants(float deltaSeconds, const Camera& camera);
	void UpdateAndBindCPURenderConstants() const;

	void SortParticlesBasedOnDistanceFromCamera(int minParticleListIndex, int maxParticleListIndex);
	int PartitionParticleListForSort(int minParticleListIndex, int maxParticleListIndex);
	void RunBitonicSort();
	//void RunOddEvenSort();

	void RemoveParticleFromList(unsigned int listIndex);
	void ReleaseResources();
	void RestartEmitter();
	
	void RenderGPUParticles() const;
	void RenderCPUParticles() const;

	void DebugPrintParticleData(const Particle& particle);
	void UpdateDebugData();
	void UpdateStatsFromGPU();

	template <typename T>
	void CopyCurveDataToAnimConstants(const AnimatedCurve<T>& curve, Vec4* arrayToFillDataIn, size_t offsetInEachEntry,
		unsigned int& bitFlagsData, unsigned int bitFlagForThisCurve, float modifierIfAny = 1.f) const;

private:
	ParticleSystem* m_particleSystem = nullptr;
	Renderer* m_renderer = nullptr;
	JobSystem* m_jobSystem = nullptr;

	ParticleEmitterData m_emitterData;
	std::vector<unsigned int> m_particles;
	ParticleEmitterDebugData m_debugData;
	float m_partialParticle = 0.f;
	unsigned int m_frameCount = 0;
	Texture* m_particleTexture = nullptr;
	bool m_restart = false;
	unsigned int m_startIndexLocation = 0u;
	int m_startVertexLocation = 0;
	unsigned int m_numIndices = 0u;
	float m_burstIntervalTimer = 999999.f;		//setting this timer high such that the burst particles emitted on the first frame
	ConstantBuffer* m_cpuParticlesCBO = nullptr;
	ID3D11Buffer* m_cpuParticleIndicies = nullptr;
	ID3D11ShaderResourceView* m_cpuParticleIndexSRV = nullptr;
	Shader* m_cpuParticleShader = nullptr;
	Camera m_currentFrameCamera;
	std::vector<float> m_particleDistFromCameraForSort;
	unsigned int m_currentParticleId = 0;

	//gpu simulation variables
	bool m_debugMode = false;
	bool m_debugStepNow = false;
	int m_aliveParticles = 0;
	bool m_gpuParticles = false;
	CountBuffer m_currentCountBuffer;

	//test gpu sim variables
	int m_currentTotalSpawnedParticles = 0;

	//new approach
	ID3D11Buffer* m_particleVertexDataVSCopy = nullptr;
	ID3D11Buffer* m_particleCount = nullptr;
	ID3D11Buffer* m_sortedParticlesList = nullptr;
	ID3D11Buffer* m_particleData = nullptr;
	ID3D11Buffer* m_deadParticleIndexList = nullptr;
	ID3D11Buffer* m_afterSpawnAliveParticleList = nullptr;
	ID3D11Buffer* m_afterSimAliveParticleList = nullptr;
	ID3D11Buffer* m_countBuffer = nullptr;
	ID3D11Buffer* m_stagingCountBuffer = nullptr;
	ID3D11Buffer* m_dataToSort = nullptr;

	ID3D11UnorderedAccessView* m_particleDataUAV = nullptr;
	ID3D11UnorderedAccessView* m_deadParticleListUAV = nullptr;
	ID3D11UnorderedAccessView* m_afterSpawnAliveParticleListUAV = nullptr;
	ID3D11UnorderedAccessView* m_afterSimAliveParticleListUAV = nullptr;
	ID3D11UnorderedAccessView* m_countBufferUAV = nullptr;
	ID3D11UnorderedAccessView* m_dataToSortUAV = nullptr;
	ID3D11UnorderedAccessView* m_sortedParticleListUAV = nullptr;
	ID3D11UnorderedAccessView* m_nullUAV = nullptr;
	ID3D11ShaderResourceView* m_sortedDataSRV = nullptr;
	ID3D11ShaderResourceView* m_particleDataVSSRV = nullptr;
	ID3D11ShaderResourceView* m_nullSRV = nullptr;

	ID3D11ComputeShader* m_setupSimCS = nullptr;
	ID3D11ComputeShader* m_spawnCS = nullptr;
	ID3D11ComputeShader* m_simulateCS = nullptr;
	ID3D11ComputeShader* m_finishSimCS = nullptr;
	ID3D11ComputeShader* m_sortCS = nullptr;
	Shader* m_gpuParticleShader = nullptr;

	ConstantBuffer* m_gpuSimCBO = nullptr;
	ConstantBuffer* m_gpuAnimatedValuesCBO = nullptr;
	ConstantBuffer* m_gpuSpawnParticlesCBO = nullptr;
	ConstantBuffer* m_gpuCountCBO = nullptr;
	ConstantBuffer* m_gpuBillboardCBO = nullptr;
	ConstantBuffer* m_gpuSortCBO = nullptr;

	SortLib m_bitonicSort;
};

template <typename T>
void ParticleEmitter::CopyCurveDataToAnimConstants(const AnimatedCurve<T>& curve, Vec4* arrayToFillDataIn, size_t offsetInEachEntry,
	unsigned int& bitFlagsData, unsigned int bitFlagForThisCurve, float modifierIfAny) const
{
	for (int i = 0; i < curve.m_curveOneKeys.size(); i++)
	{
		Vec2 dataToCopy = Vec2(curve.m_curveOneKeys[i].GetValue() * modifierIfAny, curve.m_curveOneKeys[i].GetTime());
		void* destAddressForCopy = (&arrayToFillDataIn[i].x) + offsetInEachEntry;
		memcpy(destAddressForCopy, &dataToCopy, sizeof(T) * 2);
	}
	if (curve.IsToRandomBetweenCurves())
	{
		for (int i = 0; i < curve.m_curveTwoKeys.size(); i++)
		{
			Vec2 dataToCopy = Vec2(curve.m_curveTwoKeys[i].GetValue() * modifierIfAny, curve.m_curveTwoKeys[i].GetTime());
			void* destAddressForCopy = (&arrayToFillDataIn[i + 4].x) + offsetInEachEntry;
			memcpy(destAddressForCopy, &dataToCopy, sizeof(T) * 2);
		}
		bitFlagsData |= bitFlagForThisCurve;
	}
	else
	{
		bitFlagsData &= ~bitFlagForThisCurve;
	}
}
