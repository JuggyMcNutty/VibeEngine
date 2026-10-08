#pragma once

#include "vec.h"

// Precomp.h's test for SSE2, here so that the header stands alone
#if (defined(_MSC_VER) && (defined(_M_IX86) || defined(_M_X64))) || ((defined(__i386__) || defined(__x86_64__) || defined(__e2k__)) && defined(__SSE2__))
#define MAT_USE_SSE2
#include <emmintrin.h>
#endif

enum class handedness
{
	left,
	right
};

enum class clipzrange
{
	negative_positive_w, // OpenGL, -wclip <= zclip <= wclip
	zero_positive_w      // Direct3D, 0 <= zclip <= wclip
};

struct mat4
{
	mat4() = default;

	static mat4 null();
	static mat4 identity();
	static mat4 from_values(float *matrix);
	static mat4 transpose(const mat4 &matrix);
	static mat4 translate(float x, float y, float z);
	static mat4 translate(const vec3 &v) { return translate(v.x, v.y, v.z); }
	static mat4 scale(float x, float y, float z);
	static mat4 scale(const vec3 &v) { return scale(v.x, v.y, v.z); }
	static mat4 rotate(float angle, float x, float y, float z);
	static mat4 rotate(float angle, const vec3 &v) { return rotate(angle, v.x, v.y, v.z); }
	static mat4 quaternion(float x, float y, float z, float w); // This function assumes that the quarternion is normalized.
	static mat4 quaternion(const vec4 &q) { return quaternion(q.x, q.y, q.z, q.w); }
	static mat4 swap_yz();
	static mat4 perspective(float fovy, float aspect, float z_near, float z_far, handedness handedness, clipzrange clipz);
	static mat4 frustum(float left, float right, float bottom, float top, float z_near, float z_far, handedness handedness, clipzrange clipz);
	static mat4 look_at(vec3 eye, vec3 center, vec3 up);
	static mat4 mirror(vec3 normal);

	vec4 operator*(const vec4 &v) const;
	mat4 operator*(const mat4 &m) const;

	float operator[](size_t i) const { return matrix[i]; }
	float &operator[](size_t i) { return matrix[i]; }

	float matrix[4 * 4];
};

// Inline: the renderer transforms each box corner and vertex through it, tens
// of thousands of times a frame. The same sums in the same order either way.
inline vec4 mat4::operator*(const vec4 &v) const
{
#ifndef MAT_USE_SSE2
	vec4 result;
	result.x = matrix[0 * 4 + 0] * v.x + matrix[1 * 4 + 0] * v.y + matrix[2 * 4 + 0] * v.z + matrix[3 * 4 + 0] * v.w;
	result.y = matrix[0 * 4 + 1] * v.x + matrix[1 * 4 + 1] * v.y + matrix[2 * 4 + 1] * v.z + matrix[3 * 4 + 1] * v.w;
	result.z = matrix[0 * 4 + 2] * v.x + matrix[1 * 4 + 2] * v.y + matrix[2 * 4 + 2] * v.z + matrix[3 * 4 + 2] * v.w;
	result.w = matrix[0 * 4 + 3] * v.x + matrix[1 * 4 + 3] * v.y + matrix[2 * 4 + 3] * v.z + matrix[3 * 4 + 3] * v.w;
	return result;
#else
	__m128 m0 = _mm_loadu_ps(matrix);
	__m128 m1 = _mm_loadu_ps(matrix + 4);
	__m128 m2 = _mm_loadu_ps(matrix + 8);
	__m128 m3 = _mm_loadu_ps(matrix + 12);
	__m128 mv = _mm_loadu_ps(&v.x);
	m0 = _mm_mul_ps(m0, _mm_shuffle_ps(mv, mv, _MM_SHUFFLE(0, 0, 0, 0)));
	m1 = _mm_mul_ps(m1, _mm_shuffle_ps(mv, mv, _MM_SHUFFLE(1, 1, 1, 1)));
	m2 = _mm_mul_ps(m2, _mm_shuffle_ps(mv, mv, _MM_SHUFFLE(2, 2, 2, 2)));
	m3 = _mm_mul_ps(m3, _mm_shuffle_ps(mv, mv, _MM_SHUFFLE(3, 3, 3, 3)));
	mv = _mm_add_ps(_mm_add_ps(_mm_add_ps(m0, m1), m2), m3);
	vec4 result;
	_mm_storeu_ps(&result.x, mv);
	return result;
#endif
}

struct mat3
{
	mat3() = default;
	mat3(const mat4 &m);

	static mat3 null();
	static mat3 identity();
	static mat3 from_values(float *matrix);
	static mat3 transpose(const mat3 &matrix);
	static mat3 inverse(const mat3 &matrix);
	static mat3 adjoint(const mat3 &matrix);

	static double determinant(const mat3 &m);

	float operator[](size_t i) const { return matrix[i]; }
	float &operator[](size_t i) { return matrix[i]; }

	vec3 operator*(const vec3 &v) const;
	mat3 operator*(const mat3 &m) const;

	float matrix[3 * 3];
};

struct mat2
{
	float operator[](size_t i) const { return matrix[i]; }
	float &operator[](size_t i) { return matrix[i]; }
	float matrix[2 * 2];
};

struct mat4x3
{
	float operator[](size_t i) const { return matrix[i]; }
	float &operator[](size_t i) { return matrix[i]; }
	float matrix[4 * 3];
};

struct mat4x2
{
	float operator[](size_t i) const { return matrix[i]; }
	float &operator[](size_t i) { return matrix[i]; }
	float matrix[4 * 2];
};

struct mat3x4
{
	float operator[](size_t i) const { return matrix[i]; }
	float &operator[](size_t i) { return matrix[i]; }
	float matrix[3 * 4];
};

struct mat3x2
{
	float operator[](size_t i) const { return matrix[i]; }
	float &operator[](size_t i) { return matrix[i]; }
	float matrix[3 * 2];
};

struct mat2x4
{
	float operator[](size_t i) const { return matrix[i]; }
	float &operator[](size_t i) { return matrix[i]; }
	float matrix[2 * 4];
};

struct mat2x3
{
	float operator[](size_t i) const { return matrix[i]; }
	float &operator[](size_t i) { return matrix[i]; }
	float matrix[2 * 3];
};
