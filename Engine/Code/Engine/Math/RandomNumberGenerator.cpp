#include "Engine/Math/RandomNumberGenerator.hpp"
#include "Engine/Core/ErrorWarningAssert.hpp"
#include "Engine/Math/EulerAngles.hpp"
#include "Engine/Math/Mat44.hpp"
#include "ThirdParty/Squirrel/RawNoise.hpp"

int RandomNumberGenerator::GetRandomIntLessThan(int maxNotInclusive) const
{
	GUARANTEE_OR_DIE(maxNotInclusive > 1, "Method takes only ints greater than 0 as input");
	return rand() % maxNotInclusive;
}

int RandomNumberGenerator::GetRandomIntInRange(int minInclusive, int maxInclusive) const
{
	return minInclusive + rand() % (maxInclusive - minInclusive + 1);
}

int RandomNumberGenerator::GetRandomIntInRange(int minInclusive, int maxInclusive, int index) const
{
	return minInclusive + (Get1dNoiseUint(index) % (maxInclusive - minInclusive + 1));
}

float RandomNumberGenerator::GetRandomFloatZeroToOne() const
{
	return static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
}

float RandomNumberGenerator::GetRandomFloatInRange(float minInclusive, float maxInclusive) const
{
	return minInclusive + (GetRandomFloatZeroToOne() * (maxInclusive - minInclusive));
}

float RandomNumberGenerator::GetRandomFloatInRange(float minInclusive, float maxInclusive, int index) const
{
	return minInclusive + (Get1dNoiseZeroToOne(index) * (maxInclusive - minInclusive));
}

Vec3 RandomNumberGenerator::GetRandomDirectionInCone(const Vec3& forward, float angle) const
{
	float halfAngle = angle * 0.5f;
	float baseYaw = forward.GetAngleAboutZDegrees();
	float basePitch = forward.GetAngleAboutYDegrees();
	float randomYaw = GetRandomFloatInRange(-halfAngle + baseYaw, halfAngle + baseYaw);
	float randomPitch = GetRandomFloatInRange(-halfAngle + basePitch, halfAngle + basePitch);
	EulerAngles randomOrientation(randomYaw, randomPitch, 0.f);
	Vec3 randomDirection = randomOrientation.GetAsMatrix_XFwd_YLeft_ZUp().GetIBasis3D();
	return randomDirection;
}

Vec3 RandomNumberGenerator::GetRandomDirectionInSphere() const
{
	float randomYaw = GetRandomFloatInRange(0.f, 360.f);
	float randomPitch = GetRandomFloatInRange(0.f, 360.f);
	float randomRoll = GetRandomFloatInRange(0.f, 360.f);
	EulerAngles randomOrientation(randomYaw, randomPitch, randomRoll);
	return randomOrientation.GetAsMatrix_XFwd_YLeft_ZUp().GetIBasis3D();
}
