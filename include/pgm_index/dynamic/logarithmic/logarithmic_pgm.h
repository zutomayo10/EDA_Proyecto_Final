#ifndef PGM_INDEX_DYNAMIC_LOGARITHMIC_LOGARITHMIC_PGM_H
#define PGM_INDEX_DYNAMIC_LOGARITHMIC_LOGARITHMIC_PGM_H

#include <memory>
#include <stdexcept>
#include <vector>
#include "common/key.h"
#include "common/sorted_runs.h"
#include "pgm_index/dynamic/logarithmic/logarithmic_level.h"
#include "pgm_index/dynamic/logarithmic/update_buffer.h"
using namespace std;

struct LogarithmicPGM {
  int epsilon, buffer_capacity, min_keys_for_model;
  UpdateBuffer buffer;
  vector<unique_ptr<LogarithmicLevel>> levels;
  int key_count = 0;
  long long merge_count = 0, moved_key_count = 0;

  LogarithmicPGM(vector<Key>& keys, int epsilon_ = 64, int buffer_capacity_ = 256, int min_keys_for_model_ = 4096)
      : epsilon(epsilon_), buffer_capacity(buffer_capacity_), min_keys_for_model(min_keys_for_model_) {
    if (epsilon < 1 || buffer_capacity < 2)
    {
      throw invalid_argument("eps >= 1 y B0 >= 2");
    }
    if (keys.empty())
    {
      return;
    }
    place_initial_keys(keys);
  }

  int size() const { return key_count; }

  int lookup(Key x) {
    int rank = buffer.rank(x);
    for (size_t i = 0; i < levels.size(); i++)
    {
      rank += levels[i]->rank(x);
    }
    return rank;
  }

  void insert(Key x) {
    if (x == RESERVED_KEY)
    {
      throw invalid_argument("ULLONG_MAX esta reservado como centinela");
    }
    key_count++;
    if (buffer.cancel_pending_delete(x))
    {
      return;
    }
    buffer.add_insert(x);
    flush_if_full();
  }

  bool erase(Key x) {
    if (x == RESERVED_KEY || count_occurrences(x) == 0)
    {
      return false;
    }
    key_count--;
    if (buffer.remove_pending_insert(x))
    {
      return true;
    }
    buffer.add_delete(x);
    flush_if_full();
    return true;
  }

  long long size_in_bytes() const {
    long long bytes = 0;
    for (size_t i = 0; i < levels.size(); i++)
    {
      bytes += levels[i]->index_bytes();
    }
    return bytes;
  }

  long long ram_bytes() const {
    long long bytes = buffer.ram_bytes();
    for (size_t i = 0; i < levels.size(); i++)
    {
      bytes += levels[i]->ram_bytes();
    }
    return bytes;
  }

  int segments_count() const {
    int count = 0;
    for (size_t i = 0; i < levels.size(); i++)
    {
      count += levels[i]->segment_count();
    }
    return count;
  }

  int levels_used() const {
    int used = 0;
    for (size_t k = 0; k < levels.size(); k++)
    {
      if (levels[k]->size() > 0)
      {
        used = k + 1;
      }
    }
    return used;
  }

  bool check() const {
    if (buffer.size() >= buffer_capacity || !buffer.is_valid())
    {
      return false;
    }
    vector<Key> inserts = buffer.inserts;
    vector<Key> deletes = buffer.deletes;
    for (size_t k = 0; k < levels.size(); k++) {
      const LogarithmicLevel& level = *levels[k];
      if (level.size() > capacity_of_level(k) || !level.is_valid())
      {
        return false;
      }
      inserts = merge_sorted_runs(inserts, level.inserts);
      deletes = merge_sorted_runs(deletes, level.deletes);
    }
    int live_keys = inserts.size() - deletes.size();
    cancel_matching_pairs(inserts, deletes);
    return deletes.empty() && live_keys == key_count;
  }

 private:
  long long capacity_of_level(int k) const { return (long long)buffer_capacity << (k + 1); }

  void place_initial_keys(vector<Key>& keys) {
    int k = 0;
    while (capacity_of_level(k) < (long long)keys.size())
    {
      k++;
    }
    while ((int)levels.size() <= k)
    {
      levels.emplace_back(new LogarithmicLevel);
    }
    levels[k]->inserts = keys;
    levels[k]->train(epsilon, min_keys_for_model);
    key_count = keys.size();
  }

  int count_occurrences(Key x) { return lookup(x + 1) - lookup(x); }

  void flush_if_full() {
    if (buffer.size() >= buffer_capacity)
    {
      flush();
    }
  }

  void flush() { merge_levels_into(find_target_level()); }

  int find_target_level() {
    long long needed = buffer.size();
    for (int level = 0;; level++) {
      if (level == (int)levels.size())
      {
        levels.emplace_back(new LogarithmicLevel);
      }
      if (levels[level]->size() + needed <= capacity_of_level(level))
      {
        return level;
      }
      needed += levels[level]->size();
    }
  }

  void merge_levels_into(int target) {
    vector<Key> inserts;
    vector<Key> deletes;
    inserts.swap(buffer.inserts);
    deletes.swap(buffer.deletes);
    for (int j = 0; j <= target; j++) {
      inserts = merge_sorted_runs(inserts, levels[j]->inserts);
      deletes = merge_sorted_runs(deletes, levels[j]->deletes);
      levels[j]->clear();
    }
    cancel_matching_pairs(inserts, deletes);
    moved_key_count += inserts.size() + deletes.size();
    merge_count++;
    levels[target]->inserts.swap(inserts);
    levels[target]->deletes.swap(deletes);
    levels[target]->train(epsilon, min_keys_for_model);
  }
};

#endif
