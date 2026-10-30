#include <algorithm>
#include <fstream>
#include <iostream>
#include <random>
#include <string>
#include "bptree/static/static_bplus_tree.h"
#include "pgm_index/pgm.h"
using namespace std;

int EPS = 64;
int B = 64;
int K = 5;

vector<Key> load(const string& path) {
  ifstream f(path, ios::binary);
  unsigned long long n = 0;
  f.read(reinterpret_cast<char*>(&n), 8);
  vector<Key> a(n);
  f.read(reinterpret_cast<char*>(a.data()), n * 8);
  return a;
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

int main(int argc, char** argv) {
  ofstream points("results/query_points.csv");
  points << "dataset,query,pos,key,pred,eps" << endl;

  cout << "Cada fila es una consulta `lookup(x)` = posicion de la primera llave >= x. "
       << "Parametros: PGM con eps = " << EPS << " (eps_rec = 4), B+ tree con B = " << B
       << ". El PGM **predice** una posicion (con error <= eps) y despues busca de forma exacta "
       << "en la ventana [lo, hi), asi que la respuesta final siempre es exacta.\n\n";

  for (int f = 1; f < argc; f++) {
    string path = argv[f];
    string ds = dataset_name(path);
    vector<Key> a = load(path);
    int n = a.size();
    PGMIndex pgm(a, EPS);
    StaticBPlusTree bt(a, B);

    cout << "### " << ds << "\n\n";
    cout << "n = " << n << ", segmentos nivel 0 = " << pgm.bottom_segment_count() << ", niveles = " << pgm.height()
         << ", indice = " << pgm.size_in_bytes() << " bytes\n\n";
    cout << "| tipo | x (consulta) | segmento: llave inicial / pendiente / intercepto | prediccion | real | error | ventana [lo, hi) | PGM | B+ | binaria | K=" << K << " recuperados desde la posicion |\n";
    cout << "|---|---|---|---|---|---|---|---|---|---|---|\n";

    mt19937_64 rng(11);
    for (int q = 0; q < 5; q++) {
      unsigned long long x;
      string tipo;
      if (q < 3) {
        x = a[rng() % n];
        tipo = "presente";
      } else {
        unsigned long long range = a[n - 1] - a[0];
        if (range == 0) {
          x = a[0];
        } else {
          x = a[0] + rng() % range;
        }
        tipo = "al azar";
      }

      Key model_key = max(pgm.first_key, x);
      int segment_index = pgm.locate_segment(model_key);
      Segment segment = pgm.segments[segment_index];
      ApproximatePosition position = pgm.approximate_position(x);
      int real = lower_bound(a.begin(), a.end(), x) - a.begin();
      int r_pgm = pgm.lookup(x);
      int r_bt = bt.lookup(x);
      int r_bin = real;
      if (tipo == "al azar" && real < n && a[real] == x)
      {
        tipo = "al azar (estaba)";
      }

      string ok_pgm = " ✗";
      if (r_pgm == real)
      {
        ok_pgm = " ✓";
      }
      string ok_bt = " ✗";
      if (r_bt == real)
      {
        ok_bt = " ✓";
      }
      string recuperados = "";
      for (int i = r_pgm; i < r_pgm + K && i < n; i++) {
        if (i > r_pgm)
        {
          recuperados += ", ";
        }
        recuperados += to_string(a[i]);
      }
      if (recuperados == "")
      {
        recuperados = "(fin del arreglo)";
      }

      int prediction_error = abs(position.pos - real);
      cout << "| " << tipo << " | " << x << " | " << segment.key << " / " << segment.slope << " / " << segment.intercept << " | "
           << position.pos << " | " << real << " | " << prediction_error << " | [" << position.lo << ", " << position.hi << ") | "
           << r_pgm << ok_pgm << " | " << r_bt << ok_bt << " | " << r_bin << " | " << recuperados << " |\n";

      if (q == 0) {
        int from = max(0, real - 3 * EPS);
        int to = min(n, real + 3 * EPS);
        for (int i = from; i < to; i++) {
          int prediction = pgm.approximate_position(a[i]).pos;
          points << ds << "," << x << "," << i << "," << a[i] << "," << prediction << "," << EPS << "\n";
        }
      }
    }
    cout << "\n";
  }
}
