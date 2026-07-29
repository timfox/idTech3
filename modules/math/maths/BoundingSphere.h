/* Copyright (C) 2019 Wildfire Games.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef INCLUDED_BOUNDINGSPHERE
#define INCLUDED_BOUNDINGSPHERE

#include "maths/Vector3D.h"

class CBoundingBoxAligned;

class CBoundingSphere
{
public:
	CBoundingSphere() : m_Radius(0.0f) { }

	CBoundingSphere(const CVector3D& center, float radius) : m_Center(center), m_Radius(radius) { }

	const CVector3D& GetCenter() const
	{
		return m_Center;
	}

	float GetRadius() const
	{
		return m_Radius;
	}

	/**
	 * Construct a bounding sphere that encompasses a bounding box
	 * swept through all possible rotations around the origin.
	 */
	static CBoundingSphere FromSweptBox(const CBoundingBoxAligned& bbox);

	/**
	 * Check if the ray, defined by an origin point and a direction unit vector
	 * interesects with the sphere. The direction should be normalized.
	 */
	bool RayIntersect(const CVector3D& origin, const CVector3D& dir) const;

private:
	CVector3D m_Center;
	float m_Radius;
};

#endif // INCLUDED_BOUNDINGSPHERE
