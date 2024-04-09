# -*- coding: utf-8 -*-
import json
import math
import numpy as np
import matplotlib.pyplot as plt
import custom_data
import dist_func as dist


def get_trajectory_from_json(filename, out_array):
    with open(filename, 'r') as f:
        json_dict = json.load(f)
        for traj in json_dict['Trajectory']:
            trajectoryPoints = []
            for point in traj['TrajectoryPoints']:
                trajectoryPoints.append(point['Position']['X'])
            out_array.append(trajectoryPoints)
    return json_dict


def show_trajectory(fig_name, traj_dict, traj_array):
    plt.figure(fig_name)
    # poseCount = int(traj_dict['PoseCount'])
    poseCount = 16
    row = math.ceil(poseCount / 4)
    trajSampleTime = [-0.66, -0.33, 0.33, 0.66, 1.0]

    for i in range(1, poseCount):
        plt.subplot(row, 4, i)
        plt.plot(trajSampleTime, traj_array[i-1], marker='o')
        plt.title(str(i-1))
        plt.axis([-1.0, 1.5, -500, 500])


def trajectory_test():
    # --------------------------------strafe turn--------------------------------
    strafeTurn_jsonFileName = '../UE_Demo/UE_4.27_demo/Plugins/JsonData/StrafeTurn_AnimTraj.json'
    strafeTurnTraj_array = []
    strafeTurnTraj_dict = get_trajectory_from_json(strafeTurn_jsonFileName, strafeTurnTraj_array)
    print(strafeTurnTraj_dict)
    # --------------------------------strafe stop--------------------------------
    strafeStop_jsonFileName = '../UE_Demo/UE_4.27_demo/Plugins/JsonData/StrafeStop_AnimTraj.json'
    strafeStopTraj_array = []
    strafeStopTraj_dict = get_trajectory_from_json(strafeStop_jsonFileName, strafeStopTraj_array)
    print(strafeStopTraj_dict)
    # --------------------------------strafe predict--------------------------------
    predict_jsonFileName = '../UE_Demo/UE_4.27_demo/Plugins/JsonData/PredictTrajectoryFromMMNode.json'
    predictTraj_array = []
    predictTraj_dict = get_trajectory_from_json(predict_jsonFileName, predictTraj_array)
    print(predictTraj_dict)

    # -------------------------compare--------------------------------------
    # for i in range(0, 18):
    #     print('---------------------' + str(i) + '---------------------')
    #     dtw_predict_turn = dtw(predictTraj_array[i], strafeTurnTraj_array[i])
    #     dtw_predict_stop = dtw(predictTraj_array[i], strafeStopTraj_array[i])
    #     print('dtw_predict_turn = {0:6.2f}'.format(dtw_predict_turn))
    #     print('dtw_predict_stop = {0:6.2f}'.format(dtw_predict_stop))
    #     euclid_predict_turn = euclid_dist(predictTraj_array[i], strafeTurnTraj_array[i])
    #     euclid_predict_stop = euclid_dist(predictTraj_array[i], strafeStopTraj_array[i])
    #     print('euclid_predict_turn = {0:6.2f}'.format(euclid_predict_turn))
    #     print('euclid_predict_stop = {0:6.2f}'.format(euclid_predict_stop))

    # cmp1 = predictTraj_array[3]
    # cmp2 = strafeTurnTraj_array[3]
    # cmp3 = strafeStopTraj_array[3]
    #
    # print('predict:' + str(cmp1))
    # print('Turn:' + str(cmp2))
    # print('Stop:' + str(cmp3))
    #
    # dtw_predict_turn = dtw(cmp1, cmp2)
    # dtw_predict_stop = dtw(cmp1, cmp3)
    # print('dtw_predict_turn = {0:6.2f}'.format(dtw_predict_turn))
    # print('dtw_predict_stop = {0:6.2f}'.format(dtw_predict_stop))
    # euclid_predict_turn = euclid_dist(cmp1, cmp2)
    # euclid_predict_stop = euclid_dist(cmp1, cmp3)
    # print('euclid_predict_turn = {0:6.2f}'.format(euclid_predict_turn))
    # print('euclid_predict_stop = {0:6.2f}'.format(euclid_predict_stop))

    # ---------------------------------show---------------------------------------
    show_trajectory('Strafe Turn', strafeTurnTraj_dict, strafeTurnTraj_array)
    show_trajectory('Strafe Stop', strafeStopTraj_dict, strafeStopTraj_array)
    show_trajectory('Strafe Predict', predictTraj_dict, predictTraj_array)

    # show_trajectory('Compare', strafeTurnTraj_dict, strafeTurnTraj_array)
    # show_trajectory('Compare', strafeStopTraj_dict, strafeStopTraj_array)
    # show_trajectory('Compare', predictTraj_dict, predictTraj_array)

    # plt.figure('cmp')
    # plt.plot([-0.66, -0.33, 0.33, 0.66, 1.0], cmp1, color='red', marker='o')
    # plt.plot([-0.66, -0.33, 0.33, 0.66, 1.0], cmp2, color='blue', marker='o')
    # plt.plot([-0.66, -0.33, 0.33, 0.66, 1.0], cmp3, color='black', marker='o')
    # plt.axis([-1.0, 1.5, -500, 500])
    # plt.show()

    for predict_index in range(0, 16):
        predict = predictTraj_array[predict_index]
        chosenTurnTraj = 0
        min_turn_cost = 100000
        for i in range(0, int(strafeTurnTraj_dict['PoseCount'])):
            currentCost = dist.euclid_dist_plus(predict, strafeTurnTraj_array[i])
            if min_turn_cost > currentCost:
                min_turn_cost = currentCost
                chosenTurnTraj = i

        chosenStopTraj = 0
        min_stop_cost = 100000
        for i in range(0, int(strafeStopTraj_dict['PoseCount'])):
            currentCost = dist.euclid_dist_plus(predict, strafeStopTraj_array[i])
            if min_stop_cost > currentCost:
                min_stop_cost = currentCost
                chosenStopTraj = i
        print('----------'+str(predict_index)+'-------------')
        print('Predict:' + str(predict))
        print('chosen turn:' + str(chosenTurnTraj) + '   Points: ' + str(strafeTurnTraj_array[chosenTurnTraj]))
        print('euclid_predict_turn = {0:6.2f}'.format(min_turn_cost))
        print('chosen stop:' + str(chosenStopTraj) + '   Points: ' + str(strafeStopTraj_array[chosenStopTraj]))
        print('euclid_predict_stop = {0:6.2f}'.format(min_stop_cost))

        plt.figure(str(predict_index))
        plt.plot([-0.66, -0.33, 0.33, 0.66, 1.0], predict, color='red', marker='o')
        plt.plot([-0.66, -0.33, 0.33, 0.66, 1.0], strafeTurnTraj_array[chosenTurnTraj], color='blue', marker='o', linestyle='dashed')
        plt.plot([-0.66, -0.33, 0.33, 0.66, 1.0], strafeStopTraj_array[chosenStopTraj], color='grey', marker='o', linestyle='dashed')
        plt.axis([-1.0, 1.5, -500, 500])
    plt.show()


if __name__ == '__main__':
    # custom_data.fundamental_test()
    # trajectory_test()
    custom_data.test()
