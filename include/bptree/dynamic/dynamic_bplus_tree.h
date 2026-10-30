#ifndef BPTREE_DYNAMIC_DYNAMIC_BPLUS_TREE_H
#define BPTREE_DYNAMIC_DYNAMIC_BPLUS_TREE_H

#include <algorithm>
#include <stdexcept>
#include <vector>
#include "bptree/dynamic/dynamic_bplus_node.h"
#include "common/key.h"
using namespace std;

struct DynamicBPlusTree {
  int max_size, min_size;
  DynamicBPlusNode* root;
  int height = 1;
  int key_count = 0;

  DynamicBPlusTree(int max_size_ = 64) : max_size(max_size_), min_size(max(2, max_size_ / 4)) {
    root = new DynamicBPlusNode;
  }

  DynamicBPlusTree(vector<Key>& keys, int max_size_) : DynamicBPlusTree(max_size_) {
    key_count = keys.size();
    if (keys.empty())
    {
      return;
    }
    delete root;
    int fill = max(2, max_size * 3 / 4);
    vector<DynamicBPlusNode*> level = build_leaves(keys, fill);
    while (level.size() > 1) {
      level = build_parent_level(level, fill);
      height++;
    }
    root = level[0];
  }

  ~DynamicBPlusTree() { destroy(root); }
  DynamicBPlusTree(const DynamicBPlusTree&) = delete;
  DynamicBPlusTree& operator=(const DynamicBPlusTree&) = delete;

  int size() const { return key_count; }

  int lookup(Key x) const {
    const DynamicBPlusNode* node = root;
    int rank = 0;
    while (!node->leaf) {
      int child = lower_bound(node->keys.begin(), node->keys.end(), x) - node->keys.begin();
      for (int j = 0; j < child; j++)
      {
        rank += node->kids[j]->key_total();
      }
      node = node->kids[child];
    }
    return rank + (lower_bound(node->keys.begin(), node->keys.end(), x) - node->keys.begin());
  }

  void insert(Key x) {
    if (x == RESERVED_KEY)
    {
      throw invalid_argument("ULLONG_MAX esta reservado como centinela");
    }
    Key separator;
    DynamicBPlusNode* right = insert_into(root, x, separator);
    key_count++;
    if (right)
    {
      grow_new_root(right, separator);
    }
  }

  bool erase(Key x) {
    if (!erase_from(root, x))
    {
      return false;
    }
    key_count--;
    shrink_root();
    return true;
  }

  long long size_in_bytes() const { return measure(root, false); }

  long long ram_bytes() const { return measure(root, true); }

  bool check() const {
    int leaf_depth = -1;
    int counted_keys = 0;
    Key previous_key = 0;
    return verify(root, true, 0, leaf_depth, counted_keys, previous_key) && counted_keys == key_count;
  }

 private:

  static size_t group_count(size_t total, size_t fill) { return (total + fill - 1) / fill; }

  vector<DynamicBPlusNode*> build_leaves(vector<Key>& keys, int fill) {
    vector<DynamicBPlusNode*> leaves;
    size_t groups = group_count(keys.size(), fill);
    size_t start = 0;
    for (size_t i = 0; i < groups; i++) {
      size_t size = keys.size() / groups;
      if (i < keys.size() % groups)
      {
        size++;
      }
      DynamicBPlusNode* leaf = new DynamicBPlusNode;
      leaf->keys.assign(keys.begin() + start, keys.begin() + start + size);
      start += size;
      leaves.push_back(leaf);
    }
    return leaves;
  }

  vector<DynamicBPlusNode*> build_parent_level(vector<DynamicBPlusNode*>& level, int fill) {
    vector<DynamicBPlusNode*> parents;
    size_t groups = group_count(level.size(), fill);
    size_t start = 0;
    for (size_t i = 0; i < groups; i++) {
      size_t size = level.size() / groups;
      if (i < level.size() % groups)
      {
        size++;
      }
      DynamicBPlusNode* parent = new DynamicBPlusNode;
      parent->leaf = false;
      for (size_t j = start; j < start + size; j++) {
        if (j > start)
        {
          parent->keys.push_back(level[j - 1]->max_key());
        }
        parent->kids.push_back(level[j]);
        parent->total += level[j]->key_total();
      }
      start += size;
      parents.push_back(parent);
    }
    return parents;
  }

