/* Copyright (C) 2025 Wildfire Games.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

/**
 * Contains classes for smooth splines
 * Borrowed from Game Programming Gems 4. (Slightly changed to better suit our purposes
 * and compatability. Any references to external material can be found there.
 */

#ifndef INCLUDED_NUSPLINE
#define INCLUDED_NUSPLINE

#define MAX_SPLINE_NODES 128

#include "maths/Fixed.h"
#include "maths/FixedVector3D.h"
#include "maths/Vector3D.h"

#include <vector>

/**
 * Describes a node of the spline
 */
struct SplineData
{
	// Should be fixed, because used in the simulation
	CFixedVector3D Position;
	CVector3D Velocity;
	// TODO: make rotation as other spline
	CFixedVector3D Rotation;
	// Time interval to the previous node, should be 0 for the first node
	fixed Distance;
};


/**
 * Rounded Nonuniform Spline for describing spatial curves or paths with constant speed
 */
class RNSpline
{
public:

	RNSpline();
	virtual ~RNSpline();

	void AddNode(const CFixedVector3D& pos);
	void BuildSpline();
	CVector3D GetPosition(float time) const;
	CVector3D GetRotation(float time) const;
	const std::vector<SplineData>& GetAllNodes() const;

	fixed MaxDistance;
	int NodeCount;

protected:

	std::vector<SplineData> Node;
	CVector3D GetStartVelocity(int index);
	CVector3D GetEndVelocity(int index);
};


/**
 * Smooth Nonuniform Spline for describing paths with smooth acceleration and deceleration,
 * but without turning
 */
class SNSpline : public RNSpline
{
public:
	virtual ~SNSpline();

	void BuildSpline();
	void Smooth();
};


/**
 * Timed Nonuniform Spline for paths with different time intervals between nodes
 */
class TNSpline : public SNSpline
{
public:
	virtual ~TNSpline();

	void AddNode(const CFixedVector3D& pos, const CFixedVector3D& rotation, fixed timePeriod);
	void InsertNode(const int index, const CFixedVector3D& pos, const CFixedVector3D& rotation, fixed timePeriod);
	void RemoveNode(const int index);
	void UpdateNodePos(const int index, const CFixedVector3D& pos);
	void UpdateNodeTime(const int index, fixed time);

	void BuildSpline();
	void Smooth();
	void Constrain();
};

#endif // INCLUDED_NUSPLINE
