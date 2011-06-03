/** @file
    @author Jukka Jyl�nki

    This work is copyrighted material and may NOT be used for any kind of commercial or 
    personal advantage and may NOT be copied or redistributed without prior consent
    of the author(s). 
*/
#pragma once

#include "MathFwd.h"
#include "float3.h"

class Triangle
{
public:
    float3 a;
    float3 b;
    float3 c;

    Triangle() {}
    Triangle(const float3 &a, const float3 &b, const float3 &c);

    /// Expresses the given point in barycentric (u,v,w) coordinates with respect to this triangle.
    float3 Barycentric(const float3 &point) const;

    /// Returns a point at the given barycentric coordinates.
    /// The inputted u, v and w should sum up to 1.
    float3 Point(float u, float v, float w) const;
    float3 Point(float u, float v) const { return Point(u, v, 1.f - u - v); }
    float3 Point(const float3 &barycentric) const;

    /// Returns the surface area of this triangle.
    float Area() const;

    /// Returns the plane this triangle lies on.
    Plane GetPlane() const;

    /// Returns the surface area of the given 2D triangle.
    static float Area2D(const float2 &p1, const float2 &p2, const float2 &p3);

    /// Relates the barycentric coordinate of the given point to the surface area of triangle abc.
    static float SignedArea(const float3 &point, const float3 &a, const float3 &b, const float3 &c);

    /// Returns true if this triangle is degenerate.
    /// @param epsilon If two of the vertices of this triangle are closer than this, the triangle is considered
    /// degenerate.
    bool IsDegenerate(float epsilon = 1e-3f) const;

    /// Returns true if the given triangle defined by the three given points is degenerate (zero surface area).
    static bool IsDegenerate(const float3 &p1, const float3 &p2, const float3 &p3, float epsilon = 1e-3f);

    /// Returns true if the given point is contained in this triangle.
    /// @param triangleThickness The epsilon value to use for this test. This specifies the maximum distance
    /// the point lie from the plane defined by this triangle.
    bool Contains(const float3 &point, float triangleThickness = 1e-3f) const;

    /// Returns the closest point on this triangle to the target point.
    float3 ClosestPoint(const float3 &targetPoint) const;
};

#ifdef QT_INTEROP
Q_DECLARE_METATYPE(Triangle)
Q_DECLARE_METATYPE(Triangle*)
#endif