import argparse
import os

import numpy as np

RAW = "data/raw"
OUT = "data"

SOSD = {
    "books": "books_800M_uint64",
    "books50M": "books_50M_uint64",
    "wiki": "wiki_ts_200M_uint64",
    "fb": "fb_200M_uint64",
    "osm": "osm_cellids_800M_uint64",
    "lognormal": "lognormal_uint32",
    "normal": "normal_uint32",
    "zipf": "zipf_uint32",
    "exponential": "exponential_uint32",
    "companynet": "companynet_uint32",
    "friendster": "friendster_50M_uint32",
}

def write(name, keys):
    keys = np.asarray(keys, dtype=np.uint64)
    keys = keys[keys != np.uint64(2**64 - 1)]
    assert np.all(keys[1:] >= keys[:-1]), name + " no esta ordenado"
    path = f"{OUT}/{name}.bin"
    with open(path, "wb") as output_file:
        header = np.array([len(keys)], dtype=np.uint64)
        header.tofile(output_file)
        keys.tofile(output_file)
    print(f"{name:12s} n={len(keys):>10,d}  unicos={len(np.unique(keys)):>10,d}")

def from_sosd(name, raw, n):
    path = f"{RAW}/{raw}"
    if raw.endswith("uint64"):
        dtype = np.uint64
    else:
        dtype = np.uint32
    if not os.path.exists(path):
        print(f"{name:12s} falta {path} (corre download.sh)")
        return
    total = int(np.fromfile(path, dtype=np.uint64, count=1)[0])
    width = np.dtype(dtype).itemsize
    have = (os.path.getsize(path) - 8) // width
    partial = f"{OUT}/{name}_parcial.bin"
    if have < total:
        print(f"{name:12s} incompleto ({100 * have // total}%), se usa el prefijo como {name}_parcial")
        data = np.memmap(path, dtype=dtype, mode="r", offset=8, shape=(have,))
        step = max(1, have // n)
        sample = data[::step][:n]
        write(name + "_parcial", sample)
        return
    if os.path.exists(partial):
        os.remove(partial)
    data = np.memmap(path, dtype=dtype, mode="r", offset=8, shape=(total,))
    step = max(1, total // n)
    sample = data[::step][:n]
    write(name, sample)

def status():
    if not os.path.isdir(RAW):
        return
    done = 0
    total_all = 0
    filenames = sorted(os.listdir(RAW))
    for filename in filenames:
        path = f"{RAW}/{filename}"
        if filename.endswith("uint64"):
            width = 8
        else:
            width = 4
        total = 8 + int(np.fromfile(path, dtype=np.uint64, count=1)[0]) * width
        have = os.path.getsize(path)
        done += have
        total_all += total
        print(f"{filename:26s} {have / 1e6:7.0f} / {total / 1e6:6.0f} MB  {100 * have / total:5.1f}%")
    if total_all:
        print(f"{'TOTAL':26s} {done / 1e9:7.1f} / {total_all / 1e9:6.1f} GB  {100 * done / total_all:5.1f}%")

def synthetic(n):
    rng = np.random.default_rng(42)

    write("seq", np.arange(10**12, 10**12 + n, dtype=np.uint64))

    write("uniform", np.sort(rng.integers(0, 2**63, n, dtype=np.uint64)))

    gaps = 1 + (np.uint64(1) << rng.integers(0, 31, n).astype(np.uint64))
    write("patho_loggap", np.cumsum(gaps, dtype=np.uint64))

    u = rng.random(n)
    gaps = np.minimum(u ** -2.0, 1e12).astype(np.uint64) + 1
    write("patho_pareto", np.cumsum(gaps, dtype=np.uint64))

    i = np.arange(1, n + 1, dtype=np.uint64)
    lowbit = i & (~i + np.uint64(1))
    write("patho_ruler", np.cumsum(lowbit * lowbit, dtype=np.uint64))

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--n", type=int, default=10_000_000)
    parser.add_argument("--status", action="store_true")
    args = parser.parse_args()
    if args.status:
        status()
        return
    os.makedirs(OUT, exist_ok=True)
    synthetic(args.n)
    for name, raw in SOSD.items():
        from_sosd(name, raw, args.n)

if __name__ == "__main__":
    main()
