import csv
import math
import os
import sys

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt

RESULTS = "results"
PLOTS = os.path.join(RESULTS, "plots")
WORKLOADS = ["insert_random", "insert_append", "insert_hot", "mixed_90_10", "mixed_50_50", "delete_random"]

os.makedirs(PLOTS, exist_ok=True)
csv_path = os.path.join(RESULTS, "results_dyn.csv")
if len(sys.argv) > 1:
    csv_path = sys.argv[1]

with open(csv_path) as csv_file:
    rows = list(csv.DictReader(csv_file))

datasets = []
dynamic_rows = []
static_rows = {}
for row in rows:
    dataset = row["dataset"]
    if dataset not in datasets:
        datasets.append(dataset)
    if row["structure"] == "static_rebuild":
        static_rows[(dataset, row["workload"])] = row
    else:
        dynamic_rows.append(row)
datasets.sort()

def label(row):
    if row["param"] == "0":
        return row["structure"]
    return f"{row['structure']} {row['param']}"

configs = []
for row in dynamic_rows:
    config = label(row)
    if config not in configs:
        configs.append(config)

colors = {}
for index, config in enumerate(configs):
    colors[config] = f"C{index}"

grouped_rows = {}
for row in dynamic_rows:
    group = (row["workload"], label(row))
    if group not in grouped_rows:
        grouped_rows[group] = []
    grouped_rows[group].append(row)

def rows_for(workload, config):
    return grouped_rows.get((workload, config), [])

def gmean(selected_rows, column):
    logarithms = []
    for row in selected_rows:
        value = float(row[column])
        if value > 0:
            logarithms.append(math.log(value))
    if not logarithms:
        return 0.0
    return math.exp(sum(logarithms) / len(logarithms))

def column_values(selected_rows, column):
    values = []
    for row in selected_rows:
        values.append(float(row[column]))
    return values

def bars(filename, title, column, ylabel, workloads=WORKLOADS, log=True):
    fig, ax = plt.subplots(figsize=(12, 4.8))
    width = 0.8 / len(configs)
    for index, config in enumerate(configs):
        values = []
        positions = []
        for workload_index, workload in enumerate(workloads):
            values.append(gmean(rows_for(workload, config), column))
            positions.append(workload_index + index * width)
        ax.bar(positions, values, width, label=config, color=colors[config])
    ticks = []
    for index in range(len(workloads)):
        ticks.append(index + 0.4 - width / 2)
    ax.set_xticks(ticks)
    ax.set_xticklabels(workloads)
    ax.set_ylabel(ylabel)
    if log:
        ax.set_yscale("log")
    ax.set_title(title + f" (media geométrica sobre {len(datasets)} datasets)")
    ax.legend(fontsize=7, ncol=3)
    ax.grid(alpha=0.3, axis="y")
    fig.tight_layout()
    fig.savefig(os.path.join(PLOTS, filename), dpi=130)
    plt.close(fig)

bars("dyn_write.png", "Costo medio de insert/erase", "write_mean_ns", "ns por operación de escritura")
bars("dyn_write_p99.png", "Cola de la latencia de escritura: p99", "write_p99_ns", "ns (p99)")
bars("dyn_write_max.png", "Pausa máxima de una escritura (reconstrucciones)", "write_max_ns", "ns (máximo)")
bars("dyn_read.png", "Latencia de lookup al terminar la carga", "read_ns", "ns por lookup")
bars("dyn_space.png", "Espacio del índice (sin las llaves)", "bytes", "bytes", workloads=["insert_random", "delete_random"])
bars("dyn_ram.png", "RAM total pedida al heap", "ram_bytes", "bytes", workloads=["insert_random", "delete_random"])

fig, ax = plt.subplots(figsize=(7, 5))
for config in configs:
    selected_rows = rows_for("insert_random", config)
    write_times = column_values(selected_rows, "write_mean_ns")
    read_times = column_values(selected_rows, "read_ns")
    ax.scatter(write_times, read_times, s=18, color=colors[config], label=config, alpha=0.8)
ax.set_xscale("log")
ax.set_xlabel("ns por insert (media)")
ax.set_ylabel("ns por lookup")
ax.set_title("insert_random: escritura contra lectura, un punto por dataset")
ax.legend(fontsize=7)
ax.grid(alpha=0.3)
fig.tight_layout()
fig.savefig(os.path.join(PLOTS, "dyn_tradeoff.png"), dpi=130)
plt.close(fig)

fig, axes = plt.subplots(1, 2, figsize=(12, 4.2))
for config in configs:
    selected_rows = rows_for("insert_random", config)
    if config.startswith("logpgm"):
        moved_per_insert = []
        for row in selected_rows:
            moved_per_insert.append(float(row["moved_keys"]) / max(1, int(row["ops"])))
        axes[0].plot(range(len(selected_rows)), moved_per_insert, "o-", color=colors[config], label=config)
    if config.startswith("leafpgm"):
        retrains_per_insert = []
        for row in selected_rows:
            retrains_per_insert.append(float(row["retrains"]) / max(1, int(row["ops"])))
        axes[1].plot(range(len(selected_rows)), retrains_per_insert, "o-", color=colors[config], label=config)

