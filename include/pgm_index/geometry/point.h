#ifndef PGM_INDEX_GEOMETRY_POINT_H
#define PGM_INDEX_GEOMETRY_POINT_H

#include "pgm_index/geometry/slope.h"

struct Point {
  __int128 x, y;
  Slope operator-(Point other) const { return {x - other.x, y - other.y}; }
};

inline __int128 cross_product(Point origin, Point a, Point b) {
  Slope oa = a - origin, ob = b - origin;
  return oa.dx * ob.dy - oa.dy * ob.dx;
}

#endif
