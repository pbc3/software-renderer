#pragma once


#include <iostream>
#include <format>
#include <algorithm>


constexpr float PI = 3.141592653589793f;

__forceinline  bool IsNearlyZero(float Value, float Epsilon = 1e-6f)
{
	return std::fabs(Value) < Epsilon;
}
inline float degrees_to_radians(float degrees)
{
	return degrees * (PI / 180.0f);
};

inline float radians_to_degrees(float radians)
{
	return radians * (180.0f / PI);
};
constexpr float lerp(float a, float b, float t) {
	return a + t * (b - a);
}
struct Vector3 {
	union {
		struct { float x, y, z; };
		struct { float r, g, b; };
		float data[3];
	};
	constexpr Vector3(float x, float y, float z) : x(x), y(y), z(z) {};
	constexpr Vector3(float x) : x(x), y(x), z(x) {};
	constexpr Vector3() : Vector3(0.f) {};

	Vector3& operator*=(float rhs) {
		x *= rhs;
		y *= rhs;
		z *= rhs;
		return (*this);
	}
	[[nodiscard]] Vector3 operator*(float rhs) const {
		return Vector3(x * rhs, y * rhs, z * rhs);
	}
	__forceinline Vector3& operator-=(const Vector3& rhs) {
		x -= rhs.x;
		y -= rhs.y;
		z -= rhs.z;
		return *this;
	}
	Vector3 operator/(float rhs) const
	{
		float inv = 1.0f / rhs;
		return { x * inv, y * inv, z * inv };
	}
	Vector3 operator-(const Vector3 rhs) const {
		return Vector3(x - rhs.x, y - rhs.y, z - rhs.z);
	}
	[[nodiscard]] float length() const noexcept
	{
		return std::hypot(x, y, z);
	}
	float lengthSqaured() {
		float sx = x * x;
		float sy = y * y;
		float sz = z * z;
		return sx + sy + sz;
	}
	void normalize()
	{

		float len = length();
		if (IsNearlyZero(len)) {
			return;
		}
		x = x / len; y = y / len; z = z / len;
	}
	Vector3 normalized()
	{
		float len = length();
		return Vector3(x / len, y / len, z / len);
	}
	std::string to_string() const
	{
		return std::format("{}, {}, {}", x, y, z);
	}

	__forceinline
		constexpr float Dot(const Vector3& rhs) const {
		return (this->x * rhs.x) + (this->y * rhs.y) + (this->z * rhs.z);
	}

	inline constexpr Vector3 cross(const Vector3& rhs) const {
		return Vector3
		(this->y * rhs.z - this->z * rhs.y,
			this->z * rhs.x - this->x * rhs.z,
			this->x * rhs.y - this->y * rhs.x
		);
	}
	inline constexpr Vector3 cross(const Vector3& rhs) {
		return Vector3
		(this->y * rhs.z - this->z * rhs.y,
			this->z * rhs.x - this->x * rhs.z,
			this->x * rhs.y - this->y * rhs.x
		);
	}
	Vector3& negate() {
		this->x = -x;
		this->y = -y;
		this->z = -z;
		return (*this);
	}

	bool isOrthogonal(Vector3& w) {
		return IsNearlyZero(Dot(w));
	}

	bool isZero() {
		// change this to check for near zero
		return IsNearlyZero(this->x) && IsNearlyZero(this->y) && IsNearlyZero(this->z);
	}
};


inline Vector3 UPt{ 1,1,1 };
inline Vector3 RIGHTt{ 50,1,1 };
inline Vector3 BuildOrthogonal(Vector3& v) {
	if (v.isZero()) { return Vector3{ 0 }; }

	Vector3 orthogonal = v.cross(UPt);
	if (orthogonal.isZero()) {
		orthogonal = v.cross(RIGHTt);
	}
	return orthogonal.normalized();
}

inline constexpr float EPSILON = 0.001f;
inline bool Equals(float lhs, float rhs, float epslion = EPSILON) {
	// checks if 2 floats are equal enough
	return fabs(lhs - rhs) < epslion ? true : false;
}
inline bool Vector3NearEqual(Vector3& lhs, Vector3& rhs, Vector3 epilson = Vector3(EPSILON)) {
	return
		fabs(lhs.x - rhs.x) <= epilson.x && fabs(lhs.y - rhs.y) <= epilson.y && fabs(lhs.z - rhs.z) <= epilson.z;
}
// not for real time / hot loops
 inline float angleBetweenUnitVectors(const Vector3& lhs, const Vector3& rhs) {
	return std::acosf(std::clamp(lhs.Dot(rhs), -1.0f, 1.0f));
}

