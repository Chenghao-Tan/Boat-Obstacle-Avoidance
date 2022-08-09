import depthai as dai


def create_pipeline(XLink=False, **kwargs):
    blob = dai.OpenVINO.Blob("./640_360_(10_10).blob")  # TODO MODEL PATH
    for name, tensorInfo in blob.networkInputs.items():
        print(name, tensorInfo.dims)
    INPUT_SHAPE = blob.networkInputs["rgb"].dims[:2]

    # Start defining a pipeline
    pipeline = dai.Pipeline()

    # RGB camera
    cam = pipeline.create(dai.node.ColorCamera)
    cam.setResolution(dai.ColorCameraProperties.SensorResolution.THE_1080_P)
    cam.setIspScale((1, 3), (1, 3))  # TODO RGB->640x360
    cam.setBoardSocket(dai.CameraBoardSocket.RGB)
    cam.setColorOrder(dai.ColorCameraProperties.ColorOrder.RGB)
    cam.setPreviewSize(*INPUT_SHAPE)
    cam.setInterleaved(False)

    # For now, RGB needs fixed focus to properly align with depth
    # This value was used during calibration
    if "lensPosition" in kwargs:
        cam.initialControl.setManualFocus(kwargs["lensPosition"])
    else:
        raise Exception("Missing arg: lensPosition")

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

    # Stereo mode  # TODO
    stereo.setExtendedDisparity(True)
    stereo.setSubpixel(False)

    # Depth post processing  # TODO
    # WARNING: MAY SIGNIFICANTLY INCREASE DEPTH MAP'S LATENCY!!!
    config = stereo.initialConfig.get()
    config.postProcessing.decimationFilter.decimationFactor = 1  # type: ignore
    config.postProcessing.speckleFilter.enable = False  # type: ignore
    config.postProcessing.temporalFilter.enable = False  # type: ignore
    config.postProcessing.spatialFilter.enable = False  # type: ignore

    # Depth thresholds  # TODO
    stereo.setConfidenceThreshold(255)
    config.postProcessing.thresholdFilter.minRange = 200  # type: ignore
    config.postProcessing.thresholdFilter.maxRange = 35000  # type: ignore
    stereo.initialConfig.set(config)

    # Depth output linked to NN
    stereo.depth.link(segmentation_nn.inputs["depth"])

    # NN output linked to SPIOut
    spi = pipeline.create(dai.node.SPIOut)
    spi.setStreamName("NN")
    spi.setBusId(0)
    spi.input.setBlocking(False)
    spi.input.setQueueSize(2)
    segmentation_nn.out.link(spi.input)

    # NN output linked to XLinkOut (for testing only)
    if XLink:
        xout_nn = pipeline.create(dai.node.XLinkOut)
        xout_nn.setStreamName("nn")
        segmentation_nn.out.link(xout_nn.input)
        xout_img = pipeline.create(dai.node.XLinkOut)
        xout_img.setStreamName("img")
        segmentation_nn.passthroughs["rgb"].link(xout_img.input)
        xout_depth = pipeline.create(dai.node.XLinkOut)
        xout_depth.setStreamName("depth")
        segmentation_nn.passthroughs["depth"].link(xout_depth.input)

    return pipeline
