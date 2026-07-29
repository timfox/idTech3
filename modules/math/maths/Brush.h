/* Copyright (C) 2025 Wildfire Games.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

/*
 * CBrush, a class representing a convex object
 */

#ifndef INCLUDED_BRUSH
#define INCLUDED_BRUSH

#include "maths/Vector3D.h"

#include <cstddef>
#include <vector>

class CBoundingBoxAligned;
class CFrustum;
class CPlane;


/**
 * Class CBrush: Represents a convex object, supports some CSG operations.
 */
class CBrush
{
public:
	CBrush();

	/**
	 * CBrush: Construct a brush from a bounds object.
	 *
	 * @param bounds the CBoundingBoxAligned object to construct the brush from.
	 */
	CBrush(const CBoundingBoxAligned& bounds);

	/**
	 * IsEmpty: Returns whether the brush is empty.
	 *
	 * @return @c true if the brush is empty, @c false otherwise
	 */
	bool IsEmpty() const { return m_Vertices.size() == 0; }

	/**
	 * Bounds: Calculate the axis-aligned bounding box for this brush.
	 *
	 * @param result the resulting bounding box is stored here
	 */
	void Bounds(CBoundingBoxAligned& result) const;

	/**
	 * Slice: Cut the object along the given plane, resulting in a smaller (or even empty) brush representing
	 * the part of the object that lies in front of the plane (as defined by the positive direction of its
	 * normal vector).
	 *
	 * @param plane the slicing plane
	 * @param result the resulting brush is stored here
	 */
	void Slice(const CPlane& plane, CBrush& result) const;

	/**
	 * Intersect: Intersect the brush with the given frustum.
	 *
	 * @param frustum the frustum to intersect with
	 * @param result the resulting brush is stored here
	 */
	void Intersect(const CFrustum& frustum, CBrush& result) const;

	/**
	 * Returns vertices in the brush. Intended for testing purposes; you should not need to use
	 * this method directly.
	 */
	const std::vector<CVector3D>& GetVertices() const;

	/**
	 * Writes a vector of the faces in this brush to @p out. Each face is itself a vector, listing the vertex indices
	 * that make up the face, starting and ending with the same index. Intended for testing purposes; you should not
	 * need to use this method directly.
	 */
	void GetFaces(std::vector<std::vector<size_t>>& out) const;

private:
	static const size_t NO_VERTEX = ~0u;

	typedef std::vector<CVector3D> Vertices;
	typedef std::vector<size_t> FaceIndices;

	/// Collection of unique vertices that make up this shape.
	Vertices m_Vertices;

	/**
	 * Holds the face definitions of this brush. Each face is a sequence of indices into m_Vertices that starts and ends with
	 * the same vertex index, completing a loop through all the vertices that make up the face. This vector holds all the face
	 * sequences back-to-back, thus looking something like 'x---xy--------yz--z' in the general case.
	 */
	FaceIndices m_Faces;

	struct Helper;
};

#endif // INCLUDED_BRUSH
