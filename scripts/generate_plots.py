# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2023. University of Texas at Austin. All rights reserved.

import subprocess
import re
import matplotlib.pyplot as plt
import numpy as np

def run_command(batch_number, ingest_threads, algo_threads, graph_type):
    command = f"./build/microbench/edit-scalability --algo=nop --algo-threads={algo_threads} --graph={graph_type} --ingest-threads={ingest_threads} --num-vertices=124836180 --input-file=/var/local/graphs/inputs/friendster_batched_10.el"
    result = subprocess.run(command, shell=True, capture_output=True, text=True)
    if result.returncode != 0:
        print(f"Command failed with error: {result.stderr}")
        return None
    return result.stdout

def extract_cpu_cycles_for_ingestion(batch_number, output):
    cpu_cycles_match = re.search(rf'Benchmark results for Ingestion for Batch {batch_number}:.*?CPU Cycles: (\d+)', output, re.DOTALL)
    return int(cpu_cycles_match.group(1)) if cpu_cycles_match else None

def extract_cpu_cycles_for_algorithm(batch_number, output):
    cpu_cycles_match = re.search(rf'Benchmark results for Algorithm for Batch {batch_number}:.*?CPU Cycles: (\d+)', output, re.DOTALL)
    return int(cpu_cycles_match.group(1)) if cpu_cycles_match else None

def average_cpu_cycles(batch_number, threads, n, func, graph_type):
    total_cpu_cycles = 0
    for _ in range(n):
        output = run_command(batch_number, threads if func == extract_cpu_cycles_for_ingestion else 1,
                             1 if func == extract_cpu_cycles_for_ingestion else threads, graph_type)
        if output:
            cpu_cycles = func(batch_number, output)
            if cpu_cycles is not None:
                total_cpu_cycles += cpu_cycles
            else:
                print("Failed to extract CPU cycles from command output.")
                return None
        else:
            return None
    return total_cpu_cycles / n

def plot_comparison_data(batch_number, thread_counts, avg_cpu_cycles_data_lscsr, avg_cpu_cycles_data_morph, title, filename):
    plt.figure(figsize=(10, 6))
    width = 0.35
    positions = np.arange(len(thread_counts))

    valid_positions_lscsr, valid_cycles_lscsr = zip(*[(pos, cycle) for pos, cycle in zip(positions, avg_cpu_cycles_data_lscsr) if cycle is not None])
    valid_positions_morph, valid_cycles_morph = zip(*[(pos, cycle) for pos, cycle in zip(positions, avg_cpu_cycles_data_morph) if cycle is not None])

    if valid_cycles_lscsr and valid_cycles_morph:
        plt.bar(np.array(valid_positions_lscsr) - width/2, valid_cycles_lscsr, width=width, label='lscsr')
        plt.bar(np.array(valid_positions_morph) + width/2, valid_cycles_morph, width=width, label='morph')
        plt.title(f"{title} Comparison (Batch {batch_number})")
        plt.xlabel('Number of Threads')
        plt.ylabel('Average CPU Cycles')
        plt.xticks(positions, labels=[str(tc) for tc in thread_counts])
        plt.legend()
        plt.savefig(filename, bbox_inches='tight')
        plt.close()
        print(f"Comparison bar chart saved to '{filename}'")
    else:
        print("No valid data to plot.")


def main(n, batch_numbers):
    thread_counts = [1, 2, 4, 8]
    for batch_number in batch_numbers:
        avg_cpu_cycles_ingestion_lscsr = [average_cpu_cycles(batch_number, threads, n, extract_cpu_cycles_for_ingestion, 'lscsr') for threads in thread_counts]
        avg_cpu_cycles_ingestion_morph = [average_cpu_cycles(batch_number, threads, n, extract_cpu_cycles_for_ingestion, 'morph') for threads in thread_counts]
        plot_comparison_data(batch_number, thread_counts, avg_cpu_cycles_ingestion_lscsr, avg_cpu_cycles_ingestion_morph, 'Edit Scalability (ingest)', f'comparison_plots/ingestion_cpu_cycles_batch_{batch_number}.png')
        avg_cpu_cycles_algorithm_lscsr = [average_cpu_cycles(batch_number, threads, n, extract_cpu_cycles_for_algorithm, 'lscsr') for threads in thread_counts]
        avg_cpu_cycles_algorithm_morph = [average_cpu_cycles(batch_number, threads, n, extract_cpu_cycles_for_algorithm, 'morph') for threads in thread_counts]
        plot_comparison_data(batch_number, thread_counts, avg_cpu_cycles_algorithm_lscsr, avg_cpu_cycles_algorithm_morph, 'Edit Scalability (algorithm)', f'comparison_plots/algorithm_cpu_cycles_batch_{batch_number}.png')

if __name__ == "__main__":
    n = 1
    num_batches = 10
    batch_numbers = np.arange(num_batches)
    main(n, batch_numbers)