  DynamicBPlusNode* insert_into(DynamicBPlusNode* node, Key x, Key& separator) {
    if (node->leaf) {
      node->keys.insert(lower_bound(node->keys.begin(), node->keys.end(), x), x);
      if ((int)node->keys.size() > max_size)
      {
        return split_leaf(node, separator);
      }
      return nullptr;
    }
    int child = lower_bound(node->keys.begin(), node->keys.end(), x) - node->keys.begin();
    Key child_separator;
    DynamicBPlusNode* right = insert_into(node->kids[child], x, child_separator);
    node->total++;
    if (!right)
    {
      return nullptr;
    }
    node->keys.insert(node->keys.begin() + child, child_separator);
    node->kids.insert(node->kids.begin() + child + 1, right);
    if ((int)node->kids.size() > max_size)
    {
      return split_internal(node, separator);
    }
    return nullptr;
  }

  DynamicBPlusNode* split_leaf(DynamicBPlusNode* leaf, Key& separator) {
    DynamicBPlusNode* right = new DynamicBPlusNode;
    int half = leaf->keys.size() / 2;
    right->keys.assign(leaf->keys.begin() + half, leaf->keys.end());
    leaf->keys.resize(half);
    separator = leaf->keys.back();
    return right;
  }

  DynamicBPlusNode* split_internal(DynamicBPlusNode* node, Key& separator) {
    int m = node->kids.size() / 2;
    DynamicBPlusNode* right = new DynamicBPlusNode;
    right->leaf = false;
    right->kids.assign(node->kids.begin() + m, node->kids.end());
    right->keys.assign(node->keys.begin() + m, node->keys.end());
    separator = node->keys[m - 1];
    node->kids.resize(m);
    node->keys.resize(m - 1);
    node->total = sum_of_totals(node);
    right->total = sum_of_totals(right);
    return right;
  }

  static int sum_of_totals(const DynamicBPlusNode* node) {
    int sum = 0;
    for (const DynamicBPlusNode* kid : node->kids)
    {
      sum += kid->key_total();
    }
    return sum;
  }

  void grow_new_root(DynamicBPlusNode* right, Key separator) {
    DynamicBPlusNode* new_root = new DynamicBPlusNode;
    new_root->leaf = false;
    new_root->kids = {root, right};
    new_root->keys = {separator};
    new_root->total = root->key_total() + right->key_total();
    root = new_root;
    height++;
  }

  bool erase_from(DynamicBPlusNode* node, Key x) {
    if (node->leaf) {
      vector<Key>::iterator position = lower_bound(node->keys.begin(), node->keys.end(), x);
      if (position == node->keys.end() || *position != x)
      {
        return false;
      }
      node->keys.erase(position);
      return true;
    }
    int child = lower_bound(node->keys.begin(), node->keys.end(), x) - node->keys.begin();
    while (true) {
      if (erase_from(node->kids[child], x)) {
        node->total--;
        fix_underflow(node, child);
        return true;
      }

      if (child < (int)node->keys.size() && node->keys[child] == x)
      {
        child++;
      }
      else
      {
        return false;
      }
    }
  }

  void fix_underflow(DynamicBPlusNode* parent, int i) {
    if (parent->kids[i]->entry_count() >= min_size)
    {
      return;
    }
    DynamicBPlusNode* left = nullptr;
    DynamicBPlusNode* right = nullptr;
    if (i > 0)
    {
      left = parent->kids[i - 1];
    }
    if (i + 1 < (int)parent->kids.size())
    {
      right = parent->kids[i + 1];
    }
    if (left && left->entry_count() > min_size)
    {
      borrow_from_left(parent, i);
    }
    else if (right && right->entry_count() > min_size)
    {
      borrow_from_right(parent, i);
    }
    else if (left)
    {
      merge_with_left(parent, i);
    }
    else if (right)
    {
      merge_with_right(parent, i);
    }
  }

  void borrow_from_left(DynamicBPlusNode* parent, int i) {
    DynamicBPlusNode* child = parent->kids[i];
    DynamicBPlusNode* left = parent->kids[i - 1];
    if (child->leaf) {
      child->keys.insert(child->keys.begin(), left->keys.back());
      left->keys.pop_back();
      parent->keys[i - 1] = left->keys.back();
      return;
    }
    DynamicBPlusNode* moved = left->kids.back();
    child->kids.insert(child->kids.begin(), moved);
    child->keys.insert(child->keys.begin(), parent->keys[i - 1]);
    parent->keys[i - 1] = left->keys.back();
    left->kids.pop_back();
    left->keys.pop_back();
    left->total -= moved->key_total();
    child->total += moved->key_total();
  }

