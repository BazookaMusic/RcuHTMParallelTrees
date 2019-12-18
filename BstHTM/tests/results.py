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
    fig = plt.figure(dpi = 100, figsize=(30,30))
    plt.autoscale(False)



    x = [float(x[0]) for i,x in enumerate(experiments[experiment])]
    y = [float(x[1]) for i,x in enumerate(experiments[experiment])]

    x_ideal = np.arange(1,max(x) + 1)
    y_ideal = [i*min(y) for i in range(1, int(max(x)) + 1)]

    plt.yticks(np.arange(0, int(max(x)), 0.5),  fontsize = 14)
    plt.xticks(np.arange(1, max(x)), fontsize = 14)
    print(max(x))
    
    plt.plot(x, y, marker = "x", label = "rcu htm")
    plt.plot(x_ideal, y_ideal, marker = "o", label = "ideal")

    plt.legend(loc="upper right", fontsize = 32)
    
    plt.xlim(1, max(x))
    plt.ylim(min(y), max(y_ideal));

    plt.xlabel("Thread count", fontsize = 32)
    plt.ylabel("MOPS", fontsize = 32)

    
    
    
    

    fig.suptitle('Throughput in MOPS - I:{} R:{} L:{} - Key-Range: 2E6 Keys in tree:1E6'.format(experiment[0],experiment[1],experiment[2]), fontsize=48)

    fig.savefig("{}-{}-{}.png".format(experiment[0],experiment[1],experiment[2]))