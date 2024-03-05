# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2023. University of Texas at Austin. All rights reserved.

import os
import sys
import random
import numpy as np

# command parameters:
# <csv-dir> <nodes.csv>
#

csv_dir = sys.argv[1]
out_file = sys.argv[2]

CommercialIn  = open(csv_dir + "/commercial.csv", "r")
CyberIn  = open(csv_dir + "/cyber.csv", "r")
SocialIn  = open(csv_dir + "/social.csv", "r")
UsesIn  = open(csv_dir + "/uses.csv", "r")
NodeOut  = open(out_file, "w")

person_ids = set()
device_ids = set()

while True:
  line = CommercialIn.readline()
  if line == "": break
  if line[0] == '#': continue
  fields = line[:-1].split(',')
  person_ids.add(fields[1])
  person_ids.add(fields[2])
while True:
  line = CyberIn.readline()
  if line == "": break
  if line[0] == '#': continue
  fields = line[:-1].split(',')
  device_ids.add(fields[0])
  device_ids.add(fields[1])
while True:
  line = SocialIn.readline()
  if line == "": break
  if line[0] == '#': continue
  fields = line[:-1].split(',')
  person_ids.add(fields[0])
  person_ids.add(fields[1])
while True:
  line = UsesIn.readline()
  if line == "": break
  if line[0] == '#': continue
  fields = line[:-1].split(',')
  person_ids.add(fields[0])
  device_ids.add(fields[1])

for person_id in person_ids:
  NodeOut.write("Person," + person_id + ",,,,,,\n")
for device_id in device_ids:
  NodeOut.write("Device," + device_id + ",,,,,,\n")

NodeOut.close()                           # close sale file
NodeOut = open(out_file, "r")     # reopen sale file to read

lines = NodeOut.readlines()               # read sales records
random.shuffle(lines)                     # cwshuffle sales records

NodeOut.close()                           # close sale file
NodeOut = open(out_file, "w")     # reopen sale file to write

line1 = "#delimiter: ,\n"                 # set schema information
line2 = "#columns:type,person,person,topic,date,lat,lon\n"
line3 = "#types:STRING,UINT,UINT,UINT,USDATE,DOUBLE,DOUBLE\n"

NodeOut.write(line1)
NodeOut.write(line2)
NodeOut.write(line3)
NodeOut.writelines(lines)
