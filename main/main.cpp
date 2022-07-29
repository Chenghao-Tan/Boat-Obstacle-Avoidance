extern "C"
{
    void app_main();
}

#include "main.h"
#include "config.h"

void obstacle_avoidance::send_obs_dist_3d(uint32_t time, float x, float y, float z)
{
    const float obstacle_vector[3] = {z * 0.001f, x * 0.001f, -y * 0.001f};
    uint8_t buf[2041];
    mavlink_message_t mav_msg;
    mavlink_msg_obstacle_distance_3d_pack(1, 93, &mav_msg, time, 0, MAV_FRAME_BODY_FRD, 65535, obstacle_vector[0], obstacle_vector[1], obstacle_vector[2], 0.3f, 10.0f);
    uint16_t len = mavlink_msg_to_send_buffer(buf, &mav_msg);
    serial_comm.send_message(len, buf);
}

void obstacle_avoidance::loop()
{
    dai::SpiApi SPI;
    SPI.set_send_spi_impl(&esp32_send_spi);
    SPI.set_recv_spi_impl(&esp32_recv_spi);

    while (true)
    {
        // Send heartbeat to ArduPilot at a constant rate
        serial_comm.send_heartbeat();

        dai::Message NNDataMsg;
        bool receivedAnyMessage = false;

        if (SPI.req_message(&NNDataMsg, "NN"))
        {
            const uint32_t msg_ms = serial_comm.get_millis_ms();
            dai::RawNNData rawMeta;
            SPI.parse_metadata(&NNDataMsg.raw_meta, rawMeta);

            if ((msg_ms - last_obstacle_msg_ms) > (1000 / OBSTACLE_SEND_RATE_HZ))
            {
                last_obstacle_msg_ms = msg_ms;

                /*
                for (const auto &tensor : rawMeta.tensors)
                {
                }
                */

                uint16_t raw_data[GRID_NUM * 4]; // label, x, y, z
                grid grids[GRID_NUM];
                // if (sizeof(raw_data) <= NNDataMsg.raw_data.size)
                memcpy(raw_data, NNDataMsg.raw_data.data, sizeof(raw_data));

                for (int i = 0; i < GRID_NUM * 4; i++)
                {
                    _float16_shape_type temp;
                    temp.bits = raw_data[i];
                    *((float *)grids + i) = float16_to_float32(temp);
                }

                for (int i = 0; i < GRID_NUM; i++)
                {
                    if (grids[i].label != 0) // Not background
                        send_obs_dist_3d(msg_ms, grids[i].x, grids[i].y, grids[i].z);
                    // printf("label:%d, x:%.1fm, y:%.1fm, z:%.1fm\n", int(grids[i].label), grids[i].x, grids[i].y, grids[i].z); // For debug
                }
            }

            // free up resources once you're done with the message.
            SPI.free_message(&NNDataMsg);
            SPI.spi_pop_message("NN");
            receivedAnyMessage = true;
        }

        if (!receivedAnyMessage)
        {
            // Delay pooling of messages
            usleep(1000);
        }
    }
}

// Main application
void app_main()
{
    // init spi for the esp32
    init_esp32_spi();

    obstacle_avoidance avoidance_grid;
    avoidance_grid.loop();

    // Never reached
    deinit_esp32_spi();
}
