import re
import numpy as np
import pandas as pd
import random
import glob
import sys

files = []
base_dir = sys.argv[1]

for filename in glob.iglob(base_dir + '/**/*.integer', recursive=True):
    files.append({"values": filename, "bitmap": filename.replace('.integer', '.bitmap')})

stats = []
for column in files:
    print("reading " + column['values'])
    try:
        data = np.fromfile(column['values'], dtype=np.int32)
        df = pd.DataFrame({"data": data})
        bitmap = np.fromfile(column['bitmap'], dtype=np.uint8)
        null_indices = np.where(bitmap == 0)
        if (null_indices[0].size):
            for null_i in null_indices:
                df['data'][null_i] = np.nan

        df = df['data']

        col_id = column['values'].split('/')[-2] + '/' + column['values'].split('/')[-1]

        # here we add our special stats

        custom_stats = {}
        # blocks sample [X...NXXX..]
        blocks_count = 3
        block_length = min(df.size,20)
        blocks = []

        for block_i in range(blocks_count):
            blocks.append('')
            # pick a random start number
            start_index = random.randint(1, df.size - block_length)
            for cell_i in range(start_index, start_index + block_length):
                if (df.isnull()[cell_i]):
                    blocks[block_i] += 'N'
                elif (df[cell_i] == df[cell_i - 1]):
                    blocks[block_i] += '.'
                else:
                    blocks[block_i] += 'x'

            custom_stats['block_' + str(block_i + 1)] = blocks[block_i]
            custom_stats['random_' + str(block_i + 1)] = df[start_index]

            # data blocks
            datablock = df[start_index:start_index + 65000]
            custom_stats['db_' + str(block_i + 1) + '_unique'] = datablock.nunique()
            custom_stats['db_' + str(block_i + 1) + '_null'] = datablock.isnull().sum()
            custom_stats['db_' + str(block_i + 1) + '_min'] = datablock.min()
            custom_stats['db_' + str(block_i + 1) + '_max'] = datablock.max()

        # wie haeufig sind die haeufigste Werte
        value_counts = df.value_counts()
        most_frequent = value_counts.head(3)
        i = 1
        for v, f in most_frequent.items():
            custom_stats['top_' + str(i) + '_value'] = v
            custom_stats['top_' + str(i) + '_percent'] = "{:2.1f}".format(f * 100.0 / df.size)
            i += 1

        custom_stats['unique'] = value_counts.size

        stats.append(
            {'name': col_id, 'min': df.min(), 'max': df.max(), 'null_count': df.isnull().sum(), 'size': df.size,
             'zero_count': (df == 0).sum(), 'path': column['values'], **custom_stats})

        df_stats = pd.DataFrame(stats)
        csv_file = open(sys.argv[2], 'w')
        df_stats.to_csv(csv_file)
        csv_file.flush()
        csv_file.close()

    except Exception as e:
        print("error at " + column['values'])
        print(e)
