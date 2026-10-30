#ifndef PGM_INDEX_DYNAMIC_LOGARITHMIC_LOGARITHMIC_LEVEL_H
#define PGM_INDEX_DYNAMIC_LOGARITHMIC_LOGARITHMIC_LEVEL_H

#include <algorithm>
#include <memory>
#include <vector>
#include "common/key.h"
#include "pgm_index/pgm_index.h"
using namespace std;

struct LogarithmicLevel {
  vector<Key> inserts, deletes;
  unique_ptr<PGMIndex> insert_index, delete_index;

  int size() const { return inserts.size() + deletes.size(); }

  void train(int epsilon, int min_keys_for_model) {
    insert_index.reset();
    delete_index.reset();
    if ((int)inserts.size() >= min_keys_for_model)
    {
      insert_index.reset(new PGMIndex(inserts, epsilon));
    }
    if ((int)deletes.size() >= min_keys_for_model)
    {
      delete_index.reset(new PGMIndex(deletes, epsilon));
    }
  }

  void clear() {
    vector<Key>().swap(inserts);
    vector<Key>().swap(deletes);
    insert_index.reset();
    delete_index.reset();
  }

  int rank(Key x) { return rank_in(inserts, insert_index.get(), x) - rank_in(deletes, delete_index.get(), x); }

  long long index_bytes() const {
    long long bytes = 0;
    if (insert_index)
    {
      bytes += insert_index->size_in_bytes();
    }
    if (delete_index)
    {
      bytes += delete_index->size_in_bytes();
    }
    return bytes;
  }

  long long ram_bytes() const {
    long long bytes = (inserts.capacity() + deletes.capacity()) * sizeof(Key);
    if (insert_index)
    {
      bytes += insert_index->segments.capacity() * sizeof(Segment);
    }
    if (delete_index)
    {
      bytes += delete_index->segments.capacity() * sizeof(Segment);
    }
    return bytes;
  }

  int segment_count() const {
    int count = 0;
    if (insert_index)
    {
      count += insert_index->bottom_segment_count();
    }
    if (delete_index)
    {
      count += delete_index->bottom_segment_count();
    }
    return count;
  }

  bool is_valid() const { return is_sorted(inserts.begin(), inserts.end()) && is_sorted(deletes.begin(), deletes.end()); }

 private:
  static int rank_in(vector<Key>& sorted, PGMIndex* index, Key x) {
    if (index)
    {
      return index->lookup(x);
    }
    return lower_bound(sorted.begin(), sorted.end(), x) - sorted.begin();
  }
};

#endif
