#include "Engine/Math/ConvexPoly2D.hpp"

ConvexPoly2D::ConvexPoly2D(const std::vector<Vec2>& pointsCounterClockwise)
	:m_pointsCounterClockwise(pointsCounterClockwise)
{
}

std::vector<Vec2> ConvexPoly2D::GetPoints() const
{
	return m_pointsCounterClockwise;
}
