import subprocess
import re
import matplotlib.pyplot as plt
import numpy as np

def run_command(batch_number, ingest_threads, algo_threads):
    command = f"./build/microbench/edit-scalability --algo=nop --algo-threads={algo_threads} --graph=lscsr --ingest-threads={ingest_threads} --num-vertices=65608366 --input-file=/var/local/graphs/inputs/friendster_batched_10.el"
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

def average_cpu_cycles(batch_number, threads, n, func):
    total_cpu_cycles = 0
    for _ in range(n):
        output = run_command(batch_number, threads if func == extract_cpu_cycles_for_ingestion else 1, 
                             1 if func == extract_cpu_cycles_for_ingestion else threads)
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

def plot_data(batch_number, thread_counts, avg_cpu_cycles_data, title, filename):
    plt.figure(figsize=(10, 6))
    width = 0.35
    positions = range(len(thread_counts))
    plt.bar(positions, avg_cpu_cycles_data, width=width, tick_label=[str(tc) for tc in thread_counts])
    plt.title(f"{title} (Batch {batch_number})")
    plt.xlabel('Number of Threads')
    plt.ylabel('Average CPU Cycles')
    plt.xticks(positions, labels=[str(tc) for tc in thread_counts])
    plt.savefig(filename, bbox_inches='tight')
    plt.close()
    print(f"Bar chart saved to '{filename}'")

def main(n, batch_numbers):
    thread_counts = [1, 2, 4, 8, 16]
    for batch_number in batch_numbers:
        avg_cpu_cycles_ingestion = [average_cpu_cycles(batch_number, threads, n, extract_cpu_cycles_for_ingestion) for threads in thread_counts]
        plot_data(batch_number, thread_counts, avg_cpu_cycles_ingestion, 'Edit Scalability (ingest)', f'plots/ingestion_cpu_cycles_batch_{batch_number}.png')
        avg_cpu_cycles_algorithm = [average_cpu_cycles(batch_number, threads, n, extract_cpu_cycles_for_algorithm) for threads in thread_counts]
        plot_data(batch_number, thread_counts, avg_cpu_cycles_algorithm, 'Edit Scalability (algorithm)', f'plots/algorithm_cpu_cycles_batch_{batch_number}.png')

if __name__ == "__main__":
    n = 1
    num_batches = 10
    batch_numbers = np.arange(num_batches)
    main(n, batch_numbers)