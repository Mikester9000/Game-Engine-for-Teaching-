/**
 * @file math_types.hpp
 * @brief Minimal 3D math library — Vec3, Quat, Mat4.
 *
 * ============================================================================
 * TEACHING NOTE — Why a Custom Math Library?
 * ============================================================================
 * A full engine would link against a battle-tested library like DirectXMath,
 * GLM, or Eigen.  For teaching purposes we implement just enough math to run
 * the animation system, keeping each type short and fully annotated.
 *
 * Key types:
 *   Vec3   — 3-component float vector (position, scale, translation).
 *   Quat   — Unit quaternion (rotation).
 *   Mat4   — Row-major 4×4 float matrix (transform; compatible with D3D11).
 *
 * TEACHING NOTE — Row-Major vs Column-Major
 * D3D11 HLSL uses row-major matrices by default: a vertex position is a
 * row-vector and is multiplied as  pos * matrix  (row × matrix).
 * OpenGL and Vulkan GLSL use column-major: matrix × column-vector.
 * This file stores Mat4 in row-major order to match D3D11 convention.
 * When uploading to a D3D11 constant buffer, NO transpose is needed.
 * (If you later target Vulkan, you will need to transpose the matrices
 *  or use the GLSL `layout(row_major)` qualifier.)
 *
 * ============================================================================
 *
 * @author  Educational Game Engine Project
 * @version 1.0
 * @date    2024
 * C++ Standard: C++17
 * Platform: Windows / Linux (no platform-specific intrinsics)
 */

#pragma once

#include <cmath>
#include <array>
#include <algorithm> // std::min

namespace engine {
namespace math {

// ===========================================================================
// Constants
// ===========================================================================

static constexpr float kPi    = 3.14159265358979323846f;
static constexpr float kEps   = 1e-7f;  ///< "close enough to zero" threshold

// ===========================================================================
// Vec3 — 3-component float vector
// ===========================================================================

/**
 * @struct Vec3
 * @brief Three-component floating-point vector.
 *
 * TEACHING NOTE — Separate from DirectX XMFLOAT3
 * DirectXMath (xmfloat3.h) provides SIMD-accelerated variants of this type.
 * We use a plain struct here so the animation code is readable without
 * knowing the DirectXMath API.  Performance-critical paths (e.g. 5000-bone
 * crowds) would switch to XMVECTOR operations.
 */
struct Vec3
{
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;

