import json
import math
import numpy as np
import matplotlib.pyplot as plt


def distance(t1, t2):
    return np.sqrt((t1 - t2) ** 2)


def euclid_dist(t1, t2):
    dist = 0
    for i in range(len(t1)):
        dist += distance(t1[i], t2[i])
    return dist


def euclid_dist_plus(t1, t2):
    dist = 0
    for i in range(len(t1)):
        dist += distance(t1[i], t2[i])
    average_dist = dist / len(t1)
    res = 0
    for i in range(len(t1)):
        res += pow((t1[i]-average_dist), 2)
    return pow(res, 1/2)


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