# -*- coding: utf-8 -*-

import numpy as np
import matplotlib.pyplot as plt


def distance(t1, t2):
    return np.sqrt((t1-t2)**2)


def euclid_dist(t1, t2):
    dist = 0
    for i in range(len(t1)):
        dist += distance(t1[i], t2[i])
    return dist


def dtw(s1, s2):
    dtw = {}

    for i in range(len(s1)):
        dtw[(i, -1)] = float('inf')
    for i in range(len(s2)):
        dtw[(-1, i)] = float('inf')
    dtw[(-1, -1)] = 0

    for i in range(len(s1)):
        for j in range(len(s2)):
            dist = distance(s1[i], s2[j])
            dtw[(i, j)] = dist + min(dtw[(i - 1, j)], dtw[(i, j - 1)], dtw[(i - 1, j - 1)])

    return dtw[len(s1) - 1, len(s2) - 1]


def fundamental_test():
    t = np.arange(100)
    ts1 = 5 * np.sin(2 * np.pi * t * 0.05)  # 0.05Hz sin wave, 1Hz sampling rate, amplitude=5
    ts2 = 3 * np.sin(2 * np.pi * t * 0.02)  # 0.02Hz sin wave, 1Hz sampling rate, amplitude=3
    ts3 = 5 * np.sin(2 * np.pi * t * 0.05 + np.pi)  # 0.05Hz sin wave, 1Hz sampling rate, amplitude=5
    ts4 = 0.08 * t - 4

    fig, axs = plt.subplots(figsize=(12, 8))
    axs.plot(ts1)
    axs.plot(ts2)
    axs.plot(ts3)
    axs.plot(ts4)
    axs.legend(['ts1: sin,0.05Hz, amplitude=5',
                'ts2: sin,0.02Hz, amplitude=3',
                'ts3: sin,0.05Hz, amplitude=5, phase offset=pi',
                'ts4: line with slope = 0.08'])

    dtw_dist_12 = dtw(ts1, ts2)
    dtw_dist_13 = dtw(ts1, ts3)
    dtw_dist_14 = dtw(ts1, ts4)
    dtw_dist_21 = dtw(ts2, ts1)
    dtw_dist_23 = dtw(ts2, ts3)
    dtw_dist_24 = dtw(ts2, ts4)
    dtw_dist_31 = dtw(ts3, ts1)
    dtw_dist_32 = dtw(ts3, ts2)
    dtw_dist_34 = dtw(ts3, ts4)

    print('------------------ts1---------------------')
    print('dtw_dist_12 = {0:6.2f}'.format(dtw_dist_12))
    print('dtw_dist_13 = {0:6.2f}'.format(dtw_dist_13))
    print('dtw_dist_14 = {0:6.2f}'.format(dtw_dist_14))
    print('------------------ts2---------------------')
    print('dtw_dist_21 = {0:6.2f}'.format(dtw_dist_21))
    print('dtw_dist_23 = {0:6.2f}'.format(dtw_dist_23))
    print('dtw_dist_24 = {0:6.2f}'.format(dtw_dist_24))
    print('------------------ts3---------------------')
    print('dtw_dist_31 = {0:6.2f}'.format(dtw_dist_31))
    print('dtw_dist_32 = {0:6.2f}'.format(dtw_dist_32))
    print('dtw_dist_34 = {0:6.2f}'.format(dtw_dist_34))

    plt.show()


if __name__ == '__main__':
    fundamental_test()