// projection of v1 onto w0
inline Vector3 projection(Vector3& v1, Vector3& w0) {
	auto dotofv1w0 = v1.Dot(w0);
	auto dotofw0w0 = w0.Dot(w0);
	if (dotofw0w0 < 1e-6f) {
		return Vector3(0, 0, 0);
	}
	auto r = dotofv1w0 / dotofw0w0;
	return w0 * r;
}
inline Vector3 perp(Vector3& v1, Vector3& w0) {
	Vector3 parallel = projection(v1, w0);
	return v1 - parallel;
}


inline Vector3 project(const Vector3& a, const Vector3& b) {
	float bsquared = b.Dot(b);
	if (IsNearlyZero(bsquared)) {
		return { 0,0,0 };
	}
	return b * (a.Dot(b) / bsquared);
}
inline Vector3 reject(const Vector3& a, const Vector3& b) {
	return a - project(a, b);
}
inline Vector3 projectNormalized(const Vector3& a, const Vector3& bUnit) {
	return bUnit * a.Dot(bUnit);
}



#include <vector>
inline std::vector<Vector3> GramSchmidt(std::vector<Vector3>& vecs) {
	std::vector<Vector3> orthonormalSet;
	for (const Vector3 vec : vecs) {
		Vector3 v = vec;
		// iterate over the current orthonormal set, subtract the projections of each vector already in from w
		for (Vector3 w : orthonormalSet) {
			v -= projection(v, w);
		}
		// normalize it before adding
		v.normalize();
		orthonormalSet.push_back(v);
	}
	return orthonormalSet;
}



// only for 3 vectors!
inline std::vector<Vector3> BuildOrthonomral(Vector3& v0, Vector3& v1) {
	std::vector<Vector3> orthonormal;
	Vector3 w0 = v0;
	w0.normalize();
	Vector3 w2 = w0.cross(v1);

	if (w2.lengthSqaured() < 1e-6f) {
		Vector3 fb = (fabs(w0.x) < 0.9f) ? Vector3(1, 0, 0) : Vector3(0, 1, 0);
		w2 = w0.cross(fb);
	}

	w2.normalize();

	Vector3 w1 = w2.cross(w0);
	w1.normalize();

	orthonormal.push_back(w0);

	orthonormal.push_back(w1);

	orthonormal.push_back(w2);

	return orthonormal;
}
inline float det2x2(float a, float b, float c, float d) {
	return a * d - b * c;
}
struct Matrix3D {
	// column-major ordering
	// n[column][row]
	float n[3][3];

	Matrix3D() = default;

	// this constructor takes arguments in a row-major format, then transposes them to fit column major
	Matrix3D(float n00, float n01, float n02,
		float n10, float n11, float n12,
		float n20, float n21, float n22)
	{
		n[0][0] = n00; n[0][1] = n10; n[0][2] = n20;
		n[1][0] = n01; n[1][1] = n11; n[1][2] = n21;
		n[2][0] = n02; n[2][1] = n12; n[2][2] = n22;
	}
	// this constructor takes 3 vectors acting as the indvidual columns (0,1,2) and sets them normally (no transpose)
	Matrix3D(const Vector3& a, const Vector3& b, const Vector3& c) {
		n[0][0] = a.x; n[0][1] = a.y; n[0][2] = a.z;
		n[1][0] = b.x; n[1][1] = b.y; n[1][2] = b.z;
		n[2][0] = c.x; n[2][1] = c.y; n[2][2] = c.z;
	}

	Matrix3D& operator*= (float rhs) {
		n[0][0] *= rhs; n[0][1] *= rhs; n[0][2] *= rhs;
		n[1][0] *= rhs; n[1][1] *= rhs; n[1][2] *= rhs;
		n[2][0] *= rhs; n[2][1] *= rhs; n[2][2] *= rhs;
		return (*this);
	}
	Matrix3D operator*(float rhs) {
		Matrix3D copy = *this;
		copy *= rhs;
		return copy;
	}
	// take in a row major ordering, and return the specified number by using column major access to ours (as its column major)
	float& operator ()(int i, int j) {
		return n[j][i];
	}
	const float& operator ()(int i, int j) const {
		return n[j][i];
	}
	Vector3& operator[](int j) { //return a specified column
		return (*reinterpret_cast<Vector3*>(n[j]));
	}
	const Vector3& operator[](int j) const { //return a specified column
		return (*reinterpret_cast<const Vector3*>(n[j]));
	}


