#pragma once
#include "Engine/Math/Vec3.hpp"

class RandomNumberGenerator
{
public:
	int GetRandomIntLessThan(int maxNotInclusive) const;
	int GetRandomIntInRange(int minInclusive, int maxInclusive) const;
	int GetRandomIntInRange(int minInclusive, int maxInclusive, int index) const;
	float GetRandomFloatZeroToOne() const;
	float GetRandomFloatInRange(float minInclusive, float maxInclusive) const;
	Vec3 GetRandomDirectionInCone(const Vec3& forward, float angle) const;
	Vec3 GetRandomDirectionInSphere() const;
	float GetRandomFloatInRange(float minInclusive, float maxInclusive, int index) const;
};