/* Copyright (C) 2025 Wildfire Games.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

/*
 * Axis-aligned bounding box
 */

#ifndef INCLUDED_BOUND
#define INCLUDED_BOUND

#include "maths/Vector3D.h"

#include <algorithm>

class CBoundingBoxOriented;
class CFrustum;
class CMatrix3D;

// Basic axis aligned bounding box (AABB) class
class CBoundingBoxAligned
{
public:
	static const CBoundingBoxAligned EMPTY;

	CBoundingBoxAligned() { SetEmpty(); }
	CBoundingBoxAligned(const CVector3D& min, const CVector3D& max)
	{
		m_Data[0] = min;
		m_Data[1] = max;
	}

	/**
	 * Transforms these bounds according to the specified transformation matrix @p m, and writes the axis-aligned bounds
	 * of that result to @p result.
	 */
	void Transform(const CMatrix3D& m, CBoundingBoxAligned& result) const;

	/**
	 * Transform these bounds using the matrix @p transform, and write out the result as an oriented (i.e. non-axis-aligned) box.
	 * The difference with @ref Transform(const CMatrix3D&, CBoundingBoxAligned&) is that that method is equivalent to first
	 * computing this result, and then taking the axis-aligned bounding boxes from the result again.
	 */
	void Transform(const CMatrix3D& m, CBoundingBoxOriented& result) const;

	/**
	 * Translates these bounds by @p v, and writes the result to @p result.
	 */
	void Translate(const CVector3D& v, CBoundingBoxAligned& result) const
	{
		result.m_Data[0] = m_Data[0] + v;
		result.m_Data[1] = m_Data[1] + v;
	}

	CVector3D& operator[](int index) { return m_Data[index]; }
	const CVector3D& operator[](int index) const { return m_Data[index]; }

	void SetEmpty();
	bool IsEmpty() const;

	void Extend(const CVector3D& min, const CVector3D& max)
	{
		if (min.X < m_Data[0].X) m_Data[0].X = min.X;
		if (min.Y < m_Data[0].Y) m_Data[0].Y = min.Y;
		if (min.Z < m_Data[0].Z) m_Data[0].Z = min.Z;
		if (max.X > m_Data[1].X) m_Data[1].X = max.X;
		if (max.Y > m_Data[1].Y) m_Data[1].Y = max.Y;
		if (max.Z > m_Data[1].Z) m_Data[1].Z = max.Z;
	}

	// operator+=: extend this bound to include given bound
	CBoundingBoxAligned& operator+=(const CBoundingBoxAligned& b)
	{
		Extend(b.m_Data[0], b.m_Data[1]);
		return *this;
	}

	// operator+=: extend this bound to include given point
	CBoundingBoxAligned& operator+=(const CVector3D& pt)
	{
		Extend(pt, pt);
		return *this;
	}

	/**
	 * Check if a given ray intersects this AABB.
	 * See also Real-Time Rendering, Third Edition by T. Akenine-Moller, p. 741--742.
	 *
	 * @param[in] origin Origin of the ray.
	 * @param[in] dir Direction vector of the ray, defining the positive direction of the ray. Must be of unit length.
	 * @param[out] tmin,tmax distance in the positive direction from the origin of the ray to the entry and exit points in
	 *	the bounding box. If the origin is inside the box, then this is counted as an intersection and one of @p tMin and @p tMax may be negative.
	 *
	 * @return true if the ray originating in @p origin and with unit direction vector @p dir intersects this AABB, false otherwise.
	 */
	bool RayIntersect(const CVector3D& origin, const CVector3D& dir, float& tmin, float& tmax) const;

	bool IsPointInside(const CVector3D& point) const;

	// return the volume of this bounding box
	float GetVolume() const
	{
		CVector3D v = m_Data[1] - m_Data[0];
		return (std::max(v.X, 0.0f) * std::max(v.Y, 0.0f) * std::max(v.Z, 0.0f));
	}

	// return the center of this bounding box
	void GetCenter(CVector3D& center) const
	{
		center = (m_Data[0] + m_Data[1]) * 0.5f;
	}

	/**
	 * Expand the bounding box by the given amount in every direction.
	 */
	void Expand(float amount);

	/**
	 * IntersectFrustumConservative: Approximate the intersection of this bounds object
	 * with the given frustum. The bounds object is overwritten with the results.
	 *
	 * The approximation is conservative in the sense that the result will always contain
	 * the actual intersection, but it may be larger than the intersection itself.
	 * The result will always be fully contained within the original bounds.
	 *
	 * @note While not in the spirit of this function's purpose, a no-op would be a correct
	 * implementation of this function.
	 * @note If this bound is empty, the result is the empty bound.
	 *
	 * @param frustum the frustum to intersect with
	 */
	void IntersectFrustumConservative(const CFrustum& frustum);

	/**
	 * Construct a CFrustum that describes the same volume as this bounding box.
	 * Only valid for non-empty bounding boxes - check IsEmpty() first.
	 */
	CFrustum ToFrustum() const;

private:
	// Holds the minimal and maximal coordinate points in m_Data[0] and m_Data[1], respectively.
	CVector3D m_Data[2];
};

#endif // INCLUDED_BOUND
