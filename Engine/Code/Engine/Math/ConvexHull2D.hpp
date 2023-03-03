#pragma once
#include <vector>
#include "Engine/Math/Plane2D.hpp"

struct ConvexPoly2D;

struct ConvexHull2D
{
public:
	ConvexHull2D() = default;
	ConvexHull2D(const ConvexPoly2D& fromConvexPoly);
	bool IsPointInside(const Vec2& refPoint) const;

public:
	std::vector<Plane2D> m_boundingPlanes;
};