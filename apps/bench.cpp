#include <algorithm>
#include <chrono>
#include <fstream>
#include <functional>
#include <iostream>
#include <random>
#include <string>
#include "bptree/static/static_bplus_tree.h"
#include "pgm_index/pgm.h"
using namespace std;

int QUERIES = 1000000;
int REPS = 5;
int EPS[] = {8, 16, 32, 64, 128, 256, 512, 1024};
int BS[] = {16, 64, 256};

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

double now_ns() {
  return chrono::duration<double, nano>(chrono::steady_clock::now().time_since_epoch()).count();
}

vector<Key> load(const string& path) {
  ifstream f(path, ios::binary);
  unsigned long long n = 0;
  f.read(reinterpret_cast<char*>(&n), 8);
  vector<Key> a(n);
  f.read(reinterpret_cast<char*>(a.data()), n * 8);
  return a;
}

vector<double> measure(const vector<Key>& queries, const vector<int>& expected,
                       const function<int(Key)>& lookup, bool& ok) {
  int query_count = queries.size();
  for (int i = 0; i < query_count; i++)
  {
    if (lookup(queries[i]) != expected[i])
    {
      ok = false;
    }
  }
  vector<double> t;
  long long sink = 0;
  for (int r = 0; r < REPS; r++) {
    double t0 = now_ns();
    for (int i = 0; i < query_count; i++)
    {
      sink += lookup(queries[i]);
    }
    t.push_back((now_ns() - t0) / query_count);
  }
  if (sink == 42)
  {
    cerr << "";
  }
  sort(t.begin(), t.end());
  return {t[REPS / 2], t[REPS / 4], t[REPS * 3 / 4]};
}

void write_row(string dataset, int n, string structure, int param, int segments,
               int levels, int bound, long long bytes, long long ram,
               double build, double err_avg, int err_max,
               const vector<double>& hit_times, const vector<double>& miss_times,
               string per_level) {
  cout << dataset << "," << structure << "," << param << "," << n << ","
       << segments << "," << levels << "," << bound << "," << bytes << ","
       << ram << "," << build << "," << err_avg << "," << err_max << ","
       << hit_times[0] << "," << hit_times[1] << "," << hit_times[2] << ","
       << miss_times[0] << "," << miss_times[1] << "," << miss_times[2] << ","
       << per_level << endl;
}

int main(int argc, char** argv) {
  cout << "dataset,structure,param,n,segments,levels,bound,bytes,ram_bytes,build_ms,"
          "err_avg,err_max,ns_med,ns_p25,ns_p75,miss_med,miss_p25,miss_p75,per_level"
       << endl;
  bool all_ok = true;
  for (int f = 1; f < argc; f++) {
    string path = argv[f];
    string ds = dataset_name(path);
    vector<Key> a = load(path);
    int n = a.size();
    cerr << ds << " (n=" << n << ")" << endl;

    mt19937_64 rng(7);
    vector<Key> hits(QUERIES);
    vector<Key> misses(QUERIES);
    vector<int> exp_hits(QUERIES);
    vector<int> exp_misses(QUERIES);
    unsigned long long range = a[n - 1] - a[0];
    for (int i = 0; i < QUERIES; i++) {
      hits[i] = a[rng() % n];
      if (range == 0) {
        misses[i] = a[0];
      } else {
        misses[i] = a[0] + rng() % range;
      }
      exp_hits[i] = lower_bound(a.begin(), a.end(), hits[i]) - a.begin();
      exp_misses[i] = lower_bound(a.begin(), a.end(), misses[i]) - a.begin();
    }
    bool ok = true;

    function<int(Key)> bin = [&](Key x) {
      return int(lower_bound(a.begin(), a.end(), x) - a.begin());
    };
    write_row(ds, n, "binary", 0, 0, 0, 0, 0, 0, 0, 0, 0,
              measure(hits, exp_hits, bin, ok), measure(misses, exp_misses, bin, ok), "");

    for (int B : BS) {
      double t0 = now_ns();
      StaticBPlusTree bt(a, B);
      double build = (now_ns() - t0) / 1e6;
      function<int(Key)> f = [&](Key x) {
        return bt.lookup(x);
      };
      vector<double> t = measure(hits, exp_hits, f, ok);
      vector<double> tm = measure(misses, exp_misses, f, ok);
      write_row(ds, n, "bptree", B, 0, bt.height, 0, bt.bytes, bt.ram_bytes,
                build, 0, 0, t, tm, "");
    }

    for (int eps : EPS) {
      double t0 = now_ns();
      PGMIndex pgm(a, eps);
      double build = (now_ns() - t0) / 1e6;

      double err_sum = 0;
      int err_max = 0;
      for (int i = 0; i < QUERIES; i++) {
        int err = abs(pgm.approximate_position(hits[i]).pos - exp_hits[i]);
        err_sum += err;
        err_max = max(err_max, err);
      }

      string per_level = "";
      for (int l = 0; l < pgm.height(); l++) {
        if (l > 0)
        {
          per_level += "|";
        }
        int level_segments = pgm.level_offsets[l + 1] - pgm.level_offsets[l] - 1;
        per_level += to_string(level_segments);
      }

      long long segs_cap = pgm.segments.capacity();
      long long offs_cap = pgm.level_offsets.capacity();
      long long ram = segs_cap * sizeof(Segment) + offs_cap * sizeof(int);

      function<int(Key)> f = [&](Key x) {
        return pgm.lookup(x);
      };
      vector<double> t = measure(hits, exp_hits, f, ok);
      vector<double> tm = measure(misses, exp_misses, f, ok);
      write_row(ds, n, "pgm", eps, pgm.bottom_segment_count(), pgm.height(),
                n / (2 * eps), pgm.size_in_bytes(), ram, build,
                err_sum / QUERIES, err_max, t, tm, per_level);
    }

    if (!ok)
    {
      cerr << "ERROR: respuestas distintas a lower_bound en " << ds << endl;
    }
    all_ok = all_ok && ok;
  }
  if (all_ok)
  {
    return 0;
  }
  return 1;
}