  void borrow_from_right(DynamicBPlusNode* parent, int i) {
    DynamicBPlusNode* child = parent->kids[i];
    DynamicBPlusNode* right = parent->kids[i + 1];
    if (child->leaf) {
      child->keys.push_back(right->keys.front());
      right->keys.erase(right->keys.begin());
      parent->keys[i] = child->keys.back();
      return;
    }
    DynamicBPlusNode* moved = right->kids.front();
    child->kids.push_back(moved);
    child->keys.push_back(parent->keys[i]);
    parent->keys[i] = right->keys.front();
    right->kids.erase(right->kids.begin());
    right->keys.erase(right->keys.begin());
    right->total -= moved->key_total();
    child->total += moved->key_total();
  }

  void merge_with_left(DynamicBPlusNode* parent, int i) {
    absorb(parent->kids[i - 1], parent->keys[i - 1], parent->kids[i]);
    parent->keys.erase(parent->keys.begin() + i - 1);
    parent->kids.erase(parent->kids.begin() + i);
  }

  void merge_with_right(DynamicBPlusNode* parent, int i) {
    absorb(parent->kids[i], parent->keys[i], parent->kids[i + 1]);
    parent->keys.erase(parent->keys.begin() + i);
    parent->kids.erase(parent->kids.begin() + i + 1);
  }

  void absorb(DynamicBPlusNode* left, Key separator, DynamicBPlusNode* right) {
    if (left->leaf) {
      left->keys.insert(left->keys.end(), right->keys.begin(), right->keys.end());
    } else {
      left->keys.push_back(separator);
      left->keys.insert(left->keys.end(), right->keys.begin(), right->keys.end());
      left->kids.insert(left->kids.end(), right->kids.begin(), right->kids.end());
      left->total += right->total;
      right->kids.clear();
    }
    delete right;
  }

  void shrink_root() {
    while (!root->leaf && root->kids.size() == 1) {
      DynamicBPlusNode* old_root = root;
      root = old_root->kids[0];
      old_root->kids.clear();
      delete old_root;
      height--;
    }
  }

  void destroy(DynamicBPlusNode* node) {
    if (!node)
    {
      return;
    }
    for (DynamicBPlusNode* kid : node->kids)
    {
      destroy(kid);
    }
    delete node;
  }

  long long measure(const DynamicBPlusNode* node, bool include_leaves) const {
    long long bytes = 0;
    if (node->leaf) {
      if (include_leaves)
      {
        bytes += sizeof(DynamicBPlusNode) + node->keys.capacity() * 8;
      }
      return bytes;
    }
    if (include_leaves)
    {
      bytes += sizeof(DynamicBPlusNode) + node->keys.capacity() * 8 + node->kids.capacity() * 8;
    }
    else
    {
      bytes += node->keys.size() * 8 + node->kids.size() * 8;
    }
    for (const DynamicBPlusNode* kid : node->kids)
    {
      bytes += measure(kid, include_leaves);
    }
    return bytes;
  }

  bool verify(const DynamicBPlusNode* node, bool is_root, int depth, int& leaf_depth, int& counted_keys,
              Key& previous_key) const {
    int entries = node->entry_count();
    if (entries > max_size || (!is_root && entries < min_size))
    {
      return false;
    }
    if (node->leaf)
    {
      return verify_leaf(node, depth, leaf_depth, counted_keys, previous_key);
    }
    if (node->keys.size() + 1 != node->kids.size())
    {
      return false;
    }
    int keys_before = counted_keys;
    for (size_t i = 0; i < node->kids.size(); i++) {
      if (!verify(node->kids[i], false, depth + 1, leaf_depth, counted_keys, previous_key))
      {
        return false;
      }
      if (i < node->keys.size() && !separator_fits(node, i, previous_key))
      {
        return false;
      }
    }
    return node->total == counted_keys - keys_before;
  }

  bool verify_leaf(const DynamicBPlusNode* leaf, int depth, int& leaf_depth, int& counted_keys,
                   Key& previous_key) const {
    if (leaf_depth < 0)
    {
      leaf_depth = depth;
    }
    if (leaf_depth != depth || !is_sorted(leaf->keys.begin(), leaf->keys.end()))
    {
      return false;
    }
    if (!leaf->keys.empty()) {
      if (leaf->keys[0] < previous_key)
      {
        return false;
      }
      previous_key = leaf->keys.back();
    }
    counted_keys += leaf->keys.size();
    return true;
  }

  bool separator_fits(const DynamicBPlusNode* node, size_t i, Key max_of_left_child) const {
    return max_of_left_child <= node->keys[i] && node->kids[i + 1]->min_key() >= node->keys[i];
  }
};

#endif
