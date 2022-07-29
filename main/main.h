#include <cstdio>
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <chrono>
#include <vector>

#include "esp32_spi_impl.h"
#include "spi_api.hpp"
#include "float16.h"

#include "mavlink.h"
#include "serial_interface.h"

typedef struct
{
    float label;
    float x;
    float y;
    float z;
} grid;

class obstacle_avoidance
{
public:
    void loop();

private:
    // set serial port for mavlink communication
    Serial_Interface serial_comm;
    uint32_t last_obstacle_msg_ms = 0;

    void send_obs_dist_3d(uint32_t time, float x, float y, float z);
};
