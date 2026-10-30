#ifndef PGM_INDEX_DYNAMIC_LEAF_BASED_LEAF_H
#define PGM_INDEX_DYNAMIC_LEAF_BASED_LEAF_H

#include <algorithm>
#include <memory>
#include <vector>
#include "common/key.h"
#include "pgm_index/pgm_index.h"
using namespace std;

struct Leaf {
  static constexpr int MIN_KEYS_FOR_MODEL = 16;

  vector<Key> keys;
  unique_ptr<PGMIndex> model;
  int updates_since_training = 0;

  int size() const { return keys.size(); }

  void retrain(int epsilon) {
    updates_since_training = 0;
    model.reset();
    if (size() >= MIN_KEYS_FOR_MODEL)
    {
      model.reset(new PGMIndex(keys, epsilon));
    }
  }

  int local_rank(Key x) const {
    if (!model || x == RESERVED_KEY)
    {
      return lower_bound(keys.begin(), keys.end(), x) - keys.begin();
    }
    ApproximatePosition window = model->approximate_position(x);
    int lo = max(0, window.lo - updates_since_training);
    int hi = min(size(), window.hi + updates_since_training);
    return lower_bound(keys.begin() + lo, keys.begin() + hi, x) - keys.begin();
  }

  void insert_at(int position, Key x) {
    keys.insert(keys.begin() + position, x);
    updates_since_training++;
  }

  void erase_at(int position) {
    keys.erase(keys.begin() + position);
    updates_since_training++;
  }

  long long index_bytes() const {
    if (model)
    {
      return model->size_in_bytes();
    }
    return 0;
  }

  long long ram_bytes() const {
    long long bytes = sizeof(Leaf) + keys.capacity() * sizeof(Key);
    if (model)
    {
      bytes += model->segments.capacity() * sizeof(Segment);
    }
    return bytes;
  }

  int segment_count() const {
    if (model)
    {
      return model->bottom_segment_count();
    }
    return 0;
  }
};

#endif
