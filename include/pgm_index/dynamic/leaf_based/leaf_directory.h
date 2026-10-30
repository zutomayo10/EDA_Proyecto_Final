#ifndef PGM_INDEX_DYNAMIC_LEAF_BASED_LEAF_DIRECTORY_H
#define PGM_INDEX_DYNAMIC_LEAF_BASED_LEAF_DIRECTORY_H

#include <algorithm>
#include <memory>
#include <vector>
#include "common/fenwick_tree.h"
#include "common/key.h"
#include "pgm_index/dynamic/leaf_based/leaf.h"
using namespace std;

struct LeafDirectory {
  vector<Key> min_keys;
  FenwickTree sizes;

  int leaf_count() const { return min_keys.size(); }

  void rebuild(const vector<unique_ptr<Leaf>>& leaves) {
    min_keys.clear();
    vector<int> leaf_sizes;
    for (size_t i = 0; i < leaves.size(); i++) {
      min_keys.push_back(leaves[i]->keys[0]);
      leaf_sizes.push_back(leaves[i]->size());
    }
    sizes.build(leaf_sizes);
  }

  int last_leaf_with_min_below(Key x) const {
    return lower_bound(min_keys.begin(), min_keys.end(), x) - min_keys.begin() - 1;
  }

  int leaf_for_insertion(Key x) const {
    int leaf = upper_bound(min_keys.begin(), min_keys.end(), x) - min_keys.begin() - 1;
    if (leaf < 0)
    {
      return 0;
    }
    return leaf;
  }

  bool leaf_starts_with(int leaf, Key x) const { return leaf < leaf_count() && min_keys[leaf] == x; }

  int keys_before(int leaf) const { return sizes.prefix_sum(leaf); }

  void add_to_leaf_size(int leaf, int delta) { sizes.add(leaf, delta); }

  void set_min_key(int leaf, Key key) { min_keys[leaf] = key; }

  long long index_bytes() const { return leaf_count() * (sizeof(Key) + sizeof(int)); }

  long long ram_bytes() const { return min_keys.capacity() * sizeof(Key) + sizes.ram_bytes(); }
};

#endif