	static Matrix3D identity() {
		return Matrix3D
		{ 1,0,0,
		 0,1,0,
		 0,0,1 };
	}


	static float GetMinor(Matrix3D& m, int row, int col) {
		int r = 0, c = 0;
		float out_minor[2][2];
		for (int i = 0; i < 3; ++i) {
			if (i == row) { continue; }
			c = 0;

			for (int j = 0; j < 3; ++j) {
				if (j == col) { continue; }

				out_minor[r][c] = m(i, j);
				c++;
			}
			r++;
		}
		return det2x2(out_minor[0][0], out_minor[0][1], out_minor[1][0], out_minor[1][1]);
	}

	float GetDet() {
		float a = operator()(0, 0);
		float b = operator()(0, 1);
		float c = operator()(0, 2);

		float detMinorA = GetMinor(*this, 0, 0);
		float detMinorB = -(GetMinor(*this, 0, 1));
		float detMinorC = GetMinor(*this, 0, 2);

		a *= detMinorA;
		b *= detMinorB;
		c *= detMinorC;
		return a + b + c;
	}
};





struct Vector4 {
	
	union {
		struct { float x, y, z,w; };
		struct { float r, g, b,a; };
		float data[4];
	};
	constexpr Vector4(float x, float y, float z, float w) : x(x), y(y), z(z), w(w) {};
	constexpr Vector4(float x) : x(x), y(x), z(x), w(1) {};
	constexpr Vector4() : Vector4(0.f) {};
	constexpr Vector4(Vector3& v, float w) : x(v.x), y(v.y), z(v.z), w(w) {};
	constexpr Vector4(Vector3& v) : x(v.x), y(v.y), z(v.z), w(1) {};
	Vector4& operator*=(float rhs) {
		x *= rhs;
		y *= rhs;
		z *= rhs;
		return (*this);
	}
	[[nodiscard]] Vector4 operator*(float rhs) const {
		return Vector4(x * rhs, y * rhs, z * rhs,1);
	}

	bool isEmpty() const {
		return this->x == 0 && this->y == 0 && this->z == 0;
	}
	//__forceinline Vector4& operator-=(const Vector3& rhs) {
	//	x -= rhs.x;
	//	y -= rhs.y;
	//	z -= rhs.z;
	//	return *this;
	//}
	//Vector3 operator/(float rhs) const
	//{
	//	float inv = 1.0f / rhs;
	//	return { x * inv, y * inv, z * inv };
	//}
	Vector4 operator-(const Vector4 rhs) const {
		return Vector4(x - rhs.x, y - rhs.y, z - rhs.z,1);
	}
	Vector4 operator-() const {
		return Vector4{ -x, -y, -z, -w };
	}
	void operator/=(float rhs) {
		this->x /= rhs;
		this->y /= rhs;
		this->z /= rhs;
	}
	//[[nodiscard]] float length() const noexcept
	//{
	//	return std::hypot(x, y, z);
	//}
	//float lengthSqaured() {
	//	float sx = x * x;
	//	float sy = y * y;
	//	float sz = z * z;
	//	return sx + sy + sz;
	//}
	//void normalize()
	//{

	//	float len = length();
	//	if (IsNearlyZero(len)) {
	//		return;
	//	}
	//	x = x / len; y = y / len; z = z / len;
	//}
	//Vector3 normalized()
	//{
	//	float len = length();
	//	return Vector3(x / len, y / len, z / len);
	//}
	//std::string to_string() const
	//{
	//	return std::format("{}, {}, {}", x, y, z);
	//}

	//__forceinline
	//	constexpr float Dot(const Vector4& rhs) const {
	//	return (this->x * rhs.x) + (this->y * rhs.y) + (this->z * rhs.z);
	//}

