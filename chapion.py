import sys
import os
import numpy as np
import pandas
outputs = os.listdir(os.path.join('.', 'outputs'))
bmks = ['gcc', 'GemsFDTD', 'lbm', 'leslie3d', 'libquantum', 'mcf', 'milc', 'omnetpp']
scores = []
for bmk in bmks:
    bmk_score = {}
    for o in outputs:
        bmk_name = o[:-4].split('_')[-1]
        if bmk_name == bmk:
            f = open('outputs/' + o, 'r')
            tmp = f.read().split('\n')[-4].split(' ')[-1]
            # import ipdb; ipdb.set_trace()
            try:
                bmk_score[o[:-5 - len(bmk)]] = float(tmp)
            except ValueError as e:
                print(o)
                raise e
            f.close()
    scores.append(list(bmk_score.values()))
prefetchers = list(bmk_score.keys())
df = pandas.DataFrame(np.array(scores), index=bmks, columns=prefetchers)
print(df)