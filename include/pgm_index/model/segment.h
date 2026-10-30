#ifndef PGM_INDEX_MODEL_SEGMENT_H
#define PGM_INDEX_MODEL_SEGMENT_H

#include "common/key.h"

struct Segment {
  Key key;
  float slope;
  int intercept;

  double predict_position(Key k) const {
    Key distance = k - key;
    return slope * distance + intercept;
  }
};

#endif
