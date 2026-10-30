#ifndef TREAP_ORDER_STATISTIC_TREAP_H
#define TREAP_ORDER_STATISTIC_TREAP_H

#include <random>
#include <stdexcept>
#include <vector>
#include "common/key.h"
#include "treap/treap_node.h"
using namespace std;

struct OrderStatisticTreap {
  TreapNode* root = nullptr;
  mt19937 priority_generator{12345};

  OrderStatisticTreap() {}
  OrderStatisticTreap(vector<Key>& keys) {
    for (Key key : keys)
    {
      insert(key);
    }
  }
  ~OrderStatisticTreap() { destroy(root); }
  OrderStatisticTreap(const OrderStatisticTreap&) = delete;
  OrderStatisticTreap& operator=(const OrderStatisticTreap&) = delete;

  int size() const { return subtree_size(root); }

  int lookup(Key x) const {
    int rank = 0;
    TreapNode* node = root;
    while (node != nullptr) {
      if (node->key < x) {
        rank += subtree_size(node->left) + 1;
        node = node->right;
      } else {
        node = node->left;
      }
    }
    return rank;
  }

  void insert(Key x) {
    if (x == RESERVED_KEY)
    {
      throw invalid_argument("ULLONG_MAX esta reservado como centinela");
    }
    TreapNode* smaller;
    TreapNode* not_smaller;
    split_by_key(root, x, smaller, not_smaller);
    TreapNode* node = new TreapNode;
    node->key = x;
    node->priority = priority_generator();
    TreapNode* left_part = merge_treaps(smaller, node);
    root = merge_treaps(left_part, not_smaller);
  }

  bool erase(Key x) {
    TreapNode* smaller;
    TreapNode* not_smaller;
    split_by_key(root, x, smaller, not_smaller);
    TreapNode* minimum = not_smaller;
    while (minimum && minimum->left)
    {
      minimum = minimum->left;
    }
    bool found = minimum && minimum->key == x;
    if (found)
    {
      not_smaller = remove_minimum(not_smaller);
    }
    root = merge_treaps(smaller, not_smaller);
    return found;
  }

  long long size_in_bytes() const { return (long long)size() * (sizeof(TreapNode) - 8); }
  long long ram_bytes() const { return (long long)size() * sizeof(TreapNode); }

  bool check() const { return verify(root, 0, RESERVED_KEY); }

 private:
  static int subtree_size(const TreapNode* node) {
    if (node == nullptr)
    {
      return 0;
    }
    return node->subtree_size;
  }

  static void update_size(TreapNode* node) {
    node->subtree_size = 1 + subtree_size(node->left) + subtree_size(node->right);
  }

  static void split_by_key(TreapNode* node, Key x, TreapNode*& smaller, TreapNode*& not_smaller) {
    if (!node) {
      smaller = nullptr;
      not_smaller = nullptr;
    } else if (node->key < x) {
      split_by_key(node->right, x, node->right, not_smaller);
      smaller = node;
      update_size(smaller);
    } else {
      split_by_key(node->left, x, smaller, node->left);
      not_smaller = node;
      update_size(not_smaller);
    }
  }

  static TreapNode* merge_treaps(TreapNode* a, TreapNode* b) {
    if (a == nullptr)
    {
      return b;
    }
    if (b == nullptr)
    {
      return a;
    }
    if (a->priority > b->priority) {
      a->right = merge_treaps(a->right, b);
      update_size(a);
      return a;
    }
    b->left = merge_treaps(a, b->left);
    update_size(b);
    return b;
  }

  static TreapNode* remove_minimum(TreapNode* node) {
    if (!node->left) {
      TreapNode* right = node->right;
      delete node;
      return right;
    }
    node->left = remove_minimum(node->left);
    update_size(node);
    return node;
  }

  void destroy(TreapNode* node) {
    if (!node)
    {
      return;
    }
    destroy(node->left);
    destroy(node->right);
    delete node;
  }

  bool verify(const TreapNode* node, Key low, Key high) const {
    if (!node)
    {
      return true;
    }
    if (node->key < low || node->key > high)
    {
      return false;
    }
    int expected_size = 1 + subtree_size(node->left) + subtree_size(node->right);
    if (node->subtree_size != expected_size)
    {
      return false;
    }
    if (node->left && node->left->priority > node->priority)
    {
      return false;
    }
    if (node->right && node->right->priority > node->priority)
    {
      return false;
    }
    return verify(node->left, low, node->key) && verify(node->right, node->key, high);
  }
};

#endif
