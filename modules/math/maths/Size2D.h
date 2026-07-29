/* Copyright (C) 2021 Wildfire Games.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef INCLUDED_SIZE2D
#define INCLUDED_SIZE2D

/*
 * Provides an interface for a size - geometric property in R2.
 */
class CSize2D
{
public:
	CSize2D();
	CSize2D(const CSize2D& size);
	CSize2D(const float width, const float height);

	CSize2D& operator=(const CSize2D& size);
	bool operator==(const CSize2D& size) const;
	bool operator!=(const CSize2D& size) const;

	CSize2D operator+(const CSize2D& size) const;
	CSize2D operator-(const CSize2D& size) const;
	CSize2D operator/(const float a) const;
	CSize2D operator*(const float a) const;

	void operator+=(const CSize2D& a);
	void operator-=(const CSize2D& a);
	void operator/=(const float a);
	void operator*=(const float a);

public:
	float Width = 0.0f, Height = 0.0f;
};

#endif // INCLUDED_SIZE2D
