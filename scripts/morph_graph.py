import pandas as pd
import matplotlib.cm as cm
import numpy as np
import matplotlib.pyplot as plt
import sys

plt.rcParams.update({'font.size': 24})

def plot_clustered_stacked(dfall, labels, title="",  H="/", **kwargs):
    """Given a list of dataframes, with identical columns and index, create a clustered stacked bar plot.
labels is a list of the names of the dataframe, used for the legend
title is a string for the title of the plot
H is the hatch used for identification of the different dataframe"""

    n_df = len(dfall)
    n_col = len(dfall[0].columns)
    n_ind = len(dfall[0].index)
    plt.figure(figsize=(24,9))
    axe = plt.subplot(121)

    for df in dfall : # for each data frame
        axe = df.plot(kind="bar",
                      linewidth=0,
                      stacked=True,
                      ax=axe,
                      legend=False,
                      grid=False,
                      **kwargs)  # make bar plots

    h,l = axe.get_legend_handles_labels() # get the handles we want to modify
    for i in range(0, n_df * n_col, n_col): # len(h) = n_col * n_df
        for j, pa in enumerate(h[i:i+n_col]):
            for rect in pa.patches: # for each index
                rect.set_x(rect.get_x() + 1 / float(n_df + 1) * i / float(n_col))
                rect.set_hatch(H * int(i / n_col)) #edited part
                rect.set_width(1 / float(n_df + 1))

    axe.set_xticks((np.arange(0, 2 * n_ind, 2) + 1 / float(n_df + 1)) / 2.)
    axe.set_xticklabels(df.index, rotation = 0)
    axe.spines.right.set_visible(False)
    axe.spines.top.set_visible(False)

    # Add invisible data to add another legend
    n=[]
    for i in range(n_df):
        n.append(axe.bar(0, 0, color="gray", hatch=H * i))

    l1 = axe.legend(h[:n_col], l[:n_col], loc=[1.01, 0.5])
    if labels is not None:
        l2 = plt.legend(n, labels, loc=[1.01, 0.1])
    axe.set(xlabel="Percentage of Graph Ingested", ylabel="Cycles")
    axe.add_artist(l1)
    plt.savefig("output.png")
    return axe

# create fake dataframes
cols = ["Edit", "Algo"]
rows = [25, 37.5, 50, 62.75, 75, 87.5, 100]

inputf = sys.stdin
lines = inputf.readlines()

""" INPUT ORDER: CSR LSR MOR"""

data = []

for x in range(0,3):
  data.append(np.zeros((7,2)))
  for i in range(0,2):
    for j in range(0,8):
      if j == 0:
        continue
      data[x].itemset((j-1,i), float(lines[x* 2 * 8 + j * 2 + i]))

print(data)


csr = pd.DataFrame(data[0],
                   index=rows,
                   columns=cols)
lsr = pd.DataFrame(data[1],
                   index=rows,
                   columns=cols)
mor = pd.DataFrame(data[2],
                   index=rows,
                   columns=cols)

# Then, just call :
plot_clustered_stacked([lsr, mor],["LS_CSR", "MorphGraph"])
