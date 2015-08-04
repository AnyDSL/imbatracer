#ifndef IMBA_COMMON_H
#define IMBA_COMMON_H

namespace imba {

static const float pi = 3.14159265359f;

inline float radians(float x) {
    return x * pi / 180.0f;
}

inline float degrees(float x) {
    return x * 180.0f / pi;
}

} // namespace imba

#endif // IMBA_COMMON_H
