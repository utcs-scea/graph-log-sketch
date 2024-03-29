# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2023. University of Texas at Austin. All rights reserved.

import os
import sys
import random
import numpy as np

# command parameters:
# <num_persons> <num_growers> <num_distributors> <num_wholesellers> <num_customers>
# <wholeseller multiplier> <distributor multiplier> > <grower multiplier> <seed>
#
# 2000000 100 1000 10000 200000 30 50 70 0
#

SaleFile = open("wmd.data.csv", "r")                     # WMD standard data set
SaleOut  = open("commercial.csv", "w")

socialMap = dict()
num_persons = int( sys.argv[1] )
np.random.seed(int( sys.argv[9] ))

########### READ AND PRINT SALES RECORDS ##########
while True:
  line = SaleFile.readline()
  if line == "": break
  if line[0] == '#': continue

  fields = line[:-1].split(',')

# sale record
  if fields[0] == "Sale":
     if fields[6] == "8486": continue                    # discard coffee sales, coffee.py builds coffee subgraph

     P1 = int(fields[1])
     P2 = int(fields[2])

     if P1 in socialMap:                                 # if WMD id assigned a social id
        fields[1] = str( socialMap[P1] )                 # ... get social id
     else:
        T1 = int( num_persons * np.random.random() )     # ... chose a social ids from [1 .. 41,652,230]
        socialMap[P1] = T1                               # ... map WMD id to social id
        fields[1] = str(T1)

     if P2 in socialMap:                                 # if WMD id assigned a social id
        fields[2] = str( socialMap[P2] )                 # ... get social id
     else:
        T2 = int( num_persons * np.random.random() )     # ... chose a social ids from [1 .. 41,652,230]
        socialMap[P2] = T2                               # ... map WMD id to social id
        fields[2] = str(T2)

     line = fields[0] + "," + fields[1] + "," + fields[2] + "," + fields[6] + "," + fields[7] + ",,,"
     SaleOut.write(line + "\n")

####################################################
########### CREATE COFFEE MARKET SUBGRAPH ##########
####################################################

chosenSet = set()
growerSet = set()
distributorSet = set()
wholesellerSet = set()
retailCustomerSet = set()

num_persons = int(sys.argv[1])
num_growers = int(sys.argv[2])
num_distributors = int(sys.argv[3])
num_wholesellers = int(sys.argv[4])
num_retailCustomers = int(sys.argv[5])

random.seed(int( sys.argv[9] ))
np.random.seed(int( sys.argv[9] ))

########### CHOSE PERSONS IN COFFEE SALE SUBGRAPH ##########
for i in range(0, num_growers):
  person = int( num_persons * np.random.random() )
  while person in chosenSet: person = int( num_persons * np.random.random() )
  chosenSet.add(person)
  growerSet.add(person)

for i in range(0, num_distributors):
  person = int( num_persons * np.random.random() )
  while person in chosenSet: person = int( num_persons * np.random.random() )
  chosenSet.add(person)
  distributorSet.add(person)

for i in range(0, num_wholesellers):
  person = int( num_persons * np.random.random() )
  while person in chosenSet: person = int( num_persons * np.random.random() )
  chosenSet.add(person)
  wholesellerSet.add(person)

for i in range(0, num_retailCustomers):
  person = int( num_persons * np.random.random() )
  while person in chosenSet: person = int( num_persons * np.random.random() )
  chosenSet.add(person)
  retailCustomerSet.add(person)

########### CONVERT SETS TO LISTS TO ACCESS BY RANDOM.CHOICE ##########
growers = list(growerSet)
distributors = list(distributorSet)
wholesellers = list(wholesellerSet)

########### INIATIALIZE BUYS AND SELLS AMOUNTS ##########
buys = dict()
sells = dict()

for i in range(0, num_persons):
  buys[i] = 0
  sells[i] = 0

########### CONNECT RETAIL CUSTOMERS ##########
# Retail customers buy only from wholesellers, have 3 to 5 suppliers, and buy between 1 and 5 pounds of coffee
# from each supplier.
for customer in retailCustomerSet:
  num_suppliers = int(3.0 * np.random.random() + 3.0)                  # 3 to 5 suppliers
  suppliers = set()

  for j in range(0, num_suppliers):
    supplier = random.choice(wholesellers)                             # supplier is a wholeseller
    while supplier in suppliers:                                       # buy only once from each supplier
      supplier = random.choice(wholesellers)

    amount = 5.0 * np.random.random() + 1.0                            # 1 to 5 pounds
    sells[supplier] += amount
    SaleOut.write("Sale," + str(supplier) + "," + str(customer) + ",8486,,,," + str(amount) + "\n")

