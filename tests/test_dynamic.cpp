#include <functional>
#include <iostream>
#include <memory>
#include <random>
#include "bptree/dynamic/dynamic_bplus_tree.h"
#include "pgm_index/dynamic/leaf_based/leaf_based_pgm.h"
#include "pgm_index/dynamic/logarithmic/logarithmic_pgm.h"
#include "treap/order_statistic_treap.h"
using namespace std;


//este primer bloque solo fue para debuggeaar
int fallas = 0;

void fallo(string que, int linea) {
  if (fallas < 20)
  {
    cout << "Verificacion fallida (" << que << ") en linea " << linea << endl;
  }
  fallas++;
}



#define VERIFICAR(que, cond) \
  if (!(cond)) {             \
    fallo(que, __LINE__);     \
  }

mt19937_64 rng(42);

struct Oracle {
  vector<Key> v;
  int lookup(Key x) const { return lower_bound(v.begin(), v.end(), x) - v.begin(); }
  void insert(Key x) { v.insert(lower_bound(v.begin(), v.end(), x), x); }
  bool erase(Key x) {
    vector<Key>::iterator it = lower_bound(v.begin(), v.end(), x);
    if (it == v.end() || *it != x)
    {
      return false;
    }
    v.erase(it);
    return true;
  }
};

template <class S>
struct Case {
  string name;
  unique_ptr<S> s;
  Oracle o;

  Case(string name_, function<S*(vector<Key>&)> make, vector<Key> init) : name(name_) {
    sort(init.begin(), init.end());
    o.v = init;
    s.reset(make(init));
  }

  void probe(Key x) {
    int want = o.lookup(x);
    int got = s->lookup(x);
    if (got != want) {
      fallo(name + ": lookup(" + to_string(x) + ") = " + to_string(got) + ", esperado " + to_string(want), 0);
    }
  }

  void insert(Key x) {
    s->insert(x);
    o.insert(x);
    VERIFICAR(name + " size tras insert", s->size() == (int)o.v.size());
  }

  void erase(Key x) {
    bool want = o.erase(x);
    bool got = s->erase(x);
    VERIFICAR(name + " erase devuelve lo esperado", got == want);
    VERIFICAR(name + " size tras erase", s->size() == (int)o.v.size());
  }

  void full(Key dom) {
    for (Key x = 0; x <= dom + 1; x++)
    {
      probe(x);
    }
    probe(ULLONG_MAX - 1);
    probe(ULLONG_MAX);
  }

  void check() { VERIFICAR(name + " invariantes", s->check()); }
};

template <class S>
void random_ops(string name, function<S*(vector<Key>&)> make, int n0, int ops, Key dom, int p_insert) {
  vector<Key> init(n0);
  for (Key& k : init)
  {
    k = rng() % (dom + 1);
  }
  Case<S> c(name, make, init);
  c.check();
  for (int i = 0; i < ops; i++) {
    Key x;
    bool ins = (int)(rng() % 100) < p_insert;
    if (ins) {
      x = rng() % (dom + 1);
      c.insert(x);
    } else {
      if (!c.o.v.empty() && rng() % 10 < 7)
      {
        x = c.o.v[rng() % c.o.v.size()];
      }
      else
      {
        x = rng() % (dom + 1);
      }
      c.erase(x);
    }
    Key previous = 0;
    if (x > 0)
    {
      previous = x - 1;
    }
    for (Key y : {x, x + 1, previous, Key(rng() % (dom + 2)), Key(0), Key(dom + 1)})
      c.probe(y);
    if (i % 50 == 0)
    {
      c.check();
    }
    if (i % 500 == 0)
    {
      c.full(dom);
    }
  }
  c.check();
  c.full(dom);
  cout << "OK: " << name << " n0=" << n0 << " ops=" << ops << " dominio=" << dom + 1 << " insert=" << p_insert
       << "% n_final=" << c.o.v.size() << endl;
}