    Vec3() = default;
    Vec3(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {}

    // -----------------------------------------------------------------------
    // Arithmetic operators (component-wise)
    // -----------------------------------------------------------------------

    Vec3 operator+(const Vec3& o) const { return { x + o.x, y + o.y, z + o.z }; }
    Vec3 operator-(const Vec3& o) const { return { x - o.x, y - o.y, z - o.z }; }
    Vec3 operator*(float s)        const { return { x * s,   y * s,   z * s   }; }
    Vec3 operator/(float s)        const { return { x / s,   y / s,   z / s   }; }

    Vec3& operator+=(const Vec3& o) { x += o.x; y += o.y; z += o.z; return *this; }
    Vec3& operator*=(float s)       { x *= s;   y *= s;   z *= s;   return *this; }

    // -----------------------------------------------------------------------
    // TEACHING NOTE — Dot product and cross product
    // Dot product: measures how "aligned" two vectors are.
    //   dot(a, b) = |a||b|cos θ
    //   dot > 0: angle < 90°; = 0: perpendicular; < 0: angle > 90°
    //
    // Cross product: produces a vector perpendicular to both inputs.
    //   Useful for computing normals and right-hand-rule rotations.
    // -----------------------------------------------------------------------
    float Dot(const Vec3& o)   const { return x*o.x + y*o.y + z*o.z; }

    Vec3  Cross(const Vec3& o) const {
        return { y*o.z - z*o.y,
                 z*o.x - x*o.z,
                 x*o.y - y*o.x };
    }

    float LengthSq() const { return x*x + y*y + z*z; }
    float Length()   const { return std::sqrt(LengthSq()); }

    Vec3 Normalized() const {
        float len = Length();
        if (len < kEps) return { 0.0f, 0.0f, 1.0f };
        return *this / len;
    }

    // -----------------------------------------------------------------------
    // Lerp — linear interpolation between two vectors.
    // TEACHING NOTE — Lerp is correct for translation and scale.  For
    // rotation use Slerp (on Quat) because Lerp on rotation axis-angles
    // produces non-constant-speed rotation.
    // -----------------------------------------------------------------------
    static Vec3 Lerp(const Vec3& a, const Vec3& b, float t)
    {
        return a + (b - a) * t;
    }

    static Vec3 Zero()  { return { 0.0f, 0.0f, 0.0f }; }
    static Vec3 One()   { return { 1.0f, 1.0f, 1.0f }; }
    static Vec3 Up()    { return { 0.0f, 1.0f, 0.0f }; }
    static Vec3 Right() { return { 1.0f, 0.0f, 0.0f }; }
    static Vec3 Fwd()   { return { 0.0f, 0.0f, 1.0f }; }
};

// ===========================================================================
// Quat — unit quaternion for rotation
// ===========================================================================

/**
 * @struct Quat
 * @brief Unit quaternion representing a 3D rotation.
 *
 * TEACHING NOTE — Why Quaternions Instead of Euler Angles?
 * Euler angles (pitch / yaw / roll) suffer from *gimbal lock*: when two
 * rotation axes align, one degree of freedom is lost and rotations become
 * non-intuitive.
 *
 * Quaternions represent rotations as q = (x, y, z, w) on the 4-D unit
 * hypersphere (|q| = 1).  They:
 *   1. Have no gimbal lock.
 *   2. Interpolate smoothly via SLERP.
 *   3. Compose efficiently with multiplication (no trigonometry at runtime).
 *
 * Convention: q = (xi, yj, zk, w)  where w is the scalar part.
 * Identity (no rotation): q = (0, 0, 0, 1).
 */
struct Quat
{
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float w = 1.0f;  ///< Scalar / real part; 1 = identity

    Quat() = default;
    Quat(float x_, float y_, float z_, float w_) : x(x_), y(y_), z(z_), w(w_) {}

    // -----------------------------------------------------------------------
    // Static factories
    // -----------------------------------------------------------------------

    static Quat Identity() { return { 0.0f, 0.0f, 0.0f, 1.0f }; }

    /**
     * @brief Create a quaternion from an axis-angle rotation.
     * @param axis  Unit rotation axis (normalised by this function if not).
     * @param radians Rotation angle in radians.
     *
     * TEACHING NOTE — Axis-Angle to Quaternion
     * q = (sin(θ/2)·axis, cos(θ/2))
     * This is the most intuitive way to construct a rotation quaternion.
     */
    static Quat FromAxisAngle(const Vec3& axis, float radians)
    {
        Vec3  n   = axis.Normalized();
        float h   = radians * 0.5f;
        float s   = std::sin(h);
        float c   = std::cos(h);
        return { n.x * s, n.y * s, n.z * s, c };
    }

    // -----------------------------------------------------------------------
    // Basic operations
    // -----------------------------------------------------------------------

    float LengthSq() const { return x*x + y*y + z*z + w*w; }
    float Length()   const { return std::sqrt(LengthSq()); }

    Quat Normalized() const {
        float len = Length();
        if (len < kEps) return Identity();
        return { x/len, y/len, z/len, w/len };
    }

    Quat Conjugate() const { return { -x, -y, -z, w }; }

    // -----------------------------------------------------------------------
    // Quaternion multiplication — concatenate two rotations.
    // TEACHING NOTE — Order matters: (a * b) first applies a, then b.
    // This is the opposite of matrix post-multiplication in some conventions.
    // -----------------------------------------------------------------------
    Quat operator*(const Quat& o) const
    {
        return {
             w*o.x + x*o.w + y*o.z - z*o.y,
             w*o.y - x*o.z + y*o.w + z*o.x,
             w*o.z + x*o.y - y*o.x + z*o.w,
             w*o.w - x*o.x - y*o.y - z*o.z
        };
    }

