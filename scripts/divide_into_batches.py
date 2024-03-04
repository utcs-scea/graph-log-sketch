import argparse
import random

def divide_file_into_batches(input_file, num_batches, output_file, randomize=False, sort_within_batch=False):
    try:
        with open(input_file, 'r') as f:
            lines = f.readlines()
        
        if randomize:
            random.shuffle(lines)
        
        n = len(lines)
        lines_per_batch = n // num_batches
        if n % num_batches:
            lines_per_batch += 1
        
        batches = [lines[i:i + lines_per_batch] for i in range(0, n, lines_per_batch)]
        
        if sort_within_batch:
            for i in range(len(batches)):
                batches[i] = sorted(batches[i], key=lambda x: int(x.split()[0]))
        
        with open(output_file, 'w') as f:
            for batch in batches:
                for line in batch:
                    f.write(line)
                f.write('\n')
            
        print(f"File divided into {num_batches} batches and saved to {output_file}")
    except FileNotFoundError:
        print("Input file not found.")
    except Exception as e:
        print(f"An error occurred: {e}")

parser = argparse.ArgumentParser(description='Divide a file into batches.')

parser.add_argument('--input',
                    metavar='filename',
                    type=str,
                    help='the file to be divided',
                    required=True)

parser.add_argument('--num_batches',
                    metavar='N',
                    type=int,
                    help='the number of batches',
                    required=True)

parser.add_argument('--output',
                    metavar='output_file',
                    type=str,
                    help='the output file location',
                    required=True)

parser.add_argument('--random',
                    action='store_true',
                    help='randomize the edges before batching')

parser.add_argument('--sort_within_batch',
                    action='store_true',
                    help='sort edges within each batch by source vertex')

args = parser.parse_args()

divide_file_into_batches(args.input, args.num_batches, args.output, args.random, args.sort_within_batch)
