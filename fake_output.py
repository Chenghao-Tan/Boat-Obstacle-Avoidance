import depthai as dai

# Start defining a pipeline
pipeline = dai.Pipeline()

# Script node
script = pipeline.create(dai.node.Script)
script.setScript(
    """
    while True:
        buf = NNData(800)
        buf.setLayer("out", [1.0, 1.2, 3.9, 5.5] * 99 + [0.0, 0.0, 0.1, 0.0])
        node.io['out'].send(buf)
    """
)

# XLinkOut
xout = pipeline.create(dai.node.XLinkOut)
xout.setStreamName("out")
script.outputs["out"].link(xout.input)

# SPIOut
spi = pipeline.create(dai.node.SPIOut)
spi.setStreamName("NN")
spi.setBusId(0)
spi.input.setBlocking(False)
spi.input.setQueueSize(2)
script.outputs["out"].link(spi.input)

# Connect to device with pipeline
with dai.Device(pipeline) as device:
    device.setLogLevel(dai.LogLevel.WARN)
    device.setLogOutputLevel(dai.LogLevel.WARN)

    while True:
        nndata = device.getOutputQueue("out").get()  # type: ignore

        print(f"NNData size: {len(nndata.getData())}")
        print("values:", nndata.getData())
