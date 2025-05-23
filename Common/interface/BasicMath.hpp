/*
 *  Copyright 2019-2025 Diligent Graphics LLC
 *  Copyright 2015-2019 Egor Yusov
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 *  In no event and under no legal theory, whether in tort (including negligence),
 *  contract, or otherwise, unless required by applicable law (such as deliberate
 *  and grossly negligent acts) or agreed to in writing, shall any Contributor be
 *  liable for any damages, including any direct, indirect, special, incidental,
 *  or consequential damages of any character arising as a result of this License or
 *  out of the use or inability to use the software (including but not limited to damages
 *  for loss of goodwill, work stoppage, computer failure or malfunction, or any and
 *  all other commercial damages or losses), even if such Contributor has been advised
 *  of the possibility of such damages.
 */

#pragma once

/// The library uses Direct3D-style math:
///
///   - Matrices are multiplied left to right in the order corresponding transforms are applied:
///     - WorldViewProj = World * View * Proj
///   - Vectors are row-vectors and multiplied by matrices as v * m:
///     - ClipPos = WorldPos * WorldViewProj
///   - Matrices are stored using row-major layout: m = {row0, row1, row2, row3}
///     - Note that GL-style math libraries use column-vectors and column-major matrix layout.
///       As a result, matrices that perform similar transforms use exactly the same element
///       order. However, matrix multiplication order is reversed: M1_D3D * M2_D3D = M2_GL * M1_GL
///
///  Diligent Engine shaders always use column-major matrices for the purposes of data storage. This means
///  that if you use D3D-style math in shaders (ClipPos = mul(WorldPos, WorldViewProj)), you need to
///  transpose the host-side matrix before writing it to GPU memory.
///
///  If you use GL-style math in shaders (ClipPos = mul(WorldViewProj, WorldPos)), you do not need to
///  transpose the host-side matrix and should write it to GPU memory as is. Since the matrix rows will
///  be written to the GPU matrix columns, this will have the effect of transposing the matrix.
///  Since mul(WorldViewProj, WorldPos) == mul(WorldPos, transpose(WorldViewProj)), the results will
///  be consistent with D3D case.

#include <cmath>
#include <algorithm>
#include <iostream>

#include "HashUtils.hpp"

#ifdef _MSC_VER
#    pragma warning(push)
#    pragma warning(disable : 4201) // nonstandard extension used: nameless struct/union
#endif