	//inline constexpr Vector3 cross(const Vector3& rhs) const {
	//	return Vector3
	//	(this->y * rhs.z - this->z * rhs.y,
	//		this->z * rhs.x - this->x * rhs.z,
	//		this->x * rhs.y - this->y * rhs.x
	//	);
	//}
	//inline constexpr Vector3 cross(const Vector3& rhs) {
	//	return Vector3
	//	(this->y * rhs.z - this->z * rhs.y,
	//		this->z * rhs.x - this->x * rhs.z,
	//		this->x * rhs.y - this->y * rhs.x
	//	);
	//}
	//Vector3& negate() {
	//	this->x = -x;
	//	this->y = -y;
	//	this->z = -z;
	//	return (*this);
	//}

	//bool isOrthogonal(Vector4& w) {
	//	return IsNearlyZero(Dot(w));
	//}

	//bool isZero() {
	//	// change this to check for near zero
	//	return IsNearlyZero(this->x) && IsNearlyZero(this->y) && IsNearlyZero(this->z);
	//}
};


constexpr float Dot(const Vector4& lhs, const Vector4& rhs) {
	return (lhs.x * rhs.x) + (lhs.y * rhs.y) + (lhs.z * rhs.z);
}
struct Matrix4D {
	// this matrix is COLUMN major ordering, and has 2 constructors. the first one - manually setting each entry, is row major. dont use
	// the second one takes 4 vectors, and sets them as each individual column in the matrix
	// column-major ordering
	// n[column][row]
	float n[4][4];

	Matrix4D() = default;

	// this constructor takes arguments in a row-major format, then transposes them to fit column major
	Matrix4D(float n00, float n01, float n02, float n03,
		float n10, float n11, float n12, float n13,
		float n20, float n21, float n22, float n23,
		float n30, float n31, float n32, float n33)
	{
		n[0][0] = n00; n[0][1] = n10; n[0][2] = n20; n[0][3] = n30;
		n[1][0] = n01; n[1][1] = n11; n[1][2] = n21; n[1][3] = n31;
		n[2][0] = n02; n[2][1] = n12; n[2][2] = n22; n[2][3] = n32;
		n[3][0] = n03; n[3][1] = n13; n[3][2] = n23; n[3][3] = n33;
	}
	// this constructor takes 4 vectors acting as the indvidual columns (0,1,2,3) and sets them normally (no transpose)
	Matrix4D(const Vector4& a, const Vector4& b, const Vector4& c, const Vector4& d) {
		n[0][0] = a.x; n[0][1] = a.y; n[0][2] = a.z; n[0][3] = a.w;
		n[1][0] = b.x; n[1][1] = b.y; n[1][2] = b.z; n[1][3] = b.w;
		n[2][0] = c.x; n[2][1] = c.y; n[2][2] = c.z; n[2][3] = c.w;
		n[3][0] = d.x; n[3][1] = d.y; n[3][2] = d.z; n[3][3] = d.w;
	}
	Matrix4D(const Vector4& a) {
		n[0][0] = a.x; n[0][1] = 0; n[0][2] = 0; n[0][3] = 0;
		n[1][0] = 0; n[1][1] = a.y; n[1][2] = 0; n[1][3] = 0;
		n[2][0] = 0; n[2][1] = 0; n[2][2] = a.z; n[2][3] = 0;
		n[3][0] = 0; n[3][1] = 0; n[3][2] = 0; n[3][3] = a.w;
	}
	Matrix4D& operator*= (float rhs) {
		n[0][0] *= rhs; n[0][1] *= rhs; n[0][2] *= rhs; n[0][3] *= rhs;
		n[1][0] *= rhs; n[1][1] *= rhs; n[1][2] *= rhs; n[1][3] *= rhs;
		n[2][0] *= rhs; n[2][1] *= rhs; n[2][2] *= rhs; n[2][3] *= rhs;
		n[3][0] *= rhs; n[3][1] *= rhs; n[3][2] *= rhs; n[3][3] *= rhs;
		return (*this);
	}
	Matrix4D operator*(float rhs) {
		Matrix4D copy = *this;
		copy *= rhs;
		return copy;
	}
	// take in a row major ordering, and return the specified number by using column major access to ours (as its column major)
	float& operator ()(int i, int j) {
		return n[j][i];
	}
	const float& operator ()(int i, int j) const {
		return n[j][i];
	}
	Vector4& operator[](int j) { //return a specified column
		return (*reinterpret_cast<Vector4*>(n[j]));
	}

	const Vector4& operator[](int j) const { //return a specified column
		return (*reinterpret_cast<const Vector4*>(n[j]));
	}

