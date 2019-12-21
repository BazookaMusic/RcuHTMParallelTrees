#!/usr/bin/python3
import fileinput
import re

import sys


import matplotlib.pyplot as plt
plt.style.use('seaborn-whitegrid')

import numpy as np

line = "THREADS: 18 I: 33 R: 33 L: 34 MOPS: 14.5833"
expr = re.compile(r'THREADS: ([0-9]+) I: ([0-9]+) R: ([0-9]+) L: ([0-9]+) MOPS: ([0-9]+\.[0-9]+)')
print(re.findall(expr,line))

experiments = {}
prefixes = []
for file in sys.argv[1::]:
    with open(file,"r") as f:

        prefix = file.split("_")[0]
        prefixes.append(prefix)


        expr = re.compile(r'THREADS: ([0-9]+) I: ([0-9]+) R: ([0-9]+) L: ([0-9]+) MOPS: ([0-9]+\.[0-9]+)')
        for line in f:
            if line.startswith("THREADS: "):
                try:
                    threads,I,R,L,MOPS = re.findall(expr,line)[0]

                    if (I,R,L) in experiments:
                        experiments[I,R,L].append((prefix,threads,MOPS))
                    else:
                        experiments[I,R,L] = [(prefix,threads,MOPS)]
                except:
                    print(line)

        

for value in experiments.values():
    # sort by thread count
    sorted(value, key = lambda x: int(x[1]))

from matplotlib.ticker import MaxNLocator

for experiment in experiments.keys():
    print(experiment)
    print(experiments[experiment])

    fig = plt.figure(dpi = 100, figsize=(30,30))
    plt.autoscale(True)

    xs = [[int(x[1]) for x in experiments[experiment] if x[0] == selected_prefix] for selected_prefix in prefixes]
    ys = [[float(x[2]) for x in experiments[experiment] if x[0] == selected_prefix] for selected_prefix in prefixes]

    max_x = max(xs[0])
    max_y = max([max(y) for y in ys])
    min_y = min([min(y) for y in ys])


    plt.yticks(np.arange(0, max_y + 1.0, 0.5),  fontsize = 14)
    plt.xticks(np.arange(1, max_x), fontsize = 14)
    #print(max_x)

    for i in range(len(prefixes)):
        plt.plot(xs[i], ys[i], marker = "x", label = prefixes[i])


    plt.legend(loc="upper right", fontsize = 32)
    
    plt.xlim(1, max_x)
    plt.ylim(min_y, max_y);

    plt.xlabel("Thread count", fontsize = 32)
    plt.ylabel("MOPS", fontsize = 32)

    
    

    fig.suptitle('Throughput in MOPS - I:{} R:{} L:{} - Key-Range: 2E6 Keys in tree:1E6'.format(experiment[0],experiment[1],experiment[2]), fontsize=48)

    fig.savefig("{}-{}-{}.png".format(experiment[0],experiment[1],experiment[2]))