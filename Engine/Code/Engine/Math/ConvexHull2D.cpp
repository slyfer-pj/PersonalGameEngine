#include "Engine/Math/ConvexHull2D.hpp"
#include "Engine/Math/ConvexPoly2D.hpp"
#include "Engine/Math/MathUtils.hpp"

constexpr float EPISILON = 0.0001f;

ConvexHull2D::ConvexHull2D(const ConvexPoly2D& fromConvexPoly)
{
	std::vector<Vec2> convexPolyPoints = fromConvexPoly.GetPoints();
	for (int i = 0; i < convexPolyPoints.size(); i++)
	{
		Vec2 pointA = convexPolyPoints[i];
		int pointBIndex = (i == convexPolyPoints.size() - 1) ? 0 : (i + 1);
		Vec2 pointB = convexPolyPoints[pointBIndex];
		Vec2 dispVec = (pointB - pointA).GetNormalized();
		Vec2 planeNormal = dispVec.GetRotatedMinus90Degrees();
		Plane2D plane;
		plane.m_normal = planeNormal;
		plane.m_distance = DotProduct2D(pointA, planeNormal);
		m_boundingPlanes.push_back(plane);
	}
}

bool ConvexHull2D::IsPointInside(const Vec2& refPoint) const
{
	for (int i = 0; i < m_boundingPlanes.size(); i++)
	{
		float pointProjectedOntoPlaneNormal = DotProduct2D(refPoint, m_boundingPlanes[i].m_normal);
		if (pointProjectedOntoPlaneNormal > (m_boundingPlanes[i].m_distance + EPISILON))
			return false;
	}

	return true;
}
