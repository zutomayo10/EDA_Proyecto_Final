#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <random>
#include <string>
#include <vector>
#include "bptree/dynamic/dynamic_bplus_tree.h"
#include "pgm_index/dynamic/leaf_based/leaf_based_pgm.h"
#include "pgm_index/dynamic/logarithmic/logarithmic_pgm.h"
#include "pgm_index/pgm.h"
#include "treap/order_statistic_treap.h"
using namespace std;

double now_ns() {
  return chrono::duration<double, nano>(chrono::steady_clock::now().time_since_epoch()).count();
}

string dataset_name(const string& path) {
  string name = path;
  size_t separator = name.find_last_of("/\\");
  if (separator != string::npos)
  {
    name = name.substr(separator + 1);
  }
  size_t extension = name.find_last_of('.');
  if (extension != string::npos)
  {
    name = name.substr(0, extension);
  }
  return name;
}

int environment_value(const char* name, int default_value) {
  const char* value = getenv(name);
  if (value == nullptr)
  {
    return default_value;
  }
  return atoi(value);
}

vector<Key> load(const string& path) {
  ifstream f(path, ios::binary);
  unsigned long long n = 0;
  f.read(reinterpret_cast<char*>(&n), 8);
  vector<Key> a(n);
  f.read(reinterpret_cast<char*>(a.data()), n * 8);
  return a;
}

vector<Key> sample_keys(const vector<Key>& all, int cap) {
  vector<Key> sample;
  size_t stride = max<size_t>(1, all.size() / cap);
  for (size_t i = 0; i < all.size(); i += stride)
  {
    sample.push_back(all[i]);
  }
  return sample;
}

enum OpType { INSERT, ERASE, LOOKUP };
struct Op {
  OpType type;
  Key key;
};

struct Workload {
  string name;
  vector<Key> pre;
  vector<Op> ops;
  vector<Key> final_keys;
};

struct Stats {
  long long bytes = 0, ram = 0;
  int segs = 0, levels = 0;
  long long merges = 0, moved = 0, retrains = 0, splits = 0;
};

Stats stats(LogarithmicPGM& s) {
  Stats t;
  t.bytes = s.size_in_bytes();
  t.ram = s.ram_bytes();
  t.segs = s.segments_count();
  t.levels = s.levels_used();
  t.merges = s.merge_count;
  t.moved = s.moved_key_count;
  return t;
}

Stats stats(LeafBasedPGM& s) {
  Stats t;
  t.bytes = s.size_in_bytes();
  t.ram = s.ram_bytes();
  t.segs = s.segments_count();
  t.levels = s.height();
  t.merges = s.merge_count;
  t.retrains = s.retrain_count;
  t.splits = s.split_count;
  return t;
}

Stats stats(DynamicBPlusTree& s) {
  Stats t;
  t.bytes = s.size_in_bytes();
  t.ram = s.ram_bytes();
  t.levels = s.height;
  return t;
}

Stats stats(OrderStatisticTreap& s) {
  Stats t;
  t.bytes = s.size_in_bytes();
  t.ram = s.ram_bytes();
  return t;
}

mt19937_64 rng(7);

vector<Key> shuffled(vector<Key> keys) {
  shuffle(keys.begin(), keys.end(), rng);
  return keys;
}

vector<Key> final_keys(vector<Key> live, const vector<Op>& ops) {
  vector<Key> del;
  for (const Op& o : ops) {
    if (o.type == INSERT)
    {
      live.push_back(o.key);
    }
    else if (o.type == ERASE)
    {
      del.push_back(o.key);
    }
  }
  sort(live.begin(), live.end());
  sort(del.begin(), del.end());
  vector<Key> out;
  size_t j = 0;
  for (Key k : live) {
    while (j < del.size() && del[j] < k)
    {
      j++;
    }
    if (j < del.size() && del[j] == k) {
      j++;
      continue;
    }
    out.push_back(k);
  }
  return out;
}

