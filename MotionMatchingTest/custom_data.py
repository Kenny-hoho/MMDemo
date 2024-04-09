import json
import math
import numpy as np
import matplotlib.pyplot as plt
import dist_func as dist


def test():
    predict = [262.998, 130.737, -58.0181, -61.3756, -12.6853]
    Points1 = [235.656, 107.317, -100.8839, -101.6222, -34.1765]
    Points2 = [240.462, 107.841, -57.0828, -84.106, -95.4328]

    pre_point1 = dist.euclid_dist(predict, Points1)
    pre_point2 = dist.euclid_dist(predict, Points2)
    print('euclid_predict_turn = {0:6.2f}'.format(pre_point1))
    print('euclid_predict_stop = {0:6.2f}'.format(pre_point2))
    pre_point1_dtw = dist.dtw(predict, Points1)
    pre_point2_dtw = dist.dtw(predict, Points2)
    print('dtw_predict_turn = {0:6.2f}'.format(pre_point1_dtw))
    print('dtw_predict_stop = {0:6.2f}'.format(pre_point2_dtw))
    pre_point1_plus = dist.euclid_dist_plus(predict, Points1)
    pre_point2_plus = dist.euclid_dist_plus(predict, Points2)
    print('plus_predict_turn = {0:6.2f}'.format(pre_point1_plus))
    print('plus_predict_stop = {0:6.2f}'.format(pre_point2_plus))

    plt.figure('euc_dtw')
    plt.plot([-0.66, -0.33, 0.33, 0.66, 1.0], predict, color='red', marker='o')
    plt.plot([-0.66, -0.33, 0.33, 0.66, 1.0], Points1, color='blue', marker='o', linestyle='dashed')
    plt.plot([-0.66, -0.33, 0.33, 0.66, 1.0], Points2, color='grey', marker='o', linestyle='dashed')
    plt.axis([-1.0, 1.5, -500, 500])
    plt.title('Turn&Stop')
    plt.show()


def fundamental_test():
    t = np.arange(100)
    ts1 = 5 * np.sin(2 * np.pi * t * 0.05)  # 0.05Hz sin wave, 1Hz sampling rate, amplitude=5
    ts2 = 3 * np.sin(2 * np.pi * t * 0.02)  # 0.02Hz sin wave, 1Hz sampling rate, amplitude=3
    ts3 = 5 * np.sin(2 * np.pi * t * 0.05 + np.pi)  # 0.05Hz sin wave, 1Hz sampling rate, amplitude=5
    ts4 = 0.08 * t - 4
    ts5 = 5 * np.sin(2 * np.pi * t * 0.05) - 1

    fig, axs = plt.subplots(figsize=(12, 8))
    axs.plot(ts1)
    axs.plot(ts2)
    axs.plot(ts3)
    axs.plot(ts4)
    axs.plot(ts5)
    axs.legend(['ts1: sin,0.05Hz, amplitude=5',
                'ts2: sin,0.02Hz, amplitude=3',
                'ts3: sin,0.05Hz, amplitude=5, phase offset=pi',
                'ts4: line with slope = 0.08'])

    dtw_dist_12 = dist.dtw(ts1, ts2)
    dtw_dist_13 = dist.dtw(ts1, ts3)
    dtw_dist_14 = dist.dtw(ts1, ts4)
    dtw_dist_15 = dist.dtw(ts1, ts5)
    dtw_dist_21 = dist.dtw(ts2, ts1)
    dtw_dist_23 = dist.dtw(ts2, ts3)
    dtw_dist_24 = dist.dtw(ts2, ts4)
    dtw_dist_31 = dist.dtw(ts3, ts1)
    dtw_dist_32 = dist.dtw(ts3, ts2)
    dtw_dist_34 = dist.dtw(ts3, ts4)

    euclid_dist_31 = dist.euclid_dist(ts3, ts1)

    print('------------------ts1---------------------')
    print('dtw_dist_12 = {0:6.2f}'.format(dtw_dist_12))
    print('dtw_dist_13 = {0:6.2f}'.format(dtw_dist_13))
    print('dtw_dist_14 = {0:6.2f}'.format(dtw_dist_14))
    print('dtw_dist_15 = {0:6.2f}'.format(dtw_dist_15))
    print('------------------ts2---------------------')
    print('dtw_dist_21 = {0:6.2f}'.format(dtw_dist_21))
    print('dtw_dist_23 = {0:6.2f}'.format(dtw_dist_23))
    print('dtw_dist_24 = {0:6.2f}'.format(dtw_dist_24))
    print('------------------ts3---------------------')
    print('dtw_dist_31 = {0:6.2f}'.format(dtw_dist_31))
    print('euclid_dist_31 = {0:6.2f}'.format(euclid_dist_31))
    print('dtw_dist_32 = {0:6.2f}'.format(dtw_dist_32))
    print('dtw_dist_34 = {0:6.2f}'.format(dtw_dist_34))

    plt.show()
