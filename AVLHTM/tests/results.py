#!/usr/bin/python3
import fileinput
import re


import matplotlib.pyplot as plt
plt.style.use('seaborn-whitegrid')

import numpy as np

line = "THREADS: 18 I: 33 R: 33 L: 34 MOPS: 14.5833"
expr = re.compile(r'THREADS: ([0-9]+) I: ([0-9]+) R: ([0-9]+) L: ([0-9]+) MOPS: ([0-9]+\.[0-9]+)')
print(re.findall(expr,line))

experiments = {}

for line in fileinput.input():
    if line.startswith("THREADS: "):
        expr = re.compile(r'THREADS: ([0-9]+) I: ([0-9]+) R: ([0-9]+) L: ([0-9]+) MOPS: ([0-9]+\.[0-9]+)')
        try:
            threads,I,R,L,MOPS = re.findall(expr,line)[0]

            if (I,R,L) in experiments:
                experiments[(I,R,L)].append((threads,MOPS))
            else:
                experiments[(I,R,L)] = [(threads,MOPS)]
        except:
            print(line)

        

for value in experiments.values():
    # sort by thread count
    sorted(value, key = lambda x: x[0])

from matplotlib.ticker import MaxNLocator

for experiment in experiments.keys():
    print(experiment)
    print(experiments[experiment])
    fig = plt.figure(dpi = 100, figsize=(13,8))
    plt.autoscale(False)
    plt.yticks(np.arange(0, 21, 0.5))
    plt.xticks(np.arange(1, 21))



    x = [float(x[0]) for x in experiments[experiment]]
    y = [float(x[1]) for x in experiments[experiment]]
    
    plt.plot(x, y)
    
    plt.xlim(1, max(x))
    plt.ylim(min(y), 2*max(x));
    plt.xlabel("Thread count")
    plt.ylabel("MOPS")

    
    
    
    

    fig.suptitle('Throughput in MOPS - I:{} R:{} L:{} - Key-Range: 2E6 Keys in tree:1E6'.format(experiment[0],experiment[1],experiment[2]), fontsize=16)

    fig.savefig("{}-{}-{}.png".format(experiment[0],experiment[1],experiment[2]))