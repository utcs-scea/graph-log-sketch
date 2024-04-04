# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2023. University of Texas at Austin. All rights reserved.

import os
import re
from pathlib import Path
import pandas as pd

data_dir = Path(os.environ["SCRATCH"]) / "scea" / "graph-log-sketch" / "data"
datasets = os.listdir(data_dir)
print("datasets:", " ".join(datasets))

df = pd.DataFrame(columns=["Dataset", "Algorithm", "Graph", "Threads", "Batch", "Stage", "Duration (ns)", "Max RSS (KB)", "Cache Misses", "Cache References", "Instructions"])


for dataset in datasets:
    print("processing", dataset)
    dataset_dir = data_dir / dataset
    algos = os.listdir(dataset_dir)
    print("> algos:", " ".join(algos))

    for algo in algos:
        algo_dir = dataset_dir / algo
        for file in os.listdir(algo_dir):
            if file.endswith(".out"):
                graph = re.search("g=(lscsr|adj|lccsr)", file).group(1)
                threads = re.search("t=(\d+)", file).group(1)
                print("> > processing", algo_dir / file)
                with open(algo_dir / file) as f:
                    contents = f.read()
                    for m in re.findall("Benchmark results for (Ingestion|Post-ingest|Algorithm) for Batch (\d+):\nDuration: (\d+) nanoseconds\nMax RSS: (\d+) KB\nCache Misses: (\d+)\nCache References: (\d+)\nInstructions: (\d+)", contents, re.MULTILINE):
                        stage, batch, duration, max_rss, cache_misses, cache_references, instructions = m
                        df = df.append(
                            {
                                "Dataset": dataset,
                                "Algorithm": algo,
                                "Graph": graph,
                                "Threads": threads,
                                "Batch": batch,
                                "Stage": stage,
                                "Duration (ns)": duration,
                                "Max RSS (KB)": max_rss,
                                "Cache Misses": cache_misses,
                                "Cache References": cache_references,
                                "Instructions": instructions,
                            },
                            ignore_index=True
                        )

df.to_csv('data.csv', index=False)
