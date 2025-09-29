#pragma once

#define M_RADPI		57.295779513082f
#define M_PI		3.14159265358979323846f

#define DEG2RAD(x)	(static_cast<float>(x) * (M_PI / 180.0f))
#define RAD2DEG(x)	(static_cast<float>(x) * (180.0f / M_PI))

#define DEG2RADF(f)	(f * (M_PI / 180.0f))
#define RAD2DEGF(f)	(f * (180.0f / M_PI))
#include "remix/remix_c.h"

//namespace shared::utils
//{
	typedef float vec_t;

	class Vector4D
	{
	public:
		Vector4D(void)
		{
			x = y = z = w = 0.0f;
		}

		Vector4D(float X, float Y, float Z, float W)
		{
			x = X; y = Y; z = Z; w = W;
		}

		Vector4D(float* v)
		{
			x = v[0]; y = v[1]; z = v[2]; w = v[3];
		}

		Vector4D operator+(const Vector4D& v) const
		{
			return Vector4D(x + v.x, y + v.y, z + v.z, w + v.w);
		}

		Vector4D operator-(const Vector4D& v) const
		{
			return Vector4D(x - v.x, y - v.y, z - v.z, w - v.w);
		}

		Vector4D operator*(const Vector4D& v) const
		{
			return Vector4D(x * v.x, y * v.y, z * v.z, w * v.w);
		}

		Vector4D operator/(const Vector4D& v) const
		{
			return Vector4D(x / v.x, y / v.y, z / v.z, w / v.w);
		}

		Vector4D operator+(float v) const
		{
			return Vector4D(x + v, y + v, z + v, w + v);
		}

		Vector4D operator-(float v) const
		{
			return Vector4D(x - v, y - v, z - v, w - v);
		}

		Vector4D operator*(float v) const
		{
			return Vector4D(x * v, y * v, z * v, w * v);
		}

		friend Vector4D operator*(float v, const Vector4D& vec)
		{
			return Vector4D(vec.x * v, vec.y * v, vec.z * v, vec.w * v);
		}

		Vector4D operator/(float v) const
		{
			return Vector4D(x / v, y / v, z / v, w / v);
		}

		Vector4D operator-() const
		{
			return Vector4D(-x, -y, -z, -w);
		}

		vec_t x, y, z, w;
	};

	class Vector2D
	{
	public:
		Vector2D(void)
		{
			x = y = 0.0f;
		}

		Vector2D(float X, float Y)
		{
			x = X; y = Y;
		}

		Vector2D(float* v)
		{
			x = v[0]; y = v[1];
		}

		Vector2D(const float* v)
		{
			x = v[0]; y = v[1];
		}

		Vector2D(const Vector2D& v)
		{
			x = v.x; y = v.y;
		}

		Vector2D& operator=(const Vector2D& v)
		{
			x = v.x; y = v.y; return *this;
		}

		float& operator[](int i)
		{
			return ((float*)this)[i];
		}

		float operator[](int i) const
		{
			return ((float*)this)[i];
		}

		Vector2D& operator+=(const Vector2D& v)
		{
			x += v.x; y += v.y; return *this;
		}

		Vector2D& operator-=(const Vector2D& v)
		{
			x -= v.x; y -= v.y; return *this;
		}

		Vector2D& operator*=(const Vector2D& v)
		{
			x *= v.x; y *= v.y; return *this;
		}

		Vector2D& operator/=(const Vector2D& v)
		{
			x /= v.x; y /= v.y; return *this;
		}

		Vector2D& operator+=(float v)
		{
			x += v; y += v; return *this;
		}

		Vector2D& operator-=(float v)
		{
			x -= v; y -= v; return *this;
		}

		Vector2D& operator*=(float v)
		{
			x *= v; y *= v; return *this;
		}

		Vector2D& operator/=(float v)
		{
			x /= v; y /= v; return *this;
		}

		Vector2D operator+(const Vector2D& v) const
		{
			return Vector2D(x + v.x, y + v.y);
		}

		Vector2D operator-(const Vector2D& v) const
		{
			return Vector2D(x - v.x, y - v.y);
		}

		Vector2D operator*(const Vector2D& v) const
		{
			return Vector2D(x * v.x, y * v.y);
		}

		Vector2D operator/(const Vector2D& v) const
		{
			return Vector2D(x / v.x, y / v.y);
		}

		Vector2D operator+(float v) const
		{
			return Vector2D(x + v, y + v);
		}

		Vector2D operator-(float v) const
		{
			return Vector2D(x - v, y - v);
		}

		Vector2D operator*(float v) const
		{
			return Vector2D(x * v, y * v);
		}

		Vector2D operator/(float v) const
		{
			return Vector2D(x / v, y / v);
		}

		void Set(float X = 0.0f, float Y = 0.0f)
		{
			x = X; y = Y;
		}

		float Length(void) const
		{
			return ::sqrtf(x * x + y * y);
		}

		float LengthSqr(void) const
		{
			return (x * x + y * y);
		}

		float DistTo(const Vector2D& v) const
		{
			return (*this - v).Length();
		}

		float DistToSqr(const Vector2D& v) const
		{
			return (*this - v).LengthSqr();
		}

		float Dot(const Vector2D& v) const
		{
			return (x * v.x + y * v.y);
		}

		bool IsZero(void) const
		{
			return (x > -0.01f && x < 0.01f &&
				y > -0.01f && y < 0.01f);
		}

	public:
		vec_t x, y;
	};

	class Vector
	{
	public:
		Vector(void)
		{
			x = y = z = 0.0f;
		}

		Vector(float X, float Y, float Z)
		{
			x = X; y = Y; z = Z;
		}

		Vector(float* v)
		{
			x = v[0]; y = v[1]; z = v[2];
		}

		Vector(const float* v)
		{
			x = v[0]; y = v[1]; z = v[2];
		}

		Vector(const Vector& v)
		{
			x = v.x; y = v.y; z = v.z;
		}

		Vector(const Vector4D& v)
		{
			x = v.x; y = v.y; z = v.z;
		}

		Vector(const Vector2D& v)
		{
			x = v.x; y = v.y; z = 0.0f;
		}

		Vector& operator=(const Vector& v)
		{
			x = v.x; y = v.y; z = v.z; return *this;
		}

		Vector& operator=(const Vector2D& v)
		{
			x = v.x; y = v.y; z = 0.0f; return *this;
		}

		float& operator[](int i)
		{
			return ((float*)this)[i];
		}

		float operator[](int i) const
		{
			return ((float*)this)[i];
		}

		Vector& operator+=(const Vector& v)
		{
			x += v.x; y += v.y; z += v.z; return *this;
		}

		Vector& operator-=(const Vector& v)
		{
			x -= v.x; y -= v.y; z -= v.z; return *this;
		}

		Vector& operator*=(const Vector& v)
		{
			x *= v.x; y *= v.y; z *= v.z; return *this;
		}

		Vector& operator/=(const Vector& v)
		{
			x /= v.x; y /= v.y; z /= v.z; return *this;
		}

		Vector& operator+=(float v)
		{
			x += v; y += v; z += v; return *this;
		}

		Vector& operator-=(float v)
		{
			x -= v; y -= v; z -= v; return *this;
		}

		Vector& operator*=(float v)
		{
			x *= v; y *= v; z *= v; return *this;
		}

		Vector& operator/=(float v)
		{
			x /= v; y /= v; z /= v; return *this;
		}

		Vector operator+(const Vector& v) const
		{
			return Vector(x + v.x, y + v.y, z + v.z);
		}

		Vector operator-(const Vector& v) const
		{
			return Vector(x - v.x, y - v.y, z - v.z);
		}

		Vector operator*(const Vector& v) const
		{
			return Vector(x * v.x, y * v.y, z * v.z);
		}

		Vector operator/(const Vector& v) const
		{
			return Vector(x / v.x, y / v.y, z / v.z);
		}

		Vector operator+(float v) const
		{
			return Vector(x + v, y + v, z + v);
		}

		Vector operator-(float v) const
		{
			return Vector(x - v, y - v, z - v);
		}

		Vector operator*(float v) const
		{
			return Vector(x * v, y * v, z * v);
		}

		friend Vector operator*(float v, const Vector& vec)
		{
			return Vector(vec.x * v, vec.y * v, vec.z * v);
		}

		Vector operator/(float v) const
		{
			return Vector(x / v, y / v, z / v);
		}

		Vector operator-() const
		{
			return Vector(-x, -y, -z);
		}

		bool operator==(const Vector& vec) const
		{
			if (std::fabs(x - vec.x) < 1.e-6f
				&& std::fabs(y - vec.y) < 1.e-6f
				&& std::fabs(z - vec.z) < 1.e-6f)
			{
				return true;
			}

			return false;
		}

		bool operator!=(const Vector& vec) const
		{
			if (std::fabs(x - vec.x) >= 1.e-6f
				|| std::fabs(y - vec.y) >= 1.e-6f
				|| std::fabs(z - vec.z) >= 1.e-6f)
			{
				return true;
			}

			return false;
		}

		bool operator>(const Vector& vec) const
		{
			if (x > vec.x && y > vec.y && z > vec.z) {
				return true;
			}

			return false;
		}

		bool operator<(const Vector& vec) const
		{
			if (x < vec.x && y < vec.y && z < vec.z) {
				return true;
			}

			return false;
		}

		float Length(void) const
		{
			return sqrtf(x * x + y * y + z * z);
		}

		float LengthSqr(void) const
		{
			return (x * x + y * y + z * z);
		}

		float Normalize()
		{
			float fl_lenght = Length();
			float fl_lenght_normal = 1.f / ((1.19209290E-07F) + fl_lenght);

			x = x * fl_lenght_normal;
			y = y * fl_lenght_normal;
			z = z * fl_lenght_normal;

			return fl_lenght;
		}

		float NormalizeChecked()
		{
			const float fl_lenght = Length();
			if (fl_lenght != 0.0f)
			{
				const float ilength = 1.0f / fl_lenght;
				x *= ilength;
				y *= ilength;
				z *= ilength;
			}

			return fl_lenght;
		}

		void Rotate(const float flYaw)
		{
			const float r = DEG2RAD(flYaw);
			const float s = sinf(r), c = cosf(r);
			const float flX = x, flY = y;

			x = (flX * c) - (flY * s);
			y = (flX * s) + (flY * c);
		}

		float NormalizeInPlace()
		{
			return Normalize();
		}

		float Length2D(void) const
		{
			return sqrtf(x * x + y * y);
		}

		float Lenght2DSqr(void) const
		{
			return (x * x + y * y);
		}

		float DistTo(const Vector& v) const
		{
			return (*this - v).Length();
		}

		float DistToSqr(const Vector& v) const
		{
			return (*this - v).LengthSqr();
		}

		float Dot(const Vector& v) const
		{
			return (x * v.x + y * v.y + z * v.z);
		}

		Vector Cross(const Vector& v) const
		{
			return Vector(y * v.z - z * v.y, z * v.x - x * v.z, x * v.y - y * v.x);
		}

		bool IsZero(float epsilon = 1.e-6f) const
		{
			return (
				x >= -epsilon && x <= epsilon &&
				y >= -epsilon && y <= epsilon &&
				z >= -epsilon && z <= epsilon);
		}

		Vector Scale(float fl) {
			return Vector(x * fl, y * fl, z * fl);
		}

		void Init(float ix = 0.0f, float iy = 0.0f, float iz = 0.0f)
		{
			x = ix; y = iy; z = iz;
		}

		void Add(const Vector& a, const Vector& b)
		{
			x = (a.x + b.x);
			y = (a.y + b.y);
			z = (a.z + b.z);
		}

		bool is_position_within_aabb(const Vector& min_bounds, const Vector& max_bounds, const Vector& position)
		{
			return	position.x >= min_bounds.x && position.x <= max_bounds.x &&
				position.y >= min_bounds.y && position.y <= max_bounds.y &&
				position.z >= min_bounds.z && position.z <= max_bounds.z;
		}

		remixapi_Float3D ToRemixFloat3D() const
		{
			return remixapi_Float3D{ x, y, z };
		}

	public:
		vec_t x, y, z;
	};


	class __declspec(align(16))VectorAligned : public Vector
	{
	public:
		inline VectorAligned(void) {};

		inline VectorAligned(float x, float y, float z) {
			Init(x, y, z);
		}

		explicit VectorAligned(const Vector& othr) {
			Init(othr.x, othr.y, othr.z);
		}

		VectorAligned& operator=(const Vector& othr) {
			Init(othr.x, othr.y, othr.z);
			return *this;
		}

		vec_t w = 0.0f;
	};

	struct Vertex_t
	{
		Vertex_t() {}
		Vertex_t(const Vector2D& pos, const Vector2D& coord = Vector2D(0, 0))
		{
			m_Position = pos;
			m_TexCoord = coord;
		}
		void Init(const Vector2D& pos, const Vector2D& coord = Vector2D(0, 0))
		{
			m_Position = pos;
			m_TexCoord = coord;
		}

		Vector2D m_Position;
		Vector2D m_TexCoord;
	};

	namespace utils::vector
	{
#define PITCH	0 // up / down
#define YAW		1 // left / right
#define ROLL	2 // fall over

		inline void sin_cos(const float angle, float& sine, float& cosine)
		{
			sine = std::sinf(angle);
			cosine = std::cosf(angle);
		}

		inline void AngleVectors(const Vector& angles, Vector* forward)
		{
			float sp, sy, cp, cy;
			sin_cos(DEG2RADF(angles[YAW]), sp, cp);
			sin_cos(DEG2RADF(angles[PITCH]), sy, cy);

			if (forward)
			{
				forward->x = (cp * cy);
				forward->y = (cp * sy);
				forward->z = -sp;
			}
		}

		inline void AngleVectors(const Vector& angles, Vector* forward, Vector* right, Vector* up)
		{
			float sr, sp, sy, cr, cp, cy;
			sin_cos(DEG2RAD(angles[YAW]), sy, cy);
			sin_cos(DEG2RAD(angles[PITCH]), sp, cp);
			sin_cos(DEG2RAD(angles[ROLL]), sr, cr);

			if (forward)
			{
				forward->x = cp * cy;
				forward->y = cp * sy;
				forward->z = -sp;
			}

			if (right)
			{
				right->x = (-1.0f * sr * sp * cy + -1.0f * cr * -sy);
				right->y = (-1.0f * sr * sp * sy + -1.0f * cr * cy);
				right->z = -1.0f * sr * cp;
			}

			if (up)
			{
				up->x = (cr * sp * cy + -sr * -sy);
				up->y = (cr * sp * sy + -sr * cy);
				up->z = cr * cp;
			}
		}

		inline vec_t dot_product(const Vector& a, const Vector& b)
		{
			return(a.x * b.x + a.y * b.y + a.z * b.z);
		}

		inline void vector_ma_inline(const Vector& start, float scale, const Vector& direction, Vector& dest)
		{
			dest.x = start.x + direction.x * scale;
			dest.y = start.y + direction.y * scale;
			dest.z = start.z + direction.z * scale;
		}

		inline void vector_ma(const Vector& start, float scale, const Vector& direction, Vector& dest)
		{
			vector_ma_inline(start, scale, direction, dest);
		}

		inline bool is_point_in_scaled_aabb(const Vector& point, const Vector& mins, const Vector& maxs, const float scale = 1.0f)
		{
			const Vector center = (mins + maxs) * 0.5f;
			const Vector half_size = (maxs - mins) * 0.5f * scale;

			// Compute the scaled AABB bounds
			const Vector scaled_mins = center - half_size;
			const Vector scaled_maxs = center + half_size;

			return	  (point.x >= scaled_mins.x && point.x <= scaled_maxs.x
				&& point.y >= scaled_mins.y && point.y <= scaled_maxs.y
				&& point.z >= scaled_mins.z && point.z <= scaled_maxs.z);
		}

		inline bool is_point_in_aabb(const Vector& point, const Vector& min_bounds, const Vector& max_bounds)
		{
			return	point.x >= min_bounds.x && point.x <= max_bounds.x &&
				point.y >= min_bounds.y && point.y <= max_bounds.y &&
				point.z >= min_bounds.z && point.z <= max_bounds.z;
		}

		struct matrix3x3
		{
			float m[3][3];

			// identity
			matrix3x3()
			{
				m[0][0] = 1.0f; m[0][1] = 0.0f; m[0][2] = 0.0f;
				m[1][0] = 0.0f; m[1][1] = 1.0f; m[1][2] = 0.0f;
				m[2][0] = 0.0f; m[2][1] = 0.0f; m[2][2] = 1.0f;
			}

			void scale(float x, float y, float z)
			{
				m[0][0] *= x;
				m[1][1] *= y;
				m[2][2] *= z;
			}

			void rotate_x(float radians)
			{
				matrix3x3 rotation;
				float cos_theta = cos(radians);
				float sin_theta = sin(radians);

				rotation.m[0][0] = 1.0f;
				rotation.m[1][1] = cos_theta;
				rotation.m[1][2] = -sin_theta;
				rotation.m[2][1] = sin_theta;
				rotation.m[2][2] = cos_theta;

				*this = multiply(rotation);
			}

			void rotate_y(float radians)
			{
				matrix3x3 rotation;
				float cos_theta = cos(radians);
				float sin_theta = sin(radians);

				rotation.m[0][0] = cos_theta;
				rotation.m[0][2] = sin_theta;
				rotation.m[1][1] = 1.0f;
				rotation.m[2][0] = -sin_theta;
				rotation.m[2][2] = cos_theta;

				*this = multiply(rotation);
			}

			void rotate_z(float radians)
			{
				matrix3x3 rotation;
				float cos_theta = cos(radians);
				float sin_theta = sin(radians);

				rotation.m[0][0] = cos_theta;
				rotation.m[0][1] = -sin_theta;
				rotation.m[1][0] = sin_theta;
				rotation.m[1][1] = cos_theta;
				rotation.m[2][2] = 1.0f;

				*this = multiply(rotation);
			}

			matrix3x3 multiply(const matrix3x3& other) const
			{
				matrix3x3 result;
				for (int i = 0; i < 3; ++i)
				{
					for (int j = 0; j < 3; ++j)
					{
						result.m[i][j] =
							m[i][0] * other.m[0][j] +
							m[i][1] * other.m[1][j] +
							m[i][2] * other.m[2][j];
					}
				}
				return result;
			}

			void transpose()
			{
				std::swap(m[0][1], m[1][0]);
				std::swap(m[0][2], m[2][0]);
				std::swap(m[1][2], m[2][1]);
			}

			remixapi_Transform to_remixapi_transform(const Vector& pos)
			{
				remixapi_Transform result = {};
				result.matrix[0][0] = m[0][0];
				result.matrix[0][1] = m[0][1];
				result.matrix[0][2] = m[0][2];
				result.matrix[1][0] = m[1][0];
				result.matrix[1][1] = m[1][1];
				result.matrix[1][2] = m[1][2];
				result.matrix[2][0] = m[2][0];
				result.matrix[2][1] = m[2][1];
				result.matrix[2][2] = m[2][2];
				result.matrix[0][3] = pos.x;
				result.matrix[1][3] = pos.y;
				result.matrix[2][3] = pos.z;
				return result;
			}
		};
	}
//}
