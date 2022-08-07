import depthai as dai


def create_pipeline(XLink=False):
    blob = dai.OpenVINO.Blob("./640_360_(10_10).blob")  # TODO MODEL PATH
    for name, tensorInfo in blob.networkInputs.items():
        print(name, tensorInfo.dims)
    INPUT_SHAPE = blob.networkInputs["rgb"].dims[:2]

    # Start defining a pipeline
    pipeline = dai.Pipeline()

    cam = pipeline.create(dai.node.ColorCamera)
    cam.setResolution(dai.ColorCameraProperties.SensorResolution.THE_1080_P)
    cam.setIspScale((1, 3), (1, 3))  # TODO RGB->640x360
    cam.setBoardSocket(dai.CameraBoardSocket.RGB)
    cam.setColorOrder(dai.ColorCameraProperties.ColorOrder.RGB)
    cam.setPreviewSize(*INPUT_SHAPE)
    cam.setInterleaved(False)

    # Define a neural network that will make predictions based on the source frames
    detection_nn = pipeline.create(dai.node.NeuralNetwork)
    detection_nn.setBlob(blob)
    detection_nn.input.setBlocking(False)
    detection_nn.setNumInferenceThreads(1)
    cam.preview.link(detection_nn.inputs["rgb"])

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
    stereo.setDefaultProfilePreset(dai.node.StereoDepth.PresetMode.HIGH_ACCURACY)
    stereo.setDepthAlign(dai.CameraBoardSocket.RGB)
    left.out.link(stereo.left)
    right.out.link(stereo.right)

    # Depth output linked to NN
    stereo.depth.link(detection_nn.inputs["depth"])

    # NN output linked to SPIOut
    spi = pipeline.create(dai.node.SPIOut)
    spi.setStreamName("NN")
    spi.setBusId(0)
    spi.input.setBlocking(False)
    spi.input.setQueueSize(2)
    detection_nn.out.link(spi.input)

    # NN output linked to XLinkOut (for testing only)
    if XLink:
        xout_nn = pipeline.create(dai.node.XLinkOut)
        xout_nn.setStreamName("nn")
        detection_nn.out.link(xout_nn.input)
        xout_img = pipeline.create(dai.node.XLinkOut)
        xout_img.setStreamName("img")
        detection_nn.passthroughs["rgb"].link(xout_img.input)
        xout_depth = pipeline.create(dai.node.XLinkOut)
        xout_depth.setStreamName("depth")
        detection_nn.passthroughs["depth"].link(xout_depth.input)

    return pipeline
