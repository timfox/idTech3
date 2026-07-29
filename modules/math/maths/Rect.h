/* Copyright (C) 2024 Wildfire Games.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef INCLUDED_RECT
#define INCLUDED_RECT

class CSize2D;
class CVector2D;


/**
 * Rectangle class used for screen rectangles. It's very similar to the MS
 * CRect, but with FLOATS because it's meant to be used with OpenGL which
 * takes float values.
 */
class CRect
{
public:
	CRect();
	CRect(const CVector2D& pos);
	CRect(const CSize2D& size);
	CRect(const CVector2D& upperleft, const CVector2D& bottomright);
	CRect(const CVector2D& pos, const CSize2D& size);
	CRect(const float l, const float t, const float r, const float b);
	CRect(const CRect&);

	CRect& operator=(const CRect& a);
	bool operator==(const CRect& a) const;
	bool operator!=(const CRect& a) const;
	CRect operator-() const;
	CRect operator+() const;

	CRect operator+(const CRect& a) const;
	CRect operator+(const CVector2D& a) const;
	CRect operator+(const CSize2D& a) const;
	CRect operator-(const CRect& a) const;
	CRect operator-(const CVector2D& a) const;
	CRect operator-(const CSize2D& a) const;

	void operator+=(const CRect& a);
	void operator+=(const CVector2D& a);
	void operator+=(const CSize2D& a);
	void operator-=(const CRect& a);
	void operator-=(const CVector2D& a);
	void operator-=(const CSize2D& a);

	operator bool() const { return right - left > 0 && bottom - top > 0; }

	/**
	 * @return Width of Rectangle
	 */
	float GetWidth() const;

	/**
	 * @return Height of Rectangle
	 */
	float GetHeight() const;

	/**
	 * Get Size
	 */
	CSize2D GetSize() const;

	/**
	 * Get Position equivalent to top/left corner
	 */
	CVector2D TopLeft() const;

	/**
	 * Get Position equivalent to top/right corner
	 */
	CVector2D TopRight() const;

	/**
	 * Get Position equivalent to bottom/left corner
	 */
	CVector2D BottomLeft() const;

	/**
	 * Get Position equivalent to bottom/right corner
	 */
	CVector2D BottomRight() const;

	/**
	 * Get Position equivalent to the center of the rectangle
	 */
	CVector2D CenterPoint() const;

	/**
	 * Evalutates if point is within the rectangle
	 * @param point CVector2D representing point
	 * @return true if inside.
	 */
	bool PointInside(const CVector2D &point) const;

	CRect Scale(float x, float y) const;

	bool IntersectWith(const CRect& a) const;

	CRect Intersection(const CRect& a) const;

	/**
	 * Returning CVector2D representing each corner.
	 */

public:
	/**
	 * Dimensions
	 */
	float left, top, right, bottom;
};

#endif // INCLUDED_RECT
