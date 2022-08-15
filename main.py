import time

import cv2
import depthai as dai
import numpy as np

from pipeline import create_pipeline

GRID_NUM_H = 10  # TODO
GRID_NUM_W = 10  # TODO
INPUT_SHAPE = (640, 360)  # TODO


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

    try:
        calibData = device.readCalibration2()
        lensPosition = calibData.getLensPosition(dai.CameraBoardSocket.RGB)
        print(f"RGB Cam lensPosition: {lensPosition}")
    except:
        raise Exception("Can't get lensPosition")

    device.startPipeline(create_pipeline(XLink=True, lensPosition=lensPosition))
    q_nn = device.getOutputQueue(name="nn", maxSize=4, blocking=False)  # type: ignore
    q_img = device.getOutputQueue(name="img", maxSize=4, blocking=False)  # type: ignore
    q_depth = device.getOutputQueue(name="depth", maxSize=4, blocking=False)  # type: ignore
    fps = FPSHandler()
    while True:
        msgs = q_nn.get()
        grids = msgs.getLayerFp16("out")
        img = q_img.get().getCvFrame()
        depth = q_depth.get().getFrame()
        fps.next_iter()

        grids = np.asarray(grids).reshape(GRID_NUM_H, GRID_NUM_W, 4)  # lxyz
        depth = ((depth - depth.min()) / (depth.max() - depth.min()) * 255).astype(
            np.uint8
        )
        depth = cv2.applyColorMap(depth, cv2.COLORMAP_JET)
        blend = cv2.addWeighted(img, 0.5, depth, 0.5, 0)
        blend = cv2.resize(blend, (1280, 720))  # Force 720P for bigger display

        for i in range(1, GRID_NUM_H):
            cv2.line(
                blend,
                (0, int(blend.shape[0] * i / GRID_NUM_H)),
                (blend.shape[1] - 1, int(blend.shape[0] * i / GRID_NUM_H)),
                color=(255, 255, 255),
                thickness=1,
            )
        for i in range(1, GRID_NUM_W):
            cv2.line(
                blend,
                (int(blend.shape[1] * i / GRID_NUM_W), 0),
                (int(blend.shape[1] * i / GRID_NUM_W), blend.shape[0] - 1),
                color=(255, 255, 255),
                thickness=1,
            )

        for i in range(GRID_NUM_H):
            for j in range(GRID_NUM_W):
                cv2.putText(
                    blend,
                    "label:{:d}".format(grids[i][j][0].astype(np.uint8)),
                    (
                        int(blend.shape[1] * j / GRID_NUM_W) + 3,
                        int(blend.shape[0] * i / GRID_NUM_H) + 12,
                    ),
                    cv2.FONT_HERSHEY_TRIPLEX,
                    0.4,
                    color=(255, 255, 255),
                )
                cv2.putText(
                    blend,
                    "x:{:.1f}m".format(grids[i][j][1]),
                    (
                        int(blend.shape[1] * j / GRID_NUM_W) + 3,
                        int(blend.shape[0] * i / GRID_NUM_H) + 24,
                    ),
                    cv2.FONT_HERSHEY_TRIPLEX,
                    0.4,
                    color=(255, 255, 255),
                )
                cv2.putText(
                    blend,
                    "y:{:.1f}m".format(grids[i][j][2]),
                    (
                        int(blend.shape[1] * j / GRID_NUM_W) + 3,
                        int(blend.shape[0] * i / GRID_NUM_H) + 36,
                    ),
                    cv2.FONT_HERSHEY_TRIPLEX,
                    0.4,
                    color=(255, 255, 255),
                )
                cv2.putText(
                    blend,
                    "z:{:.1f}m".format(grids[i][j][3]),
                    (
                        int(blend.shape[1] * j / GRID_NUM_W) + 3,
                        int(blend.shape[0] * i / GRID_NUM_H) + 48,
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
