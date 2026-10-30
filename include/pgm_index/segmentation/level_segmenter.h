#ifndef PGM_INDEX_SEGMENTATION_LEVEL_SEGMENTER_H
#define PGM_INDEX_SEGMENTATION_LEVEL_SEGMENTER_H

#include <vector>
#include "common/key.h"
#include "pgm_index/model/segment.h"
#include "pgm_index/segmentation/optimal_pla.h"
using namespace std;

struct LevelSegmenter {
  int epsilon;
  vector<Segment>& output;
  OptimalPLA pla;
  int segment_count = 0;

  LevelSegmenter(int epsilon_, vector<Segment>& output_) : epsilon(epsilon_), output(output_), pla(epsilon_) {}

  int segment_level(const vector<Key>& keys, Key last_data_key) {
    feed_level_points(keys);
    close_current_segment();
    append_sentinels(keys.size(), last_data_key);
    return segment_count;
  }

 private:

  void add_point(Key x, int y) {
    if (!pla.add_point(x, y)) {
      close_current_segment();
      pla.add_point(x, y);
    }
  }

  void close_current_segment() {
    output.push_back(pla.current_segment());
    segment_count++;
  }

  void feed_level_points(const vector<Key>& keys) {
    int m = keys.size();
    add_point(keys[0], 0);
    for (int i = 1; i + 1 < m; i++)
    {
      feed_middle_key(keys, i);
    }
    if (m >= 2 && keys[m - 1] != keys[m - 2])
    {
      add_point(keys[m - 1], m - 1);
    }
    add_point(keys[m - 1] + 1, m);
  }

  void feed_middle_key(const vector<Key>& keys, int i) {
    if (keys[i] != keys[i - 1])
    {
      add_point(keys[i], i);
    }
    else if (keys[i] + 1 < keys[i + 1])
    {
      add_point(keys[i] + 1, i);
    }
  }

  void append_sentinels(int level_size, Key last_data_key) {
    Segment last = output.back();
    if (last.key == RESERVED_KEY) {
      segment_count--;
      return;
    }
    if (last.predict_position(RESERVED_KEY - 1) < level_size)
    {
      output.push_back({last_data_key + 1, 0, level_size});
    }
    output.push_back({RESERVED_KEY, 0, level_size});
  }
};

#endif
