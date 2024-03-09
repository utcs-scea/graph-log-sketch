# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2023. University of Texas at Austin. All rights reserved.

import argparse
import sys

def count_lines_in_file(file_name):
    try:
        with open(file_name, 'r') as file:
            line_count = sum(1 for line in file)
        return line_count
    except FileNotFoundError:
        return "File not found."

parser = argparse.ArgumentParser(description='Count the number of lines in a file.')

parser.add_argument('--input',
                    metavar='filename',
                    type=str,
                    help='the file to count lines in',
                    required=True)

parser.add_argument('--num_batches',
                    metavar='N',
                    type=int,
                    help='the number of batches',
                    required=True)

args = parser.parse_args()

num_lines = count_lines_in_file(args.input)
print(f"Number of lines in the file: {num_lines}")
print(f"Number of batches: {args.num_batches}")
