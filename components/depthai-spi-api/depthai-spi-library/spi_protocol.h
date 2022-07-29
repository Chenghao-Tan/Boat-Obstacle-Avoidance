/*
 * spi_protocol.h
 *
 *  Created on: Mar 31, 2020
 *      Author: TheMarpe - Martin Peterlin
 *
 */

#ifndef SHARED_SPI_PROTOCOL_H
#define SHARED_SPI_PROTOCOL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
// TODO: get rid of this dupulicate define.
#define SPI_PROTOCOL_PAYLOAD_SIZE (252)

#define PAYLOAD_MAX_SIZE 252
#define BUFF_MAX_SIZE 256
#define SPI_PKT_SIZE 256

static const uint8_t START_BYTE_MAGIC = 0b10101010;
static const uint8_t END_BYTE_MAGIC = 0b00000000;

typedef struct {
    uint8_t start;
    uint8_t data[SPI_PROTOCOL_PAYLOAD_SIZE];
    uint8_t crc[2];
    uint8_t end;
} SpiProtocolPacket;

typedef struct {
    int state;
    int payloadOffset;
    int currentPacketIndex;
    // 2 packets can be decoded at a time max
    SpiProtocolPacket packet[2];
} SpiProtocolInstance;


enum SPI_PROTOCOL_RETURN_CODE {
    SPI_PROTOCOL_OK = 0,
    SPI_PROTOCOL_PACKET_NULL = -1,
    SPI_PROTOCOL_PAYLOAD_BUFFER_NULL = -2
};

/**
 * Initializes an instance of spi_protocol library
 * @param instance SpiProtocolInstance pointer;
 */
void spi_protocol_init(SpiProtocolInstance *instance);


/**
 * Parses a given buffer of bytes and returns a complete packet if enough bytes arrived
 *
 * @param instance Spi protocol instance pointer
 * @param buffer Pointer to buffer where packet bytes reside
 * @param size Number of bytes to parse (max SPI_PROTOCOL_PAYLOAD_SIZE)
 *
 * @returns SpiProtocolPacket pointer, NULL if packet wasn't parsed
 */
SpiProtocolPacket* spi_protocol_parse(SpiProtocolInstance* instance, const uint8_t* buffer, int size);


/**
 * Creates a SpiProtocolPacket from a buffer
 *
 * @param packet Pointer to SpiProtocolPacket where it will be written
 * @param payload_buffer Input buffer with payload data
 * @returns 0 OK, -1 packet is NULL, -2 payload_buffer is NULL
 */
int spi_protocol_write_packet(SpiProtocolPacket* packet, const uint8_t* payload_buffer, int size);
int spi_protocol_write_packet2(SpiProtocolPacket* packet, const uint8_t* payload_buffer1, const uint8_t* payload_buffer2, int size1, int size2);


/**
 * Adds beggining, ending and calculates CRC for existing bytes in the given SpiProtocolPacket
 *
 * @param packet Pointer to SpiProtocolPacket where header, crc and tail will be written
 * @returns 0 OK, -1 packet is NULL
 */
int spi_protocol_inplace_packet(SpiProtocolPacket* packet);


#ifdef __cplusplus
}
#endif


#endif