vector<Workload> make_workloads(const vector<Key>& a, int max_ops) {
  int n = a.size();
  vector<Workload> ws;

  vector<Key> pre_r;
  vector<Key> rest_r;
  for (Key key : a) {
    if (rng() % 2 != 0)
    {
      pre_r.push_back(key);
    }
    else
    {
      rest_r.push_back(key);
    }
  }

  {
    Workload w{"insert_random", pre_r, {}, {}};
    vector<Key> r = shuffled(rest_r);
    for (int i = 0; i < (int)r.size() && i < max_ops; i++)
    {
      w.ops.push_back({INSERT, r[i]});
    }
    ws.push_back(w);
  }
  {
    Workload w{"insert_append", vector<Key>(a.begin(), a.begin() + n / 2), {}, {}};
    for (int i = n / 2; i < n && i - n / 2 < max_ops; i++)
    {
      w.ops.push_back({INSERT, a[i]});
    }
    ws.push_back(w);
  }
  {
    Workload w{"insert_hot", pre_r, {}, {}};
    int k = min<int>(max_ops, rest_r.size() / 10);
    int start = rest_r.size() / 2 - k / 2;
    vector<Key> win = shuffled(vector<Key>(rest_r.begin() + start, rest_r.begin() + start + k));
    for (Key x : win)
    {
      w.ops.push_back({INSERT, x});
    }
    ws.push_back(w);
  }
  for (int reads : {90, 50}) {
    Workload w{"mixed_" + to_string(reads) + "_" + to_string(100 - reads), pre_r, {}, {}};
    vector<Key> live = pre_r;
    vector<Key> pending = shuffled(rest_r);
    size_t next = 0;
    for (int i = 0; i < max_ops; i++) {
      int roll = rng() % 100;
      if (roll < reads || live.empty()) {
        Key key = 0;
        if (!live.empty())
        {
          key = live[rng() % live.size()];
        }
        w.ops.push_back({LOOKUP, key});
      } else if ((roll - reads) % 2 == 0 && next < pending.size()) {
        w.ops.push_back({INSERT, pending[next]});
        live.push_back(pending[next]);
        next++;
      } else {
        size_t j = rng() % live.size();
        w.ops.push_back({ERASE, live[j]});
        live[j] = live.back();
        live.pop_back();
      }
    }
    ws.push_back(w);
  }
  {
    Workload w{"delete_random", a, {}, {}};
    vector<Key> v = shuffled(a);
    for (int i = 0; i < (int)v.size() / 2 && i < max_ops; i++)
    {
      w.ops.push_back({ERASE, v[i]});
    }
    ws.push_back(w);
  }
  for (Workload& w : ws)
  {
    w.final_keys = final_keys(w.pre, w.ops);
  }
  return ws;
}

template <class S>
bool run(const string& ds, const Workload& w, const string& name, const string& param,
         S* s, double build_ms) {
  int n_start = s->size();

  vector<double> write_times;
  double r_sum = 0;
  long long r_cnt = 0, sink = 0;
  for (const Op& o : w.ops) {
    double t = now_ns();
    if (o.type == INSERT) {
      s->insert(o.key);
    } else if (o.type == ERASE) {
      s->erase(o.key);
    } else {
      sink += s->lookup(o.key);
      r_sum += now_ns() - t;
      r_cnt++;
      continue;
    }
    write_times.push_back(now_ns() - t);
  }
  double w_mean = 0, w_p99 = 0, w_max = 0;
  if (!write_times.empty()) {
    for (double x : write_times)
    {
      w_mean += x;
    }
    w_mean /= write_times.size();
    w_max = *max_element(write_times.begin(), write_times.end());
    size_t p99 = write_times.size() * 99 / 100;
    nth_element(write_times.begin(), write_times.begin() + p99, write_times.end());
    w_p99 = write_times[p99];
  }

  const vector<Key>& f = w.final_keys;
  bool ok = s->size() == (int)f.size();
  int q = 500000;
  vector<Key> hits(q);
  for (Key& hit : hits) {
    if (f.empty())
    {
      hit = 0;
    }
    else
    {
      hit = f[rng() % f.size()];
    }
  }
  for (int i = 0; i < 20000 && !f.empty(); i++) {
    Key x;
    if (i % 2 != 0)
    {
      x = hits[i];
    }
    else
    {
      x = (rng() >> 1) % (f.back() + 1);
    }
    if (s->lookup(x) != int(lower_bound(f.begin(), f.end(), x) - f.begin()))
    {
      ok = false;
    }
  }
  double t1 = now_ns();
  for (Key h : hits)
  {
    sink += s->lookup(h);
  }
  double read_ns = (now_ns() - t1) / q;
  if (sink == 42)
  {
    cerr << "";
  }

  Stats st = stats(*s);
  double mixed_read_ns = 0;
  if (r_cnt > 0)
  {
    mixed_read_ns = r_sum / r_cnt;
  }
  cout << ds << "," << w.name << "," << name << "," << param << "," << n_start << "," << s->size() << ","
       << w.ops.size() << "," << build_ms << "," << w_mean << "," << w_p99 << "," << w_max << ","
       << read_ns << "," << mixed_read_ns << "," << st.bytes << "," << st.ram << ","
       << st.segs << "," << st.levels << "," << st.merges << "," << st.moved << "," << st.retrains
       << "," << st.splits << "," << ok << endl;
  delete s;
  if (!ok)
  {
    cerr << "ERROR: " << name << " " << param << " no coincide con lower_bound en " << ds << "/" << w.name << endl;
  }
  return ok;
}

