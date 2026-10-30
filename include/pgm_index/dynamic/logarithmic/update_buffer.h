#ifndef PGM_INDEX_DYNAMIC_LOGARITHMIC_UPDATE_BUFFER_H
#define PGM_INDEX_DYNAMIC_LOGARITHMIC_UPDATE_BUFFER_H

#include <algorithm>
#include <vector>
#include "common/key.h"
using namespace std;

struct UpdateBuffer {
  vector<Key> inserts, deletes;

  int size() const {return inserts.size() + deletes.size(); }

  int rank(Key x) const {
    vector<Key>::const_iterator insert_position = lower_bound(inserts.begin(), inserts.end(), x);
    vector<Key>::const_iterator delete_position = lower_bound(deletes.begin(), deletes.end(), x);
    return (insert_position - inserts.begin()) - (delete_position - deletes.begin());
  }

  void add_insert(Key x) {
    vector<Key>::iterator position = lower_bound(inserts.begin(), inserts.end(), x);
    inserts.insert(position, x);
  }

  void add_delete(Key x) {
    vector<Key>::iterator position = lower_bound(deletes.begin(), deletes.end(), x);
    deletes.insert(position, x);
  }

  bool cancel_pending_delete(Key x) {return remove_one(deletes, x); }

  bool remove_pending_insert(Key x) {return remove_one(inserts, x); }

  long long ram_bytes() const {return (inserts.capacity() + deletes.capacity()) * sizeof(Key); }

  bool is_valid() const {return is_sorted(inserts.begin(), inserts.end()) && is_sorted(deletes.begin(), deletes.end()); }

 private:
  static bool remove_one(vector<Key>& sorted, Key x) {
    vector<Key>::iterator position = lower_bound(sorted.begin(), sorted.end(), x);
    if (position == sorted.end() || *position != x)
    {
      return false;
    }
    sorted.erase(position);
    return true;
  }
};

#endif