	Matrix4D Transpose() const
	{
		// utlise my transpose contsructor
		return Matrix4D(
			n[0][0], n[0][1], n[0][2], n[0][3],
			n[1][0], n[1][1], n[1][2], n[1][3],
			n[2][0], n[2][1], n[2][2], n[2][3],
			n[3][0], n[3][1], n[3][2], n[3][3]
		);
	}


	
	static Matrix4D Rx(float degrees) {
		float radians = degrees_to_radians(degrees);
		Vector4 col1 = { 1,0,0,0 };
		Vector4 col2 = { 0, cosf(radians),sinf(radians),0 };
		Vector4 col3 = { 0,-sinf(radians),cosf(radians),0 };
		Vector4 col4 = { 0,0,0,1 };
		Matrix4D rotX(col1, col2, col3, col4);
		return rotX;
	}
	static Matrix4D Ry(float degrees) {
		float radians = degrees_to_radians(degrees);
		Vector4 col1 = { cosf(radians),0,-sinf(radians),0 };
		Vector4 col2 = { 0,1,0,0 };
		Vector4 col3 = { sinf(radians),0,cosf(radians),0 };
		Vector4 col4 = { 0,0,0,1 };
		Matrix4D rotY(col1, col2, col3, col4);
		return rotY;
	}
	static Matrix4D Rz(float degrees) {
		float radians = degrees_to_radians(degrees);
		Vector4 col1 = { cosf(radians),sinf(radians),0,0 };
		Vector4 col2 = { -sinf(radians),cosf(radians),0,0 };
		Vector4 col3 = { 0,0,1,0 };
		Vector4 col4 = { 0,0,0,1 };
		Matrix4D rotZ(col1, col2, col3, col4);
		for (int i = 0; i < 4; i++) {
			for (int j = 0; j < 4; j++) {
				if (IsNearlyZero(rotZ(i, j))) {
					rotZ(i, j) = 0;
				}
			}
		}
		return rotZ;
	}

	inline static Matrix4D Rotation(Vector3 euler);


	static Matrix4D identity() {
		Matrix4D n({0,0,0,0});
		n(0, 0) = 1; n(0, 1) = 0; n(0, 2) = 0; n(0, 3) = 0;
		n(1, 0) = 0; n(1, 1) = 1; n(1, 2) = 0; n(1, 3) = 0;
		n(2, 0) = 0; n(2, 1) = 0; n(2, 2) = 1; n(2, 3) = 0;
		n(3, 0) = 0; n(3, 1) = 0; n(3, 2) = 0; n(3, 3) = 1;
		return n;
		//{ 1,0,0,0,
		// 0,1,0,0,
		// 0,0,1,0,
		// 0,0,0,1 };
	}
	void toString() {
		for (int i = 0; i < 4; i++) {
			for (int j = 0; j < 4; j++) {
				std::cout << this->operator()(i, j) << ",";
			}
			std::cout << "\n";
		}
	}

};



inline Matrix3D operator*(const Matrix3D& lhs, const Matrix3D& rhs)
{
	return Matrix3D(
		lhs(0, 0) * rhs(0, 0) + lhs(0, 1) * rhs(1, 0) + lhs(0, 2) * rhs(2, 0),
		lhs(0, 0) * rhs(0, 1) + lhs(0, 1) * rhs(1, 1) + lhs(0, 2) * rhs(2, 1),
		lhs(0, 0) * rhs(0, 2) + lhs(0, 1) * rhs(1, 2) + lhs(0, 2) * rhs(2, 2),

		lhs(1, 0) * rhs(0, 0) + lhs(1, 1) * rhs(1, 0) + lhs(1, 2) * rhs(2, 0),
		lhs(1, 0) * rhs(0, 1) + lhs(1, 1) * rhs(1, 1) + lhs(1, 2) * rhs(2, 1),
		lhs(1, 0) * rhs(0, 2) + lhs(1, 1) * rhs(1, 2) + lhs(1, 2) * rhs(2, 2),

		lhs(2, 0) * rhs(0, 0) + lhs(2, 1) * rhs(1, 0) + lhs(2, 2) * rhs(2, 0),
		lhs(2, 0) * rhs(0, 1) + lhs(2, 1) * rhs(1, 1) + lhs(2, 2) * rhs(2, 1),
		lhs(2, 0) * rhs(0, 2) + lhs(2, 1) * rhs(1, 2) + lhs(2, 2) * rhs(2, 2)
	);
}