template <class S>
void patterns(string name, function<S*(vector<Key>&)> make) {
  {
    Case<S> c(name, make, {1, 2, 3, 5000});
    c.insert(4);
    for (Key x : {0, 1, 2, 3, 4, 5, 6, 4999, 5000, 5001})
      c.probe(x);
    VERIFICAR(name + " 4 desplaza el rank de 5000", c.s->lookup(5000) == 4);
    c.check();
  }
  {
    Case<S> c(name, make, {});
    for (Key x = 3000; x > 0; x--)
    {
      c.insert(x * 10);
    }
    c.check();
    for (Key x = 1; x <= 3000; x++)
    {
      c.insert(100000 + x * 10);
    }
    c.check();
    for (int i = 0; i < 3000; i++)
    {
      c.insert(15000 + rng() % 200);
    }
    c.check();
    for (Key x = 0; x <= 140000; x += 7)
    {
      c.probe(x);
    }

    vector<Key> all = c.o.v;
    shuffle(all.begin(), all.end(), rng);
    for (size_t i = 0; i < all.size(); i++) {
      c.erase(all[i]);
      if (i % 97 == 0) {
        c.probe(all[i]);
        c.probe(all[(i * 31) % all.size()]);
        c.check();
      }
    }
    VERIFICAR(name + " vacio", c.s->size() == 0);
    c.probe(0);
    c.probe(12345);
    c.check();
  }
  {
    Case<S> c(name, make, {5, 5, 9});
    for (int i = 0; i < 4000; i++)
    {
      c.insert(7);
    }
    c.probe(7);
    c.probe(8);
    c.check();
    for (int i = 0; i < 4000; i++) {
      c.erase(7);
      if (i % 331 == 0) {
        c.probe(7);
        c.probe(8);
        c.check();
      }
    }
    c.erase(7);
    c.probe(7);
    c.probe(9);
    c.check();
  }
  {
    Case<S> c(name, make, {10, 20, 30});
    for (int i = 0; i < 3000; i++) {
      c.insert(25);
      c.probe(26);
      c.erase(25);
      c.probe(26);
      c.erase(25);
    }
    c.erase(0);
    c.erase(ULLONG_MAX - 1);
    c.full(40);
    c.check();
  }
}

template <class S>
void battery(string name, function<S*(vector<Key>&)> make) {
  patterns<S>(name, make);
  random_ops<S>(name, make, 0, 4000, 50, 60);
  random_ops<S>(name, make, 300, 6000, 1000, 50);
  random_ops<S>(name, make, 2000, 6000, 30000, 40);
  random_ops<S>(name, make, 500, 6000, 80, 55);
}

template <class S>
void big(string name, function<S*(vector<Key>&)> make, int n0, int ops) {
  vector<Key> init(n0);
  for (Key& k : init)
  {
    k = rng() >> 2;
  }
  Case<S> c(name, make, init);
  for (int i = 0; i < ops; i++) {
    if (rng() % 2 || c.o.v.empty()) {
      c.insert(rng() >> 2);
    } else {
      Key x;
      if (rng() % 4 == 0)
      {
        x = rng() >> 2;
      }
      else
      {
        x = c.o.v[rng() % c.o.v.size()];
      }
      c.erase(x);
    }
    if (i % 4999 == 0) {
      c.check();
      for (int q = 0; q < 3000; q++) {
        c.probe(rng() >> 2);
        c.probe(c.o.v[rng() % c.o.v.size()]);
      }
    }
  }
  c.check();
  cout << "OK: " << name << " n0=" << n0 << " ops=" << ops << " n_final=" << c.o.v.size() << endl;
}

void amortized_bound_logarithmic(int buffer_capacity, int ops) {
  vector<Key> empty;
  LogarithmicPGM s(empty, 2, buffer_capacity, 8);
  vector<Key> live;
  for (int i = 1; i <= ops; i++) {
    if (live.empty() || rng() % 10 < 6) {
      Key x = rng() >> 2;
      s.insert(x);
      live.push_back(x);
    } else {
      size_t j = rng() % live.size();
      s.erase(live[j]);
      live[j] = live.back();
      live.pop_back();
    }
    if (i % 1000 == 0 || i == ops)
    {
      VERIFICAR("LogarithmicPGM: cota amortizada 2 * ops * niveles", s.moved_key_count <= 2LL * i * (long long)s.levels.size());
    }
  }
  cout << "OK: LogarithmicPGM(B0=" << buffer_capacity << ") cota amortizada tras " << ops << " operaciones: "
       << s.moved_key_count << " entradas movidas <= " << 2LL * ops * (long long)s.levels.size() << " (niveles="
       << s.levels.size() << ", " << double(s.moved_key_count) / ops << " por operacion)" << endl;
}

