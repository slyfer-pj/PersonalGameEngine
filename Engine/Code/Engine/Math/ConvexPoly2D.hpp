#pragma once
#include <vector>
#include "Engine/Math/Vec2.hpp"

struct ConvexPoly2D
{
public:
	ConvexPoly2D() = default;
	ConvexPoly2D(const std::vector<Vec2>& pointsCounterClockwise);
	std::vector<Vec2> GetPoints() const;

private:
	std::vector<Vec2> m_pointsCounterClockwise;
};