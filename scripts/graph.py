#!/usr/bin/python3

import argparse
import sys
import matplotlib.pyplot as plt
import numpy as np
import re

parser = argparse.ArgumentParser()
parser.add_argument('-t', action='store', dest="gtitle", help="Graph Title", required=True)
parser.add_argument('-x', action='store', dest="xlabel", help="XAxis Label", required=True)
parser.add_argument('-y', action='store', dest="ylabel", help="YAxis Label", required=True)
parser.add_argument('-g', action='store', dest="groups", help="Group ARows", default=1, type=int)
parser.add_argument('--horizon', action='store_true', dest="horizon")
parser.add_argument('--no-horizon', dest='horizon', action='store_false')
parser.set_defaults(horizon=False)
parser.add_argument('-o', action='store', dest="ouputf", help="Ouput FileN", required=True)
parser.add_argument('-i', action='store', dest="inputf", help="Input FileN", default="")


def graph(gtitle, xlabel, ylabel, inputf, ouputf, groups, horizon):
  fig = plt.figure(figsize=(10,7))
  #ax = fig.add_axes([0,0,1,1])
  xaxis = []
  yaxis = []
  eaxis = []
  for i in range (0,groups):
    yaxis.append([])
    eaxis.append([])

  """
    Input Style:
    group0  group1
    h0      h1      xlabel  e0  e1
  """
  group_label = []
  for line in inputf.readlines():
    broken_line = re.split(r'\s+', line.rstrip())
    print(broken_line)
    if groups > 1 and group_label == []:
      group_label = broken_line
      continue
    for i in range(0,groups) :
      yaxis[i].append(float(broken_line[i]))
      if len(broken_line) > groups + 1 +i:
       eaxis[i].append(float(broken_line[groups + 1 + i]))
      else :
        eaxis[i].append(0)
    xaxis.append(broken_line[groups])

  width_orig = 0.8
  width = width_orig/groups
  if group_label == [] :
    group_label.append("")
  ind = np.arange(len(yaxis[0]))
  print(yaxis)
  print(xaxis)
  print(eaxis)
  for i in range(0, groups):
    pos = ind + (-(groups -1)/2.0 +i)*width
    print(pos)
    if horizon:
      plt.barh(pos, yaxis[i], width, yerr=eaxis[i], label=group_label[i])
    else:
      plt.bar(pos, yaxis[i], width, yerr=eaxis[i], label=group_label[i])

  if horizon:
    plt.yticks(ind, xaxis,fontsize=8)
    plt.title(gtitle)
    plt.xlabel(ylabel)
    plt.ylabel(xlabel)
  else:
    plt.xticks(ind, xaxis,fontsize=8)
    plt.title(gtitle)
    plt.ylabel(ylabel)
    plt.xlabel(xlabel)
  if groups > 1:
    plt.legend(bbox_to_anchor=(1.12,.5),loc='right')
  plt.savefig(ouputf)


if __name__ == "__main__":
  args = parser.parse_args()
  inputf = sys.stdin
  if args.inputf != "" or args.inputf is None:
    inputf = open(args.inputf)
  graph(args.gtitle,args.xlabel,args.ylabel,inputf,args.ouputf,args.groups,args.horizon)
