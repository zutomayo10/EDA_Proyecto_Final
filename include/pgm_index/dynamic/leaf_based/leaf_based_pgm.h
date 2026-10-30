#ifndef PGM_INDEX_DYNAMIC_LEAF_BASED_LEAF_BASED_PGM_H
#define PGM_INDEX_DYNAMIC_LEAF_BASED_LEAF_BASED_PGM_H

#include <memory>
#include <stdexcept>
#include <vector>
#include "common/key.h"
#include "pgm_index/dynamic/leaf_based/leaf.h"
#include "pgm_index/dynamic/leaf_based/leaf_directory.h"
using namespace std;

struct LeafBasedPGM {
  int epsilon, leaf_capacity, retrain_threshold;
  vector<unique_ptr<Leaf>> leaves;
  LeafDirectory directory;
  int key_count = 0;
  long long retrain_count = 0, split_count = 0, merge_count = 0;

  LeafBasedPGM(vector<Key>& keys, int epsilon_ = 64, int leaf_capacity_ = 4096, int retrain_threshold_ = -1) : epsilon(epsilon_), leaf_capacity(leaf_capacity_), retrain_threshold(retrain_threshold_) {
    if (retrain_threshold < 0){
      retrain_threshold = max(epsilon_, leaf_capacity_ / 8);
    }
    if (epsilon < 1 || leaf_capacity < 8)
    {
      throw invalid_argument("eps >= 1 y C >= 8");
    }
    key_count = keys.size();
    build_leaves(keys);
    for (size_t i = 0; i < leaves.size(); i++){
      retrain(*leaves[i]);
    }
    retrain_count = 0;
    directory.rebuild(leaves);
  }

  int size() const { return key_count; }

  int lookup(Key x) {
    int leaf = directory.last_leaf_with_min_below(x);
    if (leaf < 0)
    {
      return 0;
    }
    return directory.keys_before(leaf) + leaves[leaf]->local_rank(x);
  }

  void insert(Key x) {
    if (x == RESERVED_KEY){
      throw invalid_argument("ULLONG_MAX esta reservado como centinela");
    }

    key_count++;

    if (leaves.empty()) {
      create_first_leaf(x);
      return;
    }

    int index = directory.leaf_for_insertion(x);
    Leaf& leaf = *leaves[index];
    int position = leaf.local_rank(x);
    leaf.insert_at(position, x);

    if (position == 0){
      directory.set_min_key(index, x);
    }
    directory.add_to_leaf_size(index, 1);
    maintain_leaf_after_insert(index);
  }

  bool erase(Key x) {
    int index;
    int position;
    if (!find_occurrence(x, index, position))
    {
      return false;
    }
    
    key_count--;
    Leaf& leaf = *leaves[index];
    leaf.erase_at(position);
    directory.add_to_leaf_size(index, -1);
    
    if (leaf.keys.empty()) {
      remove_leaf(index);
      return true;
    }
    if (position == 0)
    {
      directory.set_min_key(index, leaf.keys[0]);
    }
    maintain_leaf_after_erase(index);
    return true;
  }

  long long size_in_bytes() const {
    long long bytes = directory.index_bytes();
    for (size_t i = 0; i < leaves.size(); i++)
    {
      bytes += leaves[i]->index_bytes();
    }
    return bytes;
  }

  long long ram_bytes() const {
    long long bytes = directory.ram_bytes();
    for (size_t i = 0; i < leaves.size(); i++)
    {
      bytes += leaves[i]->ram_bytes();
    }
    return bytes;
  }

  int segments_count() const {
    int count = 0;
    for (size_t i = 0; i < leaves.size(); i++)
    {
      count += leaves[i]->segment_count();
    }
    return count;
  }

  int height() const { return 2; }

  bool check() const {
    int total = 0;
    int leaf_total = leaves.size();
    if (directory.leaf_count() != leaf_total)
    {
      return false;
    }
    for (int j = 0; j < leaf_total; j++) {
      const Leaf& leaf = *leaves[j];
      if (!leaf_is_valid(leaf, leaf_total) || directory.min_keys[j] != leaf.keys[0] ||
          directory.keys_before(j) != total)
      {
        return false;
      }
      if (j > 0 && leaves[j - 1]->keys.back() > leaf.keys[0])
      {
        return false;
      }
      total += leaf.size();
    }
    return total == key_count;
  }

