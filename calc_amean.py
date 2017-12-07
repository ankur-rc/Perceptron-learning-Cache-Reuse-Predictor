import matplotlib.pyplot as plt
import numpy as np
plt.rcdefaults()

with open('results.log') as f:
    data = f.read()
    lines = data.split('\n')

lines = lines[0:len(lines) - 1]
lru_mpki = {}
crc_mpki = {}
lru_mpki_sum = 0.0
crc_mpki_sum = 0.0

for line in lines:
    info = line.split()

    benchmark = info[0]
    mpki_lru = float(info[1])
    mpki_crc = float(info[2])

    lru_mpki[benchmark] = mpki_lru
    crc_mpki[benchmark] = mpki_crc
    lru_mpki_sum += mpki_lru
    crc_mpki_sum += mpki_crc

# calculate amean
amean_lru = lru_mpki_sum / len(lines)
amean_crc = crc_mpki_sum / len(lines)
print 'Arithmetic mean MPKI: LRU= ' + str(amean_lru) + "\tCRC= " + str(amean_crc)

keys = []
values_lru = []
values_crc = []

for key in sorted(crc_mpki, key=crc_mpki.get):
    values_crc.append(crc_mpki[key])
    values_lru.append(lru_mpki[key])
    keys.append(key)

min_crc = min(values_crc)
min_lru = min(values_lru)

min_y = min([min_crc,min_lru])

values_lru.append(0.0)
values_lru.append(amean_lru)

values_crc.append(0.0)
values_crc.append(amean_crc)

keys.append("")
keys.append("Aritmetic Mean")

# plt.bar(range(len(values_crc)), values_crc, align='center')
# plt.bar(range(len(values_lru)), values_lru, align='center')
# plt.xticks(range(len(keys)), keys, rotation=90)

fig, ax = plt.subplots()

ind = np.arange(len(keys))    # the x locations for the groups
width = 0.35         # the width of the bars
p1 = ax.bar(ind, values_crc, width)
p2 = ax.bar(ind + width, values_lru, width)

ax.set_title('Misses per 1000 instructions')
ax.set_xticks(ind + width / 2)
ax.set_xticklabels(keys)

ax.legend((p1[0], p2[0]), ('Perceptron', 'LRU'))
ax.autoscale_view()

min, max = plt.ylim()
plt.ylim(min_y - 0.1, max)
plt.grid(True, axis='y')
plt.xticks(rotation=90)
plt.show()