inline Matrix4D operator*(const Matrix4D& lhs, const Matrix4D& rhs)
{
	return Matrix4D(
		lhs(0, 0) * rhs(0, 0) + lhs(0, 1) * rhs(1, 0) + lhs(0, 2) * rhs(2, 0) + lhs(0, 3) * rhs(3, 0),
		lhs(0, 0) * rhs(0, 1) + lhs(0, 1) * rhs(1, 1) + lhs(0, 2) * rhs(2, 1) + lhs(0, 3) * rhs(3, 1),
		lhs(0, 0) * rhs(0, 2) + lhs(0, 1) * rhs(1, 2) + lhs(0, 2) * rhs(2, 2) + lhs(0, 3) * rhs(3, 2),
		lhs(0, 0) * rhs(0, 3) + lhs(0, 1) * rhs(1, 3) + lhs(0, 2) * rhs(2, 3) + lhs(0, 3) * rhs(3, 3),

		lhs(1, 0) * rhs(0, 0) + lhs(1, 1) * rhs(1, 0) + lhs(1, 2) * rhs(2, 0) + lhs(1, 3) * rhs(3, 0),
		lhs(1, 0) * rhs(0, 1) + lhs(1, 1) * rhs(1, 1) + lhs(1, 2) * rhs(2, 1) + lhs(1, 3) * rhs(3, 1),
		lhs(1, 0) * rhs(0, 2) + lhs(1, 1) * rhs(1, 2) + lhs(1, 2) * rhs(2, 2) + lhs(1, 3) * rhs(3, 2),
		lhs(1, 0) * rhs(0, 3) + lhs(1, 1) * rhs(1, 3) + lhs(1, 2) * rhs(2, 3) + lhs(1, 3) * rhs(3, 3),

		lhs(2, 0) * rhs(0, 0) + lhs(2, 1) * rhs(1, 0) + lhs(2, 2) * rhs(2, 0) + lhs(2, 3) * rhs(3, 0),
		lhs(2, 0) * rhs(0, 1) + lhs(2, 1) * rhs(1, 1) + lhs(2, 2) * rhs(2, 1) + lhs(2, 3) * rhs(3, 1),
		lhs(2, 0) * rhs(0, 2) + lhs(2, 1) * rhs(1, 2) + lhs(2, 2) * rhs(2, 2) + lhs(2, 3) * rhs(3, 2),
		lhs(2, 0) * rhs(0, 3) + lhs(2, 1) * rhs(1, 3) + lhs(2, 2) * rhs(2, 3) + lhs(2, 3) * rhs(3, 3),

		lhs(3, 0) * rhs(0, 0) + lhs(3, 1) * rhs(1, 0) + lhs(3, 2) * rhs(2, 0) + lhs(3, 3) * rhs(3, 0),
		lhs(3, 0) * rhs(0, 1) + lhs(3, 1) * rhs(1, 1) + lhs(3, 2) * rhs(2, 1) + lhs(3, 3) * rhs(3, 1),
		lhs(3, 0) * rhs(0, 2) + lhs(3, 1) * rhs(1, 2) + lhs(3, 2) * rhs(2, 2) + lhs(3, 3) * rhs(3, 2),
		lhs(3, 0) * rhs(0, 3) + lhs(3, 1) * rhs(1, 3) + lhs(3, 2) * rhs(2, 3) + lhs(3, 3) * rhs(3, 3)
	);
}

inline Matrix4D Matrix4D::Rotation(Vector3 euler)
{
	return Matrix4D::Rz(euler.z) * Matrix4D::Ry(euler.y) * Matrix4D::Rx(euler.x);
}

[[nodiscard]] inline float angle_between_acos(const Vector3& lhs, const Vector3& rhs)
{
	// introduces rounding error xd? 
	float x = std::acosf(lhs.Dot(rhs));
	return std::acosf((lhs.Dot(rhs)) / (lhs.length() * rhs.length()));
}

[[nodiscard]] inline float angle_between(const Vector3& lhs, const Vector3& rhs)
{
	Vector3 cross = lhs.cross(rhs);
	float len = cross.length();
	return std::atan2(len, lhs.Dot(rhs));
}


inline constexpr auto Orthonormalize = GramSchmidt;


