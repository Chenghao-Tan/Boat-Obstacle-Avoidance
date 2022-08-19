import depthai as dai


def create_pipeline(blob, lensPosition, passthroughs=False, **kwargs):
    # Get INPUT_SHAPE automatically
    INPUT_SHAPE = blob.networkInputs["rgb"].dims[:2]

    # Start defining a pipeline
    pipeline = dai.Pipeline()

    # RGB camera
    cam = pipeline.create(dai.node.ColorCamera)
    cam.setResolution(dai.ColorCameraProperties.SensorResolution.THE_1080_P)
    if "ISP_SCALE" in kwargs:
        cam.setIspScale(*kwargs["ISP_SCALE"])  # Keep 16:9
    else:
        cam.setIspScale((1, 3), (1, 3))  # RGB->640x360
    cam.setBoardSocket(dai.CameraBoardSocket.RGB)
    cam.setColorOrder(dai.ColorCameraProperties.ColorOrder.RGB)
    cam.setPreviewSize(*INPUT_SHAPE)
    cam.setInterleaved(False)

    # For now, RGB needs fixed focus to properly align with depth
    # This value was used during calibration
    cam.initialControl.setManualFocus(lensPosition)

    # Neural network
    segmentation_nn = pipeline.create(dai.node.NeuralNetwork)
    segmentation_nn.setBlob(blob)
    segmentation_nn.input.setBlocking(False)
    segmentation_nn.setNumInferenceThreads(0)  # 0 for auto
    cam.preview.link(segmentation_nn.inputs["rgb"])

    # Left mono camera
    left = pipeline.create(dai.node.MonoCamera)
    left.setResolution(dai.MonoCameraProperties.SensorResolution.THE_400_P)
    left.setBoardSocket(dai.CameraBoardSocket.LEFT)
    # Right mono camera
    right = pipeline.create(dai.node.MonoCamera)
    right.setResolution(dai.MonoCameraProperties.SensorResolution.THE_400_P)
    right.setBoardSocket(dai.CameraBoardSocket.RIGHT)

    # Create a node that will produce the depth map
    stereo = pipeline.create(dai.node.StereoDepth)
    stereo.setPostProcessingHardwareResources(1, 0)  # (extra)numShaves,numMemorySlices
    stereo.setNumFramesPool(25)  # TODO may be related to the FPS
    stereo.setDefaultProfilePreset(dai.node.StereoDepth.PresetMode.HIGH_ACCURACY)
    stereo.initialConfig.setMedianFilter(dai.MedianFilter.KERNEL_7x7)  # type: ignore
    stereo.setLeftRightCheck(True)  # LR-CHECK IS REQUIRED FOR DEPTH ALIGNMENT
    stereo.setDepthAlign(dai.CameraBoardSocket.RGB)
    left.out.link(stereo.left)
    right.out.link(stereo.right)

    # Stereo mode
    if "EXTENDED_DISPARITY" in kwargs:
        stereo.setExtendedDisparity(kwargs["EXTENDED_DISPARITY"])
    else:
        stereo.setExtendedDisparity(False)
    if "SUBPIXEL" in kwargs:
        stereo.setSubpixel(kwargs["SUBPIXEL"])
    else:
        stereo.setSubpixel(True)

    # Depth post processing
    # WARNING: MAY SIGNIFICANTLY INCREASE DEPTH MAP'S LATENCY!!!
    config = stereo.initialConfig.get()
    config.postProcessing.decimationFilter.decimationFactor = kwargs["DECIMATION_FACTOR"] if "DECIMATION_FACTOR" in kwargs else 1  # type: ignore
    config.postProcessing.speckleFilter.enable = kwargs["SPECKLE_FILTER"] if "SPECKLE_FILTER" in kwargs else False  # type: ignore
    config.postProcessing.temporalFilter.enable = kwargs["TEMPORAL_FILTER"] if "TEMPORAL_FILTER" in kwargs else False  # type: ignore
    config.postProcessing.spatialFilter.enable = kwargs["SPATIAL_FILTER"] if "SPATIAL_FILTER" in kwargs else False  # type: ignore

    # Depth thresholds
    if "CONFIDENCE_THRESHOLD" in kwargs:
        stereo.setConfidenceThreshold(kwargs["CONFIDENCE_THRESHOLD"])
    else:
        stereo.setConfidenceThreshold(190)
    config.postProcessing.thresholdFilter.minRange = int(kwargs["MIN_DISTANCE"] * 1000) if "MIN_DISTANCE" in kwargs else 350  # type: ignore
    config.postProcessing.thresholdFilter.maxRange = int(kwargs["MAX_DISTANCE"] * 1000) if "MAX_DISTANCE" in kwargs else 35000  # type: ignore
    stereo.initialConfig.set(config)

    # Depth output linked to NN
    stereo.depth.link(segmentation_nn.inputs["depth"])

    # NN output linked to XLinkOut
    xout_nn = pipeline.create(dai.node.XLinkOut)
    xout_nn.setStreamName("nn")
    segmentation_nn.out.link(xout_nn.input)

    # For test
    if passthroughs:
        xout_img = pipeline.create(dai.node.XLinkOut)
        xout_img.setStreamName("img")
        segmentation_nn.passthroughs["rgb"].link(xout_img.input)
        xout_depth = pipeline.create(dai.node.XLinkOut)
        xout_depth.setStreamName("depth")
        segmentation_nn.passthroughs["depth"].link(xout_depth.input)

    return pipeline
