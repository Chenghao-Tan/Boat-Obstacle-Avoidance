#ifndef SHARED_SPI_MESSAGING_H
#define SHARED_SPI_MESSAGING_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <spi_protocol.h>

#define MAX_STREAMNAME 16
#define MAX_STREAMS 12

typedef enum{
    // SpiGetSizeResp commands
    GET_SIZE,
    GET_METASIZE,

    // SpiGetMessageResp commands
    GET_MESSAGE,
    GET_METADATA,
    GET_MESSAGE_PART,

    // SpiStatusResp commands
    POP_MESSAGES,
    POP_MESSAGE,

    // SpiGetStreamsResp commands
    GET_STREAMS,

    // Sends message
    SEND_DATA,
    // Gets message fast (TODO)
    GET_MESSAGE_FAST,
} spi_command;
static const spi_command GET_SIZE_CMDS[] = {GET_SIZE, GET_METASIZE};
static const spi_command GET_MESS_CMDS[] = {GET_MESSAGE, GET_METADATA, GET_MESSAGE_PART};

static const int SPI_MSG_SUCCESS_RESP = 0;
static const int SPI_MSG_FAIL_RESP = 1;

typedef struct {
    uint16_t total_size;
    uint8_t cmd;
    uint8_t stream_name_len;
    uint32_t extra_offset;
    uint32_t extra_size;
    uint32_t metadata_size;
    char stream_name[MAX_STREAMNAME];
} SpiCmdMessage;

typedef struct {
    uint32_t size;
} SpiGetSizeResp;

typedef struct {
    uint32_t data_type;
    uint32_t data_size;
    uint8_t *data;
} SpiGetMessageResp;

typedef struct {
    uint8_t status;
} SpiStatusResp;

typedef struct {
    uint8_t numStreams;
    char stream_names[MAX_STREAMS][MAX_STREAMNAME];
} SpiGetStreamsResp;

uint8_t isGetSizeCmd(spi_command cmd);
uint8_t isGetMessageCmd(spi_command cmd);

void spi_generate_command(SpiProtocolPacket* spiPacket, spi_command command, uint8_t streamNameLen, const char* streamName);
void spi_generate_command_partial(SpiProtocolPacket* spiPacket, spi_command command, uint8_t stream_name_len, const char* stream_name, uint32_t offset, uint32_t offset_size);
void spi_generate_command_send(SpiProtocolPacket* spiPacket, spi_command command, uint8_t stream_name_len, const char* stream_name, uint32_t metadata_size, uint32_t send_data_size);
void spi_parse_command(SpiCmdMessage* message, uint8_t* data);

void spi_parse_get_size_resp(SpiGetSizeResp* parsedResp, uint8_t* data);
void spi_status_resp(SpiStatusResp* parsedResp, uint8_t* data);
void spi_parse_get_streams_resp(SpiGetStreamsResp* parsedResp, uint8_t* data);

void spi_parse_get_message(SpiGetMessageResp* parsedResp, uint32_t size, spi_command get_mess_cmd);

#ifdef __cplusplus
}
#endif


#endif