#ifndef IMBA_MATH_HPP
#define IMBA_MATH_HPP

namespace imba {

const float pi = 3.14159265359f;

inline float to_radians(float x) {
    return x * pi / 180.0f;
}

} // namespace imba

#endif // IMBA_MATH_HPP