 private:

  void build_leaves(vector<Key>& keys) {
    int fill = leaf_capacity * 3 / 4;
    for (size_t start = 0; start < keys.size(); start += fill) {
      size_t end = min(keys.size(), start + fill);
      if (end - start < (size_t)leaf_capacity / 4 && !leaves.empty()) {
        leaves.back()->keys.insert(leaves.back()->keys.end(), keys.begin() + start, keys.begin() + end);
        break;
      }
      Leaf* leaf = new Leaf;
      leaf->keys.assign(keys.begin() + start, keys.begin() + end);
      leaves.emplace_back(leaf);
    }
  }

  void retrain(Leaf& leaf) {
    leaf.retrain(epsilon);
    retrain_count++;
  }

  void create_first_leaf(Key x) {
    Leaf* leaf = new Leaf;
    leaf->keys.push_back(x);
    leaves.emplace_back(leaf);
    retrain(*leaf);
    directory.rebuild(leaves);
  }

  bool find_occurrence(Key x, int& leaf_index, int& position) const {
    int candidate = directory.last_leaf_with_min_below(x);
    if (candidate >= 0) {
      position = leaves[candidate]->local_rank(x);
      if (position < leaves[candidate]->size() && leaves[candidate]->keys[position] == x) {
        leaf_index = candidate;
        return true;
      }
    }
    if (directory.leaf_starts_with(candidate + 1, x)) {
      leaf_index = candidate + 1;
      position = 0;
      return true;
    }
    return false;
  }

  void maintain_leaf_after_insert(int index) {
    Leaf& leaf = *leaves[index];
    if (leaf.size() > leaf_capacity)
    {
      split(index);
    }
    else if (leaf.updates_since_training > retrain_threshold)
    {
      retrain(leaf);
    }
  }

  void maintain_leaf_after_erase(int index) {
    Leaf& leaf = *leaves[index];
    if (leaf.size() < leaf_capacity / 4 && leaves.size() > 1)
    {
      merge_with_neighbor(index);
    }
    else if (leaf.updates_since_training > retrain_threshold)
    {
      retrain(leaf);
    }
  }

  void remove_leaf(int index) {
    leaves.erase(leaves.begin() + index);
    directory.rebuild(leaves);
  }

  void split(int index) {
    split_count++;
    Leaf* right = new Leaf;
    Leaf& left = *leaves[index];
    int half = left.size() / 2;
    right->keys.assign(left.keys.begin() + half, left.keys.end());
    left.keys.resize(half);
    retrain(left);
    retrain(*right);
    leaves.insert(leaves.begin() + index + 1, unique_ptr<Leaf>(right));
    directory.rebuild(leaves);
  }

  void merge_with_neighbor(int index) {
    int first = index;
    if (index + 1 >= (int)leaves.size())
    {
      first = index - 1;
    }
    int second = first + 1;
    vector<Key>& first_keys = leaves[first]->keys;
    vector<Key>& second_keys = leaves[second]->keys;
    first_keys.insert(first_keys.end(), second_keys.begin(), second_keys.end());
    if ((int)first_keys.size() > leaf_capacity) {
      redistribute(first, second);
    } else {
      merge_count++;
      leaves.erase(leaves.begin() + second);
    }
    retrain(*leaves[first]);
    directory.rebuild(leaves);
  }

  void redistribute(int first, int second) {
    vector<Key>& first_keys = leaves[first]->keys;
    int half = first_keys.size() / 2;
    leaves[second]->keys.assign(first_keys.begin() + half, first_keys.end());
    first_keys.resize(half);
    retrain(*leaves[second]);
  }

  bool leaf_is_valid(const Leaf& leaf, int leaf_total) const {
    int size = leaf.size();
    if (size <= 0 || size > leaf_capacity)
    {
      return false;
    }
    if (leaf_total > 1 && size < leaf_capacity / 4)
    {
      return false;
    }
    if (!is_sorted(leaf.keys.begin(), leaf.keys.end()))
    {
      return false;
    }
    return leaf.updates_since_training <= retrain_threshold;
  }
};

#endif
