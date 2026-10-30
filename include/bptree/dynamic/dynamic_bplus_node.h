#ifndef BPTREE_DYNAMIC_DYNAMIC_BPLUS_NODE_H
#define BPTREE_DYNAMIC_DYNAMIC_BPLUS_NODE_H

#include <vector>
#include "common/key.h"
using namespace std;

struct DynamicBPlusNode {
  bool leaf = true;
  int total = 0;
  vector<Key> keys;
  vector<DynamicBPlusNode*> kids;

  int key_total() const {
    if (leaf)
    {
      return keys.size();
    }
    return total;
  }

  int entry_count() const {
    if (leaf)
    {
      return keys.size();
    }
    return kids.size();
  }

  Key max_key() const {
    const DynamicBPlusNode* node = this;
    while (!node->leaf)
    {
      node = node->kids.back();
    }
    return node->keys.back();
  }

  Key min_key() const {
    const DynamicBPlusNode* node = this;
    while (!node->leaf)
    {
      node = node->kids.front();
    }
    if (node->keys.empty())
    {
      return 0;
    }
    return node->keys.front();
  }
};

#endif
