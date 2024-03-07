# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2023. University of Texas at Austin. All rights reserved.

def count_lines_until_break(file_path):
    count = 0
    with open(file_path, 'r') as file:
        for line in file:
            if line.strip():
                count += 1
            else:
                break
    return count

file_path = '/var/local/graphs/inputs/friendster_batched_10.el'
number_of_lines = count_lines_until_break(file_path)
print(f'Number of lines until the first line break: {number_of_lines}')