    /**
     * @brief Rotate a vector by this quaternion.
     * v' = q * (0,v) * q^{-1}   (sandwich product)
     *
     * TEACHING NOTE — The sandwich product is the standard way to apply a
     * quaternion rotation to a vector.  The result is equivalent to building
     * a rotation matrix from the quaternion and multiplying — but faster.
     */
    Vec3 Rotate(const Vec3& v) const
    {
        // Efficient form of the sandwich product:
        // t = 2 * cross(q.xyz, v)
        // v' = v + q.w * t + cross(q.xyz, t)
        Vec3 qv  { x, y, z };
        Vec3 t   = qv.Cross(v) * 2.0f;
        return v + t * w + qv.Cross(t);
    }

    // -----------------------------------------------------------------------
    // Slerp — Spherical Linear Interpolation
    // -----------------------------------------------------------------------

    /**
     * @brief Slerp from this quaternion to `b` by parameter `t` ∈ [0,1].
     *
     * TEACHING NOTE — SLERP vs LERP for Rotations
     * LERP (linear interpolation) on quaternions gives correct results only
     * near t=0 and t=1.  For t in the middle the interpolated rotation can
     * move at a non-constant angular speed ("ease in / ease out").
     * SLERP (spherical linear interpolation) always produces constant-speed,
     * shortest-arc rotation.  It is the standard for animation blending.
     *
     * When dot(q1, q2) < 0 we negate q2 to ensure the shortest arc (the
     * quaternion q and -q represent the same rotation but opposite paths on
     * the 4-D sphere).
     */
    static Quat Slerp(const Quat& a, const Quat& b, float t)
    {
        Quat an = a.Normalized();
        Quat bn = b.Normalized();

        float dot = an.x*bn.x + an.y*bn.y + an.z*bn.z + an.w*bn.w;

        // Choose shortest arc.
        if (dot < 0.0f)
        {
            bn  = { -bn.x, -bn.y, -bn.z, -bn.w };
            dot = -dot;
        }

        // When quaternions are very close, LERP to avoid numerical instability.
        if (dot > 1.0f - kEps)
        {
            float s = 1.0f - t;
            return Quat{ an.x*s + bn.x*t, an.y*s + bn.y*t,
                         an.z*s + bn.z*t, an.w*s + bn.w*t }.Normalized();
        }

        float theta = std::acos(dot);
        float sinT  = std::sin(theta);
        float fa    = std::sin((1.0f - t) * theta) / sinT;
        float fb    = std::sin(        t  * theta) / sinT;

        return Quat{ fa*an.x + fb*bn.x,
                     fa*an.y + fb*bn.y,
                     fa*an.z + fb*bn.z,
                     fa*an.w + fb*bn.w }.Normalized();
    }
};

// ===========================================================================
// Mat4 — row-major 4×4 float matrix
// ===========================================================================

/**
 * @struct Mat4
 * @brief 4×4 floating-point matrix in row-major order.
 *
 * TEACHING NOTE — Row-Major and D3D11
 * D3D11 HLSL matrices are row-major by default.  A transform applied to a
 * row-vector looks like:
 *
 *   float4 pos = mul(float4(v, 1), worldMatrix);
 *
 * where worldMatrix's rows are the world-space axes of the object.
 * This matches how we store Mat4 here: m[row][col].
 *
 * TEACHING NOTE — Homogeneous Coordinates
 * The 4×4 matrix adds a 4th component (w) to every vector, enabling
 * translation to be expressed as a linear operation.  Without the w
 * component, translation would require an addition separate from the
 * matrix multiply.
 *
 * A position vector has w=1 (so translation applies).
 * A direction vector has w=0 (translation does NOT apply).
 */
struct Mat4
{
    // m[row][col] — row-major storage.
    std::array<std::array<float, 4>, 4> m;

    Mat4() { m = {}; }

    // -----------------------------------------------------------------------
    // Static factories
    // -----------------------------------------------------------------------

    static Mat4 Identity()
    {
        Mat4 r;
        r.m[0][0] = r.m[1][1] = r.m[2][2] = r.m[3][3] = 1.0f;
        return r;
    }

