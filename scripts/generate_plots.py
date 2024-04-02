# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2023. University of Texas at Austin. All rights reserved.

import subprocess
import re
import matplotlib.pyplot as plt
import numpy as np
import csv
import argparse


def run_benchmark(threads, graph):
   command = f"./build/microbench/edit-scalability --algo=bfs --bfs-src=101 --algo-threads={threads} --graph={graph} --ingest-threads={threads} --num-vertices=124836180 --input-file=/var/local/graphs/friendster_batched_25.txt"

   process = subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.PIPE, shell=True)
   stdout, stderr = process.communicate()

   output = stdout.decode()

   ingestion_pattern = re.compile(r"Benchmark results for Ingestion for Batch (\d+):.*?Duration: (\d+) nanoseconds", re.DOTALL)
   algorithm_pattern = re.compile(r"Benchmark results for Algorithm for Batch (\d+):.*?Duration: (\d+) nanoseconds", re.DOTALL)

   ingestion_durations = {}
   algorithm_durations = {}

   ingestion_matches = re.findall(ingestion_pattern, output)
   algorithm_matches = re.findall(algorithm_pattern, output)

   for batch, duration in ingestion_matches:
       ingestion_durations[int(batch)] = int(duration)

   for batch, duration in algorithm_matches:
       algorithm_durations[int(batch)] = int(duration)

   return ingestion_durations, algorithm_durations

def plot_batch_durations(results, batches, plot_ingestion, plot_algorithm):
    plt.rcParams.update({'font.size': 14, 'legend.fontsize': 12})
    n_graph_types = len(results)
    graph_types = list(results.keys())
    thread_counts = list(results[graph_types[0]].keys())
    width = 0.35
    colors = ['skyblue', 'orange', 'lightgreen', 'purple', 'red', 'yellow']
    thread_spacing = 0.5
    batch_spacing = 1

    fig, ax = plt.subplots(figsize=(14, 8))
    all_positions = []

    current_position = 0
    for batch in batches:
        for thread_count in thread_counts:
            positions_within_group = []
            for i, graph_type in enumerate(graph_types):
                pos = current_position + i * width
                positions_within_group.append(pos)

                if plot_ingestion:
                    ingestion_duration = results[graph_type][thread_count]['ingestion'].get(batch, 0)
                    ax.bar(pos, ingestion_duration, width, color=colors[i*2], label=f'Ingestion {graph_type}' if batch == batches[0] and thread_count == thread_counts[0] else "")

                if plot_algorithm:
                    algorithm_duration = results[graph_type][thread_count]['algorithm'].get(batch, 0)
                    bottom = ingestion_duration if plot_ingestion else 0
                    ax.bar(pos, algorithm_duration, width, bottom=bottom, color=colors[i*2 + 1], label=f'Algorithm {graph_type}' if batch == batches[0] and thread_count == thread_counts[0] else "")

            all_positions.extend(positions_within_group)
            current_position += (n_graph_types * width) + thread_spacing

        current_position += batch_spacing

    tick_positions = [np.mean(all_positions[i:i+n_graph_types]) for i in range(0, len(all_positions), n_graph_types)]

    ax.set_xticks(tick_positions)
    ax.set_xticklabels([f'{thread}' for batch in batches for thread in thread_counts])

    ax.set_xlabel('Number of Threads')
    ax.set_ylabel('Duration (nanoseconds)')
    ax.set_title('Edit Scalability Benchmark')
    ax.legend(loc='upper left', bbox_to_anchor=(1, 1), ncol=1)

    plt.tight_layout()
    plt.savefig('plots/edit_scalability_lccsr.png')

def main():
    parser = argparse.ArgumentParser(description='Run and plot benchmark results based on command line flags.')
    parser.add_argument('--ingest', action='store_true', help='Plot ingestion durations.')
    parser.add_argument('--algo', action='store_true', help='Plot algorithm durations.')

    args = parser.parse_args()

    plot_ingestion = args.ingest
    plot_algorithm = args.algo

    if not (plot_ingestion or plot_algorithm):
        print("No plot flags provided. Use --ingest, --algo, or both.")
        return

    graph_types = ['lscsr', 'lccsr', 'adj']
    thread_counts = [8, 16]
    results = {}

    for graph in graph_types:
        results[graph] = {}
        for threads in thread_counts:
            ingestion_durations, algorithm_durations = run_benchmark(threads, graph)
            results[graph][threads] = {
                'ingestion': ingestion_durations,
                'algorithm': algorithm_durations
            }

    plot_batch_durations(results, [0, 1], plot_ingestion, plot_algorithm)

if __name__ == "__main__":
   main()
