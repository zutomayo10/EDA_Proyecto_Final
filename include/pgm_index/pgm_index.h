#ifndef PGM_INDEX_PGM_INDEX_H
#define PGM_INDEX_PGM_INDEX_H

#include <algorithm>
#include <stdexcept>
#include <vector>
#include "common/key.h"
#include "pgm_index/model/approximate_position.h"
#include "pgm_index/model/segment.h"
#include "pgm_index/segmentation/level_segmenter.h"
using namespace std;

struct PGMIndex {
  vector<Key>* sorted_keys;
  int key_count, epsilon, epsilon_recursive;
  Key first_key = 0;
  vector<Segment> segments;
  vector<int> level_offsets;

  PGMIndex(vector<Key>& keys, int epsilon_, int epsilon_recursive_ = 4) {
    sorted_keys = &keys;
    key_count = keys.size();
    epsilon = epsilon_;
    epsilon_recursive = epsilon_recursive_;
    if (epsilon < 1 || epsilon_recursive < 1)
    {
      throw invalid_argument("eps y eps_rec deben ser >= 1");
    }
    if (key_count == 0)
    {
      return;
    }
    if (keys.back() == RESERVED_KEY)
    {
      throw invalid_argument("ULLONG_MAX esta reservado como centinela");
    }
    first_key = keys[0];
    build_levels(keys);
  }

  int lookup(Key x) {
    if (key_count == 0 || x == RESERVED_KEY)
    {
      return key_count;
    }
    ApproximatePosition window = approximate_position(x);
    return lower_bound(sorted_keys->begin() + window.lo, sorted_keys->begin() + window.hi, x) -
           sorted_keys->begin();
  }

  ApproximatePosition approximate_position(Key key) const {
    Key k = max(first_key, key);
    int pos = predict_position(locate_segment(k), k);
    int lo = pos - epsilon;
    int hi = pos + epsilon + 2;
    if (lo < 0)
    {
      lo = 0;
    }
    if (hi > key_count)
    {
      hi = key_count;
    }
    return {pos, lo, hi};
  }

  int locate_segment(Key key) const {
    int segment = level_offsets[level_offsets.size() - 2];
    for (int level = height() - 2; level >= 0; level--)
    {
      segment = descend_to_level(level, segment, key);
    }
    return segment;
  }

  int predict_position(int segment_index, Key key) const {
    double position = segments[segment_index].predict_position(key);
    if (position > segments[segment_index + 1].intercept)
    {
      position = segments[segment_index + 1].intercept;
    }
    if (position < 0)
    {
      position = 0;
    }
    return position;
  }

  long long size_in_bytes() const {
    long long segment_count = segments.size();
    long long offset_count = level_offsets.size();
    return segment_count * sizeof(Segment) + offset_count * sizeof(int);
  }

  int height() const {
    if (level_offsets.empty())
    {
      return 0;
    }
    return level_offsets.size() - 1;
  }

  int bottom_segment_count() const {
    if (segments.empty())
    {
      return 0;
    }
    return level_offsets[1] - 1;
  }

 private:
  void build_levels(vector<Key>& keys) {
    level_offsets.push_back(0);
    int level_size = segment_level(keys, epsilon);
    level_offsets.push_back(segments.size());

    while (level_size > 1) {
      vector<Key> level_keys = first_keys_of_last_level(level_size);
      level_size = segment_level(level_keys, epsilon_recursive);
      level_offsets.push_back(segments.size());
    }
  }

  int segment_level(const vector<Key>& keys, int level_epsilon) {
    LevelSegmenter segmenter(level_epsilon, segments);
    return segmenter.segment_level(keys, sorted_keys->back());
  }

  vector<Key> first_keys_of_last_level(int level_size) const {
    int offset = level_offsets[level_offsets.size() - 2];
    vector<Key> level_keys(level_size);
    for (int i = 0; i < level_size; i++)
    {
      level_keys[i] = segments[offset + i].key;
    }
    return level_keys;
  }

  int descend_to_level(int level, int parent_segment, Key key) const {
    int candidate = predict_position(parent_segment, key) - epsilon_recursive - 1;
    if (candidate < 0)
    {
      candidate = 0;
    }
    candidate += level_offsets[level];
    while (segments[candidate + 1].key <= key)
    {
      candidate++;
    }
    return candidate;
  }
};

#endif
