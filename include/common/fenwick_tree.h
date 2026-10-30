#ifndef COMMON_FENWICK_TREE_H
#define COMMON_FENWICK_TREE_H

#include <vector>
using namespace std;

struct FenwickTree {
  vector<int> tree;

  void build(const vector<int>& values) {
    int size = values.size();
    tree.assign(size + 1, 0);
    for (int i = 1; i <= size; i++) {
      tree[i] += values[i - 1];
      int parent = i + (i & -i);
      if (parent <= size)
      {
        tree[parent] += tree[i];
      }
    }
  }

  void add(int index, int delta) {
    for (int i = index + 1; i < (int)tree.size(); i += i & -i)
    {
      tree[i] += delta;
    }
  }

  int prefix_sum(int count) const {
    int sum = 0;
    for (int i = count; i > 0; i -= i & -i)
    {
      sum += tree[i];
    }
    return sum;
  }

  long long ram_bytes() const { return tree.capacity() * sizeof(int); }
};

#endif
