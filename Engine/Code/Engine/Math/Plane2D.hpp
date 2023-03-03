#pragma once
#include "Engine/Math/Vec2.hpp"

struct Plane2D
{
public:
	Vec2 m_normal = Vec2::ZERO;
	float m_distance = 0.f;
};