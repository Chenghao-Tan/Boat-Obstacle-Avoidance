# depthai-spi-api
## SPI Protocol

SPI messaging is currently arranged in 2 layers. The first is the spi protocol. The spi protocol is the lowest level. It defines a standard packet for all SPI communication. It is a 256 byte packet arranged in the following manner:

```
typedef struct {
    uint8_t start;
    uint8_t data[SPI_PROTOCOL_PAYLOAD_SIZE];
    uint8_t crc[2];
    uint8_t end;
} SpiProtocolPacket;
```
start and end are constant bytes to mark the beginning and end of packets.
```
static const uint8_t START_BYTE_MAGIC = 0b10101010;
static const uint8_t END_BYTE_MAGIC = 0b00000000;
```

### SPI implementation

When implementing SPI communication between OAK SOM and another device, make sure to follow the notes below. Reference implementation (for ESP32) can [be found here](common/esp32_spi_impl.c).

- Mode should be SPI Mode 1
- CS should be enabled pre and post transaction for a couple extra cycles
- Clock speed 4MHz works more reliably than higher.
- On MX side, the interrupt pin is set to `MXIO34` (On ref implementation with ESP32 that pin is connected to ESP32 GPIO2)
- `GET_SIZE` should return the size of the message buffer and `GET_METASIZE` the size of the accompanying metadata. Unless you receive `0xFFFFFFFF` (uint32_t), which indicates "no message"


## SPI Messaging
On top of this we have a layer called SPI messaging. This code defines the following:
* A list of a supported commands,
* A way to encapsulate commands going to the MyriadX over SPI.
* A way to receive and parse command responses. 

## SPI API
Finally, on top of the messaging layer, we have a SPI API to make basic useage more straightforward. This layer will manage most of the things necessary for sending and receiving SPI messages.

An example of current capabilites can be found at this repo:
https://github.com/luxonis/esp32-spi-message-demo/tree/gen2_common_objdet

NOTE: This is still in flux so there may be potential changes to the API.