    /**
     * @brief Build a translation matrix.
     * TEACHING NOTE — Translation in a 4×4 matrix lives in the last column
     * (column-major) or last row (row-major).  In row-major (D3D11):
     *   1 0 0 0
     *   0 1 0 0
     *   0 0 1 0
     *   tx ty tz 1    ← translation goes in the 4th ROW, columns 0-2
     */
    static Mat4 Translation(float tx, float ty, float tz)
    {
        Mat4 r = Identity();
        r.m[3][0] = tx;
        r.m[3][1] = ty;
        r.m[3][2] = tz;
        return r;
    }

    static Mat4 Translation(const Vec3& t) { return Translation(t.x, t.y, t.z); }

    /**
     * @brief Build a uniform scale matrix.
     */
    static Mat4 Scale(float sx, float sy, float sz)
    {
        Mat4 r = Identity();
        r.m[0][0] = sx;
        r.m[1][1] = sy;
        r.m[2][2] = sz;
        return r;
    }

    static Mat4 Scale(const Vec3& s) { return Scale(s.x, s.y, s.z); }

    /**
     * @brief Build a rotation matrix from a unit quaternion.
     *
     * TEACHING NOTE — Quaternion to Rotation Matrix
     * The 3×3 rotation block of the 4×4 matrix can be computed from the
     * quaternion components without any trigonometric functions:
     *
     *   R = | 1-2(y²+z²)   2(xy-wz)    2(xz+wy) |
     *       | 2(xy+wz)    1-2(x²+z²)   2(yz-wx) |
     *       | 2(xz-wy)    2(yz+wx)    1-2(x²+y²)|
     *
     * Because quaternions encode the rotation directly, this avoids the
     * gimbal-lock problem that Euler-angle rotation matrices can suffer from.
     */
    static Mat4 Rotation(const Quat& q)
    {
        Quat n  = q.Normalized();
        float x = n.x, y = n.y, z = n.z, w = n.w;

        Mat4 r = Identity();
        r.m[0][0] = 1.0f - 2.0f*(y*y + z*z);
        r.m[0][1] = 2.0f*(x*y - w*z);
        r.m[0][2] = 2.0f*(x*z + w*y);

        r.m[1][0] = 2.0f*(x*y + w*z);
        r.m[1][1] = 1.0f - 2.0f*(x*x + z*z);
        r.m[1][2] = 2.0f*(y*z - w*x);

        r.m[2][0] = 2.0f*(x*z - w*y);
        r.m[2][1] = 2.0f*(y*z + w*x);
        r.m[2][2] = 1.0f - 2.0f*(x*x + y*y);

        return r;
    }

    /**
     * @brief Build a TRS (Translation × Rotation × Scale) matrix.
     *
     * TEACHING NOTE — TRS Matrix Order
     * In D3D11 row-major convention (row vectors) the transform order is:
     *   final = Scale · Rotation · Translation
     * which means: first scale, then rotate, then translate.
     * (In column-major / OpenGL convention the order is reversed.)
     */
    static Mat4 TRS(const Vec3& t, const Quat& r, const Vec3& s)
    {
        return Scale(s) * Rotation(r) * Translation(t);
    }

    // -----------------------------------------------------------------------
    // Matrix multiplication — row-major convention.
    // TEACHING NOTE — Matrix multiply: (A * B)[i][j] = Σ_k A[i][k] * B[k][j]
    // -----------------------------------------------------------------------
    Mat4 operator*(const Mat4& o) const
    {
        Mat4 res;
        for (int row = 0; row < 4; ++row)
            for (int col = 0; col < 4; ++col)
            {
                float sum = 0.0f;
                for (int k = 0; k < 4; ++k)
                    sum += m[row][k] * o.m[k][col];
                res.m[row][col] = sum;
            }
        return res;
    }

    // -----------------------------------------------------------------------
    // Flat float array — for uploading to D3D11 constant buffers.
    // TEACHING NOTE — D3D11 expects a contiguous array of 16 floats in
    // row-major order, which is exactly what std::array<std::array<float,4>,4>
    // provides (both arrays are contiguous).
    // -----------------------------------------------------------------------
    const float* Data() const { return &m[0][0]; }
};

} // namespace math
} // namespace engine
