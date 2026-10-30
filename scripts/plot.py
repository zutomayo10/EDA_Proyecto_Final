import csv
import os

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt

RESULTS = "results"
PLOTS = os.path.join(RESULTS, "plots")
with open(os.path.join(RESULTS, "results.csv")) as file:
    rows = list(csv.DictReader(file))
pgm = {}
bpt = {}
binary = {}
for r in rows:
    if r["structure"] == "pgm":
        if r["dataset"] not in pgm:
            pgm[r["dataset"]] = []
        pgm[r["dataset"]].append(r)
    elif r["structure"] == "bptree":
        if r["dataset"] not in bpt:
            bpt[r["dataset"]] = []
        bpt[r["dataset"]].append(r)
    else:
        binary[r["dataset"]] = r
datasets = list(pgm)
for dataset in datasets:
    if dataset not in bpt:
        bpt[dataset] = []
os.makedirs(PLOTS, exist_ok=True)
COLORS = {"16": "C1", "64": "C2", "256": "C3"}

def eps_of(ds):
    values = []
    for row in pgm[ds]:
        values.append(int(row["param"]))
    return values

def col(ds, key):
    values = []
    for row in pgm[ds]:
        values.append(float(row[key]))
    return values

def positive_col(ds, key):
    values = col(ds, key)
    for i in range(len(values)):
        values[i] = max(1, values[i])
    return values

def grid(title, fname, chart, prefix=""):
    cols = 4
    nrows = (len(datasets) + cols - 1) // cols
    fig, axes = plt.subplots(nrows, cols, figsize=(4.2 * cols, 3.2 * nrows), squeeze=False)
    for ax, ds in zip(axes.flat, datasets):
        if chart == "space":
            draw_space(ax, ds)
        elif chart == "latency":
            draw_latency(ax, ds, prefix)
        elif chart == "error":
            draw_error(ax, ds)
        ax.set_xscale("log", base=2)
        ax.set_title(ds, fontsize=9)
        ax.set_xlabel("ε", fontsize=8)
        ax.grid(alpha=0.3)
    for ax in list(axes.flat)[len(datasets):]:
        ax.axis("off")
    axes.flat[0].legend(fontsize=7)
    fig.suptitle(title)
    fig.tight_layout()
    fig.savefig(os.path.join(PLOTS, fname), dpi=120)
    plt.close(fig)

plt.figure(figsize=(9, 6))
for ds in datasets:
    plt.plot(eps_of(ds), positive_col(ds, "segments"), marker="o", label=ds)
ref = datasets[0]
plt.plot(eps_of(ref), col(ref, "bound"), "k--", lw=2, label="cota n/(2ε)")
plt.xscale("log", base=2)
plt.yscale("log")
plt.xlabel("ε")
plt.ylabel("segmentos en el nivel 0 (m)")
plt.legend(fontsize=7, ncol=2)
plt.grid(alpha=0.3)
plt.savefig(os.path.join(PLOTS, "segments.png"), dpi=150, bbox_inches="tight")
plt.close()

def draw_space(ax, ds):
    ax.plot(eps_of(ds), positive_col(ds, "bytes"), "o-", color="C0", label="PGM bytes")
    ax.plot(eps_of(ds), positive_col(ds, "ram_bytes"), ":", color="C0", label="PGM RAM")
    for r in bpt[ds]:
        c = COLORS.get(r["param"], "k")
        ax.axhline(float(r["bytes"]), ls="--", lw=1, color=c, label=f"B+ B={r['param']} bytes")
        ax.axhline(float(r["ram_bytes"]), ls=":", lw=1, color=c, label=f"B+ B={r['param']} RAM")
    ax.set_yscale("log")
    ax.set_ylabel("bytes", fontsize=8)

grid("Espacio del índice (sin contar los datos)", "space.png", "space")

def draw_latency(ax, ds, prefix):
    y = col(ds, prefix + "_med")
    p25 = col(ds, prefix + "_p25")
    p75 = col(ds, prefix + "_p75")
    lo = []
    hi = []
    for i in range(len(y)):
        lo.append(y[i] - p25[i])
        hi.append(p75[i] - y[i])
    ax.errorbar(eps_of(ds), y, yerr=[lo, hi], marker="o", capsize=2, label="PGM")
    for r in bpt[ds]:
        ax.axhline(float(r[prefix + "_med"]), ls="--", lw=1, color=COLORS.get(r["param"], "k"),
                   label=f"B+ B={r['param']}")
    ax.axhline(float(binary[ds][prefix + "_med"]), ls="--", lw=1, color="gray", label="binaria")
    ax.set_ylabel("ns por lookup", fontsize=8)

grid("Latencia: llaves presentes (mediana, barras p25–p75)", "latency.png", "latency", "ns")
grid("Latencia: llaves al azar en el rango (miss)", "latency_miss.png", "latency", "miss")

