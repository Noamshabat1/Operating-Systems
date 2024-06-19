# OS 24 EX1
import pandas as pd
import matplotlib.pyplot as plt

data = pd.read_csv(r"/cs/usr/no.amshabat1/Desktop/Ex1_OS/output.csv")
data = data.to_numpy()

l1_size = 32768  # 32 KB
l2_size = 262144  # 256 KB
l3_size = 9437184  # 9MB
l4_size = 0.5*(4096/8)*l3_size  # 2304 MB
# linear', 'log', 'symlog', 'logit', 'function', 'functionlog'
plt.plot(data[:, 0], data[:, 1], label="Random access")
plt.plot(data[:, 0], data[:, 2], label="Sequential access")
plt.xscale('log')
plt.yscale('log')
plt.axvline(x=l1_size, label="L1 (32 KiB)", c='r')
plt.axvline(x=l2_size, label="L2 (256 KiB)", c='g')
plt.axvline(x=l3_size, label="L3 (9 MiB)", c='brown')
plt.axvline(x=l4_size, label="Page Table Eviction Threshold (2304 MiB)", c='purple')
plt.legend()
title1 = "Latency as a function of array size"
title2 = "CPU Model Name: Intel (R) Core(TM) i5-8500 CPU @ 3.00GHz"
full_title = title1 + "\n" + title2
plt.title(full_title)
plt.ylabel("Latency (ns log scale)")
plt.xlabel("Bytes allocated (log scale)")
plt.show()
