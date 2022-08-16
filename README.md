# Boat-Obstacle-Avoidance
**WARNING: Due to an unknown bug in depthai's SPI implementation, this program CAN NOT work properly for a long time.**

**Use model from [DDRNet OAK-D-IoT branch](https://github.com/Agent-Birkhoff/DDRNet/tree/OAK-D-IoT)!!!**


- **vpu_setup**: Run in CLI. Used for uploading the program to VPU's flash. See the script detail for more usage.
- **vpu_test**: Used for visualization like what CamDemo(WithDepth) does. **WILL NOT** upload the program to VPU's flash.
- **fake_output**: Use Script node to generate fake NNData, and send them through SPI.
- **main**: ESP32's main code. You need to build and flash it using ESP-IDF.
- components and others: Libraries used in the project.
- Configurations:
    - pipeline.py: **model path** and **IspScale**, the same as CamDemo(WithDepth).
    - vpu_test.py: **GRID_NUM_H**, **GRID_NUM_W** is the same as CamDemo(WithDepth). However, the model's **INPUT_SHAPE** needs to be specified manually. You can ignore these settings if you don't want to visualize.
    - main/config.h: **OBSTACLE_SEND_RATE_HZ** is the frequency of sending obstacle locations. **GRID_NUM**=GRID_NUM_H*GRID_NUM_H, is the total number of grids.
