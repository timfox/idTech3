/* Copyright (C) 2025 Wildfire Games.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

/*
 * CFrustum is a collection of planes which define a viewing space.
 */

/*
Usually associated with the camera, there are 6 planes which define the
view pyramid. But we allow more planes per frustum which may be used for
portal rendering, where a portal may have 3 or more edges.
*/

#ifndef INCLUDED_FRUSTUM
#define INCLUDED_FRUSTUM

#include "maths/Vector3D.h"
#include "maths/Plane.h"

#include <cstddef>

class CBoundingBoxAligned;
class CMatrix3D;

class CFrustum
{
public:
	CFrustum();
	~CFrustum();

	// Set the number of planes to use for calculations. This is clamped to
	// [0, MAX_NUM_FRUSTUM_PLANES].
	void SetNumPlanes(size_t num);

	size_t GetNumPlanes() const { return m_NumPlanes; }

	void AddPlane(const CPlane& plane);

	void Transform(const CMatrix3D& m);

	// The following methods return true if the shape is
	// partially or completely in front of the frustum planes.
	bool IsPointVisible(const CVector3D& point) const;
	bool DoesSegmentIntersect(const CVector3D& start, const CVector3D& end) const;
	bool IsSphereVisible(const CVector3D& center, float radius) const;
	bool IsBoxVisible(const CVector3D& position, const CBoundingBoxAligned& bounds) const;
	bool IsBoxVisible(const CBoundingBoxAligned& bounds) const;

	CPlane& operator[](size_t idx) { return m_Planes[idx]; }
	const CPlane& operator[](size_t idx) const { return m_Planes[idx]; }

private:
	static const size_t MAX_NUM_FRUSTUM_PLANES = 10;

	CPlane m_Planes[MAX_NUM_FRUSTUM_PLANES];
	size_t m_NumPlanes;
};

#endif // INCLUDED_FRUSTUM