LogarithmicPGM* make_logarithmic_small(vector<Key>& keys) {
  return new LogarithmicPGM(keys, 2, 4, 8);
}

LogarithmicPGM* make_logarithmic_minimal(vector<Key>& keys) {
  return new LogarithmicPGM(keys, 1, 2, 2);
}

LogarithmicPGM* make_logarithmic_default(vector<Key>& keys) {
  return new LogarithmicPGM(keys);
}

LeafBasedPGM* make_leaf_small(vector<Key>& keys) {
  return new LeafBasedPGM(keys, 2, 16, 2);
}

LeafBasedPGM* make_leaf_minimal(vector<Key>& keys) {
  return new LeafBasedPGM(keys, 1, 8, 0);
}

LeafBasedPGM* make_leaf_medium(vector<Key>& keys) {
  return new LeafBasedPGM(keys, 4, 64, 8);
}

LeafBasedPGM* make_leaf_default(vector<Key>& keys) {
  return new LeafBasedPGM(keys);
}

DynamicBPlusTree* make_bplus_4(vector<Key>& keys) {
  return new DynamicBPlusTree(keys, 4);
}

DynamicBPlusTree* make_bplus_8(vector<Key>& keys) {
  return new DynamicBPlusTree(keys, 8);
}

DynamicBPlusTree* make_bplus_16(vector<Key>& keys) {
  return new DynamicBPlusTree(keys, 16);
}

DynamicBPlusTree* make_bplus_64(vector<Key>& keys) {
  return new DynamicBPlusTree(keys, 64);
}

OrderStatisticTreap* make_treap(vector<Key>& keys) {
  return new OrderStatisticTreap(keys);
}

int main() {
  battery<LogarithmicPGM>("LogarithmicPGM(eps=2,B0=4,min_pgm=8)", make_logarithmic_small);
  battery<LogarithmicPGM>("LogarithmicPGM(eps=1,B0=2,min_pgm=2)", make_logarithmic_minimal);
  battery<LeafBasedPGM>("LeafBasedPGM(eps=2,C=16,delta=2)", make_leaf_small);
  battery<LeafBasedPGM>("LeafBasedPGM(eps=1,C=8,delta=0)", make_leaf_minimal);
  battery<LeafBasedPGM>("LeafBasedPGM(eps=4,C=64,delta=8)", make_leaf_medium);
  battery<DynamicBPlusTree>("DynamicBPlusTree(B=4)", make_bplus_4);
  battery<DynamicBPlusTree>("DynamicBPlusTree(B=8)", make_bplus_8);
  battery<DynamicBPlusTree>("DynamicBPlusTree(B=16)", make_bplus_16);
  battery<OrderStatisticTreap>("OrderStatisticTreap", make_treap);

  amortized_bound_logarithmic(4, 200000);
  amortized_bound_logarithmic(256, 400000);

  big<LogarithmicPGM>("LogarithmicPGM por defecto", make_logarithmic_default, 100000, 60000);
  big<LeafBasedPGM>("LeafBasedPGM por defecto", make_leaf_default, 100000, 60000);
  big<DynamicBPlusTree>("DynamicBPlusTree(B=64)", make_bplus_64, 100000, 60000);
  big<OrderStatisticTreap>("OrderStatisticTreap", make_treap, 100000, 60000);

  if (fallas) {
    cout << "FALLO: " << fallas << " verificaciones" << endl;
    return 1;
  }
  cout << "OK: todas las verificaciones pasaron" << endl;
  return 0;
}
