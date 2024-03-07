# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2023. University of Texas at Austin. All rights reserved.

def count_edges_in_first_batch(file_path):
    total_edges = 0
    batch_ended = False
    with open(file_path, 'r') as file:
        for line in file:
            if line.strip():
                parts = line.strip().split()
                total_edges += len(parts) - 1
            else:
                batch_ended = True
                break
    if not batch_ended:
        print("Note: The file doesn't contain an empty line to denote the end of the first batch.")
    return total_edges

file_path = '/var/local/graphs/inputs/friendster_batched_10.el'
number_of_edges = count_edges_in_first_batch(file_path)
print(f'Total number of edges in the first batch: {number_of_edges}')
