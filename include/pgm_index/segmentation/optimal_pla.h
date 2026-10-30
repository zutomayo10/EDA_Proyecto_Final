#ifndef PGM_INDEX_SEGMENTATION_OPTIMAL_PLA_H
#define PGM_INDEX_SEGMENTATION_OPTIMAL_PLA_H

#include <vector>
#include "common/key.h"
#include "pgm_index/geometry/point.h"
#include "pgm_index/model/segment.h"
using namespace std;

struct OptimalPLA {
  int epsilon;
  vector<Point> lower_hull, upper_hull;
  int lower_start = 0, upper_start = 0;
  Key first_key = 0;
  Point rectangle[4];

  OptimalPLA(int epsilon_) : epsilon(epsilon_) {}

  bool add_point(Key x, int y) {
    Point upper_point{x, static_cast<__int128>(y) + epsilon};
    __int128 lower_y = 0;
    if (y > epsilon)
    {
      lower_y = static_cast<__int128>(y) - epsilon;
    }
    Point lower_point{x, lower_y};

    if (lower_hull.empty()) {
      start_segment(x, upper_point, lower_point);
      return true;
    }
    if (lower_hull.size() == 1) {
      add_second_point(upper_point, lower_point);
      return true;
    }

    Slope max_slope = rectangle[2] - rectangle[0];
    Slope min_slope = rectangle[3] - rectangle[1];
    if (is_outside_feasible_region(upper_point, lower_point, max_slope, min_slope)) {
      lower_hull.clear();
      upper_hull.clear();
      return false;
    }
    if (upper_point - rectangle[1] < min_slope)
    {
      narrow_max_slope(upper_point);
    }
    if (lower_point - rectangle[0] > max_slope)
    {
      narrow_min_slope(lower_point);
    }
    return true;
  }

  Segment current_segment() const {
    Point r[4] = {rectangle[0], rectangle[1], rectangle[2], rectangle[3]};
    if (lower_hull.size() == 1) {
      r[2] = r[0];
      r[3] = r[1];
    }
    bool same_first_pair = r[0].x == r[2].x && r[0].y == r[2].y;
    bool same_second_pair = r[1].x == r[3].x && r[1].y == r[3].y;
    bool one_point = same_first_pair && same_second_pair;
    Segment segment;
    segment.key = first_key;
    if (one_point) {
      segment.slope = 0;
      segment.intercept = (r[0].y + r[1].y) / 2;
    } else {
      Slope m = r[3] - r[1];
      __int128 numerator = m.dy * (first_key - r[1].x);
      __int128 rounding = m.dx / 2;
      if (numerator < 0)
      {
        rounding = -rounding;
      }
      segment.intercept = (numerator + rounding) / m.dx + r[1].y;
      long double dy = m.dy;
      segment.slope = dy / m.dx;
    }
    return segment;
  }

 private:

  void start_segment(Key x, Point upper_point, Point lower_point) {
    first_key = x;
    rectangle[0] = upper_point;
    rectangle[1] = lower_point;
    upper_hull.clear();
    lower_hull.clear();
    upper_hull.push_back(upper_point);
    lower_hull.push_back(lower_point);
    upper_start = 0;
    lower_start = 0;
  }

  void add_second_point(Point upper_point, Point lower_point) {
    rectangle[2] = lower_point;
    rectangle[3] = upper_point;
    upper_hull.push_back(upper_point);
    lower_hull.push_back(lower_point);
  }

  bool is_outside_feasible_region(Point upper_point, Point lower_point, Slope max_slope,
                                  Slope min_slope) const {
    bool outside_max_line = upper_point - rectangle[2] < max_slope;
    bool outside_min_line = lower_point - rectangle[3] > min_slope;
    return outside_max_line || outside_min_line;
  }

  void narrow_max_slope(Point upper_point) {
    Slope best = lower_hull[lower_start] - upper_point;
    int best_index = lower_start;
    int size = lower_hull.size();
    for (int i = lower_start + 1; i < size; i++) {
      Slope candidate = lower_hull[i] - upper_point;
      if (candidate > best)
      {
        break;
      }
      best = candidate;
      best_index = i;
    }
    rectangle[1] = lower_hull[best_index];
    rectangle[3] = upper_point;
    lower_start = best_index;

    int end = upper_hull.size();
    while (end >= upper_start + 2 && cross_product(upper_hull[end - 2], upper_hull[end - 1], upper_point) <= 0)
    {
      end--;
    }
    upper_hull.resize(end);
    upper_hull.push_back(upper_point);
  }

  void narrow_min_slope(Point lower_point) {
    Slope best = upper_hull[upper_start] - lower_point;
    int best_index = upper_start;
    int size = upper_hull.size();
    for (int i = upper_start + 1; i < size; i++) {
      Slope candidate = upper_hull[i] - lower_point;
      if (candidate < best)
      {
        break;
      }
      best = candidate;
      best_index = i;
    }
    rectangle[0] = upper_hull[best_index];
    rectangle[2] = lower_point;
    upper_start = best_index;

    int end = lower_hull.size();
    while (end >= lower_start + 2 && cross_product(lower_hull[end - 2], lower_hull[end - 1], lower_point) >= 0)
    {
      end--;
    }
    lower_hull.resize(end);
    lower_hull.push_back(lower_point);
  }
};

#endif
