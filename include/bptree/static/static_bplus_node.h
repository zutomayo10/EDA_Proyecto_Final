#ifndef BPTREE_STATIC_STATIC_BPLUS_NODE_H
#define BPTREE_STATIC_STATIC_BPLUS_NODE_H

#include <vector>
#include "common/key.h"
using namespace std;

struct StaticBPlusNode {
  bool leaf;
  int start;
  vector<Key> keys;
  vector<StaticBPlusNode*> kids;
};

#endif
