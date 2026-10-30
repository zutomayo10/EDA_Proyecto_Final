#include <iostream>
#include <random>
#include "bptree/static/static_bplus_tree.h"
#include "pgm_index/pgm.h"
using namespace std;

int fallas = 0;

void fallo(int linea) {
  cout << "Verificacion fallida en linea " << linea << endl;
  fallas++;
}

int linear_position(vector<Key>& keys, Key key) {
  int position = 0;
  int size = keys.size();
  while (position < size && keys[position] < key)
  {
    position++;
  }
  return position;
}

int lower_bound_position(vector<Key>& keys, Key key) {
  return lower_bound(keys.begin(), keys.end(), key) - keys.begin();
}

mt19937_64 rng(42);

vector<Key> generate_keys(string kind, int n) {
  vector<Key> a(n);
  if (kind == "uniform") {
    for (int i = 0; i < n; i++)
    {
      a[i] = rng() >> 1;
    }
  } else if (kind == "lognormal") {
    lognormal_distribution<double> d(0, 2);
    for (int i = 0; i < n; i++)
    {
      a[i] = 1 + d(rng) * 1e9;
    }
  } else if (kind == "ruler") {
    Key cur = 0;
    for (Key i = 1; i <= n; i++) {
      Key low = i & -i;
      cur += low * low;
      a[i - 1] = cur;
    }
  } else if (kind == "dups") {
    for (int i = 0; i < n; i++)
    {
      a[i] = rng() % (n / 10 + 1);
    }
  }
  sort(a.begin(), a.end());
  return a;
}

void check_all(vector<Key>& a, int eps, int extra) {
  PGMIndex pgm(a, eps);
  StaticBPlusTree bt(a, 16);
  int n = a.size();
  for (int i = 0; i < n; i++) {
    int expected = lower_bound_position(a, a[i]);
    ApproximatePosition p = pgm.approximate_position(a[i]);
    if (!(p.lo <= expected && expected < p.hi))
    {
      fallo(__LINE__);
    }
    if (pgm.lookup(a[i]) != expected)
    {
      fallo(__LINE__);
    }
    if (bt.lookup(a[i]) != expected)
    {
      fallo(__LINE__);
    }
  }
  for (int q = 0; q < extra; q++) {
    Key x = a[rng() % n] + rng() % 3 - 1;
    if (q % 2)
    {
      x = rng();
    }
    if (pgm.lookup(x) != lower_bound_position(a, x))
    {
      fallo(__LINE__);
    }
    if (bt.lookup(x) != lower_bound_position(a, x))
    {
      fallo(__LINE__);
    }
  }
}

int main() {
  vector<Key> vacio;
  PGMIndex p0(vacio, 4);
  StaticBPlusTree b0(vacio, 4);
  if (p0.lookup(5) != 0 || b0.lookup(5) != 0)
  {
    fallo(__LINE__);
  }

  vector<Key> uno = {10};
  PGMIndex p1(uno, 4);
  if (p1.lookup(10) != 0 || p1.lookup(3) != 0 || p1.lookup(11) != 1 || p1.lookup(ULLONG_MAX) != 1)
  {
    fallo(__LINE__);
  }
  cout << "OK: arreglo vacio y de una llave\n";

  vector<Key> seq(100000);
  for (int i = 0; i < 100000; i++)
  {
    seq[i] = 1000000 + i;
  }
  PGMIndex ps(seq, 8);
  if (ps.bottom_segment_count() != 1 || ps.height() != 1)
  {
    fallo(__LINE__);
  }
  if (ps.lookup(0) != 0 || ps.lookup(1000500) != 500 || ps.lookup(ULLONG_MAX - 1) != 100000)
  {
    fallo(__LINE__);
  }
  cout << "OK: llaves consecutivas -> " << ps.bottom_segment_count() << " segmento, " << ps.height() << " nivel, "
       << ps.size_in_bytes() << " bytes\n";

  vector<Key> chico = generate_keys("lognormal", 2000);
  for (int eps : {1, 2, 8}) {
    PGMIndex p(chico, eps);
    StaticBPlusTree b(chico, 4);
    for (int i = 0; i < 2000; i++)
    {
      for (Key x : {chico[i] - 1, chico[i], chico[i] + 1}) {
    }
        if (p.lookup(x) != linear_position(chico, x))
        {
          fallo(__LINE__);
        }
        if (b.lookup(x) != linear_position(chico, x))
        {
          fallo(__LINE__);
        }
      }
  }
  cout << "OK: 2000 llaves lognormal, PGM y B+ tree = recorrido lineal (eps 1, 2, 8)\n";

  for (string kind : {"uniform", "lognormal", "ruler", "dups"}) {
    vector<Key> a = generate_keys(kind, 100000);
    for (int eps : {1, 4, 16, 64, 256}) {
      check_all(a, eps, 100000);
      PGMIndex p(a, eps);

      if (p.bottom_segment_count() > 100000 / (2 * eps) + 2)
      {
        fallo(__LINE__);
      }
    }
    PGMIndex p64(a, 64);
    cout << "OK: " << kind << " n=100000, eps 1..256, m(64)=" << p64.bottom_segment_count()
         << " cota n/(2eps)=" << 100000 / 128 << ", niveles=" << p64.height() << "\n";
  }

  vector<Key> con_centinela = {1, 2, ULLONG_MAX};
  bool lanzo = false;
  try {
    PGMIndex p(con_centinela, 4);
  } catch (invalid_argument& e) {
    lanzo = true;
  }
  if (!lanzo)
  {
    fallo(__LINE__);
  }
  cout << "OK: ULLONG_MAX rechazado (centinela)\n";

  if (fallas > 0)
  {
    cout << fallas << " verificaciones fallidas\n";
  }
  else
  {
    cout << "OK: todas las verificaciones pasaron\n";
  }
  if (fallas > 0)
  {
    return 1;
  }
  return 0;
}
