#!/usr/bin/env python3
import subprocess
import sys


n_threads = [1,2,4,7,14,20,24,28]
sizes = [1000000, 10000, 1000]
sizes.reverse()
tests = [(100,0,0),(80,10,10),(0,50,50)]

file =sys.argv[1]

seeds = [1996,1453,1821]


with open(file + "_big_log.log", "w") as f:
    for sz in sizes:
        print("Start of tests for tree size: {}".format(sz))
        print("Start of tests for tree size: {}".format(sz), file=f)
        for test in tests:
            for t in n_threads:
                

                l,i,r = test
                print("Testing T,I,R,L: ", (t,i,r,l))

                results = []

                max_diff = 0

                for j,seed in enumerate(seeds):
                    min_val = 1000
                    max_val = 0
                    for iind in range(3):
                        print("  {}%  ".format(int(((j*3) + iind)/(3*len(seeds)) * 100)),end='\r')
                        try:
                            res = subprocess.check_output([file,"[tp]",str(sz),str(t),str(l),str(i),str(seed)])
                            for line in res.split(b'\n'):
                                if b'MOPS: ' in line:
                                    mops = float(line.split(b'MOPS: ')[1])
                                    if mops < min_val:
                                        min_val = mops
                                    if mops > max_val:
                                        max_val = mops
                                    
                                    results.append(mops)
                        
                        except subprocess.CalledProcessError as e:
                            print(e.output)
                        
                        max_diff = max(max_diff,max_val - min_val)
                print("100%")
                mean = sum(results)/len(results)
                print("THREADS: {} I: {} R: {} L: {} MOPS: {}".format(t,i,r,l,mean))
                print("Max diff: {} MOPS".format(max_diff))
                print("THREADS: {} I: {} R: {} L: {} MOPS: {}".format(t,i,r,l,mean), file=f)
                print("Max diff: {} MOPS".format(max_diff), file=f)