int main(int argc, char** argv) {
  int cap = environment_value("DYN_N", 2000000);
  int max_ops = environment_value("DYN_OPS", 1000000);
  cout << "dataset,workload,structure,param,n_start,n_end,ops,build_ms,write_mean_ns,write_p99_ns,"
          "write_max_ns,read_ns,mixed_read_ns,bytes,ram_bytes,segments,levels,merges,moved_keys,"
          "retrains,splits,ok"
       << endl;
  bool all_ok = true;
  for (int f = 1; f < argc; f++) {
    string path = argv[f];
    string ds = dataset_name(path);
    vector<Key> all = load(path);
    vector<Key> a = sample_keys(all, cap);
    cerr << ds << " (n=" << a.size() << " de " << all.size() << ")" << endl;

    vector<Workload> ws = make_workloads(a, max_ops);
    for (const Workload& w : ws) {
      cerr << "  " << w.name << endl;
      for (int buffer_capacity : {256, 2048}) {
        vector<Key> keys = w.pre;
        double start = now_ns();
        LogarithmicPGM* index = new LogarithmicPGM(keys, 64, buffer_capacity);
        double build_ms = (now_ns() - start) / 1e6;
        string param = "eps64_B0" + to_string(buffer_capacity);
        all_ok &= run(ds, w, "logpgm", param, index, build_ms);
      }

      int capacities[] = {4096, 1024, 4096};
      int thresholds[] = {64, 128, 512};
      for (int i = 0; i < 3; i++) {
        vector<Key> keys = w.pre;
        double start = now_ns();
        LeafBasedPGM* index = new LeafBasedPGM(keys, 64, capacities[i], thresholds[i]);
        double build_ms = (now_ns() - start) / 1e6;
        string param = "eps64_C" + to_string(capacities[i]) + "_d" + to_string(thresholds[i]);
        all_ok &= run(ds, w, "leafpgm", param, index, build_ms);
      }

      for (int capacity : {64, 256}) {
        vector<Key> keys = w.pre;
        double start = now_ns();
        DynamicBPlusTree* index = new DynamicBPlusTree(keys, capacity);
        double build_ms = (now_ns() - start) / 1e6;
        string param = "B" + to_string(capacity);
        all_ok &= run(ds, w, "bptree_dyn", param, index, build_ms);
      }

      vector<Key> keys = w.pre;
      double start = now_ns();
      OrderStatisticTreap* index = new OrderStatisticTreap(keys);
      double build_ms = (now_ns() - start) / 1e6;
      all_ok &= run(ds, w, "treap", "0", index, build_ms);
    }

    for (const Workload& w : ws) {
      vector<Key> fk = w.final_keys;
      if (fk.empty())
      {
        continue;
      }
      double t0 = now_ns();
      PGMIndex pgm(fk, 64);
      double ms = (now_ns() - t0) / 1e6;
      cout << ds << "," << w.name << ",static_rebuild,eps64," << fk.size() << "," << fk.size() << ",0," << ms
           << ",0,0,0,0,0," << pgm.size_in_bytes() << ",0," << pgm.bottom_segment_count() << "," << pgm.height()
           << ",0,0,0,0,1" << endl;
    }
  }
  if (!all_ok)
  {
    return 1;
  }
  return 0;
}