########### CONNECT WHOLESELLERS ##########
# Wholesellers buy from other wholesellers, distributors, and growers. They have 5 to 10 wholeseller suppliers
# from whom they buy 15 to 30 pounds of coffee. They buy arg[6] percent more coffee then they sell split 70/30
# between distributors and growers.
multiplier = 1.0 + float(sys.argv[6]) / 100

for wholeseller in wholesellerSet:
  num_suppliers = 5 + int(5.0 * np.random.random())                    # 5 to 10 wholeseller suppliers
  suppliers = set()

  for j in range(0, num_suppliers):
    supplier = random.choice(wholesellers)                             # suppoer is a wholesellers
    while (supplier == wholeseller) or (supplier in suppliers):        # buy only once from each supplier
      supplier = random.choice(wholesellers)

    amount = 15.0 * np.random.random() + 15.0                          # 15 to 30 pounds
    buys[wholeseller] += amount
    sells[supplier] += amount
    SaleOut.write("Sale," + str(supplier) + "," + str(wholeseller) + ",8486,,,," + str(amount) + "\n")

for wholeseller in wholesellerSet:
  to_buy = (sells[wholeseller] - buys[wholeseller]) * multiplier
  suppliers = set()

  while to_buy > 0:
    if np.random.random() < 0.70:                                      # 70% of suppliers are distributors
       supplier = random.choice(distributors)
       while (supplier == wholeseller) or (supplier in suppliers):     # ... buy only once from each supplier
         supplier = random.choice(distributors)
    else:                                                              # supplier is a grower 30% of the time
       supplier = random.choice(growers)
       while (supplier == wholeseller) or (supplier in suppliers):     # ... buy only once from each supplier
         supplier = random.choice(growers)

    amount = 15.0 * np.random.random() + 15.0                          # 15 to 30 pounds
    amount = min(amount, to_buy)                                       # don't buy more than to_buy
    to_buy -= amount
    sells[supplier] += amount
    SaleOut.write("Sale," + str(supplier) + "," + str(wholeseller) + ",8486,,,," + str(amount) + "\n")

########### CONNECT DISTRIBUTORS ##########
# Distributors buy from other distributors and growers. They have 5 to 10 distributor suppliers from whom they
# buy 150 to 300 pounds of coffee.  They buy arg[7] percent more coffee then they sell from growers.
multiplier = 1.0 + float(sys.argv[7]) / 100

for distributor in distributorSet:
  num_suppliers = 5 + int(5.0 * np.random.random())                    # 5 to 10 distributor suppliers
  suppliers = set()

  for j in range(0, num_suppliers):
    supplier = random.choice(distributors)                             # supplier is a distributors
    while (supplier == distributor) or (supplier in suppliers):        # buy only once from each supplier
      supplier = random.choice(distributors)

    amount = 150.0 * np.random.random() + 150.0                        # 150 to 300 pounds
    buys[distributor] += amount
    sells[supplier] += amount
    SaleOut.write("Sale," + str(supplier) + "," + str(distributor) + ",8486,,,," + str(amount) + "\n")

for distributor in distributorSet:
  to_buy = (sells[distributor] - buys[distributor]) * multiplier
  suppliers = set()

  while to_buy > 0:
    supplier = random.choice(growers)
    while (supplier == distributor) or (supplier in suppliers):        # buy only once from each supplier
      supplier = random.choice(growers)

    amount = 150.0 * np.random.random() + 150.0                        # 150 to 300 pounds
    amount = min(amount, to_buy)                                       # don't buy more than to_buy
    to_buy -= amount
    sells[supplier] += amount
    SaleOut.write("Sale," + str(supplier) + "," + str(distributor) + ",8486,,,," + str(amount) + "\n")

########### CONNECT GROWERS ##########
# Growers buy arg[8] percent more coffee then they sell from themelves (self_edge).
multiplier = 1.0 + float(sys.argv[8]) / 100

for grower in growerSet:
  amount = sells[grower] * multiplier
  SaleOut.write("Sale," + str(grower) + "," + str(grower) + ",8486,,,," + str(amount) + "\n")

SaleOut.close()                           # close sale file
SaleOut = open("commercial.csv", "r")     # reopen sale file to read

lines = SaleOut.readlines()               # read sales records
random.shuffle(lines)                     # cwshuffle sales records

SaleOut.close()                           # close sale file
SaleOut = open("commercial.csv", "w")     # reopen sale file to write

line1 = "#delimiter: ,\n"                 # set schema information
line2 = "#columns:type,person,person,topic,date,lat,lon,amount\n"
line3 = "#types:STRING,UINT,UINT,UINT,USDATE,DOUBLE,DOUBLE,DOUBLE\n"

SaleOut.write(line1)
SaleOut.write(line2)
SaleOut.write(line3)
SaleOut.writelines(lines)
