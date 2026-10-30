#ifndef TREAP_TREAP_NODE_H
#define TREAP_TREAP_NODE_H

#include "common/key.h"

struct TreapNode {
  Key key;
  unsigned priority;
  int subtree_size = 1;
  TreapNode *left = nullptr, *right = nullptr;
};

#endif
