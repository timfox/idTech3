/* Copyright (C) 2021 Wildfire Games.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "precompiled.h"

#include "Vector2D.h"

#include "maths/Size2D.h"

CVector2D::CVector2D(const CSize2D& size) : X(size.Width), Y(size.Height)
{
}

bool CVector2D::operator==(const CVector2D& v) const
{
	return X == v.X && Y == v.Y;
}

bool CVector2D::operator!=(const CVector2D& v) const
{
	return !(*this == v);
}

CVector2D CVector2D::operator+(const CSize2D& size) const
{
	return CVector2D(X + size.Width, Y + size.Height);
}

CVector2D CVector2D::operator-(const CSize2D& size) const
{
	return CVector2D(X - size.Width, Y - size.Height);
}

void CVector2D::operator+=(const CSize2D& size)
{
	X += size.Width;
	Y += size.Height;
}

void CVector2D::operator-=(const CSize2D& size)
{
	X -= size.Width;
	Y -= size.Height;
}