def draw_error(ax, ds):
    e = eps_of(ds)
    ax.plot(e, col(ds, "err_avg"), "o-", label="error promedio")
    ax.plot(e, col(ds, "err_max"), "s-", label="error máximo")
    ax.plot(e, e, "k--", lw=1, label="ε (garantía)")
    ax.set_yscale("log")
    ax.set_ylabel("|predicción − real|", fontsize=8)

grid("Precisión de la aproximación del PGM", "error.png", "error")

def draw_tradeoff(ax, ds):
    ax.plot(positive_col(ds, "bytes"), col(ds, "ns_med"), "o-", label="PGM (ε)")
    for r, e in zip(pgm[ds], eps_of(ds)):
        ax.annotate(str(e), (max(1, float(r["bytes"])), float(r["ns_med"])), fontsize=6)
    for r in bpt[ds]:
        ax.plot(float(r["bytes"]), float(r["ns_med"]), "^", color=COLORS.get(r["param"], "k"),
                label=f"B+ B={r['param']}")
    ax.set_xscale("log")
    ax.set_ylabel("ns por lookup", fontsize=8)

cols = 4
nrows = (len(datasets) + cols - 1) // cols
fig, axes = plt.subplots(nrows, cols, figsize=(4.2 * cols, 3.2 * nrows), squeeze=False)
for ax, ds in zip(axes.flat, datasets):
    draw_tradeoff(ax, ds)
    ax.set_title(ds, fontsize=9)
    ax.set_xlabel("bytes del índice", fontsize=8)
    ax.grid(alpha=0.3)
for ax in list(axes.flat)[len(datasets):]:
    ax.axis("off")
axes.flat[0].legend(fontsize=7)
fig.suptitle("Espacio vs latencia")
fig.tight_layout()
fig.savefig(os.path.join(PLOTS, "tradeoff.png"), dpi=120)
plt.close(fig)

def human(b):
    b = float(b)
    for unit in ["B", "KB", "MB", "GB"]:
        if b < 1024:
            return f"{b:.0f} {unit}"
        b /= 1024
    return f"{b:.1f} TB"

def fastest(rows):
    if not rows:
        raise ValueError("min() arg is an empty sequence")
    best = rows[0]
    for row in rows:
        if float(row["ns_med"]) < float(best["ns_med"]):
            best = row
    return best

lines = [
    "| dataset | m(ε=64) | cota | PGM mejor ε | PGM ns | PGM bytes | B+ mejor B | B+ ns | B+ bytes | binaria ns | PGM vs B+ (tiempo) | B+/PGM (espacio) |",
    "|---|---|---|---|---|---|---|---|---|---|---|---|",
]
for ds in datasets:
    best = fastest(pgm[ds])
    bbest = fastest(bpt[ds])
    m64 = None
    for row in pgm[ds]:
        if row["param"] == "64":
            m64 = row
            break
    if m64 is None:
        raise StopIteration
    lines.append(
        f"| {ds} | {m64['segments']} | {m64['bound']} | {best['param']} | {float(best['ns_med']):.0f} | "
        f"{human(best['bytes'])} | {bbest['param']} | {float(bbest['ns_med']):.0f} | {human(bbest['bytes'])} | "
        f"{float(binary[ds]['ns_med']):.0f} | {float(bbest['ns_med']) / float(best['ns_med']):.2f}× | "
        f"{float(bbest['bytes']) / max(1, float(best['bytes'])):.1f}× |"
    )
with open(os.path.join(RESULTS, "summary.md"), "w") as file:
    file.write("\n".join(lines) + "\n")
print("\n".join(lines))
print("\nplots/: segments space latency latency_miss error tradeoff")

if os.path.exists(os.path.join(RESULTS, "query_points.csv")):
    pts = {}
    with open(os.path.join(RESULTS, "query_points.csv")) as file:
        for row in csv.DictReader(file):
            dataset = row["dataset"]
            if dataset not in pts:
                pts[dataset] = []
            pts[dataset].append(row)
    zds = list(pts)
    cols = 4
    nrows = (len(zds) + cols - 1) // cols
    fig, axes = plt.subplots(nrows, cols, figsize=(4.2 * cols, 3.2 * nrows), squeeze=False)
    for ax, ds in zip(axes.flat, zds):
        q = int(pts[ds][0]["query"])
        eps = int(pts[ds][0]["eps"])
        x = []
        real = []
        pred = []
        lower = []
        upper = []
        for row in pts[ds]:
            position = int(row["pred"])
            x.append(int(row["key"]) - q)
            real.append(int(row["pos"]))
            pred.append(position)
            lower.append(position - eps)
            upper.append(position + eps)
        ax.fill_between(x, lower, upper, alpha=0.2, label="predicción ± ε")
        ax.plot(x, pred, "-", lw=1, label="predicción PGM")
        ax.plot(x, real, ".", ms=2, color="k", label="posición real")
        ax.axvline(0, color="r", lw=1, label="consulta")
        ax.set_title(ds, fontsize=9)
        ax.set_xlabel("llave − consulta", fontsize=8)
        ax.set_ylabel("posición", fontsize=8)
        ax.tick_params(labelsize=6)
        ax.grid(alpha=0.3)
    for ax in list(axes.flat)[len(zds):]:
        ax.axis("off")
    axes.flat[0].legend(fontsize=7)
    fig.suptitle("Zoom sobre una consulta (ε = 64): la llave siempre cae dentro de la banda")
    fig.tight_layout()
    fig.savefig(os.path.join(PLOTS, "query_zoom.png"), dpi=120)
    plt.close(fig)

