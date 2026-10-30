#ifndef COMMON_SORTED_RUNS_H
#define COMMON_SORTED_RUNS_H

#include <algorithm>
#include <vector>
#include "common/key.h"
using namespace std;

inline vector<Key> merge_sorted_runs(const vector<Key>& a, const vector<Key>& b) {
  vector<Key> merged(a.size() + b.size());
  merge(a.begin(), a.end(), b.begin(), b.end(), merged.begin());
  return merged;
}

inline void cancel_matching_pairs(vector<Key>& inserts, vector<Key>& deletes) {
  vector<Key> kept_inserts;
  vector<Key> kept_deletes;
  size_t insert_position = 0;
  size_t delete_position = 0;
  while (insert_position < inserts.size() && delete_position < deletes.size()) {
    if (inserts[insert_position] == deletes[delete_position]) {
      insert_position++;
      delete_position++;
    } else if (inserts[insert_position] < deletes[delete_position]) {
      kept_inserts.push_back(inserts[insert_position]);
      insert_position++;
    } else {
      kept_deletes.push_back(deletes[delete_position]);
      delete_position++;
    }
  }
  kept_inserts.insert(kept_inserts.end(), inserts.begin() + insert_position, inserts.end());
  kept_deletes.insert(kept_deletes.end(), deletes.begin() + delete_position, deletes.end());
  inserts.swap(kept_inserts);
  deletes.swap(kept_deletes);
}

#endif
