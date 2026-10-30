#ifndef BPTREE_STATIC_STATIC_BPLUS_TREE_H
#define BPTREE_STATIC_STATIC_BPLUS_TREE_H

#include <algorithm>
#include <vector>
#include "bptree/static/static_bplus_node.h"
#include "common/key.h"
using namespace std;

struct StaticBPlusTree {
  vector<Key>* sorted_keys;
  int node_size;
  StaticBPlusNode* root = nullptr;
  int height = 0;
  long long bytes = 0;
  long long ram_bytes = 0;

  StaticBPlusTree(vector<Key>& keys, int node_size_) {
    sorted_keys = &keys;
    node_size = node_size_;
    if (keys.empty())
    {
      return;
    }
    vector<Key> level_mins;
    vector<StaticBPlusNode*> level = build_leaves(keys, level_mins);
    while (level.size() > 1) {
      level = build_parent_level(level, level_mins);
      height++;
    }
    root = level[0];
  }

  ~StaticBPlusTree() { destroy(root); }

  int lookup(Key x) {
    if (!root)
    {
      return 0;
    }
    StaticBPlusNode* node = root;
    while (!node->leaf) {
      int child = lower_bound(node->keys.begin(), node->keys.end(), x) - node->keys.begin();
      node = node->kids[child];
    }
    int key_count = sorted_keys->size();
    int page_end = min(key_count, node->start + node_size);
    return lower_bound(sorted_keys->begin() + node->start, sorted_keys->begin() + page_end, x) -
           sorted_keys->begin();
  }

 private:

  vector<StaticBPlusNode*> build_leaves(vector<Key>& keys, vector<Key>& level_mins) {
    vector<StaticBPlusNode*> leaves;
    for (int start = 0; start < (int)keys.size(); start += node_size) {
      StaticBPlusNode* leaf = new StaticBPlusNode;
      leaf->leaf = true;
      leaf->start = start;
      leaves.push_back(leaf);
      level_mins.push_back(keys[start]);
      ram_bytes += sizeof(StaticBPlusNode);
    }
    return leaves;
  }

  vector<StaticBPlusNode*> build_parent_level(vector<StaticBPlusNode*>& level, vector<Key>& level_mins) {
    vector<StaticBPlusNode*> parents;
    vector<Key> parent_mins;
    int size = level.size();
    for (int first = 0; first < size; first += node_size) {
      StaticBPlusNode* parent = new StaticBPlusNode;
      parent->leaf = false;
      parent->start = 0;
      for (int j = first; j < first + node_size && j < size; j++) {
        parent->kids.push_back(level[j]);
        if (j > first)
        {
          parent->keys.push_back(level_mins[j]);
        }
      }
      account_internal_node(parent);
      parents.push_back(parent);
      parent_mins.push_back(level_mins[first]);
    }
    level_mins = parent_mins;
    return parents;
  }

  void account_internal_node(StaticBPlusNode* node) {
    long long key_count = node->keys.size(), kid_count = node->kids.size();
    long long key_capacity = node->keys.capacity(), kid_capacity = node->kids.capacity();
    bytes += key_count * 8 + kid_count * 8;
    ram_bytes += sizeof(StaticBPlusNode) + key_capacity * 8 + kid_capacity * 8;
  }

  void destroy(StaticBPlusNode* node) {
    if (!node)
    {
      return;
    }
    for (StaticBPlusNode* kid : node->kids)
    {
      destroy(kid);
    }
    delete node;
  }
};

#endif
