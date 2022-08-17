# Boat-Obstacle-Avoidance
## Prepare
- Follow the **Installation** instructions in [depthai repository](https://github.com/luxonis/depthai) to install depthai library.
- Follow the **Installation** instructions in [pymavlink repository](https://github.com/ArduPilot/pymavlink) to install pymavlink.


## Usage
### Executables:
- **main.py**: Main script running on the companion computer.
- **test.py**: Visualization only.

### Configurations:
- pipeline.py:
    - model path: dai.OpenVINO.Blob("path to the blob")
    - IspScale: See [setIspScale](https://docs.luxonis.com/projects/api/en/latest/components/nodes/color_camera/#:~:text=setIspScale%28*,numerator%2C%20denominator%3E%20tuples). The scaled resolution must be bigger than the model's input. Using the same aspect ratio is highly recommended, or it will result in losing FOV.
- main.py:
    - GRID_NUM_H: Detection grid number on the vertical axis (must be compatible with the blob).
    - GRID_NUM_W: Detection grid number on the horizontal axis (must be compatible with the blob).
    - INPUT_SHAPE: AI model's input resolution (must be compatible with the blob).
    - BAUD_RATE: UART baud rate (if using serial).
    - DEVICE_STR: Interface to use. See [connection_string](https://mavlink.io/en/mavgen_python/#connection_string).
    - MSG_RATE_MAX: Max reporting frequency, in Hz.
    - MIN_DISTANCE: Min depth threshold, in m.
    - MAX_DISTANCE: Max depth threshold, in m.
- test.py: Same as in main.py.


## AI Model
**Use a modified DDRNet [here](https://github.com/Agent-Birkhoff/DDRNet).**
- Naming rules:
    - resolution (*W_H*)
    - resolution + grid number on each axis (*W_H_(GW_GH)*)
- Build your own model:
    - Follow the instructions in that repository (**Prepare->Training->Export ONNX**). Make sure **net.extra_process(True)** and remember to set **IO resolution and grid settings** properly.
    - Use the [blob converter](http://blobconverter.luxonis.com/) provided by Luxonis to convert the **ONNX** to **blob**. You can also use OpenVINO to do that locally (mo.py and compile_tool).
        - model optimizer params: **--data_type=FP16**
        - compile params: **-ip U8**
        - shaves: **6** (CMX slices: also 6)


## OAK-D-IoT Branch
**WARNING: This branch CAN NOT work properly for a long time!**

You might want to check [OAK-D-IoT branch](https://github.com/Agent-Birkhoff/Boat-Obstacle-Avoidance/tree/OAK-D-IoT) if you have OAK-D-IoT series cameras. Any help in debugging this branch is welcomed!
