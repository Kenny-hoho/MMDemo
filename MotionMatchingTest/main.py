# -*- coding: utf-8 -*-
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


def get_trajectory_from_json(filename, out_array):
    with open(filename, 'r') as f:
        json_dict = json.load(f)
        for traj in json_dict['Trajectory']:
            trajectoryPoints = []
            for point in traj['TrajectoryPoints']:
                trajectoryPoints.append(point['Position']['X'])
            out_array.append(trajectoryPoints)
    return json_dict


def trajectory_test():
    # --------------------------------strafe turn--------------------------------
    strafeTurn_jsonFileName = '../UE_Demo/UE_4.27_demo/Plugins/JsonData/StrafeTurn_AnimTraj.json'
    strafeTurnTraj = []
    strafeTurn_animTraj = get_trajectory_from_json(strafeTurn_jsonFileName, strafeTurnTraj)
    print(strafeTurn_animTraj)

    plt.figure('Strafe Turn')
    column = int(strafeTurn_animTraj['PoseCount'])
    row = math.ceil(column/4)
    trajSampleTime = [-0.66, -0.33, 0.33, 0.66, 1.0]

    for i in range(1, column):
        plt.subplot(row, 4, i)
        plt.plot(trajSampleTime, strafeTurnTraj[i], marker='o')
        plt.axis([-1.0, 1.5, -500, 500])
    # --------------------------------strafe stop--------------------------------
    strafeStop_jsonFileName = '../UE_Demo/UE_4.27_demo/Plugins/JsonData/StrafeStop_AnimTraj.json'
    strafeStopTraj = []
    strafeStop_animTraj = get_trajectory_from_json(strafeStop_jsonFileName, strafeStopTraj)
    print(strafeStop_animTraj)

    plt.figure('Strafe Stop')
    column = int(strafeStop_animTraj['PoseCount'])
    row = math.ceil(column/4)
    trajSampleTime = [-0.66, -0.33, 0.33, 0.66, 1.0]

    for i in range(1, column):
        plt.subplot(row, 4, i)
        plt.plot(trajSampleTime, strafeStopTraj[i], marker='o')
        plt.axis([-1.0, 1.5, -500, 500])
    # --------------------------------strafe predict--------------------------------
    predict_jsonFileName = '../UE_Demo/UE_4.27_demo/Plugins/JsonData/PredictTrajectory.json'
    predictTraj = []
    predict_animTraj = get_trajectory_from_json(predict_jsonFileName, predictTraj)
    print(predict_animTraj)

    plt.figure('Strafe Predict')
    column = int(predict_animTraj['PoseCount'])
    print(column)
    row = math.ceil(column/4)
    trajSampleTime = [-0.66, -0.33, 0.33, 0.66, 1.0]

    for i in range(1, column):
        plt.subplot(row, 4, i)
        plt.plot(trajSampleTime, predictTraj[i], marker='o')
        plt.axis([-1.0, 1.5, -500, 500])

    plt.show()


if __name__ == '__main__':
    # fundamental_test()
    trajectory_test()
