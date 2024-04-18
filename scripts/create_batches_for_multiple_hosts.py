import argparse
import random

def distribute_edges(file_path, num_batches, num_hosts):
    with open(file_path, 'r') as file:
        edges = file.readlines()

    random.shuffle(edges)

    total_parts = num_batches * num_hosts
    total_edges = len(edges)
    part_size = total_edges // total_parts

    remainder = total_edges % total_parts

    current_edge = 0
    for i in range(num_batches):
        for j in range(num_hosts):
            this_part_size = part_size + (1 if remainder > 0 else 0)
            remainder -= 1

            part_edges = edges[current_edge:current_edge + this_part_size]
            current_edge += this_part_size

            filename = f"edits_batch{i}_host{j}.el"
            with open(filename, 'w') as outfile:
                outfile.writelines(part_edges)

def main():
    parser = argparse.ArgumentParser(description="Distribute edge list into batches for hosts.")
    parser.add_argument('file_path', type=str, help="The path to the edge list file.")
    parser.add_argument('num_batches', type=int, help="The number of batches.")
    parser.add_argument('num_hosts', type=int, help="The number of hosts.")

    args = parser.parse_args()

    distribute_edges(args.file_path, args.num_batches, args.num_hosts)

if __name__ == "__main__":
    main()
