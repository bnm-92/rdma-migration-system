import matplotlib.pyplot as plt
import csv

access = []
latency = []
nfivep_latency = []

file_name = "test.txt"

with open(file_name,'r') as file:
    csv_file = file.readlines()[1:]
    plots = csv.reader(csv_file, delimiter=',')
    for row in plots:
        access.append(int(row[0]))
        latency.append(float(row[1]))
        nfivep_latency.append(float(row[2]))


# del access[1000:]
# del latency[1000:]
# del nfivep_latency[1000:]

fig, ax1 = plt.subplots()
base = ax1.plot(access, latency, linewidth=0.2, color="blue", label="baseline")
# base = ax1.plot(access, nfivep_latency, linewidth=0.2, color="red", label="95th percentile")

ax1.legend()

plt.xlabel('Request Access bin')
plt.ylabel('Mean Latency (us)')
title = "Local Access to rdma_unordered_container"
plt.title(title)
plt.show()
