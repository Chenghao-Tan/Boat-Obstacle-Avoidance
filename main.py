#!/usr/bin/env python


GRID_NUM_H = 10  # TODO
GRID_NUM_W = 10  # TODO
INPUT_SHAPE = (640, 360)  # TODO
BAUD_RATE = 115200  # TODO
DEVICE_STR = "/dev/ttyAMA0"  # TODO
MSG_RATE_MAX = 30  # TODO in Hz
MIN_DISTANCE = 0.35  # TODO in m
MAX_DISTANCE = 35  # TODO in m


import os

os.environ["MAVLINK20"] = "1"  # Set MAVLink protocol to 2
# os.environ["MAVLINK_DIALECT"] = "ardupilotmega"  # Default: ardupilotmega


import threading
import time

import depthai as dai
import numpy as np
from pymavlink import mavutil

from pipeline import create_pipeline
from z2xy import z2xy_coefficient, z2xy_coefficient_fov

exit = False
connection = mavutil.mavlink_connection(
    device=DEVICE_STR,
    baud=BAUD_RATE,
    source_system=1,
    source_component=93,
    autoreconnect=True,
    force_connected=True,
)
start_time = int(round(time.time() * 1000))  # in ms
get_current_time = lambda: int(round(time.time() * 1000) - start_time)  # in ms
message_interval_min = 1000 / MSG_RATE_MAX  # in ms


# Heartbeat thread
def heartbeat():
    global exit, connection
    while not exit:
        connection.mav.heartbeat_send(
            mavutil.mavlink.MAV_TYPE_ONBOARD_CONTROLLER,
            mavutil.mavlink.MAV_AUTOPILOT_INVALID,
            0,
            0,
            0,
        )
        print(f"{get_current_time()} Heartbeat sent")
        time.sleep(1)  # 1Hz


heartbeat_thread = threading.Thread(target=heartbeat)
heartbeat_thread.start()


# Main
with dai.Device() as device:
    try:
        # Getting calibration data
        calibData = device.readCalibration2()

        lensPosition = calibData.getLensPosition(dai.CameraBoardSocket.RGB)
        print(f"RGB Cam lensPosition: {lensPosition}")

        intrinsics = calibData.getCameraIntrinsics(
            dai.CameraBoardSocket.RGB, *INPUT_SHAPE
        )
        print(f"RGB Cam intrinsics: {intrinsics}")

        hfov = calibData.getFov(dai.CameraBoardSocket.RGB)
        print(f"RGB Cam HFOV: {hfov}")

        # Generating fixed z to x,y coefficient
        assert INPUT_SHAPE[1] % GRID_NUM_H == 0
        assert INPUT_SHAPE[0] % GRID_NUM_W == 0
        grid_height = INPUT_SHAPE[1] // GRID_NUM_H
        grid_width = INPUT_SHAPE[0] // GRID_NUM_W
        # Use intrinsic matrix
        z2x, z2y = z2xy_coefficient(
            grid_height, grid_width, GRID_NUM_H, GRID_NUM_W, np.array(intrinsics)
        )
        # Use HFOV only
        """
        z2x, z2y = z2xy_coefficient_fov(
            grid_height, grid_width, GRID_NUM_H, GRID_NUM_W, hfov
        )
        """
        z2x = z2x.reshape(GRID_NUM_H, GRID_NUM_W)
        z2y = z2y.reshape(GRID_NUM_H, GRID_NUM_W)

        # Start pipeline
        device.startPipeline(create_pipeline(lensPosition=lensPosition))
        q_nn = device.getOutputQueue(name="nn", maxSize=4, blocking=False)  # type: ignore

        # Main loop
        last_message_time = 0  # in ms
        while True:
            current_time = get_current_time()

            # Try to get label and depth(z)
            msgs = q_nn.get()
            grids = msgs.getLayerFp16("out")
            grids = np.asarray(grids).reshape(
                GRID_NUM_H, GRID_NUM_W, 2
            )  # label,z(in m)

            # Sending obstacle lacations
            if current_time - last_message_time >= message_interval_min:
                for i in range(GRID_NUM_H):
                    for j in range(GRID_NUM_W):
                        if grids[i][j][0] > 0:  # label>1
                            z = grids[i][j][1]  # depth(z) in m
                            x = z2x[i][j] * z  # in m
                            y = z2y[i][j] * z  # in m

                            connection.mav.obstacle_distance_3d_send(
                                current_time,  # UNIX Timestamp in ms
                                4,  # MAV_DISTANCE_SENSOR_UNKNOWN
                                12,  # MAV_FRAME_BODY_FRD
                                65535,  # UINT16_MAX (Unknown)
                                float(z),  # Forward, in m
                                float(x),  # Right, in m
                                float(-y),  # Down, in m
                                float(MIN_DISTANCE),  # in m
                                float(MAX_DISTANCE),  # in m
                            )
                            print(f"{current_time} x:{x:.2f}m y:{y:.2f}m z:{z:.2f}m")
                last_message_time = current_time

    except Exception as e:
        print(e)

    finally:
        exit = True
        heartbeat_thread.join()
        connection.close()
