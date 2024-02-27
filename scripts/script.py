import subprocess
import re
import matplotlib.pyplot as plt

def run_command(ingest_threads, algo_threads):
    command = f"./build/edit-scalability --algo=nop --algo_threads={algo_threads} --graph,g=lscsr --ingest_threads={ingest_threads} --num-vertices=4 ./build/sample.txt"
    result = subprocess.run(command, shell=True, capture_output=True, text=True)
    if result.returncode != 0:
        print(f"Command failed with error: {result.stderr}")
        return None
    return result.stdout

def extract_cpu_cycles_for_ingestion(output):
    cpu_cycles_match = re.search(r'Benchmark results for Ingestion for Batch 0:.*?CPU Cycles: (\d+)', output, re.DOTALL)
    return int(cpu_cycles_match.group(1)) if cpu_cycles_match else None

def extract_cpu_cycles_for_algorithm(output):
    cpu_cycles_match = re.search(r'Benchmark results for Algorithm for Batch 0:.*?CPU Cycles: (\d+)', output, re.DOTALL)
    return int(cpu_cycles_match.group(1)) if cpu_cycles_match else None

def average_cpu_cycles(threads, n, func):
    total_cpu_cycles = 0
    for _ in range(n):
        output = run_command(threads if func == extract_cpu_cycles_for_ingestion else 1, 
                             1 if func == extract_cpu_cycles_for_ingestion else threads)
        if output:
            cpu_cycles = func(output)
            if cpu_cycles is not None:
                total_cpu_cycles += cpu_cycles
            else:
                print("Failed to extract CPU cycles from command output.")
                return None
        else:
            return None
    return total_cpu_cycles / n

def plot_data(thread_counts, avg_cpu_cycles_data, title, filename):
    plt.figure(figsize=(10, 6))
    width = 0.35
    positions = range(len(thread_counts))
    plt.bar(positions, avg_cpu_cycles_data, width=width, tick_label=[str(tc) for tc in thread_counts])
    plt.title(title)
    plt.xlabel('Number of Threads')
    plt.ylabel('Average CPU Cycles')
    plt.xticks(positions, labels=[str(tc) for tc in thread_counts])
    plt.savefig(filename, bbox_inches='tight')
    print(f"Bar chart saved to '{filename}'")

def main(n):
    thread_counts = [1, 2, 4, 8, 16]
    avg_cpu_cycles_ingestion = [average_cpu_cycles(threads, n, extract_cpu_cycles_for_ingestion) for threads in thread_counts]
    plot_data(thread_counts, avg_cpu_cycles_ingestion, 'Edit Scalability (ingest)', 'plots/ingestion_cpu_cycles.png')
    avg_cpu_cycles_algorithm = [average_cpu_cycles(threads, n, extract_cpu_cycles_for_algorithm) for threads in thread_counts]
    plot_data(thread_counts, avg_cpu_cycles_algorithm, 'Edit Scalability (algorithm)', 'plots/algorithm_cpu_cycles.png')

if __name__ == "__main__":
    n = 5
    main(n)