//float GetDeterminant(Matrix3D& m);
//float GetMinor(Matrix3D& m, int i, int j);
//bool GetInverse(Matrix3D& m, Matrix3D& out_Inverse);

struct Vector2{
	float x, y;
	constexpr Vector2(float x, float y) : x(x), y(y) {};
	constexpr Vector2(float x) : x(x), y(x) {};
	constexpr Vector2() : Vector2(0.f) {};
	constexpr Vector2(Vector3 v3) : x(v3.x), y(v3.y) {};




	Vector2& operator*=(float rhs) {
		x *= rhs;
		y *= rhs;
		return (*this);
	}
	[[nodiscard]] Vector2 operator*(float rhs) const {
		return Vector2(x * rhs, y * rhs);
	}
	[[nodiscard]] Vector2 operator+(Vector2 lhs) {
		return Vector2{
			this->x + lhs.x,
			this->y + lhs.y
		};
	}
	__forceinline Vector2& operator-=(const Vector2& rhs) {
		x -= rhs.x;
		y -= rhs.y;
		return *this;
	}
	Vector2& operator/(float rhs) {
		float s = 1.0f / rhs;
		x *= s;
		y *= s;
		return (*this);
	}
	Vector2 operator-(Vector2 rhs) {
		return Vector2(x - rhs.x, y - rhs.y);
	}
	[[nodiscard]] float length() const noexcept
	{
		return std::hypot(x, y);
	}
	float lengthSqaured() {
		float sx = x * x;
		float sy = y * y;
		return sx + sy;
	}
	// in place
	void normalize()
	{
		float len = length();
		//if(IsNearlyZero(len))
		x = x / len; y = y / len;
	}
	// returns new, doesnt modify og
	Vector2 normalized()
	{
		float len = length();
		return Vector2(x / len, y / len);
	}
	std::string to_string() const
	{
		return std::format("{}, {}", x, y);
	}

	__forceinline
		constexpr float Dot(const Vector2& rhs) const {
		return (this->x * rhs.x) + (this->y * rhs.y);
	}


	Vector2& negate() {
		this->x = -x;
		this->y = -y;
		return (*this);
	}

	bool isOrthogonal(Vector2& w) {
		return IsNearlyZero(Dot(w));
	}

	bool isZero() {
		// change this to check for near zero
		return IsNearlyZero(this->x) && IsNearlyZero(this->y);
	}
};


//inline float Cross2D(const Vector2& lhs, const Vector2& rhs) {
//	return lhs.x * rhs.y - lhs.y * rhs.x;
//}
//inline int Cross2D(const Vector2& lhs, const Vector2& rhs) {
//	return lhs.x * rhs.y - lhs.y * rhs.x;
//}

inline Vector2 AngleToDirection(float degrees) {
	float radians = degrees_to_radians(degrees);
	return {
		cosf(radians),sinf(radians)
	};
}
inline Vector3 AngleToDirectionV3(float degrees) {
	float radians = degrees_to_radians(degrees);
	return {
		cosf(radians),sinf(radians),0.f
	};
}


inline Vector2 rotateV2(Vector2 v, float degrees) {
	float rads = degrees_to_radians(degrees);
	float cosRads = cosf(rads);
	float sinRads = sinf(rads);
	Vector2 rotated = {
		 (v.x * cosRads - v.y * sinRads),
		 (v.x * sinRads + v.y * cosRads)
	};
	if (IsNearlyZero(rotated.x)) { rotated.x = 0; }
	if (IsNearlyZero(rotated.y)) { rotated.y = 0; }
	return rotated;
}

inline Vector4 operator*(const Matrix4D& mat4, const Vector4& vec4) {
	return Vector4{
		(mat4(0,0) * vec4.x) + (mat4(0,1) * vec4.y) + (mat4(0,2) * vec4.z) + (mat4(0,3) * vec4.w),
		(mat4(1,0) * vec4.x) + (mat4(1,1) * vec4.y) + (mat4(1,2) * vec4.z) + (mat4(1,3) * vec4.w),
		(mat4(2,0) * vec4.x) + (mat4(2,1) * vec4.y) + (mat4(2,2) * vec4.z) + (mat4(2,3) * vec4.w),
		(mat4(3,0) * vec4.x) + (mat4(3,1) * vec4.y) + (mat4(3,2) * vec4.z) + (mat4(3,3) * vec4.w)
	};
}
