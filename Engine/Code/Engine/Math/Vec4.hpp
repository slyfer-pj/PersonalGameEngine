#pragma once
#include "Engine/Math/Vec3.hpp"

struct Vec4
{
	float x = 0.f;
	float y = 0.f;
	float z = 0.f;
	float w = 0.f;

	Vec4() = default;
	explicit Vec4(float initialX, float initialY, float initialZ, float initialW);
	explicit Vec4(Vec3 fromVec3, float w);

	const Vec4 operator-(const Vec4& vectorToSubtract) const;
	const Vec4 operator*(float uniformScale) const;
	const Vec4 operator*(const Vec4& vectorToMultiply) const;
	const Vec4 operator/(float inverseScale) const;
	const void operator*=(float uniformScale);

	float GetLength() const;
	void Normalize();
};