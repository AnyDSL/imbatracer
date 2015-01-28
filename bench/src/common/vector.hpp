#ifndef IMBA_VECTOR_HPP
#define IMBA_VECTOR_HPP

#include <cmath>
#include <ostream>

namespace imba {

/// Two dimensional vector
struct Vec2 {
    Vec2() {}

    Vec2(float xy) {
        v[0] = xy;
        v[1] = xy;
    }

    Vec2(float x, float y) {
        v[0] = x;
        v[1] = y;
    }

    float operator[] (int i) const {
        return v[i];    
    }

    float& operator[] (int i) {
        return v[i];    
    }
    
    float v[2];
};

inline Vec2 operator + (const Vec2& a, const Vec2& b) {
    return Vec2(a[0] + b[0], a[1] + b[1]);
}

inline Vec2 operator - (const Vec2& a, const Vec2& b) {
    return Vec2(a[0] - b[0], a[1] - b[1]);
}

inline Vec2 operator * (const Vec2& a, const Vec2& b) {
    return Vec2(a[0] * b[0], a[1] * b[1]);
}

inline Vec2 operator / (const Vec2& a, const Vec2& b) {
    return Vec2(a[0] / b[0], a[1] / b[1]);
}

inline Vec2 operator * (const Vec2& v, float t) {
    return Vec2(v[0] * t, v[1] * t);
}

inline Vec2 operator * (float t, const Vec2& v) {
    return Vec2(v[0] * t, v[1] * t);
}

inline float dot(const Vec2& a, const Vec2& b) {
    return a[0] * b[0] + a[1] * b[1];
}

inline float length(const Vec2& v) {
    return sqrtf(dot(v, v));
}

inline Vec2 normalize(const Vec2& v) {
    return v / length(v);
}

/// Three dimensional vector
struct Vec3 {
    Vec3() {}

    Vec3(float xyz) {
        v[0] = xyz;
        v[1] = xyz;
        v[2] = xyz;
    }

    Vec3(float x, float y, float z) {
        v[0] = x;
        v[1] = y;
        v[2] = z;
    }

    float operator[] (int i) const {
        return v[i];    
    }

    float& operator[] (int i) {
        return v[i];    
    }
    
    float v[3];
};

inline Vec3 operator + (const Vec3& a, const Vec3& b) {
    return Vec3(a[0] + b[0], a[1] + b[1], a[2] + b[2]);
}

inline Vec3 operator - (const Vec3& a, const Vec3& b) {
    return Vec3(a[0] - b[0], a[1] - b[1], a[2] - b[2]);
}

inline Vec3 operator * (const Vec3& a, const Vec3& b) {
    return Vec3(a[0] * b[0], a[1] * b[1], a[2] * b[2]);
}

inline Vec3 operator / (const Vec3& a, const Vec3& b) {
    return Vec3(a[0] / b[0], a[1] / b[1], a[2] / b[2]);
}

inline Vec3 operator * (const Vec3& v, float t) {
    return Vec3(v[0] * t, v[1] * t, v[2] * t);
}

inline Vec3 operator * (float t, const Vec3& v) {
    return Vec3(v[0] * t, v[1] * t, v[2] * t);
}

inline Vec3 cross(const Vec3& a, const Vec3& b) {
    return Vec3(a[1] * b[2] - a[2] * b[1],
                a[2] * b[0] - a[0] * b[2],
                a[0] * b[1] - a[1] * b[0]);
}

inline float dot(const Vec3& a, const Vec3& b) {
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

inline float length(const Vec3& v) {
    return sqrtf(dot(v, v));
}

inline Vec3 normalize(const Vec3& v) {
    return v / length(v);
}

inline Vec3 rotate(const Vec3& v, const Vec3& axis, float angle) {
    float q[4];
    q[0] = axis[0] * sinf(angle / 2);
    q[1] = axis[1] * sinf(angle / 2);
    q[2] = axis[2] * sinf(angle / 2);
    q[3] = cosf(angle / 2);
    
    float p[4];
    p[0] = q[3] * v[0] + q[1] * v[2] - q[2] * v[1];
    p[1] = q[3] * v[1] - q[0] * v[2] + q[2] * v[0];
    p[2] = q[3] * v[2] + q[0] * v[1] - q[1] * v[0];
    p[3] = -(q[0] * v[0] + q[1] * v[1] + q[2] * v[2]);

    return Vec3(p[3] * -q[0] + p[0] * q[3] + p[1] * -q[2] - p[2] * -q[1],
                p[3] * -q[1] - p[0] * -q[2] + p[1] * q[3] + p[2] * -q[0],
                p[3] * -q[2] + p[0] * -q[1] - p[1] * -q[0] + p[2] * q[3]);
}

/// Three dimensional vector
struct Vec4 {
    Vec4() {}

    Vec4(float xyzw) {
        v[0] = xyzw;
        v[1] = xyzw;
        v[2] = xyzw;
        v[3] = xyzw;
    }

    Vec4(float x, float y, float z, float w) {
        v[0] = x;
        v[1] = y;
        v[2] = z;
        v[3] = w;
    }

    float operator[] (int i) const {
        return v[i];    
    }

    float& operator[] (int i) {
        return v[i];    
    }
    
    float v[4];
};

inline Vec4 operator + (const Vec4& a, const Vec4& b) {
    return Vec4(a[0] + b[0], a[1] + b[1], a[2] + b[2], a[3] + b[3]);
}

inline Vec4 operator - (const Vec4& a, const Vec4& b) {
    return Vec4(a[0] - b[0], a[1] - b[1], a[2] - b[2], a[3] - b[3]);
}

inline Vec4 operator * (const Vec4& a, const Vec4& b) {
    return Vec4(a[0] * b[0], a[1] * b[1], a[2] * b[2], a[3] * b[3]);
}

inline Vec4 operator / (const Vec4& a, const Vec4& b) {
    return Vec4(a[0] / b[0], a[1] / b[1], a[2] / b[2], a[3] / b[3]);
}

inline Vec4 operator * (const Vec4& v, float t) {
    return Vec4(v[0] * t, v[1] * t, v[2] * t, v[3] * t);
}

inline Vec4 operator * (float t, const Vec4& v) {
    return Vec4(v[0] * t, v[1] * t, v[2] * t, v[3] * t);
}

inline float dot(const Vec4& a, const Vec4& b) {
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2] + a[3] * b[3];
}

inline float length(const Vec4& v) {
    return sqrtf(dot(v, v));
}

inline Vec4 normalize(const Vec4& v) {
    return v / length(v);
}

inline std::ostream& operator << (std::ostream& os, const Vec2& v) {
    os << v[0] << " " << v[1];
    return os;
}

inline std::ostream& operator << (std::ostream& os, const Vec3& v) {
    os << v[0] << " " << v[1] << " " << v[2];
    return os;
}

inline std::ostream& operator << (std::ostream& os, const Vec4& v) {
    os << v[0] << " " << v[1] << " " << v[2] << " " << v[3];
    return os;
}

} // namespace imba

#endif // IMBA_VECTOR_HPP

