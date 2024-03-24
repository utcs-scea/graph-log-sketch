# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2023. University of Texas at Austin. All rights reserved.

import subprocess
import resource
import shlex
import matplotlib.pyplot as plt

def run_command_and_measure_memory(command):
    args = shlex.split(command)

    def preexec_fn():
        resource.setrlimit(resource.RLIMIT_AS, (resource.RLIM_INFINITY, resource.RLIM_INFINITY))

    process = subprocess.Popen(args, preexec_fn=preexec_fn)

    process.wait()

    max_memory_usage = resource.getrusage(resource.RUSAGE_CHILDREN).ru_maxrss

    return max_memory_usage

def plot_memory_usage(commands):
    memory_usages = [run_command_and_measure_memory(cmd) for cmd in commands]
    command_labels = ['morph', 'lscsr']

    plt.figure(figsize=(10, 6))
    plt.bar(command_labels, memory_usages, width=0.35)
    plt.xlabel('Commands')
    plt.ylabel('Maximum Memory Usage (KB)')
    plt.title('Maximum Memory Usage Comparison')
    plt.xticks(rotation=45, ha="right")

    plt.savefig('max_memory_usage_comparison2.png', bbox_inches='tight')
    plt.close()

commands = [
    './build/microbench/edit-scalability --algo=nop --algo-threads=8 --graph=lscsr --ingest-threads=8 --num-vertices=124836180 --input-file=/var/local/graphs/inputs/friendster_batched_10.el',
    './build/microbench/edit-scalability --algo=nop --algo-threads=8 --graph=morph --ingest-threads=8 --num-vertices=124836180 --input-file=/var/local/graphs/inputs/friendster_batched_10.el'
]

plot_memory_usage(commands)
print("Plot saved as 'max_memory_usage_comparison.png'.")
