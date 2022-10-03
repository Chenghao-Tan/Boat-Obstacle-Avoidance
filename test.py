import time

import cv2
import depthai as dai
import numpy as np
import yaml

from host_side_detection import detection
from pipeline import create_pipeline
from z2xy import z2xy_coefficient, z2xy_coefficient_fov

# Reading config
with open("config.yaml") as f:  # Read only
    config = yaml.safe_load(f)
    print(f"Config: {config}")


class FPSHandler:
    def __init__(self):
        self.timestamp = time.time()
        self.start = time.time()
        self.frame_cnt = 0

    def next_iter(self):
        self.timestamp = time.time()
        self.frame_cnt += 1

    def fps(self):
        return self.frame_cnt / (self.timestamp - self.start)


with dai.Device() as device:
    device.setLogLevel(dai.LogLevel.DEBUG)
    device.setLogOutputLevel(dai.LogLevel.DEBUG)

    # Loading blob
    blob = dai.OpenVINO.Blob(config["MODEL_PATH"])
    for name, tensorInfo in blob.networkInputs.items():
        print(name, tensorInfo.dims)
    INPUT_SHAPE = blob.networkInputs["rgb"].dims[:2]  # Auto INPUT_SHAPE

    # Getting calibration data
    try:
        calibData = device.readCalibration2()

        lensPosition = calibData.getLensPosition(dai.CameraBoardSocket.RGB)
        print(f"RGB Cam lensPosition: {lensPosition}")

        intrinsics = calibData.getCameraIntrinsics(
            dai.CameraBoardSocket.RGB, *INPUT_SHAPE  # type: ignore
        )
        print(f"RGB Cam intrinsics: {intrinsics}")

        hfov = calibData.getFov(dai.CameraBoardSocket.RGB)
        print(f"RGB Cam HFOV: {hfov}")
    except:
        raise Exception("Fail to get calibration data!")

    # Generating fixed z to x,y coefficient
    assert INPUT_SHAPE[1] % config["GRID_NUM"][0] == 0
    assert INPUT_SHAPE[0] % config["GRID_NUM"][1] == 0
    grid_height = INPUT_SHAPE[1] // config["GRID_NUM"][0]
    grid_width = INPUT_SHAPE[0] // config["GRID_NUM"][1]
    if config["USE_INTRINSIC"]:
        # Use intrinsic matrix
        z2x, z2y = z2xy_coefficient(
            grid_height,
            grid_width,
            config["GRID_NUM"][0],
            config["GRID_NUM"][1],
            np.array(intrinsics),
        )
    else:
        # Use HFOV only
        z2x, z2y = z2xy_coefficient_fov(
            grid_height, grid_width, config["GRID_NUM"][0], config["GRID_NUM"][1], hfov
        )
    z2x = z2x.reshape(config["GRID_NUM"][0], config["GRID_NUM"][1])
    z2y = z2y.reshape(config["GRID_NUM"][0], config["GRID_NUM"][1])

    # Start pipeline
    device.startPipeline(
        create_pipeline(
            blob=blob, lensPosition=lensPosition, passthroughs=True, **config
        )
    )
    q_nn = device.getOutputQueue(name="nn", maxSize=4, blocking=False)  # type: ignore
    q_img = device.getOutputQueue(name="img", maxSize=4, blocking=False)  # type: ignore
    q_depth = device.getOutputQueue(name="depth", maxSize=4, blocking=False)  # type: ignore
    fps = FPSHandler()

    while True:
        msgs = q_nn.get()
        nn = msgs.getLayerFp16("out")
        img = q_img.get().getCvFrame()
        depth = q_depth.get().getFrame()
        fps.next_iter()

        if config["HOST_SIDE"]:
            grids = detection(config, INPUT_SHAPE, mask=nn, depth=depth)
        else:
            grids = nn

        grids = np.asarray(grids).reshape(
            config["GRID_NUM"][0], config["GRID_NUM"][1], 2
        )  # label,depth(z)
        depth = ((depth - depth.min()) / (depth.max() - depth.min()) * 255).astype(
            np.uint8
        )
        depth = cv2.applyColorMap(depth, cv2.COLORMAP_JET)
        blend = cv2.addWeighted(img, 0.5, depth, 0.5, 0)
        blend = cv2.resize(blend, (1280, 720))  # Force 720P for bigger display

        for i in range(1, config["GRID_NUM"][0]):
            cv2.line(
                blend,
                (0, int(blend.shape[0] * i / config["GRID_NUM"][0])),
                (blend.shape[1] - 1, int(blend.shape[0] * i / config["GRID_NUM"][0])),
                color=(255, 255, 255),
                thickness=1,
            )
        for i in range(1, config["GRID_NUM"][1]):
            cv2.line(
                blend,
                (int(blend.shape[1] * i / config["GRID_NUM"][1]), 0),
                (int(blend.shape[1] * i / config["GRID_NUM"][1]), blend.shape[0] - 1),
                color=(255, 255, 255),
                thickness=1,
            )

        for i in range(config["GRID_NUM"][0]):
            for j in range(config["GRID_NUM"][1]):
                cv2.putText(
                    blend,
                    "label:{:d}".format(grids[i][j][0].astype(np.uint8)),
                    (
                        int(blend.shape[1] * j / config["GRID_NUM"][1]) + 3,
                        int(blend.shape[0] * i / config["GRID_NUM"][0]) + 12,
                    ),
                    cv2.FONT_HERSHEY_TRIPLEX,
                    0.4,
                    color=(255, 255, 255),
                )
                cv2.putText(
                    blend,
                    "x:{:.1f}m".format(grids[i][j][1] * z2x[i][j]),
                    (
                        int(blend.shape[1] * j / config["GRID_NUM"][1]) + 3,
                        int(blend.shape[0] * i / config["GRID_NUM"][0]) + 24,
                    ),
                    cv2.FONT_HERSHEY_TRIPLEX,
                    0.4,
                    color=(255, 255, 255),
                )
                cv2.putText(
                    blend,
                    "y:{:.1f}m".format(grids[i][j][1] * z2y[i][j]),
                    (
                        int(blend.shape[1] * j / config["GRID_NUM"][1]) + 3,
                        int(blend.shape[0] * i / config["GRID_NUM"][0]) + 36,
                    ),
                    cv2.FONT_HERSHEY_TRIPLEX,
                    0.4,
                    color=(255, 255, 255),
                )
                cv2.putText(
                    blend,
                    "z:{:.1f}m".format(grids[i][j][1]),
                    (
                        int(blend.shape[1] * j / config["GRID_NUM"][1]) + 3,
                        int(blend.shape[0] * i / config["GRID_NUM"][0]) + 48,
                    ),
                    cv2.FONT_HERSHEY_TRIPLEX,
                    0.4,
                    color=(255, 255, 255),
                )

        cv2.putText(
            blend,
            "Fps: {:.2f}".format(fps.fps()),
            (2, blend.shape[0] - 4),
            cv2.FONT_HERSHEY_TRIPLEX,
            0.4,
            color=(255, 255, 255),
        )
        cv2.imshow("BLEND", blend)

        if cv2.waitKey(1) == ord("q"):
            break
