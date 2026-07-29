/* Copyright (C) 2021 Wildfire Games.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "precompiled.h"

#include "Size2D.h"

CSize2D::CSize2D() = default;

CSize2D::CSize2D(const CSize2D& size) : Width(size.Width), Height(size.Height)
{
}

CSize2D::CSize2D(const float width, const float height) : Width(width), Height(height)
{
}

CSize2D& CSize2D::operator=(const CSize2D& size)
{
	Width = size.Width;
	Height = size.Height;
	return *this;
}

bool CSize2D::operator==(const CSize2D& size) const
{
	return Width == size.Width && Height == size.Height;
}

bool CSize2D::operator!=(const CSize2D& size) const
{
	return !(*this == size);
}

CSize2D CSize2D::operator+(const CSize2D& size) const
{
	return CSize2D(Width + size.Width, Height + size.Height);
}

CSize2D CSize2D::operator-(const CSize2D& size) const
{
	return CSize2D(Width - size.Width, Height - size.Height);
}

CSize2D CSize2D::operator/(const float a) const
{
	return CSize2D(Width / a, Height / a);
}

CSize2D CSize2D::operator*(const float a) const
{
	return CSize2D(Width * a, Height * a);
}

void CSize2D::operator+=(const CSize2D& size)
{
	Width += size.Width;
	Height += size.Height;
}

void CSize2D::operator-=(const CSize2D& size)
{
	Width -= size.Width;
	Height -= size.Height;
}

void CSize2D::operator/=(const float a)
{
	Width /= a;
	Height /= a;
}

void CSize2D::operator*=(const float a)
{
	Width *= a;
	Height *= a;
}
