import sys

import depthai as dai

from pipeline import create_pipeline


def flash(bootloader_only=True):
    found, bl = dai.DeviceBootloader.getFirstAvailableDevice()  # type: ignore
    if found:
        bootloader = dai.DeviceBootloader(bl, allowFlashingBootloader=bootloader_only)
        print(bootloader.getVersion())

        progress = lambda p: print(f"Progress: {p*100:.1f}%")
        if not bootloader_only:
            pipeline = create_pipeline(XLink=False, lensPosition=lensPosition)
            bootloader.flash(progress, pipeline)
        else:
            bootloader.flashBootloader(progress)
    else:
        print("No device available...")


def write_image_to_file(filename):
    pipeline = create_pipeline(XLink=False, lensPosition=lensPosition)
    dai.DeviceBootloader.saveDepthaiApplicationPackage(filename, pipeline)


with dai.Device() as device:
    try:
        calibData = device.readCalibration2()
        lensPosition = calibData.getLensPosition(dai.CameraBoardSocket.RGB)
        print(f"RGB Cam lensPosition: {lensPosition}")
    except:
        raise Exception("Can't get lensPosition")

import time

print("Wait 5 seconds for the device to reset...")
time.sleep(5)

if len(sys.argv) >= 2 and sys.argv[1] == "bootloader":
    print("Flashing bootloader")
    flash(bootloader_only=True)
elif len(sys.argv) >= 2 and sys.argv[1] == "save":
    filename = "pipeline.dap"
    print("Saving pipeline to disk as " + filename)
    write_image_to_file(filename)
else:
    print("Flashing pipeline")
    flash(bootloader_only=False)
