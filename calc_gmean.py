import matplotlib.pyplot as plt
import numpy as np
plt.rcdefaults()

# get the data into lines array
with open('results.log') as f:
    data = f.read()
    lines = data.split('\n')

# calculate product of lru_ipc/crc_ipc
prod = 1.0
lines = lines[0:len(lines) - 1]
speedup = {}


for line in lines:
    info = line.split()

    benchmark = info[0]
    lru_ipc = float(info[3])
    crc_ipc = float(info[4])

    current_speedup = crc_ipc / lru_ipc

    speedup[benchmark] = float(current_speedup)

    prod = prod * current_speedup

# calculate gmean
gmean = prod**(1.0 / len(lines))
print 'Geometric mean IPC = ' + str(gmean) + "\nSpeedup = " + str((gmean - 1.0) * 100) + "%"

keys = []
values = []

for key in sorted(speedup, key=speedup.get):
    values.append(speedup[key])
    keys.append(key)

min_y = min(values)

values.append(0.0)
values.append(gmean)

keys.append("")
keys.append("Geometric Mean")

plt.bar(range(len(values)), values, align='center')
plt.xticks(range(len(keys)), keys, rotation=90)

min, max = plt.ylim()
plt.ylim(min_y - 0.1, max)
plt.grid(True, axis='y')
plt.show()