def ns(r, k):
    return f"{float(r[k + '_med']):.0f} [{float(r[k + '_p25']):.0f}–{float(r[k + '_p75']):.0f}]"

out = ["# Reporte de experimentos: PGM-index vs B+ tree vs búsqueda binaria", ""]
if os.path.exists(os.path.join(RESULTS, "machine.txt")):
    machine = []
    with open(os.path.join(RESULTS, "machine.txt")) as file:
        for line in file:
            if line.strip():
                machine.append(line.strip())
    out += ["**Máquina:** " + " · ".join(machine), ""]
parciales = []
for ds in datasets:
    if ds.endswith("_parcial"):
        parciales.append(ds)
out += [
    f"**Datasets:** {len(datasets)}, con n = {pgm[datasets[0]][0]['n']} llaves cada uno (companynet tiene 1M). "
    "Lookups: 1M consultas, 5 repeticiones; se reporta la mediana [p25–p75] en ns. "
    "Espacio sin contar los datos.",
    "",
]
if parciales:
    out += [f"> ⚠️ Todavía parciales (solo el prefijo descargado): {', '.join(parciales)}. Correr `make all` cuando `make status` muestre 100%.", ""]

out += ["## 1. Resumen: mejor configuración de cada estructura", "",
        "La columna *PGM vs B+ (tiempo)* dice cuántas veces más rápido es el PGM; *B+/PGM (espacio)*, cuántas veces más ocupa el B+ tree.", ""]
out += lines + [""]

out += ["## 2. Gráficas", ""]
for f, t in [
    ("segments", "Segmentos m(ε) contra la cota n/(2ε)"),
    ("space", "Espacio: bytes del índice (línea) y RAM real (punteada)"),
    ("latency", "Latencia de llaves presentes"),
    ("latency_miss", "Latencia de valores al azar (casi siempre ausentes)"),
    ("error", "Precisión de la predicción del PGM"),
    ("tradeoff", "Espacio contra latencia"),
    ("query_zoom", "Zoom sobre una consulta"),
]:
    if os.path.exists(os.path.join(PLOTS, f"{f}.png")):
        out += [f"### {t}", "", f"![{t}](plots/{f}.png)", ""]

out += ["## 3. Detalle por dataset", "",
        "*m* = segmentos del nivel 0; *por nivel* = segmentos de las hojas a la raíz; "
        "*error* = |posición predicha − real| (promedio / máximo); la garantía es ≤ ε.", ""]
for ds in datasets:
    out += [f"### {ds}", "",
            "| estructura | parámetro | m | por nivel | bytes | RAM real | construcción | error prom / máx | ns presentes | ns al azar |",
            "|---|---|---|---|---|---|---|---|---|---|"]
    b = binary[ds]
    out.append(f"| binaria | — | — | — | 0 | 0 | — | — | {ns(b, 'ns')} | {ns(b, 'miss')} |")
    for r in bpt[ds]:
        out.append(f"| B+ tree | B = {r['param']} | — | {r['levels']} niveles | {human(r['bytes'])} | {human(r['ram_bytes'])} | "
                   f"{float(r['build_ms']):.1f} ms | — | {ns(r, 'ns')} | {ns(r, 'miss')} |")
    for r in pgm[ds]:
        out.append(f"| PGM | ε = {r['param']} | {r['segments']} | {r['per_level'].replace('|', ' → ')} | {human(r['bytes'])} | "
                   f"{human(r['ram_bytes'])} | {float(r['build_ms']):.0f} ms | {float(r['err_avg']):.1f} / {r['err_max']} | "
                   f"{ns(r, 'ns')} | {ns(r, 'miss')} |")
    out.append("")

if os.path.exists(os.path.join(RESULTS, "queries.md")):
    with open(os.path.join(RESULTS, "queries.md")) as file:
        out += ["## 4. Consultas de ejemplo", "", file.read()]

with open(os.path.join(RESULTS, "REPORT.md"), "w") as file:
    file.write("\n".join(out) + "\n")
print(os.path.join(RESULTS, "REPORT.md"))
