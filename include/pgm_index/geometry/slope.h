#ifndef PGM_INDEX_GEOMETRY_SLOPE_H
#define PGM_INDEX_GEOMETRY_SLOPE_H

struct Slope {
  __int128 dx, dy;
  bool operator<(Slope other) const {return dy * other.dx < dx * other.dy; }
  bool operator>(Slope other) const {return dy * other.dx > dx * other.dy; }
};

#endif