namespace Diligent
{

static constexpr double PI   = 3.14159265358979323846;
static constexpr float  PI_F = 3.1415927f;

// Template Vector & Matrix Classes
template <class T> struct Matrix2x2;
template <class T> struct Matrix3x3;
template <class T> struct Matrix4x4;
template <class T> struct Vector4;

template <class T> struct Vector2
{
    using ValueType = T;

    union
    {
        struct
        {
            T x;
            T y;
        };
        struct
        {
            T r;
            T g;
        };
        struct
        {
            T u;
            T v;
        };
    };

    constexpr Vector2(const Vector2&) = default;
    constexpr Vector2(Vector2&&)      = default;
    constexpr Vector2& operator=(const Vector2&) = default;
    constexpr Vector2& operator=(Vector2&&) = default;

    constexpr Vector2 operator-(const Vector2<T>& right) const
    {
        return Vector2{x - right.x, y - right.y};
    }

    Vector2& operator-=(const Vector2<T>& right)
    {
        x -= right.x;
        y -= right.y;
        return *this;
    }

    constexpr Vector2 operator-() const
    {
        return Vector2{-x, -y};
    }

    constexpr Vector2 operator+(const Vector2<T>& right) const
    {
        return Vector2{x + right.x, y + right.y};
    }

    Vector2& operator+=(const Vector2<T>& right)
    {
        x += right.x;
        y += right.y;
        return *this;
    }

    constexpr Vector2 operator*(T s) const
    {
        return Vector2{x * s, y * s};
    }

    constexpr Vector2 operator*(const Vector2& right) const
    {
        return Vector2{x * right.x, y * right.y};
    }

    Vector2& operator*=(const Vector2& right)
    {
        x *= right.x;
        y *= right.y;
        return *this;
    }

    Vector2& operator*=(T s)
    {
        x *= s;
        y *= s;
        return *this;
    }

    constexpr Vector2 operator*(const Matrix2x2<T>& m) const
    {
        Vector2 out;
        out[0] = x * m[0][0] + y * m[1][0];
        out[1] = x * m[0][1] + y * m[1][1];
        return out;
    }

    constexpr Vector2 operator/(const Vector2& right) const
    {
        return Vector2{x / right.x, y / right.y};
    }

    constexpr Vector2& operator/=(const Vector2& right)
    {
        x /= right.x;
        y /= right.y;
        return *this;
    }

    constexpr Vector2 operator/(T s) const
    {
        return Vector2{x / s, y / s};
    }

    Vector2& operator/=(T s)
    {
        x /= s;
        y /= s;
        return *this;
    }

    constexpr bool operator==(const Vector2& right) const
    {
        return x == right.x && y == right.y;
    }

    constexpr bool operator!=(const Vector2& right) const
    {
        return !(*this == right);
    }

    constexpr Vector2 operator<(const Vector2& right) const
    {
        return Vector2{x < right.x ? static_cast<T>(1) : static_cast<T>(0),
                       y < right.y ? static_cast<T>(1) : static_cast<T>(0)};
    }

    constexpr Vector2 operator>(const Vector2& right) const
    {
        return Vector2{x > right.x ? static_cast<T>(1) : static_cast<T>(0),
                       y > right.y ? static_cast<T>(1) : static_cast<T>(0)};
    }

    constexpr Vector2 operator<=(const Vector2& right) const
    {
        return Vector2{x <= right.x ? static_cast<T>(1) : static_cast<T>(0),
                       y <= right.y ? static_cast<T>(1) : static_cast<T>(0)};
    }

    constexpr Vector2 operator>=(const Vector2& right) const
    {
        return Vector2{x >= right.x ? static_cast<T>(1) : static_cast<T>(0),
                       y >= right.y ? static_cast<T>(1) : static_cast<T>(0)};
    }

    T* Data() { return reinterpret_cast<T*>(this); }

    const T* Data() const { return reinterpret_cast<const T*>(this); }

    static constexpr size_t GetComponentCount() { return 2; }

    T& operator[](size_t index)
    {
        VERIFY_EXPR(index < GetComponentCount());
        return Data()[index];
    }

    const T& operator[](size_t index) const
    {
        VERIFY_EXPR(index < GetComponentCount());
        return Data()[index];
    }

    constexpr Vector2() noexcept :
        x{0}, y{0} {}
    constexpr explicit Vector2(T s) noexcept :
        x{s}, y{s} {}
    constexpr Vector2(T _x, T _y) noexcept :
        x{_x}, y{_y} {}

    template <typename Y>
    static constexpr Vector2 MakeVector(const Y& vals)
    {
        return Vector2 //
            {
                static_cast<T>(vals[0]),
                static_cast<T>(vals[1]) //
            };
    }

    template <typename Y>
    constexpr Vector2<Y> Recast() const
    {
        return Vector2<Y>{static_cast<Y>(x),
                          static_cast<Y>(y)};
    }
};

template <class T>
constexpr Vector2<T> operator*(T s, const Vector2<T>& a)
{
    return a * s;
}


template <class T> struct Vector3
{
    using ValueType = T;

    union
    {
        struct
        {
            T x;
            T y;
            T z;
        };
        struct
        {
            T r;
            T g;
            T b;
        };
        struct
        {
            T u;
            T v;
            T w;
        };
    };

    constexpr Vector3(const Vector3&) = default;
    constexpr Vector3(Vector3&&)      = default;
    constexpr Vector3& operator=(const Vector3&) = default;
    constexpr Vector3& operator=(Vector3&&) = default;

    constexpr Vector3 operator-(const Vector3& right) const
    {
        return Vector3{x - right.x, y - right.y, z - right.z};
    }

    constexpr Vector3 operator-() const
    {
        return Vector3{-x, -y, -z};
    }

    Vector3& operator-=(const Vector3<T>& right)
    {
        x -= right.x;
        y -= right.y;
        z -= right.z;
        return *this;
    }

    constexpr Vector3 operator+(const Vector3& right) const
    {
        return Vector3{x + right.x, y + right.y, z + right.z};
    }

    Vector3& operator+=(const Vector3<T>& right)
    {
        x += right.x;
        y += right.y;
        z += right.z;
        return *this;
    }

    constexpr Vector3 operator*(T s) const
    {
        return Vector3{x * s, y * s, z * s};
    }

    Vector3& operator*=(T s)
    {
        x *= s;
        y *= s;
        z *= s;
        return *this;
    }

    constexpr Vector3 operator*(const Vector3& right) const
    {
        return Vector3{x * right.x, y * right.y, z * right.z};
    }

    constexpr Vector3 operator*(const Matrix4x4<T>& m) const
    {
        Vector4<T> out4 = Vector4<T>(x, y, z, 1) * m;
        return Vector3{out4.x / out4.w, out4.y / out4.w, out4.z / out4.w};
    }

    Vector3& operator*=(const Vector3& right)
    {
        x *= right.x;
        y *= right.y;
        z *= right.z;
        return *this;
    }

    constexpr Vector3 operator*(const Matrix3x3<T>& m) const
    {
        Vector3 out;
        out[0] = x * m[0][0] + y * m[1][0] + z * m[2][0];
        out[1] = x * m[0][1] + y * m[1][1] + z * m[2][1];
        out[2] = x * m[0][2] + y * m[1][2] + z * m[2][2];
        return out;
    }

    constexpr Vector3 operator/(T s) const
    {
        return Vector3{x / s, y / s, z / s};
    }

    Vector3& operator/=(T s)
    {
        x /= s;
        y /= s;
        z /= s;
        return *this;
    }

    constexpr Vector3 operator/(const Vector3& right) const
    {
        return Vector3{x / right.x, y / right.y, z / right.z};
    }

    Vector3& operator/=(const Vector3& right)
    {
        x /= right.x;
        y /= right.y;
        z /= right.z;
        return *this;
    }

    constexpr bool operator==(const Vector3& right) const
    {
        return x == right.x && y == right.y && z == right.z;
    }

    constexpr bool operator!=(const Vector3& right) const
    {
        return !(*this == right);
    }

    constexpr Vector3 operator<(const Vector3& right) const
    {
        return Vector3{x < right.x ? static_cast<T>(1) : static_cast<T>(0),
                       y < right.y ? static_cast<T>(1) : static_cast<T>(0),
                       z < right.z ? static_cast<T>(1) : static_cast<T>(0)};
    }

    constexpr Vector3 operator>(const Vector3& right) const
    {
        return Vector3{x > right.x ? static_cast<T>(1) : static_cast<T>(0),
                       y > right.y ? static_cast<T>(1) : static_cast<T>(0),
                       z > right.z ? static_cast<T>(1) : static_cast<T>(0)};
    }

    constexpr Vector3 operator<=(const Vector3& right) const
    {
        return Vector3{x <= right.x ? static_cast<T>(1) : static_cast<T>(0),
                       y <= right.y ? static_cast<T>(1) : static_cast<T>(0),
                       z <= right.z ? static_cast<T>(1) : static_cast<T>(0)};
    }

    constexpr Vector3 operator>=(const Vector3& right) const
    {
        return Vector3{x >= right.x ? static_cast<T>(1) : static_cast<T>(0),
                       y >= right.y ? static_cast<T>(1) : static_cast<T>(0),
                       z >= right.z ? static_cast<T>(1) : static_cast<T>(0)};
    }

    T* Data() { return reinterpret_cast<T*>(this); }

    const T* Data() const { return reinterpret_cast<const T*>(this); }

    static constexpr size_t GetComponentCount() { return 3; }

    T& operator[](size_t index)
    {
        VERIFY_EXPR(index < GetComponentCount());
        return Data()[index];
    }

    const T& operator[](size_t index) const
    {
        VERIFY_EXPR(index < GetComponentCount());
        return Data()[index];
    }

    constexpr Vector3() noexcept :
        x{0}, y{0}, z{0} {}
    constexpr explicit Vector3(T s) noexcept :
        x{s}, y{s}, z{s} {}
    constexpr Vector3(T _x, T _y, T _z) noexcept :
        x{_x}, y{_y}, z{_z} {}
    constexpr Vector3(const Vector2<T>& v2, T _z) noexcept :
        x{v2.x}, y{v2.y}, z{_z} {}


    template <typename Y>
    constexpr static Vector3 MakeVector(const Y& vals)
    {
        return Vector3 //
            {
                static_cast<T>(vals[0]),
                static_cast<T>(vals[1]),
                static_cast<T>(vals[2]) //
            };
    }

    template <typename Y>
    constexpr Vector3<Y> Recast() const
    {
        return Vector3<Y>{static_cast<Y>(x),
                          static_cast<Y>(y),
                          static_cast<Y>(z)};
    }

    constexpr operator Vector2<T>() const { return Vector2<T>(x, y); }
};

template <class T>
constexpr Vector3<T> operator*(T s, const Vector3<T>& a)
{
    return a * s;
}


template <class T> struct Vector4
{
    using ValueType = T;

    union
    {
        struct
        {
            T x;
            T y;
            T z;
            T w;
        };
        struct
        {
            T r;
            T g;
            T b;
            T a;
        };
    };

    constexpr Vector4(const Vector4&) = default;
    constexpr Vector4(Vector4&&)      = default;
    constexpr Vector4& operator=(const Vector4&) = default;
    constexpr Vector4& operator=(Vector4&&) = default;

    constexpr Vector4 operator-(const Vector4& right) const
    {
        return Vector4{x - right.x, y - right.y, z - right.z, w - right.w};
    }

    constexpr Vector4 operator-() const
    {
        return Vector4{-x, -y, -z, -w};
    }

    Vector4& operator-=(const Vector4<T>& right)
    {
        x -= right.x;
        y -= right.y;
        z -= right.z;
        w -= right.w;
        return *this;
    }

    constexpr Vector4 operator+(const Vector4& right) const
    {
        return Vector4{x + right.x, y + right.y, z + right.z, w + right.w};
    }

    Vector4& operator+=(const Vector4<T>& right)
    {
        x += right.x;
        y += right.y;
        z += right.z;
        w += right.w;
        return *this;
    }

    constexpr Vector4 operator*(T s) const
    {
        return Vector4{x * s, y * s, z * s, w * s};
    }

    Vector4& operator*=(T s)
    {
        x *= s;
        y *= s;
        z *= s;
        w *= s;
        return *this;
    }

    constexpr Vector4 operator*(const Vector4& right) const
    {
        return Vector4{x * right.x, y * right.y, z * right.z, w * right.w};
    }

    Vector4& operator*=(const Vector4& right)
    {
        x *= right.x;
        y *= right.y;
        z *= right.z;
        w *= right.w;
        return *this;
    }

    constexpr Vector4 operator/(T s) const
    {
        return Vector4{x / s, y / s, z / s, w / s};
    }

    Vector4& operator/=(T s)
    {
        x /= s;
        y /= s;
        z /= s;
        w /= s;
        return *this;
    }

    constexpr Vector4 operator/(const Vector4& right) const
    {
        return Vector4{x / right.x, y / right.y, z / right.z, w / right.w};
    }

    Vector4& operator/=(const Vector4& right)
    {
        x /= right.x;
        y /= right.y;
        z /= right.z;
        w /= right.w;
        return *this;
    }

    constexpr bool operator==(const Vector4& right) const
    {
        return x == right.x && y == right.y && z == right.z && w == right.w;
    }

    constexpr bool operator!=(const Vector4& right) const
    {
        return !(*this == right);
    }

    constexpr Vector4 operator*(const Matrix4x4<T>& m) const
    {
        Vector4 out;
        out[0] = x * m[0][0] + y * m[1][0] + z * m[2][0] + w * m[3][0];
        out[1] = x * m[0][1] + y * m[1][1] + z * m[2][1] + w * m[3][1];
        out[2] = x * m[0][2] + y * m[1][2] + z * m[2][2] + w * m[3][2];
        out[3] = x * m[0][3] + y * m[1][3] + z * m[2][3] + w * m[3][3];
        return out;
    }

    Vector4& operator=(const Vector3<T>& v3)
    {
        x = v3.x;
        y = v3.y;
        z = v3.z;
        w = 1;
        return *this;
    }

    constexpr Vector4 operator<(const Vector4& right) const
    {
        return Vector4{x < right.x ? static_cast<T>(1) : static_cast<T>(0),
                       y < right.y ? static_cast<T>(1) : static_cast<T>(0),
                       z < right.z ? static_cast<T>(1) : static_cast<T>(0),
                       w < right.w ? static_cast<T>(1) : static_cast<T>(0)};
    }

    constexpr Vector4 operator>(const Vector4& right) const
    {
        return Vector4{x > right.x ? static_cast<T>(1) : static_cast<T>(0),
                       y > right.y ? static_cast<T>(1) : static_cast<T>(0),
                       z > right.z ? static_cast<T>(1) : static_cast<T>(0),
                       w > right.w ? static_cast<T>(1) : static_cast<T>(0)};
    }

    constexpr Vector4 operator<=(const Vector4& right) const
    {
        return Vector4{x <= right.x ? static_cast<T>(1) : static_cast<T>(0),
                       y <= right.y ? static_cast<T>(1) : static_cast<T>(0),
                       z <= right.z ? static_cast<T>(1) : static_cast<T>(0),
                       w <= right.w ? static_cast<T>(1) : static_cast<T>(0)};
    }

    constexpr Vector4 operator>=(const Vector4& right) const
    {
        return Vector4{x >= right.x ? static_cast<T>(1) : static_cast<T>(0),
                       y >= right.y ? static_cast<T>(1) : static_cast<T>(0),
                       z >= right.z ? static_cast<T>(1) : static_cast<T>(0),
                       w >= right.w ? static_cast<T>(1) : static_cast<T>(0)};
    }

    T* Data() { return reinterpret_cast<T*>(this); }

    const T* Data() const { return reinterpret_cast<const T*>(this); }

    static constexpr size_t GetComponentCount() { return 4; }

    T& operator[](size_t index)
    {
        VERIFY_EXPR(index < GetComponentCount());
        return Data()[index];
    }

    const T& operator[](size_t index) const
    {
        VERIFY_EXPR(index < GetComponentCount());
        return Data()[index];
    }

    constexpr Vector4() noexcept :
        x{0}, y{0}, z{0}, w{0} {}
    constexpr explicit Vector4(T s) noexcept :
        x{s}, y{s}, z{s}, w{s} {}
    constexpr Vector4(T _x, T _y, T _z, T _w) noexcept :
        x{_x}, y{_y}, z{_z}, w{_w} {}
    constexpr Vector4(const Vector3<T>& v3, T _w) noexcept :
        x{v3.x}, y{v3.y}, z{v3.z}, w{_w} {}
    constexpr Vector4(const Vector2<T>& v2, T _z, T _w) noexcept :
        x{v2.x}, y{v2.y}, z{_z}, w{_w} {}
    constexpr Vector4(const Vector2<T>& xy, const Vector2<T>& zw) noexcept :
        x{xy.x}, y{xy.y}, z{zw.x}, w{zw.y} {}

    template <typename Y>
    constexpr static Vector4 MakeVector(const Y& vals)
    {
        return Vector4 //
            {
                static_cast<T>(vals[0]),
                static_cast<T>(vals[1]),
                static_cast<T>(vals[2]),
                static_cast<T>(vals[3]) //
            };
    }

    template <typename Y>
    constexpr Vector4<Y> Recast() const
    {
        return Vector4<Y>{static_cast<Y>(x),
                          static_cast<Y>(y),
                          static_cast<Y>(z),
                          static_cast<Y>(w)};
    }

    constexpr operator Vector3<T>() const
    {
        return Vector3<T>(x, y, z);
    }
};


template <class T>
constexpr Vector4<T> operator*(T s, const Vector4<T>& a)
{
    return a * s;
}


template <class T> struct Matrix2x2
{
    using ValueType = T;

    union
    {
        struct
        {
            T _11;
            T _12;
            T _21;
            T _22;
        };
        struct
        {
            T m00;
            T m01;
            T m10;
            T m11;
        };
        T m[2][2];
    };

    constexpr explicit Matrix2x2(T value) noexcept :
        // clang-format off
        _11{value}, _12{value},
        _21{value}, _22{value}
    // clang-format on
    {
    }

    constexpr Matrix2x2() noexcept :
        Matrix2x2{0} {}

    // clang-format off
    constexpr Matrix2x2(T i11, T i12,
                        T i21, T i22) noexcept:
        _11{i11}, _12{i12},
        _21{i21}, _22{i22}
    // clang-format on
    {
    }

    // clang-format off
    constexpr Matrix2x2(const Vector2<T>& Row0,
                        const Vector2<T>& Row1) noexcept:
        _11{Row0.x}, _12{Row0.y},
        _21{Row1.x}, _22{Row1.y}
    // clang-format on
    {
    }



    template <typename Y>
    constexpr static Matrix2x2 MakeMatrix(const Y& vals)
    {
        return Matrix2x2 //
            {
                static_cast<T>(vals[0]), static_cast<T>(vals[1]),
                static_cast<T>(vals[2]), static_cast<T>(vals[3]) //
            };
    }

    constexpr bool operator==(const Matrix2x2& r) const
    {
        for (int i = 0; i < 2; ++i)
            for (int j = 0; j < 2; ++j)
                if ((*this)[i][j] != r[i][j])
                    return false;

        return true;
    }

    constexpr bool operator!=(const Matrix2x2& r) const
    {
        return !(*this == r);
    }

    T* operator[](size_t row)
    {
        return m[row];
    }

    const T* operator[](size_t row) const
    {
        return m[row];
    }

    T* Data() { return (*this)[0]; }

    const T* Data() const { return (*this)[0]; }


    Matrix2x2& operator*=(T s)
    {
        for (int i = 0; i < 4; ++i)
            (reinterpret_cast<T*>(this))[i] *= s;

        return *this;
    }

    Matrix2x2& operator/=(T s)
    {
        for (int i = 0; i < 4; ++i)
            (reinterpret_cast<T*>(this))[i] /= s;

        return *this;
    }

    Matrix2x2& operator*=(const Matrix2x2& right)
    {
        *this = Mul(*this, right);
        return *this;
    }

    Matrix2x2& operator+=(const Matrix2x2& right)
    {
        for (int i = 0; i < 4; ++i)
            Data()[i] += right.Data()[i];
        return *this;
    }

    Matrix2x2 operator+(const Matrix2x2& right) const
    {
        // clang-format off
        return Matrix2x2
        {
            _11 + right._11, _12 + right._12,
            _21 + right._21, _22 + right._22
        };
        // clang-format on
    }

    Matrix2x2& operator-=(const Matrix2x2& right)
    {
        for (int i = 0; i < 4; ++i)
            Data()[i] -= right.Data()[i];
        return *this;
    }

    Matrix2x2 operator-(const Matrix2x2& right) const
    {
        // clang-format off
        return Matrix2x2
        {
            _11 - right._11, _12 - right._12,
            _21 - right._21, _22 - right._22
        };
        // clang-format on
    }

    constexpr Matrix2x2 Transpose() const
    {
        return Matrix2x2{
            _11, _21,
            _12, _22};
    }

    constexpr static Matrix2x2 Identity()
    {
        return Matrix2x2{
            1, 0,
            0, 1};
    }

    constexpr static Matrix2x2 Scale(T x, T y)
    {
        return Matrix2x2{
            x, 0,
            0, y};
    }

    constexpr static Matrix2x2 Mul(const Matrix2x2& m1, const Matrix2x2& m2)
    {
        Matrix2x2 mOut;
        for (int i = 0; i < 2; i++)
        {
            for (int j = 0; j < 2; j++)
            {
                for (int k = 0; k < 2; k++)
                {
                    mOut.m[i][j] += m1.m[i][k] * m2.m[k][j];
                }
            }
        }
        return mOut;
    }

    constexpr static Matrix2x2 Rotation(T angleInRadians)
    {
        T s = std::sin(angleInRadians);
        T c = std::cos(angleInRadians);

        return Matrix2x2 //
            {
                c, s,
                -s, c //
            };
    }

    constexpr T Determinant() const
    {
        return _11 * _22 - _12 * _21;
    }

    constexpr Matrix2x2 Inverse() const
    {
        Matrix2x2 Inv //
            {
                +_22, -_12,
                -_21, +_11 //
            };

        Inv *= static_cast<T>(1) / Determinant();
        return Inv;
    }

    template <typename Y>
    constexpr Matrix2x2<Y> Recast() const
    {
        return Matrix2x2<Y>::MakeMatrix(Data());
    }

    template <typename Y = T>
    constexpr Vector4<Y> ToVec4() const
    {
        return Vector4<Y>{static_cast<Y>(_11), static_cast<Y>(_12), static_cast<Y>(_21), static_cast<Y>(_22)};
    }

    template <typename Y>
    static constexpr Matrix2x2 FromVec4(const Vector4<Y>& v)
    {
        return Matrix2x2{static_cast<T>(v.x), static_cast<T>(v.y), static_cast<T>(v.z), static_cast<T>(v.w)};
    }
};

template <typename T>
inline constexpr Matrix2x2<T> operator*(const Matrix2x2<T>& Mat, T s)
{
    // clang-format off
    return 
    {
        Mat._11 * s, Mat._12 * s,
        Mat._21 * s, Mat._22 * s
    };
    // clang-format on
}

template <typename T>
inline constexpr Matrix2x2<T> operator*(T s, const Matrix2x2<T>& Mat)
{
    return Mat * s;
}

template <typename T>
inline constexpr Matrix2x2<T> operator/(const Matrix2x2<T>& Mat, T s)
{
    // clang-format off
    return 
    {
        Mat._11 / s, Mat._12 / s,
        Mat._21 / s, Mat._22 / s
    };
    // clang-format on
}


template <class T> struct Matrix3x3
{
    using ValueType = T;

    union
    {
        struct
        {
            T _11;
            T _12;
            T _13;
            T _21;
            T _22;
            T _23;
            T _31;
            T _32;
            T _33;
        };
        struct
        {
            T m00;
            T m01;
            T m02;
            T m10;
            T m11;
            T m12;
            T m20;
            T m21;
            T m22;
        };
        T m[3][3];
    };

    constexpr explicit Matrix3x3(T value) noexcept :
        // clang-format off
        _11{value}, _12{value}, _13{value},
        _21{value}, _22{value}, _23{value},
        _31{value}, _32{value}, _33{value}
    // clang-format on
    {
    }

    constexpr Matrix3x3() noexcept :
        Matrix3x3{0} {}

    // clang-format off
    constexpr Matrix3x3(T i11, T i12, T i13,
                        T i21, T i22, T i23,
                        T i31, T i32, T i33) noexcept :
        _11{i11}, _12{i12}, _13{i13},
        _21{i21}, _22{i22}, _23{i23},
        _31{i31}, _32{i32}, _33{i33}
    // clang-format on
    {
    }

    // clang-format off
    constexpr Matrix3x3(const Vector3<T>& Row0,
                        const Vector3<T>& Row1,
                        const Vector3<T>& Row2) noexcept :
        _11{Row0.x}, _12{Row0.y}, _13{Row0.z},
        _21{Row1.x}, _22{Row1.y}, _23{Row1.z},
        _31{Row2.x}, _32{Row2.y}, _33{Row2.z}
    // clang-format on
    {
    }

    template <typename Y>
    constexpr static Matrix3x3 MakeMatrix(const Y& vals)
    {
        return Matrix3x3 //
            {
                static_cast<T>(vals[0]), static_cast<T>(vals[1]), static_cast<T>(vals[2]),
                static_cast<T>(vals[3]), static_cast<T>(vals[4]), static_cast<T>(vals[5]),
                static_cast<T>(vals[6]), static_cast<T>(vals[7]), static_cast<T>(vals[8]) //
            };
    }

    constexpr bool operator==(const Matrix3x3& r) const
    {
        for (int i = 0; i < 3; ++i)
            for (int j = 0; j < 3; ++j)
                if ((*this)[i][j] != r[i][j])
                    return false;

        return true;
    }

    constexpr bool operator!=(const Matrix3x3& r) const
    {
        return !(*this == r);
    }

    T* operator[](size_t row)
    {
        return m[row];
    }

    const T* operator[](size_t row) const
    {
        return m[row];
    }

    T* Data() { return (*this)[0]; }

    const T* Data() const { return (*this)[0]; }

    Matrix3x3& operator*=(T s)
    {
        for (int i = 0; i < 9; ++i)
            (reinterpret_cast<T*>(this))[i] *= s;

        return *this;
    }

    Matrix3x3& operator/=(T s)
    {
        for (int i = 0; i < 9; ++i)
            (reinterpret_cast<T*>(this))[i] /= s;

        return *this;
    }

    Matrix3x3& operator+=(const Matrix3x3& right)
    {
        for (int i = 0; i < 9; ++i)
            Data()[i] += right.Data()[i];
        return *this;
    }

    Matrix3x3 operator+(const Matrix3x3& right) const
    {
        // clang-format off
        return Matrix3x3
        {
            _11 + right._11, _12 + right._12, _13 + right._13,
            _21 + right._21, _22 + right._22, _23 + right._23,
            _31 + right._31, _32 + right._32, _33 + right._33
        };
        // clang-format on
    }

    Matrix3x3& operator-=(const Matrix3x3& right)
    {
        for (int i = 0; i < 9; ++i)
            Data()[i] -= right.Data()[i];
        return *this;
    }

    Matrix3x3 operator-(const Matrix3x3& right) const
    {
        // clang-format off
        return Matrix3x3
        {
            _11 - right._11, _12 - right._12, _13 - right._13,
            _21 - right._21, _22 - right._22, _23 - right._23,
            _31 - right._31, _32 - right._32, _33 - right._33
        };
        // clang-format on
    }

    Matrix3x3& operator*=(const Matrix3x3& right)
    {
        *this = Mul(*this, right);
        return *this;
    }

    constexpr Matrix3x3 Transpose() const
    {
        return Matrix3x3 //
            {
                _11, _21, _31,
                _12, _22, _32,
                _13, _23, _33 //
            };
    }

    constexpr static Matrix3x3 Identity()
    {
        return Matrix3x3 //
            {
                1, 0, 0,
                0, 1, 0,
                0, 0, 1 //
            };
    }

    constexpr static Matrix3x3 Scale(T x, T y, T z)
    {
        return Matrix3x3 //
            {
                x, 0, 0,
                0, y, 0,
                0, 0, z //
            };
    }

    // D3D-style left-handed matrix that rotates a point around the x-axis. Angle (in radians)
    // is measured clockwise when looking along the rotation axis toward the origin:
    // (x' y' z') = (x y z) * RotationX
    static Matrix3x3 RotationX(T angleInRadians)
    {
        T s = std::sin(angleInRadians);
        T c = std::cos(angleInRadians);

        return Matrix3x3 // clang-format off
            {
                1,  0,  0,
                0,  c,  s,
                0, -s,  c // clang-format on
            };
    }

    // D3D-style left-handed matrix that rotates a point around the y-axis. Angle (in radians)
    // is measured clockwise when looking along the rotation axis toward the origin:
    // (x' y' z' 1) = (x y z 1) * RotationY
    static Matrix3x3 RotationY(T angleInRadians)
    {
        T s = std::sin(angleInRadians);
        T c = std::cos(angleInRadians);

        return Matrix3x3 // clang-format off
            {
                c,  0, -s,
                0,  1,  0,
                s,  0,  c // clang-format on
            };
    }

    // D3D-style left-handed matrix that rotates a point around the z axis. Angle (in radians)
    // is measured clockwise when looking along the rotation axis toward the origin:
    // (x' y' z' 1) = (x y z 1) * RotationZ
    static Matrix3x3 RotationZ(T angleInRadians)
    {
        T s = std::sin(angleInRadians);
        T c = std::cos(angleInRadians);

        return Matrix3x3 // clang-format off
            {
                 c,  s,  0,
                -s,  c,  0,
                 0,  0,  1 // clang-format on
            };
    }

    constexpr static Matrix3x3 Mul(const Matrix3x3& m1, const Matrix3x3& m2)
    {
        Matrix3x3 mOut;
        for (int i = 0; i < 3; i++)
        {
            for (int j = 0; j < 3; j++)
            {
                for (int k = 0; k < 3; k++)
                {
                    mOut.m[i][j] += m1.m[i][k] * m2.m[k][j];
                }
            }
        }

        return mOut;
    }

    constexpr T Determinant() const
    {
        T det = 0;
        det += _11 * (_22 * _33 - _32 * _23);
        det -= _12 * (_21 * _33 - _31 * _23);
        det += _13 * (_21 * _32 - _31 * _22);
        return det;
    }

    constexpr Matrix3x3 Inverse() const
    {
        Matrix3x3 Inv;

        Inv._11 = Matrix2x2<T>(_22, _23,
                               _32, _33)
                      .Determinant();

        Inv._12 = -Matrix2x2<T>(_21, _23,
                                _31, _33)
                       .Determinant();

        Inv._13 = Matrix2x2<T>(_21, _22,
                               _31, _32)
                      .Determinant();


        Inv._21 = -Matrix2x2<T>(_12, _13,
                                _32, _33)
                       .Determinant();

        Inv._22 = Matrix2x2<T>(_11, _13,
                               _31, _33)
                      .Determinant();

        Inv._23 = -Matrix2x2<T>(_11, _12,
                                _31, _32)
                       .Determinant();


        Inv._31 = Matrix2x2<T>(_12, _13,
                               _22, _23)
                      .Determinant();

        Inv._32 = -Matrix2x2<T>(_11, _13,
                                _21, _23)
                       .Determinant();

        Inv._33 = Matrix2x2<T>(_11, _12,
                               _21, _22)
                      .Determinant();

        T det = _11 * Inv._11 + _12 * Inv._12 + _13 * Inv._13;
        Inv   = Inv.Transpose();
        Inv *= static_cast<T>(1) / det;

        return Inv;
    }

    template <typename Y>
    constexpr Matrix3x3<Y> Recast() const
    {
        return Matrix3x3<Y>::MakeMatrix(Data());
    }
};

template <typename T>
inline constexpr Matrix3x3<T> operator*(const Matrix3x3<T>& Mat, T s)
{
    // clang-format off
    return 
    {
        Mat._11 * s, Mat._12 * s, Mat._13 * s,
        Mat._21 * s, Mat._22 * s, Mat._23 * s,
        Mat._31 * s, Mat._32 * s, Mat._33 * s
    };
    // clang-format on
}

template <typename T>
inline constexpr Matrix3x3<T> operator*(T s, const Matrix3x3<T>& Mat)
{
    return Mat * s;
}

template <typename T>
inline constexpr Matrix3x3<T> operator/(const Matrix3x3<T>& Mat, T s)
{
    // clang-format off
    return 
    {
        Mat._11 / s, Mat._12 / s, Mat._13 / s,
        Mat._21 / s, Mat._22 / s, Mat._23 / s,
        Mat._31 / s, Mat._32 / s, Mat._33 / s
    };
    // clang-format on
}


template <class T> struct Matrix4x4
{
    using ValueType = T;

    union
    {
        struct
        {
            T _11;
            T _12;
            T _13;
            T _14;
            T _21;
            T _22;
            T _23;
            T _24;
            T _31;
            T _32;
            T _33;
            T _34;
            T _41;
            T _42;
            T _43;
            T _44;
        };
        struct
        {
            T m00;
            T m01;
            T m02;
            T m03;
            T m10;
            T m11;
            T m12;
            T m13;
            T m20;
            T m21;
            T m22;
            T m23;
            T m30;
            T m31;
            T m32;
            T m33;
        };
        T m[4][4];
    };

    constexpr explicit Matrix4x4(T value) noexcept :
        // clang-format off
        _11{value}, _12{value}, _13{value}, _14{value},
        _21{value}, _22{value}, _23{value}, _24{value},
        _31{value}, _32{value}, _33{value}, _34{value},
        _41{value}, _42{value}, _43{value}, _44{value}
    // clang-format on
    {
    }

    constexpr Matrix4x4() noexcept :
        Matrix4x4{0} {}

    // clang-format off
    constexpr Matrix4x4(T i11, T i12, T i13, T i14,
                        T i21, T i22, T i23, T i24,
                        T i31, T i32, T i33, T i34,
                        T i41, T i42, T i43, T i44) noexcept :
        _11{i11}, _12{i12}, _13{i13}, _14{i14},
        _21{i21}, _22{i22}, _23{i23}, _24{i24},
        _31{i31}, _32{i32}, _33{i33}, _34{i34},
        _41{i41}, _42{i42}, _43{i43}, _44{i44}
    {
    }
    // clang-format on

    // clang-format off
    constexpr Matrix4x4(const Vector4<T>& Row0,
                        const Vector4<T>& Row1,
                        const Vector4<T>& Row2,
                        const Vector4<T>& Row3) noexcept :
        _11{Row0.x}, _12{Row0.y}, _13{Row0.z}, _14{Row0.w},
        _21{Row1.x}, _22{Row1.y}, _23{Row1.z}, _24{Row1.w},
        _31{Row2.x}, _32{Row2.y}, _33{Row2.z}, _34{Row2.w},
        _41{Row3.x}, _42{Row3.y}, _43{Row3.z}, _44{Row3.w}
    {
    }
    // clang-format on

    template <typename Y>
    constexpr static Matrix4x4 MakeMatrix(const Y& vals)
    {
        // clang-format off
        return Matrix4x4
            {
                static_cast<T>(vals[ 0]), static_cast<T>(vals[ 1]), static_cast<T>(vals[ 2]), static_cast<T>(vals[ 3]),
                static_cast<T>(vals[ 4]), static_cast<T>(vals[ 5]), static_cast<T>(vals[ 6]), static_cast<T>(vals[ 7]),
                static_cast<T>(vals[ 8]), static_cast<T>(vals[ 9]), static_cast<T>(vals[10]), static_cast<T>(vals[11]),
                static_cast<T>(vals[12]), static_cast<T>(vals[13]), static_cast<T>(vals[14]), static_cast<T>(vals[15])
            };
        // clang-format on
    }

    constexpr bool operator==(const Matrix4x4& r) const
    {
        for (int i = 0; i < 4; ++i)
            for (int j = 0; j < 4; ++j)
                if ((*this)[i][j] != r[i][j])
                    return false;

        return true;
    }

    constexpr bool operator!=(const Matrix4x4& r) const
    {
        return !(*this == r);
    }

    T* operator[](size_t row)
    {
        return m[row];
    }

    const T* operator[](size_t row) const
    {
        return m[row];
    }

    T* Data() { return (*this)[0]; }

    const T* Data() const { return (*this)[0]; }

    Matrix4x4& operator*=(T s)
    {
        for (int i = 0; i < 16; ++i)
            (reinterpret_cast<T*>(this))[i] *= s;

        return *this;
    }

    Matrix4x4& operator/=(T s)
    {
        for (int i = 0; i < 16; ++i)
            (reinterpret_cast<T*>(this))[i] /= s;

        return *this;
    }

    Matrix4x4& operator*=(const Matrix4x4& right)
    {
        *this = Mul(*this, right);
        return *this;
    }

    Matrix4x4& operator+=(const Matrix4x4& right)
    {
        for (int i = 0; i < 16; ++i)
            Data()[i] += right.Data()[i];
        return *this;
    }

    Matrix4x4 operator+(const Matrix4x4& right) const
    {
        // clang-format off
        return Matrix4x4
        {
            _11 + right._11, _12 + right._12, _13 + right._13, _14 + right._14,
            _21 + right._21, _22 + right._22, _23 + right._23, _24 + right._24,
            _31 + right._31, _32 + right._32, _33 + right._33, _34 + right._34,
            _41 + right._41, _42 + right._42, _43 + right._43, _44 + right._44
        };
        // clang-format on
    }

    Matrix4x4& operator-=(const Matrix4x4& right)
    {
        for (int i = 0; i < 16; ++i)
            Data()[i] -= right.Data()[i];
        return *this;
    }

    Matrix4x4 operator-(const Matrix4x4& right) const
    {
        // clang-format off
        return Matrix4x4
        {
            _11 - right._11, _12 - right._12, _13 - right._13, _14 - right._14,
            _21 - right._21, _22 - right._22, _23 - right._23, _24 - right._24,
            _31 - right._31, _32 - right._32, _33 - right._33, _34 - right._34,
            _41 - right._41, _42 - right._42, _43 - right._43, _44 - right._44
        };
        // clang-format on
    }

    constexpr Matrix4x4 Transpose() const
    {
        return Matrix4x4 //
            {
                _11, _21, _31, _41,
                _12, _22, _32, _42,
                _13, _23, _33, _43,
                _14, _24, _34, _44 //
            };
    }

    constexpr static Matrix4x4 Identity()
    {
        return Matrix4x4 //
            {
                1, 0, 0, 0,
                0, 1, 0, 0,
                0, 0, 1, 0,
                0, 0, 0, 1 //
            };
    }

    constexpr static Matrix4x4 Translation(T x, T y, T z)
    {
        return Matrix4x4 //
            {
                1, 0, 0, 0,
                0, 1, 0, 0,
                0, 0, 1, 0,
                x, y, z, 1 //
            };
    }

    constexpr static Matrix4x4 Translation(const Vector3<T>& v)
    {
        return Translation(v.x, v.y, v.z);
    }

    constexpr static Matrix4x4 Scale(T x, T y, T z)
    {
        return Matrix4x4 //
            {
                x, 0, 0, 0,
                0, y, 0, 0,
                0, 0, z, 0,
                0, 0, 0, 1 //
            };
    }

    constexpr static Matrix4x4 Scale(const Vector3<T>& v)
    {
        return Scale(v.x, v.y, v.z);
    }

    constexpr static Matrix4x4 Scale(T s)
    {
        return Scale(s, s, s);
    }


    // D3D-style left-handed matrix that rotates a point around the x-axis. Angle (in radians)
    // is measured clockwise when looking along the rotation axis toward the origin:
    // (x' y' z' 1) = (x y z 1) * RotationX
    static Matrix4x4 RotationX(T angleInRadians)
    {
        T s = std::sin(angleInRadians);
        T c = std::cos(angleInRadians);

        return Matrix4x4 // clang-format off
            {
                1,  0,  0,  0,
                0,  c,  s,  0,
                0, -s,  c,  0,
                0,  0,  0,  1 // clang-format on
            };
    }

    // D3D-style left-handed matrix that rotates a point around the y-axis. Angle (in radians)
    // is measured clockwise when looking along the rotation axis toward the origin:
    // (x' y' z' 1) = (x y z 1) * RotationY
    static Matrix4x4 RotationY(T angleInRadians)
    {
        T s = std::sin(angleInRadians);
        T c = std::cos(angleInRadians);

        return Matrix4x4 // clang-format off
            {
                c,  0, -s,  0,
                0,  1,  0,  0,
                s,  0,  c,  0,
                0,  0,  0,  1 // clang-format on
            };
    }

    // D3D-style left-handed matrix that rotates a point around the z axis. Angle (in radians)
    // is measured clockwise when looking along the rotation axis toward the origin:
    // (x' y' z' 1) = (x y z 1) * RotationZ
    static Matrix4x4 RotationZ(T angleInRadians)
    {
        T s = std::sin(angleInRadians);
        T c = std::cos(angleInRadians);

        return Matrix4x4 // clang-format off
            {
                 c,  s,  0,  0,
                -s,  c,  0,  0,
                 0,  0,  1,  0,
                 0,  0,  0,  1 // clang-format on
            };
    }

    // 3D Rotation matrix for an arbitrary axis specified by x, y and z
    static Matrix4x4 RotationArbitrary(Vector3<T> axis, T angleInRadians)
    {
        axis = normalize(axis);

        T sinAngle         = std::sin(angleInRadians);
        T cosAngle         = std::cos(angleInRadians);
        T oneMinusCosAngle = 1 - cosAngle;

        Matrix4x4 mOut;

        mOut._11 = 1 + oneMinusCosAngle * (axis.x * axis.x - 1);
        mOut._12 = axis.z * sinAngle + oneMinusCosAngle * axis.x * axis.y;
        mOut._13 = -axis.y * sinAngle + oneMinusCosAngle * axis.x * axis.z;
        mOut._41 = 0;

        mOut._21 = -axis.z * sinAngle + oneMinusCosAngle * axis.y * axis.x;
        mOut._22 = 1 + oneMinusCosAngle * (axis.y * axis.y - 1);
        mOut._23 = axis.x * sinAngle + oneMinusCosAngle * axis.y * axis.z;
        mOut._24 = 0;

        mOut._31 = axis.y * sinAngle + oneMinusCosAngle * axis.z * axis.x;
        mOut._32 = -axis.x * sinAngle + oneMinusCosAngle * axis.z * axis.y;
        mOut._33 = 1 + oneMinusCosAngle * (axis.z * axis.z - 1);
        mOut._34 = 0;

        mOut._41 = 0;
        mOut._42 = 0;
        mOut._43 = 0;
        mOut._44 = 1;

        return mOut;
    }

    static Matrix4x4 ViewFromBasis(const Vector3<T>& f3X, const Vector3<T>& f3Y, const Vector3<T>& f3Z)
    {
        return Matrix4x4 // clang-format off
            {
                f3X.x, f3Y.x, f3Z.x,   0,
                f3X.y, f3Y.y, f3Z.y,   0,
                f3X.z, f3Y.z, f3Z.z,   0,
                    0,     0,     0,   1 // clang-format on
            };
    }

    // All graphics APIs except for OpenGL use [0, 1] as NDC Z range.
    // OpenGL uses [-1, 1] unless glClipControl is used to change it.
    // Use IRenderDevice::GetDeviceInfo().NDC to get the NDC Z range.
    // See https://github.com/DiligentGraphics/DiligentCore/blob/master/doc/CoordinateSystem.md
    void SetNearFarClipPlanes(T zNear, T zFar, bool NegativeOneToOneZ)
    {
        if (_44 == 0)
        {
            // Perspective projection
            if (NegativeOneToOneZ)
            {
                // https://www.opengl.org/sdk/docs/man2/xhtml/gluPerspective.xml
                // http://www.terathon.com/gdc07_lengyel.pdf
                // Note that OpenGL uses right-handed coordinate system, where
                // camera is looking in negative z direction:
                //   OO
                //  |__|<--------------------
                //         -z             +z
                // Consequently, OpenGL projection matrix given by these two
                // references inverts z axis.

                // We do not need to do this, because we use DX coordinate
                // system for the camera space. Thus we need to invert the
                // sign of the values in the third column in the matrix
                // from the references:

                _33 = -(-(zFar + zNear) / (zFar - zNear));
                _43 = -2 * zNear * zFar / (zFar - zNear);
                _34 = -(-1);
            }
            else
            {
                _33 = zFar / (zFar - zNear);
                _43 = -zNear * zFar / (zFar - zNear);
                _34 = 1;
            }
        }
        else
        {
            // Orthographic projection
            _33 = (NegativeOneToOneZ ? 2 : 1) / (zFar - zNear);
            _43 = (NegativeOneToOneZ ? zNear + zFar : zNear) / (zNear - zFar);
        }
    }

    void GetNearFarClipPlanes(T& zNear, T& zFar, bool NegativeOneToOneZ) const
    {
        if (_44 == 0)
        {
            // Perspective projection
            if (NegativeOneToOneZ)
            {
                zNear = _43 / (-1 - _33);
                zFar  = _43 / (+1 - _33);
            }
            else
            {
                zNear = -_43 / _33;
                zFar  = _33 / (_33 - 1) * zNear;
            }
        }
        else
        {
            // Orthographic projection
            if (NegativeOneToOneZ)
            {
                zNear = (-1 - _43) / _33;
                zFar  = (+1 - _43) / _33;
            }
            else
            {
                zNear = -_43 / _33;
                zFar  = (1 - _43) / _33;
            }
        }
    }

    static Matrix4x4 Projection(T fov, T aspectRatio, T zNear, T zFar, bool NegativeOneToOneZ) // Left-handed projection
    {
        Matrix4x4 mOut;

        T yScale = static_cast<T>(1) / std::tan(fov / static_cast<T>(2));
        T xScale = yScale / aspectRatio;
        mOut._11 = xScale;
        mOut._22 = yScale;

        mOut.SetNearFarClipPlanes(zNear, zFar, NegativeOneToOneZ);

        return mOut;
    }

    static Matrix4x4 OrthoOffCenter(T left, T right, T bottom, T top, T zNear, T zFar, bool NegativeOneToOneZ) // Left-handed ortho projection
    {
        // clang-format off
        Matrix4x4 Proj
            {
                         2   / (right - left),                                 0,   0,    0,
                                            0,                2 / (top - bottom),   0,    0,
                                            0,                                 0,   0,    0,
                (left + right)/(left - right),   (top + bottom) / (bottom - top),   0,    1
            };
        // clang-format on
        Proj.SetNearFarClipPlanes(zNear, zFar, NegativeOneToOneZ);
        return Proj;
    }

    static Matrix4x4 Ortho(T width, T height, T zNear, T zFar, bool NegativeOneToOneZ) // Left-handed ortho projection
    {
        return OrthoOffCenter(
            -width * static_cast<T>(0.5),
            +width * static_cast<T>(0.5),
            -height * static_cast<T>(0.5),
            +height * static_cast<T>(0.5),
            zNear, zFar, NegativeOneToOneZ);
    }

    static Matrix4x4 Mul(const Matrix4x4& m1, const Matrix4x4& m2)
    {
        Matrix4x4 mOut;
        for (int i = 0; i < 4; i++)
        {
            for (int j = 0; j < 4; j++)
            {
                for (int k = 0; k < 4; k++)
                {
                    mOut.m[i][j] += m1.m[i][k] * m2.m[k][j];
                }
            }
        }
        return mOut;
    }


    constexpr T Determinant() const
    {
        T det = 0.f;

        det += _11 *
            Matrix3x3<T>(_22, _23, _24,
                         _32, _33, _34,
                         _42, _43, _44)
                .Determinant();

        det -= _12 *
            Matrix3x3<T>(_21, _23, _24,
                         _31, _33, _34,
                         _41, _43, _44)
                .Determinant();

        det += _13 *
            Matrix3x3<T>(_21, _22, _24,
                         _31, _32, _34,
                         _41, _42, _44)
                .Determinant();

        det -= _14 *
            Matrix3x3<T>(_21, _22, _23,
                         _31, _32, _33,
                         _41, _42, _43)
                .Determinant();

        return det;
    }

    constexpr Matrix4x4 Inverse() const
    {
        Matrix4x4 inv;

        // row 1
        inv._11 =
            Matrix3x3<T>(_22, _23, _24,
                         _32, _33, _34,
                         _42, _43, _44)
                .Determinant();

        inv._12 =
            -Matrix3x3<T>(_21, _23, _24,
                          _31, _33, _34,
                          _41, _43, _44)
                 .Determinant();

        inv._13 =
            Matrix3x3<T>(_21, _22, _24,
                         _31, _32, _34,
                         _41, _42, _44)
                .Determinant();

        inv._14 =
            -Matrix3x3<T>(_21, _22, _23,
                          _31, _32, _33,
                          _41, _42, _43)
                 .Determinant();


        // row 2
        inv._21 =
            -Matrix3x3<T>(_12, _13, _14,
                          _32, _33, _34,
                          _42, _43, _44)
                 .Determinant();

        inv._22 =
            Matrix3x3<T>(_11, _13, _14,
                         _31, _33, _34,
                         _41, _43, _44)
                .Determinant();

        inv._23 =
            -Matrix3x3<T>(_11, _12, _14,
                          _31, _32, _34,
                          _41, _42, _44)
                 .Determinant();

        inv._24 =
            Matrix3x3<T>(_11, _12, _13,
                         _31, _32, _33,
                         _41, _42, _43)
                .Determinant();


        // row 3
        inv._31 =
            Matrix3x3<T>(_12, _13, _14,
                         _22, _23, _24,
                         _42, _43, _44)
                .Determinant();

        inv._32 =
            -Matrix3x3<T>(_11, _13, _14,
                          _21, _23, _24,
                          _41, _43, _44)
                 .Determinant();

        inv._33 =
            Matrix3x3<T>(_11, _12, _14,
                         _21, _22, _24,
                         _41, _42, _44)
                .Determinant();

        inv._34 =
            -Matrix3x3<T>(_11, _12, _13,
                          _21, _22, _23,
                          _41, _42, _43)
                 .Determinant();


        // row 4
        inv._41 =
            -Matrix3x3<T>(_12, _13, _14,
                          _22, _23, _24,
                          _32, _33, _34)
                 .Determinant();

        inv._42 =
            Matrix3x3<T>(_11, _13, _14,
                         _21, _23, _24,
                         _31, _33, _34)
                .Determinant();

        inv._43 =
            -Matrix3x3<T>(_11, _12, _14,
                          _21, _22, _24,
                          _31, _32, _34)
                 .Determinant();

        inv._44 =
            Matrix3x3<T>(_11, _12, _13,
                         _21, _22, _23,
                         _31, _32, _33)
                .Determinant();

        T det = _11 * inv._11 + _12 * inv._12 + _13 * inv._13 + _14 * inv._14;
        inv   = inv.Transpose();
        inv *= static_cast<T>(1) / det;

        return inv;
    }

    constexpr Matrix4x4 RemoveTranslation() const
    {
        return Matrix4x4 // clang-format off
            {
                _11, _12, _13, _14,
                _21, _22, _23, _24,
                _31, _32, _33, _34,
                  0,   0,   0, _44 // clang-format on
            };
    }

    template <typename Y>
    constexpr Matrix4x4<Y> Recast() const
    {
        return Matrix4x4<Y>::MakeMatrix(Data());
    }
};

template <typename T>
inline constexpr Matrix4x4<T> operator*(const Matrix4x4<T>& Mat, T s)
{
    // clang-format off
    return 
    {
        Mat._11 * s, Mat._12 * s, Mat._13 * s, Mat._14 * s,
        Mat._21 * s, Mat._22 * s, Mat._23 * s, Mat._24 * s,
        Mat._31 * s, Mat._32 * s, Mat._33 * s, Mat._34 * s,
        Mat._41 * s, Mat._42 * s, Mat._43 * s, Mat._44 * s
    };
    // clang-format on
}

template <typename T>
inline constexpr Matrix4x4<T> operator*(T s, const Matrix4x4<T>& Mat)
{
    return Mat * s;
}

template <typename T>
inline constexpr Matrix4x4<T> operator/(const Matrix4x4<T>& Mat, T s)
{
    // clang-format off
    return 
    {
        Mat._11 / s, Mat._12 / s, Mat._13 / s, Mat._14 / s,
        Mat._21 / s, Mat._22 / s, Mat._23 / s, Mat._24 / s,
        Mat._31 / s, Mat._32 / s, Mat._33 / s, Mat._34 / s,
        Mat._41 / s, Mat._42 / s, Mat._43 / s, Mat._44 / s
    };
    // clang-format on
}

// Template Vector Operations


template <class T>
constexpr T dot(const Vector2<T>& a, const Vector2<T>& b)
{
    return a.x * b.x + a.y * b.y;
}

template <class T>
constexpr T dot(const Vector3<T>& a, const Vector3<T>& b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

template <class T>
constexpr T dot(const Vector4<T>& a, const Vector4<T>& b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
}

template <class VectorType>
constexpr auto length(const VectorType& a) -> decltype(dot(a, a))
{
    return sqrt(dot(a, a));
}


template <class T>
constexpr Vector3<T>(min)(const Vector3<T>& a, const Vector3<T>& b)
{
    return Vector3<T>((std::min)(a.x, b.x), (std::min)(a.y, b.y), (std::min)(a.z, b.z));
}

template <class T>
constexpr Vector4<T>(min)(const Vector4<T>& a, const Vector4<T>& b)
{
    return Vector4<T>((std::min)(a.x, b.x), (std::min)(a.y, b.y), (std::min)(a.z, b.z), (std::min)(a.w, b.w));
}

template <class T>
constexpr Vector3<T>(max)(const Vector3<T>& a, const Vector3<T>& b)
{
    return Vector3<T>((std::max)(a.x, b.x), (std::max)(a.y, b.y), (std::max)(a.z, b.z));
}

template <class T>
constexpr Vector4<T>(max)(const Vector4<T>& a, const Vector4<T>& b)
{
    return Vector4<T>((std::max)(a.x, b.x), (std::max)(a.y, b.y), (std::max)(a.z, b.z), (std::max)(a.w, b.w));
}

template <class T>
constexpr Vector2<T> abs(const Vector2<T>& a)
{
    // WARNING: abs() on gcc is for integers only!
    return Vector2<T>(a.x < 0 ? -a.x : a.x,
                      a.y < 0 ? -a.y : a.y);
}

template <class T>
constexpr Vector3<T> abs(const Vector3<T>& a)
{
    // WARNING: abs() on gcc is for integers only!
    return Vector3<T>(a.x < 0 ? -a.x : a.x,
                      a.y < 0 ? -a.y : a.y,
                      a.z < 0 ? -a.z : a.z);
}

template <class T>
constexpr Vector4<T> abs(const Vector4<T>& a)
{
    // WARNING: abs() on gcc is for integers only!
    return Vector4<T>(a.x < 0 ? -a.x : a.x,
                      a.y < 0 ? -a.y : a.y,
                      a.z < 0 ? -a.z : a.z,
                      a.w < 0 ? -a.w : a.w);
}


template <typename T>
constexpr T clamp(T val, T _min, T _max)
{
    return val < _min ? _min : (val > _max ? _max : val);
}

template <class T>
constexpr Vector2<T> clamp(const Vector2<T>& a, const Vector2<T>& _min, const Vector2<T>& _max)
{
    return Vector2<T>(clamp(a.x, _min.x, _max.x),
                      clamp(a.y, _min.y, _max.y));
}

template <class T>
constexpr Vector3<T> clamp(const Vector3<T>& a, const Vector3<T>& _min, const Vector3<T>& _max)
{
    return Vector3<T>(clamp(a.x, _min.x, _max.x),
                      clamp(a.y, _min.y, _max.y),
                      clamp(a.z, _min.z, _max.z));
}

template <class T>
constexpr Vector4<T> clamp(const Vector4<T>& a, const Vector4<T>& _min, const Vector4<T>& _max)
{
    return Vector4<T>(clamp(a.x, _min.x, _max.x),
                      clamp(a.y, _min.y, _max.y),
                      clamp(a.z, _min.z, _max.z),
                      clamp(a.w, _min.w, _max.w));
}


template <class T>
constexpr Vector3<T> cross(const Vector3<T>& a, const Vector3<T>& b)
{
    // |   i    j    k   |
    // |  a.x  a.y  a.z  |
    // |  b.x  b.y  b.z  |
    return Vector3<T>((a.y * b.z) - (a.z * b.y), (a.z * b.x) - (a.x * b.z), (a.x * b.y) - (a.y * b.x));
}

template <class T, class Y>
constexpr Vector3<T> cross(const Vector3<T>& a, const Vector3<T>& b)
{
    // |   i    j    k   |
    // |  a.x  a.y  a.z  |
    // |  b.x  b.y  b.z  |
    return Vector3<T> //
        {
            static_cast<T>(Y{a.y} * Y{b.z} - Y{a.z} * Y{b.y}),
            static_cast<T>(Y{a.z} * Y{b.x} - Y{a.x} * Y{b.z}),
            static_cast<T>(Y{a.x} * Y{b.y} - Y{a.y} * Y{b.x}) //
        };
}

constexpr inline Vector3<float> high_precision_cross(const Vector3<float>& a, const Vector3<float>& b)
{
    return cross<float, double>(a, b);
}

constexpr inline Vector3<int32_t> high_precision_cross(const Vector3<int32_t>& a, const Vector3<int32_t>& b)
{
    return cross<int32_t, int64_t>(a, b);
}

template <class VectorType>
constexpr VectorType normalize(const VectorType& a)
{
    auto len = length(a);
    return a / len;
}


template <class T>
constexpr bool any(const Vector2<T>& a)
{
    return (a.x != T{}) || (a.y != T{});
}

template <class T>
constexpr bool any(const Vector3<T>& a)
{
    return (a.x != T{}) || (a.y != T{}) || (a.z != T{});
}

template <class T>
constexpr bool any(const Vector4<T>& a)
{
    return (a.x != T{}) || (a.y != T{}) || (a.z != T{}) || (a.w != T{});
}

template <class T>
constexpr bool all(const Vector2<T>& a)
{
    return (a.x != T{}) && (a.y != T{});
}

template <class T>
constexpr bool all(const Vector3<T>& a)
{
    return (a.x != T{}) && (a.y != T{}) && (a.z != T{});
}

template <class T>
constexpr bool all(const Vector4<T>& a)
{
    return (a.x != T{}) && (a.y != T{}) && (a.z != T{}) && (a.w != T{});
}


// Template Matrix-Matrix multiplications

template <class T>
constexpr Matrix4x4<T> operator*(const Matrix4x4<T>& m1, const Matrix4x4<T>& m2)
{
    return Matrix4x4<T>::Mul(m1, m2);
}

template <class T>
constexpr Matrix3x3<T> operator*(const Matrix3x3<T>& m1, const Matrix3x3<T>& m2)
{
    return Matrix3x3<T>::Mul(m1, m2);
}

template <class T>
constexpr Matrix2x2<T> operator*(const Matrix2x2<T>& m1, const Matrix2x2<T>& m2)
{
    return Matrix2x2<T>::Mul(m1, m2);
}


// Template Matrix-Vector multiplications

template <class T>
constexpr Vector4<T> operator*(const Matrix4x4<T>& m, const Vector4<T>& v)
{
    Vector4<T> out;
    out[0] = m[0][0] * v.x + m[0][1] * v.y + m[0][2] * v.z + m[0][3] * v.w;
    out[1] = m[1][0] * v.x + m[1][1] * v.y + m[1][2] * v.z + m[1][3] * v.w;
    out[2] = m[2][0] * v.x + m[2][1] * v.y + m[2][2] * v.z + m[2][3] * v.w;
    out[3] = m[3][0] * v.x + m[3][1] * v.y + m[3][2] * v.z + m[3][3] * v.w;
    return out;
}

template <class T>
constexpr Vector3<T> operator*(const Matrix3x3<T>& m, Vector3<T>& v)
{
    Vector3<T> out;
    out[0] = m[0][0] * v.x + m[0][1] * v.y + m[0][2] * v.z;
    out[1] = m[1][0] * v.x + m[1][1] * v.y + m[1][2] * v.z;
    out[2] = m[2][0] * v.x + m[2][1] * v.y + m[2][2] * v.z;
    return out;
}

template <class T>
constexpr Vector2<T> operator*(const Matrix2x2<T>& m, const Vector2<T>& v)
{
    Vector2<T> out;
    out[0] = m[0][0] * v.x + m[0][1] * v.y;
    out[1] = m[1][0] * v.x + m[1][1] * v.y;
    return out;
}

// Common HLSL-compatible vector typedefs

using uint  = uint32_t;
using uint2 = Vector2<uint>;
using uint3 = Vector3<uint>;
using uint4 = Vector4<uint>;

using int2 = Vector2<int32_t>;
using int3 = Vector3<int32_t>;
using int4 = Vector4<int32_t>;

using float2 = Vector2<float>;
using float3 = Vector3<float>;
using float4 = Vector4<float>;

using double2 = Vector2<double>;
using double3 = Vector3<double>;
using double4 = Vector4<double>;

using bool2 = Vector2<bool>;
using bool3 = Vector3<bool>;
using bool4 = Vector4<bool>;

using float4x4 = Matrix4x4<float>;
using float3x3 = Matrix3x3<float>;
using float2x2 = Matrix2x2<float>;

using double4x4 = Matrix4x4<double>;
using double3x3 = Matrix3x3<double>;
using double2x2 = Matrix2x2<double>;

using int4x4 = Matrix4x4<Int32>;
using int3x3 = Matrix3x3<Int32>;
using int2x2 = Matrix2x2<Int32>;

template <typename T = float>
struct Quaternion
{
    using ValueType = T;

    Vector4<T> q{0, 0, 0, 1};

    constexpr Quaternion(const Vector4<T>& _q) noexcept :
        q{_q}
    {}
    constexpr Quaternion(T x, T y, T z, T w) noexcept :
        q{x, y, z, w}
    {
    }
    constexpr Quaternion() noexcept {}

    constexpr Quaternion(const Quaternion&) noexcept = default;

    constexpr bool operator==(const Quaternion& right) const
    {
        return q == right.q;
    }

    constexpr bool operator!=(const Quaternion& right) const
    {
        return q != right.q;
    }

    template <typename Y>
    constexpr static Quaternion MakeQuaternion(const Y& vals)
    {
        return Quaternion{Vector4<T>::MakeVector(vals)};
    }

    static Quaternion RotationFromAxisAngle(const Vector3<T>& axis, T angle)
    {
        Quaternion out{0, 0, 0, 1};
        const T    norm = length(axis);
        if (norm != 0)
        {
            T sina2  = std::sin(T{0.5} * angle);
            out.q[0] = sina2 * axis[0] / norm;
            out.q[1] = sina2 * axis[1] / norm;
            out.q[2] = sina2 * axis[2] / norm;
            out.q[3] = std::cos(T{0.5} * angle);
        }
        return out;
    }

    void GetAxisAngle(Vector3<T>& outAxis, T& outAngle) const
    {
        T sina2    = std::sqrt(q[0] * q[0] + q[1] * q[1] + q[2] * q[2]);
        outAngle   = T{2} * std::atan2(sina2, q[3]);
        T r        = (sina2 > 0) ? (T{1} / sina2) : 0;
        outAxis[0] = r * q[0];
        outAxis[1] = r * q[1];
        outAxis[2] = r * q[2];
    }

    Matrix4x4<T> ToMatrix() const
    {
        Matrix4x4<T> out;

        T yy2 = 2.0f * q[1] * q[1];
        T xy2 = 2.0f * q[0] * q[1];
        T xz2 = 2.0f * q[0] * q[2];
        T yz2 = 2.0f * q[1] * q[2];
        T zz2 = 2.0f * q[2] * q[2];
        T wz2 = 2.0f * q[3] * q[2];
        T wy2 = 2.0f * q[3] * q[1];
        T wx2 = 2.0f * q[3] * q[0];
        T xx2 = 2.0f * q[0] * q[0];

        out[0][0] = -yy2 - zz2 + 1.0f;
        out[0][1] = xy2 + wz2;
        out[0][2] = xz2 - wy2;
        out[0][3] = 0;
        out[1][0] = xy2 - wz2;
        out[1][1] = -xx2 - zz2 + 1.0f;
        out[1][2] = yz2 + wx2;
        out[1][3] = 0;
        out[2][0] = xz2 + wy2;
        out[2][1] = yz2 - wx2;
        out[2][2] = -xx2 - yy2 + 1.0f;
        out[2][3] = 0;
        out[3][0] = out[3][1] = out[3][2] = 0;
        out[3][3]                         = 1;
        return out;
    }

    constexpr static Quaternion Mul(const Quaternion& q1, const Quaternion& q2)
    {
        Quaternion q1_q2;
        q1_q2.q.x = +q1.q.x * q2.q.w + q1.q.y * q2.q.z - q1.q.z * q2.q.y + q1.q.w * q2.q.x;
        q1_q2.q.y = -q1.q.x * q2.q.z + q1.q.y * q2.q.w + q1.q.z * q2.q.x + q1.q.w * q2.q.y;
        q1_q2.q.z = +q1.q.x * q2.q.y - q1.q.y * q2.q.x + q1.q.z * q2.q.w + q1.q.w * q2.q.z;
        q1_q2.q.w = -q1.q.x * q2.q.x - q1.q.y * q2.q.y - q1.q.z * q2.q.z + q1.q.w * q2.q.w;
        return q1_q2;
    }

    Quaternion& operator=(const Quaternion& rhs)
    {
        q = rhs.q;
        return *this;
    }

    Quaternion& operator*=(const Quaternion& rhs)
    {
        *this = Mul(*this, rhs);
        return *this;
    }

    Vector3<T> RotateVector(const Vector3<T>& v) const
    {
        const Vector3<T> axis{q.x, q.y, q.z};
        return v + T{2} * cross(axis, cross(axis, v) + q.w * v);
    }
};
using QuaternionF = Quaternion<float>;
using QuaternionD = Quaternion<double>;

template <typename T>
constexpr inline Quaternion<T> operator*(const Quaternion<T>& q1, const Quaternion<T>& q2)
{
    return Quaternion<T>::Mul(q1, q2);
}

template <typename T>
constexpr inline Quaternion<T> normalize(const Quaternion<T>& q)
{
    return Quaternion<T>{normalize(q.q)};
}

// https://en.wikipedia.org/wiki/Slerp
template <typename T>
inline Quaternion<T> slerp(Quaternion<T> v0, Quaternion<T> v1, T t, bool DoNotNormalize = false)
{
    // Only unit quaternions are valid rotations.
    // Normalize to avoid undefined behavior.
    if (!DoNotNormalize)
    {
        v0 = normalize(v0);
        v1 = normalize(v1);
    }

    // Compute the cosine of the angle between the two vectors.
    T dp = dot(v0.q, v1.q);

    // If the dot product is negative, slerp won't take
    // the shorter path. Note that v1 and -v1 are equivalent when
    // the negation is applied to all four components. Fix by
    // reversing one quaternion.
    if (dp < 0)
    {
        v1.q = -v1.q;
        dp   = -dp;
    }

    const double DOT_THRESHOLD = 0.9995;
    if (dp > DOT_THRESHOLD)
    {
        // If the inputs are too close for comfort, linearly interpolate
        // and normalize the result.

        Quaternion<T> result{v0.q + t * (v1.q - v0.q)};
        result.q = normalize(result.q);
        return result;
    }

    // Since dot is in range [0, DOT_THRESHOLD], acos is safe
    T theta_0     = std::acos(dp);     // theta_0 = angle between input vectors
    T theta       = theta_0 * t;       // theta = angle between v0 and result
    T sin_theta   = std::sin(theta);   // compute this value only once
    T sin_theta_0 = std::sin(theta_0); // compute this value only once

    T s0 = cos(theta) - dp * sin_theta / sin_theta_0; // == sin(theta_0 - theta) / sin(theta_0)
    T s1 = sin_theta / sin_theta_0;

    Quaternion<T> v{v0.q * s0 + v1.q * s1};
    if (!DoNotNormalize)
    {
        v = normalize(v);
    }
    return v;
}


template <typename T>
constexpr T lerp(const T& Left, const T& Right, float w)
{
    return Left * (1.f - w) + Right * w;
}

template <typename T>
constexpr T SmoothStep(T Left, T Right, T w)
{
    T t = clamp((w - Left) / (Right - Left), static_cast<T>(0), static_cast<T>(1));
    return t * t * (static_cast<T>(3) - static_cast<T>(2) * t);
}

constexpr inline float4 RGBA8Unorm_To_F4Color(Uint32 RGBA8)
{
    // clang-format off
    return float4
    {
            static_cast<float>((RGBA8 >>  0u) & 0xFF) / 255.f,
            static_cast<float>((RGBA8 >>  8u) & 0xFF) / 255.f,
            static_cast<float>((RGBA8 >> 16u) & 0xFF) / 255.f,
            static_cast<float>((RGBA8 >> 24u) & 0xFF) / 255.f
    };
    // clang-format on
}

constexpr inline Uint32 F4Color_To_RGBA8Unorm(const float4& f4Color)
{
    Uint32 RGBA8U = 0;
    RGBA8U |= static_cast<Uint32>(clamp(f4Color.x, 0.f, 1.f) * 255.f) << 0u;
    RGBA8U |= static_cast<Uint32>(clamp(f4Color.y, 0.f, 1.f) * 255.f) << 8u;
    RGBA8U |= static_cast<Uint32>(clamp(f4Color.z, 0.f, 1.f) * 255.f) << 16u;
    RGBA8U |= static_cast<Uint32>(clamp(f4Color.w, 0.f, 1.f) * 255.f) << 24u;
    return RGBA8U;
}

template <typename T>
struct _FastFloatIntermediateType
{
};

template <>
struct _FastFloatIntermediateType<float>
{
    // All floats that have fractional part are representable as 32-bit int
    using Type = Int32;

    // First float that does not have fractional part
    static constexpr float NoFracThreshold = 8388608; // 2^23
};

template <>
struct _FastFloatIntermediateType<double>
{
    // All doubles that have fractional part are representable as 64-bit int
    using Type = Int64;

    // First double that does not have fractional part
    static constexpr double NoFracThreshold = 9007199254740992; // 2^53
};

// At least on MSVC std::floor is an actual function call into ucrtbase.dll.
// All floats/doubles that have fractional parts also fit into integer
// representable range, so we can do much better.
template <typename T>
constexpr T FastFloor(T x)
{
    constexpr auto NoFracThreshold = _FastFloatIntermediateType<T>::NoFracThreshold;
    if (x >= NoFracThreshold || x <= -NoFracThreshold)
        return x;

    auto i   = static_cast<typename _FastFloatIntermediateType<T>::Type>(x);
    auto flr = static_cast<T>(i);
    //   x         flr    floor(x)  flr <= x
    // +1.0   ->   1.0      1.0       true
    // +0.5   ->   0.0      0.0       true
    //  0.0   ->   0.0      0.0       true
    // -0.5   ->   0.0     -1.0      false
    // -1.0   ->  -1.0     -1.0       true

    return flr <= x ? flr : flr - 1;
}

template <typename T>
constexpr T FastCeil(T x)
{
    return -FastFloor(-x);
}


template <typename T>
constexpr Vector2<T> FastFloor(const Vector2<T>& vec)
{
    return Vector2<T>{
        FastFloor(vec.x),
        FastFloor(vec.y)};
}

template <typename T>
constexpr Vector3<T> FastFloor(const Vector3<T>& vec)
{
    return Vector3<T>{
        FastFloor(vec.x),
        FastFloor(vec.y),
        FastFloor(vec.z)};
}

template <typename T>
constexpr Vector4<T> FastFloor(const Vector4<T>& vec)
{
    return Vector4<T>{
        FastFloor(vec.x),
        FastFloor(vec.y),
        FastFloor(vec.z),
        FastFloor(vec.w)};
}


template <typename T>
constexpr Vector2<T> FastCeil(const Vector2<T>& vec)
{
    return Vector2<T>{
        FastCeil(vec.x),
        FastCeil(vec.y)};
}

template <typename T>
constexpr Vector3<T> FastCeil(const Vector3<T>& vec)
{
    return Vector3<T>{
        FastCeil(vec.x),
        FastCeil(vec.y),
        FastCeil(vec.z)};
}

template <typename T>
constexpr Vector4<T> FastCeil(const Vector4<T>& vec)
{
    return Vector4<T>{
        FastCeil(vec.x),
        FastCeil(vec.y),
        FastCeil(vec.z),
        FastCeil(vec.w)};
}

template <typename Type>
constexpr Type FastFrac(const Type& val)
{
    return val - FastFloor(val);
}

constexpr inline Uint32 BitInterleave16(Uint16 _x, Uint16 _y)
{
    // https://graphics.stanford.edu/~seander/bithacks.html#InterleaveBMN

    // Interleave lower 16 bits of x and y, so the bits of x
    // are in the even positions and bits from y in the odd;
    // x | (y << 1) gets the resulting 32-bit Morton Number.
    // x and y must initially be less than 65536.
    Uint32 x = _x;
    Uint32 y = _y;

    x = (x | (x << 8u)) & 0x00FF00FFu;
    x = (x | (x << 4u)) & 0x0F0F0F0Fu;
    x = (x | (x << 2u)) & 0x33333333u;
    x = (x | (x << 1u)) & 0x55555555u;

    y = (y | (y << 8u)) & 0x00FF00FFu;
    y = (y | (y << 4u)) & 0x0F0F0F0Fu;
    y = (y | (y << 2u)) & 0x33333333u;
    y = (y | (y << 1u)) & 0x55555555u;

    return x | (y << 1u);
}

/// Returns the least-significant bit and clears it in the input argument
template <typename T>
typename std::enable_if<std::is_integral<T>::value, T>::type ExtractLSB(T& bits)
{
    if (bits == T{0})
        return 0;

    const T bit = bits & ~(bits - T{1});
    bits &= ~bit;

    return bit;
}

/// Returns the enum value representing the least-significant bit and clears it in the input argument
template <typename T>
typename std::enable_if<std::is_enum<T>::value, T>::type ExtractLSB(T& bits)
{
    return static_cast<T>(ExtractLSB(reinterpret_cast<typename std::underlying_type<T>::type&>(bits)));
}

/// Wraps Value to the range [Min, Min + Range)
template <typename T>
T WrapToRange(T Value, T Min, T Range)
{
    VERIFY_EXPR(Range >= 0);
    if (Range <= 0)
        return Min;

    T Result = (Value - Min) % Range;
    if (Result < 0)
        Result += Range;

    return Result + Min;
}

/// Constructs an orthonormal basis from the given direction vector.
///
/// \param[in] Dir           - The direction vector.
/// \param[in] IsRightHanded - Flag indicating if the basis should be right-handed.
/// \param[out] X            - The resulting X basis vector.
/// \param[out] Y            - The resulting Y basis vector.
/// \param[out] Z            - The resulting Z basis vector.
/// \return                    True if the basis was successfully constructed, and false otherwise.
template <typename T>
bool BasisFromDirection(const Vector3<T>& Dir, bool IsRightHanded, Vector3<T>& X, Vector3<T>& Y, Vector3<T>& Z)
{
    T Len = length(Dir);
    if (Len < static_cast<T>(1e-5))
        return false;

    Z = Dir / Len;

    Vector3<T> AbsZ{
        std::abs(Z.x),
        std::abs(Z.y),
        std::abs(Z.z),
    };
    T min_cmp = (std::min)((std::min)(AbsZ.x, AbsZ.y), AbsZ.z);
    if (min_cmp == AbsZ.x)
        X = {1, 0, 0};
    else if (min_cmp == AbsZ.y)
        X = {0, 1, 0};
    else
        X = {0, 0, 1};

    Y = cross(Z, X);
    X = cross(Y, Z);
    X = normalize(X);
    Y = normalize(Y);
    if (!IsRightHanded)
        Y = -Y;

    return true;
}

inline std::ostream& operator<<(std::ostream& os, const float4& vec)
{
    return os << "float4(" << vec.x << ", " << vec.y << ", " << vec.z << ", " << vec.w << ')';
}
inline std::ostream& operator<<(std::ostream& os, const float3& vec)
{
    return os << "float3(" << vec.x << ", " << vec.y << ", " << vec.z << ')';
}
inline std::ostream& operator<<(std::ostream& os, const float2& vec)
{
    return os << "float2(" << vec.x << ", " << vec.y << ')';
}

inline std::ostream& operator<<(std::ostream& os, const int4& vec)
{
    return os << "int4(" << vec.x << ", " << vec.y << ", " << vec.z << ", " << vec.w << ')';
}
inline std::ostream& operator<<(std::ostream& os, const int3& vec)
{
    return os << "int3(" << vec.x << ", " << vec.y << ", " << vec.z << ')';
}
inline std::ostream& operator<<(std::ostream& os, const int2& vec)
{
    return os << "int2(" << vec.x << ", " << vec.y << ')';
}


inline std::ostream& operator<<(std::ostream& os, const uint4& vec)
{
    return os << "uint4(" << vec.x << ", " << vec.y << ", " << vec.z << ", " << vec.w << ')';
}
inline std::ostream& operator<<(std::ostream& os, const uint3& vec)
{
    return os << "uint3(" << vec.x << ", " << vec.y << ", " << vec.z << ')';
}
inline std::ostream& operator<<(std::ostream& os, const uint2& vec)
{
    return os << "uint2(" << vec.x << ", " << vec.y << ')';
}

template <typename T>
T DegToRad(T Deg)
{
    return Deg * (static_cast<T>(PI) / static_cast<T>(180));
}

template <typename T>
T RadToDeg(T Rad)
{
    return Rad * (static_cast<T>(180) / static_cast<T>(PI));
}

} // namespace Diligent



namespace std
{
template <typename T>
constexpr Diligent::Vector2<T>(max)(const Diligent::Vector2<T>& Left, const Diligent::Vector2<T>& Right)
{
    return Diligent::Vector2<T>(
        (std::max)(Left.x, Right.x),
        (std::max)(Left.y, Right.y));
}

template <typename T>
constexpr Diligent::Vector3<T>(max)(const Diligent::Vector3<T>& Left, const Diligent::Vector3<T>& Right)
{
    return Diligent::Vector3<T>(
        (std::max)(Left.x, Right.x),
        (std::max)(Left.y, Right.y),
        (std::max)(Left.z, Right.z));
}

template <typename T>
constexpr Diligent::Vector4<T>(max)(const Diligent::Vector4<T>& Left, const Diligent::Vector4<T>& Right)
{
    return Diligent::Vector4<T>(
        (std::max)(Left.x, Right.x),
        (std::max)(Left.y, Right.y),
        (std::max)(Left.z, Right.z),
        (std::max)(Left.w, Right.w));
}


template <typename T>
constexpr Diligent::Vector2<T>(min)(const Diligent::Vector2<T>& Left, const Diligent::Vector2<T>& Right)
{
    return Diligent::Vector2<T>(
        (std::min)(Left.x, Right.x),
        (std::min)(Left.y, Right.y));
}

template <typename T>
constexpr Diligent::Vector3<T>(min)(const Diligent::Vector3<T>& Left, const Diligent::Vector3<T>& Right)
{
    return Diligent::Vector3<T>(
        (std::min)(Left.x, Right.x),
        (std::min)(Left.y, Right.y),
        (std::min)(Left.z, Right.z));
}

template <typename T>
constexpr Diligent::Vector4<T>(min)(const Diligent::Vector4<T>& Left, const Diligent::Vector4<T>& Right)
{
    return Diligent::Vector4<T>(
        (std::min)(Left.x, Right.x),
        (std::min)(Left.y, Right.y),
        (std::min)(Left.z, Right.z),
        (std::min)(Left.w, Right.w));
}

template <typename T>
constexpr Diligent::Vector2<T> floor(const Diligent::Vector2<T>& vec)
{
    return Diligent::Vector2<T>(
        std::floor(vec.x),
        std::floor(vec.y));
}

template <typename T>
constexpr Diligent::Vector3<T> floor(const Diligent::Vector3<T>& vec)
{
    return Diligent::Vector3<T>(
        std::floor(vec.x),
        std::floor(vec.y),
        std::floor(vec.z));
}

template <typename T>
constexpr Diligent::Vector4<T> floor(const Diligent::Vector4<T>& vec)
{
    return Diligent::Vector4<T>(
        std::floor(vec.x),
        std::floor(vec.y),
        std::floor(vec.z),
        std::floor(vec.w));
}


template <typename T>
constexpr Diligent::Vector2<T> ceil(const Diligent::Vector2<T>& vec)
{
    return Diligent::Vector2<T>(
        std::ceil(vec.x),
        std::ceil(vec.y));
}

template <typename T>
constexpr Diligent::Vector3<T> ceil(const Diligent::Vector3<T>& vec)
{
    return Diligent::Vector3<T>(
        std::ceil(vec.x),
        std::ceil(vec.y),
        std::ceil(vec.z));
}

template <typename T>
constexpr Diligent::Vector4<T> ceil(const Diligent::Vector4<T>& vec)
{
    return Diligent::Vector4<T>(
        std::ceil(vec.x),
        std::ceil(vec.y),
        std::ceil(vec.z),
        std::ceil(vec.w));
}


template <typename T>
struct hash<Diligent::Vector2<T>>
{
    size_t operator()(const Diligent::Vector2<T>& v2) const
    {
        return Diligent::ComputeHash(v2.x, v2.y);
    }
};

template <typename T>
struct hash<Diligent::Vector3<T>>
{
    size_t operator()(const Diligent::Vector3<T>& v3) const
    {
        return Diligent::ComputeHash(v3.x, v3.y, v3.z);
    }
};

template <typename T>
struct hash<Diligent::Vector4<T>>
{
    size_t operator()(const Diligent::Vector4<T>& v4) const
    {
        return Diligent::ComputeHash(v4.x, v4.y, v4.z, v4.w);
    }
};

template <typename T>
struct hash<Diligent::Matrix2x2<T>>
{
    size_t operator()(const Diligent::Matrix2x2<T>& m) const
    {
        return Diligent::ComputeHash(
            m.m00, m.m01,
            m.m10, m.m11);
    }
};

template <typename T>
struct hash<Diligent::Matrix3x3<T>>
{
    size_t operator()(const Diligent::Matrix3x3<T>& m) const
    {
        return Diligent::ComputeHash(
            m.m00, m.m01, m.m02,
            m.m10, m.m11, m.m12,
            m.m20, m.m21, m.m22);
    }
};

template <typename T>
struct hash<Diligent::Matrix4x4<T>>
{
    size_t operator()(const Diligent::Matrix4x4<T>& m) const
    {
        return Diligent::ComputeHash(
            m.m00, m.m01, m.m02, m.m03,
            m.m10, m.m11, m.m12, m.m13,
            m.m20, m.m21, m.m22, m.m23,
            m.m30, m.m31, m.m32, m.m33);
    }
};

} // namespace std

namespace Diligent
{

template <typename Type>
constexpr Type Frac(const Type& val)
{
    return val - std::floor(val);
}


template <typename T>
constexpr T max_n(const T& x, const T& y)
{
    return (std::max)(x, y);
}

template <typename T, typename... RestArgsType>
constexpr T max_n(const T& x, const RestArgsType&... RestArgs)
{
    return (std::max)(x, max_n(RestArgs...));
}

template <typename T>
constexpr T min_n(const T& x, const T& y)
{
    return (std::min)(x, y);
}

template <typename T, typename... RestArgsType>
constexpr T min_n(const T& x, const RestArgsType&... RestArgs)
{
    return (std::min)(x, min_n(RestArgs...));
}

template <typename T, typename... RestArgsType>
constexpr T(max)(const T& x, const T& y, const T& z, const RestArgsType&... RestArgs)
{
    return max_n(x, y, z, RestArgs...);
}

template <typename T, typename... RestArgsType>
constexpr T(min)(const T& x, const T& y, const T& z, const RestArgsType&... RestArgs)
{
    return min_n(x, y, z, RestArgs...);
}

} // namespace Diligent

#ifdef _MSC_VER
#    pragma warning(pop)
#endif
