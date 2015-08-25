#ifndef IMBA_CONSTANTS_H

namespace imba {

const float pi = 3.1415926f;

inline float to_radians(float deg) { return deg * pi / 180.0f; }
inline float to_degree(float rad) { return rad * 180.0f / pi; }

}

#endif