maintenance_titles = ["LogarithmicPGM: llaves movidas por insert (amortizado)", "LeafBasedPGM: reentrenamientos por insert"]
maintenance_labels = ["llaves / insert", "reentrenamientos / insert"]
for index, ax in enumerate(axes):
    ax.set_xticks(range(len(datasets)))
    ax.set_xticklabels(datasets, rotation=70, fontsize=7)
    ax.set_title(maintenance_titles[index], fontsize=10)
    ax.set_ylabel(maintenance_labels[index])
    ax.set_yscale("log")
    ax.legend(fontsize=7)
    ax.grid(alpha=0.3)
fig.tight_layout()
fig.savefig(os.path.join(PLOTS, "dyn_maintenance.png"), dpi=130)
plt.close(fig)

out = ["# Variantes dinámicas del PGM-index", ""]
out.append(
    f"Resultados de `make bench-dyn` sobre {len(datasets)} datasets: {', '.join(datasets)}. "
    "Cada celda es la media geométrica sobre los datasets (los números por dataset están en `results/results_dyn.csv`). "
    "Explicación de las estructuras y de las columnas en `docs/DINAMICO.md`."
)
out.append("")
for workload in WORKLOADS:
    out.append(f"## {workload}")
    out.append("")
    out.append("| estructura | insert/erase medio (ns) | p99 (ns) | máx (ns) | lookup final (ns) | lookup en la mezcla (ns) | bytes del índice | RAM (bytes) | segmentos | niveles |")
    out.append("|---|---|---|---|---|---|---|---|---|---|")
    for config in configs:
        selected_rows = rows_for(workload, config)
        write_mean = gmean(selected_rows, "write_mean_ns")
        write_p99 = gmean(selected_rows, "write_p99_ns")
        write_max = gmean(selected_rows, "write_max_ns")
        read_time = gmean(selected_rows, "read_ns")
        mixed_read_time = gmean(selected_rows, "mixed_read_ns")
        index_bytes = gmean(selected_rows, "bytes")
        ram_bytes = gmean(selected_rows, "ram_bytes")
        segments = gmean(selected_rows, "segments")
        levels = gmean(selected_rows, "levels")
        out.append(
            f"| {config} | {write_mean:.0f} | {write_p99:.0f} | {write_max:.0f} | "
            f"{read_time:.0f} | {mixed_read_time:.0f} | {index_bytes:.0f} | "
            f"{ram_bytes:.0f} | {segments:.0f} | {levels:.1f} |"
        )
    out.append("")
    if workload == WORKLOADS[0]:
        out.append(f"![{workload}](plots/dyn_write.png)")
    else:
        out.append("")
    out.append("")

out.append("## Reconstruir el PGM estático en vez de actualizar")
out.append("")
out.append(
    "`static_rebuild` es el tiempo de construir `PGMIndex` (ε = 64) sobre las llaves finales. "
    "La columna es cuántos inserts de cada estructura dinámica cuestan lo mismo que UNA reconstrucción: "
    "si se reconstruye el estático cada menos de esa cantidad de inserts, la estructura dinámica sale ganando."
)
out.append("")
out.append("| carga | reconstrucción (ms) | " + " | ".join(configs) + " |")
out.append("|---|---|" + "---|" * len(configs))
for workload in WORKLOADS:
    rebuild_times = []
    for dataset in datasets:
        group = (dataset, workload)
        if group in static_rows:
            rebuild_times.append(float(static_rows[group]["build_ms"]))
    if not rebuild_times:
        continue
    logarithms = []
    for rebuild_time in rebuild_times:
        logarithms.append(math.log(rebuild_time))
    rebuild_mean = math.exp(sum(logarithms) / len(rebuild_times))
    cells = []
    for config in configs:
        write_mean = gmean(rows_for(workload, config), "write_mean_ns")
        if write_mean:
            cells.append(f"{rebuild_mean * 1e6 / write_mean:,.0f}")
        else:
            cells.append("-")
    out.append(f"| {workload} | {rebuild_mean:.1f} | " + " | ".join(cells) + " |")
out.append("")
out.append("## Gráficas")
out.append("")
figures = [
    ("dyn_write.png", "Costo medio de escritura"),
    ("dyn_write_p99.png", "p99 de escritura"),
    ("dyn_write_max.png", "Pausa máxima"),
    ("dyn_read.png", "Latencia de lookup"),
    ("dyn_tradeoff.png", "Escritura contra lectura"),
    ("dyn_space.png", "Espacio del índice"),
    ("dyn_ram.png", "RAM total"),
    ("dyn_maintenance.png", "Mantenimiento"),
]
for filename, title in figures:
    out.append(f"### {title}")
    out.append("")
    out.append(f"![{title}](plots/{filename})")
    out.append("")

report_path = os.path.join(RESULTS, "REPORT_DYN.md")
with open(report_path, "w") as report_file:
    report_file.write("\n".join(out).replace("\n\n\n", "\n\n") + "\n")
print(report_path)
