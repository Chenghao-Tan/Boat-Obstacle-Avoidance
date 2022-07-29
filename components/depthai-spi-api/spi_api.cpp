#include "spi_api.hpp"

#include <cstdio>
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <cassert>
#include <chrono>
#include <thread>

#define DEBUG_CMD 0
#define debug_cmd_print(...) \
    do { if (DEBUG_CMD) fprintf(stderr, __VA_ARGS__); } while (0)

#define DEBUG_MESSAGE_CONTENTS 0

namespace dai {
// namespace spi {

// static function definitions
static std::vector<std::uint8_t> serialize_metadata(const RawBuffer& msg);

void SpiApi::debug_print_hex(uint8_t * data, int len){
    for(int i=0; i<len; i++){
        if(i%80==0){
            printf("\n");
        }
        printf("%02x", data[i]);
    }
    printf("\n");
}

void SpiApi::debug_print_char(char * data, int len){
    for(int i=0; i<len; i++){
        printf("%c", data[i]);
    }
    printf("\n");
}



SpiApi::SpiApi(){
    chunk_message_cb = NULL;

    spi_proto_instance = (SpiProtocolInstance*) malloc(sizeof(SpiProtocolInstance));
    spi_send_packet = (SpiProtocolPacket*) malloc(sizeof(SpiProtocolPacket));
    spi_protocol_init(spi_proto_instance);
}

SpiApi::~SpiApi(){
    free(spi_proto_instance);
    free(spi_send_packet);
}

void SpiApi::set_send_spi_impl(uint8_t (*passed_send_spi)(const char*)){
    send_spi_impl = passed_send_spi;
}

void SpiApi::set_recv_spi_impl(uint8_t (*passed_recv_spi)(char*)){
    recv_spi_impl = passed_recv_spi;
}

void SpiApi::set_spi_transfer_impl(uint8_t (*transfer_impl)(const void*, size_t, void*, size_t)){
    spi_transfer_impl = transfer_impl;
}

uint8_t SpiApi::generic_send_spi(const char* spi_send_packet){
    return (*send_spi_impl)(spi_send_packet);
}

uint8_t SpiApi::generic_recv_spi(char* recvbuf){
    return (*recv_spi_impl)(recvbuf);
}

uint8_t SpiApi::generic_spi_transfer(const void* send_buffer, size_t send_size, void* receive_buffer, size_t receive_size){
    return (*spi_transfer_impl)(send_buffer, send_size, receive_buffer, receive_size);
}

uint8_t SpiApi::spi_get_size(SpiGetSizeResp *response, spi_command get_size_cmd, const char * stream_name){
    assert(isGetSizeCmd(get_size_cmd));

    uint8_t success = 0;
    debug_cmd_print("sending spi_get_size cmd.\n");
    spi_generate_command(spi_send_packet, get_size_cmd, strlen(stream_name)+1, stream_name);
    generic_send_spi((char*)spi_send_packet);

    debug_cmd_print("receive spi_get_size response from remote device...\n");
    char recvbuf[BUFF_MAX_SIZE] = {0};
    uint8_t recv_success = generic_recv_spi(recvbuf);

    if(recv_success){
        if(recvbuf[0]==START_BYTE_MAGIC){
            SpiProtocolPacket* spiRecvPacket = spi_protocol_parse(spi_proto_instance, (uint8_t*)recvbuf, sizeof(recvbuf));
            if(spiRecvPacket == nullptr){
                return false;
            }
            spi_parse_get_size_resp(response, spiRecvPacket->data);

            // Check if size = 0xFFFFFFFF -> no message available
            // TODO(themarpe) - hack, refactor SPI library as a whole
            if(response->size == 0xFFFFFFFFU){
                success = 0;
            } else {
                success = 1;
            }

        }else if(recvbuf[0] != 0x00){
            printf("*************************************** got a half/non aa packet ************************************************\n");
            success = 0;
        }
    } else {
        printf("failed to recv packet\n");
        success = 0;
    }

    return success;
}

uint8_t SpiApi::spi_get_message(SpiGetMessageResp *response, spi_command get_mess_cmd, const char * stream_name, uint32_t size){
    assert(isGetMessageCmd(get_mess_cmd));

    uint8_t success = 0;
    debug_cmd_print("sending spi_get_message cmd.\n");
    spi_generate_command(spi_send_packet, get_mess_cmd, strlen(stream_name)+1, stream_name);
    generic_send_spi((char*)spi_send_packet);

    uint32_t total_recv = 0;
    int debug_skip = 0;
    int error_count = 0;
    while(total_recv < size){
        if(debug_skip%20 == 0){
            debug_cmd_print("receive spi_get_message response from remote device... %d/%d\n", total_recv, size);
        }
        debug_skip++;

        char recvbuf[BUFF_MAX_SIZE] = {0};
        uint8_t recv_success = generic_recv_spi(recvbuf);
        if(recv_success){
            if(recvbuf[0]==START_BYTE_MAGIC){
                SpiProtocolPacket* spiRecvPacket = spi_protocol_parse(spi_proto_instance, (uint8_t*)recvbuf, sizeof(recvbuf));
                if(spiRecvPacket == nullptr){
                    error_count++;
                    if(error_count > 5){
                        //printf("Error %d/5...\n", error_count);
                        return false;
                    } else {
                        continue;
                    }
                }

                uint32_t remaining_data = size-total_recv;
                if ( remaining_data < PAYLOAD_MAX_SIZE ){
                    memcpy(response->data+total_recv, spiRecvPacket->data, remaining_data);
                    total_recv += remaining_data;
                } else {
                    memcpy(response->data+total_recv, spiRecvPacket->data, PAYLOAD_MAX_SIZE);
                    total_recv += PAYLOAD_MAX_SIZE;
                }

            }else if(recvbuf[0] != 0x00){
                //printf("*************************************** got a half/non aa packet ************************************************\n");
                error_count++;
                if(error_count > 5){
                    return false;
                }
                continue;
            }
        } else {
            //printf("failed to recv packet\n");
            error_count++;
            if(error_count > 5){
                return false;
            }
            continue;
        }
    }

    if(error_count == 0 && total_recv==size){
        spi_parse_get_message(response, size, get_mess_cmd);

        if(DEBUG_MESSAGE_CONTENTS){
            printf("data_size: %d\n", response->data_size);
            debug_print_hex((uint8_t*)response->data, response->data_size);
        }
        return true;
    } else {
        //printf("full packet not received %d/%d!\n", total_recv, size);
        return false;
    }

}



uint8_t SpiApi::spi_get_message_partial(SpiGetMessageResp *response, const char * stream_name, uint32_t offset, uint32_t size){
    uint8_t success = 0;
    debug_cmd_print("sending GET_MESSAGE_PART cmd.\n");
    spi_generate_command_partial(spi_send_packet, GET_MESSAGE_PART, strlen(stream_name)+1, stream_name, offset, size);
    generic_send_spi((char*)spi_send_packet);

    uint32_t total_recv = 0;
    int debug_skip = 0;
    int error_count = 0;
    while(total_recv < size){
        if(debug_skip%20 == 0){
            debug_cmd_print("receive GET_MESSAGE_PART response from remote device... %d/%d\n", total_recv, size);
        }
        debug_skip++;

        char recvbuf[BUFF_MAX_SIZE] = {0};
        uint8_t recv_success = generic_recv_spi(recvbuf);
        if(recv_success){
            if(recvbuf[0]==START_BYTE_MAGIC){
                SpiProtocolPacket* spiRecvPacket = spi_protocol_parse(spi_proto_instance, (uint8_t*)recvbuf, sizeof(recvbuf));
                if(spiRecvPacket == nullptr){
                    error_count++;
                    if(error_count > 5){
                        //printf("Error %d/5...\n", error_count);
                        return false;
                    } else {
                        continue;
                    }
                }

                uint32_t remaining_data = size-total_recv;
                if ( remaining_data < PAYLOAD_MAX_SIZE ){
                    memcpy(response->data+total_recv, spiRecvPacket->data, remaining_data);
                    total_recv += remaining_data;
                } else {
                    memcpy(response->data+total_recv, spiRecvPacket->data, PAYLOAD_MAX_SIZE);
                    total_recv += PAYLOAD_MAX_SIZE;
                }

            }else if(recvbuf[0] != 0x00){
                printf("*************************************** got a half/non aa packet ************************************************\n");
                break;
            }
        } else {
            printf("failed to recv packet\n");
            break;
        }
    }

    if(total_recv==size){
        spi_parse_get_message(response, size, GET_MESSAGE_PART);

        if(DEBUG_MESSAGE_CONTENTS){
            printf("data_size: %d\n", response->data_size);
            debug_print_hex((uint8_t*)response->data, response->data_size);
        }
        success = 1;
    } else {
        printf("full packet not received %d/%d!\n", total_recv, size);
        success = 0;
    }

    return success;
}





//-----------------------------------------------------------------------------------------------------
// public methods
//-----------------------------------------------------------------------------------------------------
uint8_t SpiApi::spi_pop_messages(){
    SpiStatusResp response;
    uint8_t success = 0;

    debug_cmd_print("sending POP_MESSAGES cmd.\n");
    spi_generate_command(spi_send_packet, POP_MESSAGES, strlen(NOSTREAM)+1, NOSTREAM);
    generic_send_spi((char*)spi_send_packet);

    debug_cmd_print("receive POP_MESSAGES response from remote device...\n");
    char recvbuf[BUFF_MAX_SIZE] = {0};
    uint8_t recv_success = generic_recv_spi(recvbuf);

    if(recv_success){
        if(recvbuf[0]==START_BYTE_MAGIC){
            SpiProtocolPacket* spiRecvPacket = spi_protocol_parse(spi_proto_instance, (uint8_t*)recvbuf, sizeof(recvbuf));
            if(spiRecvPacket == nullptr){
                return false;
            }

            spi_status_resp(&response, spiRecvPacket->data);
            if(response.status == SPI_MSG_SUCCESS_RESP){
                success = 1;
            }

        }else if(recvbuf[0] != 0x00){
            printf("*************************************** got a half/non aa packet ************************************************\n");
            success = 0;
        }
    } else {
        printf("failed to recv packet\n");
        success = 0;
    }

    return success;
}

uint8_t SpiApi::spi_pop_message(const char * stream_name){
    uint8_t success = 0;
    SpiStatusResp response;

    debug_cmd_print("sending POP_MESSAGE cmd.\n");
    spi_generate_command(spi_send_packet, POP_MESSAGE, strlen(stream_name)+1, stream_name);
    generic_send_spi((char*)spi_send_packet);

    debug_cmd_print("receive POP_MESSAGE response from remote device...\n");
    char recvbuf[BUFF_MAX_SIZE] = {0};
    uint8_t recv_success = generic_recv_spi(recvbuf);

    if(recv_success){
        if(recvbuf[0]==START_BYTE_MAGIC){
            SpiProtocolPacket* spiRecvPacket = spi_protocol_parse(spi_proto_instance, (uint8_t*)recvbuf, sizeof(recvbuf));
            if(spiRecvPacket == nullptr){
                return false;
            }

            spi_status_resp(&response, spiRecvPacket->data);
            if(response.status == SPI_MSG_SUCCESS_RESP){
                success = 1;
            }

        }else if(recvbuf[0] != 0x00){
            printf("*************************************** got a half/non aa packet ************************************************\n");
            success = 0;
        }
    } else {
        printf("failed to recv packet\n");
        success = 0;
    }

    return success;
}

std::vector<std::string> SpiApi::spi_get_streams(){
    SpiGetStreamsResp response;
    std::vector<std::string> streams;

    debug_cmd_print("sending GET_STREAMS cmd.\n");
    spi_generate_command(spi_send_packet, GET_STREAMS, 1, NOSTREAM);
    generic_send_spi((char*)spi_send_packet);

    debug_cmd_print("receive GET_STREAMS response from remote device...\n");
    char recvbuf[BUFF_MAX_SIZE] = {0};
    uint8_t recv_success = generic_recv_spi(recvbuf);

    if(recv_success){
        if(recvbuf[0]==START_BYTE_MAGIC){
            SpiProtocolPacket* spiRecvPacket = spi_protocol_parse(spi_proto_instance, (uint8_t*)recvbuf, sizeof(recvbuf));
            if(spiRecvPacket == nullptr){
                return {};
            }

            spi_parse_get_streams_resp(&response, spiRecvPacket->data);

            std::string currStr;
            for(int i=0; i<response.numStreams; i++){
                currStr = response.stream_names[i];
                streams.push_back(currStr);
            }
        }else if(recvbuf[0] != 0x00){
            printf("*************************************** got a half/non aa packet ************************************************\n");
        }
    } else {
        printf("failed to recv packet\n");
    }

    return streams;
}


//-----------------------------------------------------------------------------------------------------
// public methods
//-----------------------------------------------------------------------------------------------------

void SpiApi::transfer(const void* buffer, int size){

    const uint8_t* p_buffer = (const uint8_t*) buffer;

    //execute command
    int maxPayloadSize = SPI_PROTOCOL_PAYLOAD_SIZE;

    int numPackets = ( (size - 1) / maxPayloadSize) + 1;

    // Get a pointer to packet
    SpiProtocolPacket* packet = spi_send_packet;


    for(int i = 0; i < numPackets; i++){
        int toWrite = std::min(maxPayloadSize, size - (i*maxPayloadSize));
        // Create a packet to transmit
        auto ret = spi_protocol_write_packet(packet, p_buffer + i * maxPayloadSize, toWrite);
        assert(ret == SPI_PROTOCOL_OK);

        // Transmit the packet
        generic_send_spi((char*)packet);
    }
}

void SpiApi::transfer2(const void* buffer1, const void* buffer2, int size1, int size2){

    const uint8_t* p_buffer1 = (const uint8_t*) buffer1;
    const uint8_t* p_buffer2 = (const uint8_t*) buffer2;
    int totalsize = size1 + size2;

    //execute command
    int maxPayloadSize = SPI_PROTOCOL_PAYLOAD_SIZE;

    int numPackets = ( (totalsize - 1) / maxPayloadSize) + 1;

    // Get a pointer to packet
    SpiProtocolPacket* packet = spi_send_packet;

    for(int i = 0; i < numPackets; i++){
        int currOffset = i * maxPayloadSize;
        int toWrite = std::min(maxPayloadSize, totalsize - (currOffset));

        // case where we are sending from buffer1
        if(currOffset + maxPayloadSize <= size1){
            auto ret = spi_protocol_write_packet(packet, p_buffer1 + currOffset, toWrite);
            assert(ret == SPI_PROTOCOL_OK);
        // case where we are sending from both buffers
        } else if(currOffset + maxPayloadSize > size1 && currOffset < size1) {
            int buf1size = size1 - currOffset;
            int buf2size = maxPayloadSize - buf1size;
            auto ret = spi_protocol_write_packet2(packet, p_buffer1 + currOffset, p_buffer2, buf1size, buf2size);
            assert(ret == SPI_PROTOCOL_OK);
        // case where we are sending from buffer2
        } else {
            int currOffset2 = currOffset-size1;
            auto ret = spi_protocol_write_packet(packet, p_buffer2 + currOffset2, toWrite);
            assert(ret == SPI_PROTOCOL_OK);
        }

        // Transmit the packet
        generic_send_spi((char*)packet);
    }
}

uint8_t SpiApi::send_data(Data *sdata, const char* stream_name){
    uint8_t req_success = 0;
    SpiStatusResp response;

    spi_generate_command_send(spi_send_packet, SEND_DATA, strlen(stream_name)+1, stream_name, 0, sdata->size);
    generic_send_spi((char*)spi_send_packet);

    char recvbuf[BUFF_MAX_SIZE] = {0};
    uint8_t recv_success = generic_recv_spi(recvbuf);

    // actually send the data.
    if(recv_success){
        if(recvbuf[0]==START_BYTE_MAGIC){
            SpiProtocolPacket* spiRecvPacket = spi_protocol_parse(spi_proto_instance, (uint8_t*)recvbuf, sizeof(recvbuf));
            if(spiRecvPacket == nullptr){
                return false;
            }

            spi_status_resp(&response, spiRecvPacket->data);
            if(response.status == SPI_MSG_SUCCESS_RESP){
                transfer(sdata->data, sdata->size);
                req_success = 1;
            }

        }else if(recvbuf[0] != 0x00){
            printf("*************************************** got a half/non aa packet ************************************************\n");
            req_success = 0;
        }
    } else {
        printf("failed to recv packet\n");
        req_success = 0;
    }


    return req_success;
}



bool SpiApi::send_message(const std::shared_ptr<RawBuffer>& sp_msg, const char* stream_name){
    return send_message(*sp_msg, stream_name);
}

bool SpiApi::send_message(const RawBuffer& msg, const char* stream_name){
    bool req_success = false;
    SpiStatusResp response;
    uint32_t total_send_size;

    std::vector<uint8_t> metadata = serialize_metadata(msg);
    total_send_size = metadata.size() + msg.data.size();

    spi_generate_command_send(spi_send_packet, SEND_DATA, strlen(stream_name)+1, stream_name, metadata.size(), total_send_size);
    generic_send_spi((char*)spi_send_packet);

    char recvbuf[BUFF_MAX_SIZE] = {0};
    uint8_t recv_success = generic_recv_spi(recvbuf);

    // actually send the data.
    if(recv_success){
        // TODO: check for SPI_MSG_FAIL_RESP and don't send.
        if(recvbuf[0]==START_BYTE_MAGIC){
            SpiProtocolPacket* spiRecvPacket = spi_protocol_parse(spi_proto_instance, (uint8_t*)recvbuf, sizeof(recvbuf));
            if(spiRecvPacket == nullptr){
                return false;
            }

            spi_status_resp(&response, spiRecvPacket->data);
            if(response.status == SPI_MSG_SUCCESS_RESP){
                transfer2(msg.data.data(), metadata.data(), msg.data.size(), metadata.size());
                req_success = true;
            }

        }else if(recvbuf[0] != 0x00){
            printf("*************************************** got a half/non aa packet ************************************************\n");
            req_success = false;
        }
    } else {
        printf("failed to recv packet\n");
        req_success = false;
    }


    return req_success;
}


uint8_t SpiApi::req_data(Data *requested_data, const char* stream_name){
    uint8_t req_success = 0;
    SpiGetMessageResp get_message_resp;

    // do a get_size before trying to retreive message.
    SpiGetSizeResp get_size_resp;
    req_success = spi_get_size(&get_size_resp, GET_SIZE, stream_name);
    debug_cmd_print("req_data | spi_get_size response: %d, ret: %d\n", get_size_resp.size, req_success);

    // get message (assuming we got size)
    if(req_success){

        // If message has any data
        if(get_size_resp.size > 0){
            get_message_resp.data = (uint8_t*) malloc(get_size_resp.size);
            req_success = spi_get_message(&get_message_resp, GET_MESSAGE, stream_name, get_size_resp.size);
            if(req_success){
                requested_data->data = get_message_resp.data;
                requested_data->size = get_message_resp.data_size;
            } else {
                free(get_message_resp.data);
                return false;
            }
        } else {
            // message doesn't have any data
            requested_data->data = nullptr;
            requested_data->size = 0;
            req_success = 1;
        }

    }

    return req_success;
}

uint8_t SpiApi::req_metadata(Metadata *requested_data, const char* stream_name){
    uint8_t req_success = 0;
    SpiGetMessageResp get_message_resp;

    // do a get_size before trying to retreive message.
    SpiGetSizeResp get_size_resp;
    req_success = spi_get_size(&get_size_resp, GET_METASIZE, stream_name);
    debug_cmd_print("response: %d\n", get_size_resp.size);

    // get message (assuming we got size)
    if(req_success){
        get_message_resp.data = (uint8_t*) malloc(get_size_resp.size);
        if(get_message_resp.data){
            req_success = spi_get_message(&get_message_resp, GET_METADATA, stream_name, get_size_resp.size);
            if(req_success){
                requested_data->data = get_message_resp.data;
                requested_data->size = get_message_resp.data_size;
                requested_data->type = (dai::DatatypeEnum) get_message_resp.data_type;
            } else {
                free(get_message_resp.data);
                return false;
            }
        }else{
            printf("failed to allocate %d bytes\n", get_size_resp.size);
            req_success = 0;
        }
    }

    return req_success;
}

uint8_t SpiApi::req_data_partial(Data *requested_data, const char* stream_name, uint32_t offset, uint32_t offset_size){
    uint8_t req_success = 0;
    SpiGetMessageResp get_message_resp;

    SpiGetSizeResp get_size_resp;
    req_success = spi_get_size(&get_size_resp, GET_SIZE, stream_name);
    debug_cmd_print("response: %d\n", get_size_resp.size);

    if(req_success){
        // verify the specified part can be grabbed.
        if(offset+offset_size <= get_size_resp.size){
            get_message_resp.data = (uint8_t*) malloc(offset_size);
            if(get_message_resp.data){
                req_success = spi_get_message_partial(&get_message_resp, stream_name, offset, offset_size);
                if(req_success){
                    requested_data->data = get_message_resp.data;
                    requested_data->size = get_message_resp.data_size;
                } else {
                    free(get_message_resp.data);
                    return false;
                }
            } else {
                printf("failed to allocate %d bytes\n", get_size_resp.size);
                req_success = 0;
            }
        }else{
            printf("requested data is out of bounds\n");
            req_success = 0;
        }
    }

    return req_success;
}




uint8_t SpiApi::req_message(Message* received_msg, const char* stream_name){
    uint8_t req_success = 0;
    uint8_t req_data_success = 0;
    uint8_t req_meta_success = 0;

    // Nulling out just in case
    Metadata raw_meta = {};
    Data raw_data = {};

    // ----------------------------------------
    // example of receiving messages.
    // ----------------------------------------
    // the req_data method allocates memory for the received packet. we need to be sure to free it when we're done with it.
    req_data_success = req_data(&raw_data, stream_name);
    if(!req_data_success){
        return false;
    }

    // ----------------------------------------
    // example of getting message metadata
    // ----------------------------------------
    // the req_metadata method allocates memory for the received packet. we need to be sure to free it when we're done with it.
    req_meta_success = req_metadata(&raw_meta, stream_name);
    if(!req_meta_success){
        return false;
    }

    // TODO(themarpe) - check success return
    if(req_data_success && req_meta_success){
        received_msg->raw_data = raw_data;
        received_msg->raw_meta = raw_meta;
        received_msg->type = raw_meta.type;
        req_success = 1;
    }

    return req_success;
}

void SpiApi::free_message(Message* received_msg){
    free(received_msg->raw_data.data);
    free(received_msg->raw_meta.data);
}



void SpiApi::set_chunk_packet_cb(void (*passed_chunk_message_cb)(void*, uint32_t, uint32_t)){
    chunk_message_cb = passed_chunk_message_cb;
}

/*
void SpiApi::chunk_message(const char* stream_name){
    uint8_t req_success = 0;

    // do a get_size before trying to retreive message.
    SpiGetSizeResp get_size_resp;
    req_success = spi_get_size(&get_size_resp, GET_SIZE, stream_name);
    debug_cmd_print("get_size_resp: %d\n", get_size_resp.size);

    if(req_success){
        // send a get message command (assuming we got size)
        spi_generate_command(spi_send_packet, GET_MESSAGE, strlen(stream_name)+1, stream_name);
        generic_send_spi((char *)spi_send_packet);
        //usleep(1000);
        uint32_t message_size = get_size_resp.size;
        uint32_t total_recv = 0;
        printf("got message size - %d", message_size);
        while(total_recv < message_size){
            char recvbuf[BUFF_MAX_SIZE] = {0};
            req_success = generic_recv_spi(recvbuf);
            if(req_success){
                //if(recvbuf[0]==START_BYTE_MAGIC){

                    SpiProtocolPacket* spiRecvPacket = spi_protocol_parse(spi_proto_instance, (uint8_t*)recvbuf, sizeof(recvbuf));

                    if(spiRecvPacket != nullptr){
                        printf("chund - spi packet != null first byte: 0x%02X 0x%02X 0x%02X 0x%02X\n", recvbuf[0], recvbuf[1], recvbuf[2], recvbuf[3]);

                        // got packet, call callback with data
                        uint32_t remaining_data = message_size-total_recv;
                        uint32_t curr_packet_size = 0;
                        if ( remaining_data < PAYLOAD_MAX_SIZE ){
                            curr_packet_size = remaining_data;
                        } else {
                            curr_packet_size = PAYLOAD_MAX_SIZE;
                        }

                        chunk_message_cb((char*)spiRecvPacket->data, curr_packet_size, message_size);
                        total_recv += curr_packet_size;

                    }


                // }else if(recvbuf[0] != 0x00){
                //     //printf("*************************************** got a half/non aa packet ************************************************\n");
                //     //req_success = 0;
                //     //break;
                // }
            } else {
                printf("failed to recv packet\n");
                req_success = 0;
                break;
            }
        }
    }
}
*/

bool SpiApi::chunk_message(const char* stream_name){
    uint8_t req_success = 1;

    // do a get_size before trying to retreive message.
    SpiGetSizeResp get_size_resp;
    req_success = spi_get_size(&get_size_resp, GET_SIZE, stream_name);
    debug_cmd_print("get_size_resp: %d\n", get_size_resp.size);

    if(req_success){
        // send a get message command (assuming we got size)
        spi_generate_command(spi_send_packet, GET_MESSAGE, strlen(stream_name)+1, stream_name);
        generic_send_spi((char *)spi_send_packet);

        uint32_t message_size = get_size_resp.size;
        uint32_t total_recv = 0;
        int error_count = 0;

        while(total_recv < message_size){
            char recvbuf[BUFF_MAX_SIZE] = {0};
            req_success = generic_recv_spi(recvbuf);
            if(req_success){
                if(recvbuf[0]==START_BYTE_MAGIC){
                    SpiProtocolPacket* spiRecvPacket = spi_protocol_parse(spi_proto_instance, (uint8_t*)recvbuf, sizeof(recvbuf));
                    if(spiRecvPacket == nullptr){
                        error_count++;
                        if(error_count > 5){
                            //printf("Error %d/5...\n", error_count);
                            return false;
                        } else {
                            continue;
                        }
                    }

                    uint32_t remaining_data = message_size-total_recv;
                    uint32_t curr_packet_size = 0;
                    if ( remaining_data < PAYLOAD_MAX_SIZE ){
                        curr_packet_size = remaining_data;
                    } else {
                        curr_packet_size = PAYLOAD_MAX_SIZE;
                    }

                    if(chunk_message_cb != NULL){
                        chunk_message_cb((char*)spiRecvPacket->data, curr_packet_size, message_size);
                        if(DEBUG_MESSAGE_CONTENTS){
                            debug_print_hex((uint8_t*)spiRecvPacket->data, curr_packet_size);
                        }
                    } else {
                        printf("WARNING: chunk_message called without setting callback!");
                    }
                    total_recv += curr_packet_size;

                }else if(recvbuf[0] != 0x00){
                    printf("*************************************** got a half/non aa packet ************************************************\n");
                    req_success = 0;
                    break;
                }
            } else {
                printf("failed to recv packet\n");
                req_success = 0;
                break;
            }
        }

        if(error_count > 0){
            return false;
        }

    }

    return req_success;
}


bool SpiApi::chunk_message_buffer(const char* stream_name, uint8_t* buffer, size_t size){
    uint8_t req_success = 1;

    // do a get_size before trying to retreive message.
    SpiGetSizeResp get_size_resp;
    req_success = spi_get_size(&get_size_resp, GET_SIZE, stream_name);
    debug_cmd_print("get_size_resp: %d\n", get_size_resp.size);

    if(req_success == 0 && get_size_resp.size != 0xFFFFFFFFU){
        printf("Error receiving message\n");
        return false;
    }

    if(req_success){
        // send a get message command (assuming we got size)
        spi_generate_command(spi_send_packet, GET_MESSAGE, strlen(stream_name)+1, stream_name);
        generic_send_spi((char *)spi_send_packet);

        uint32_t message_size = get_size_resp.size;
        uint32_t total_recv = 0;
        bool errorReceiving = false;

        size_t offset = 0;
        std::thread pingPongThread;
        uint8_t* currentTemp = buffer;
        size_t currentTempSize = size / 2;
        uint8_t* currentSend = buffer + currentTempSize;

        while(total_recv < message_size){
            char recvbuf[BUFF_MAX_SIZE] = {0};
            req_success = generic_recv_spi(recvbuf);
            if(req_success){
                if(recvbuf[0]==START_BYTE_MAGIC){
                    SpiProtocolPacket* spiRecvPacket = spi_protocol_parse(spi_proto_instance, (uint8_t*)recvbuf, sizeof(recvbuf));
                    if(spiRecvPacket == nullptr){
                        errorReceiving = true;
                        break;
                    }

                    uint32_t remaining_data = message_size-total_recv;
                    uint32_t curr_packet_size = 0;
                    if ( remaining_data < PAYLOAD_MAX_SIZE ){
                        curr_packet_size = remaining_data;
                    } else {
                        curr_packet_size = PAYLOAD_MAX_SIZE;
                    }


                    // If buffer is full, send it out first
                    if(curr_packet_size + offset > currentTempSize){
                        //printf("Added up to: %d, with cur packet size: %d\n", offset, curr_packet_size);

                        // Wait until its send
                        if(pingPongThread.joinable()) pingPongThread.join();
                        std::swap(currentTemp, currentSend);
                        pingPongThread = std::thread([this, currentSend, offset, message_size]{
                            if(chunk_message_cb != nullptr){
                                chunk_message_cb((char*)currentSend, offset, message_size);
                            }
                        });

                        offset = 0;
                    }
                    // Append to temporary buffer
                    memcpy(&currentTemp[offset], spiRecvPacket->data, curr_packet_size);
                    offset += curr_packet_size;

                    // if(chunk_message_cb != NULL){
                    //     chunk_message_cb((char*)spiRecvPacket->data, curr_packet_size, message_size);
                    //     if(DEBUG_MESSAGE_CONTENTS){
                    //         debug_print_hex((uint8_t*)spiRecvPacket->data, curr_packet_size);
                    //     }
                    // } else {
                    //     printf("WARNING: chunk_message called without setting callback!");
                    // }

                    total_recv += curr_packet_size;

                }else if(recvbuf[0] != 0x00){
                    errorReceiving = true;
                    break;
                }
            } else {
                errorReceiving = true;
                break;
            }
        }


        if(!errorReceiving){
            if(offset != 0){
                if(pingPongThread.joinable()) pingPongThread.join();
                std::swap(currentTemp, currentSend);
                pingPongThread = std::thread([this, currentSend, offset, message_size]{
                    if(chunk_message_cb != nullptr){
                        chunk_message_cb((char*)currentSend, offset, message_size);
                    }
                });

                offset = 0;
            }
        } else {
            printf("Error receiving message\n");
            req_success = 0;
        }

        // At the end join ping pong thread if possible
        if(pingPongThread.joinable()) pingPongThread.join();
    }

    return req_success;
}

// Static functions

// Serialize only metadata into a separate vector
std::vector<std::uint8_t> serialize_metadata(const RawBuffer& msg) {
    std::vector<std::uint8_t> ser;

    // Serialization:
    // 1. serialize and append metadata
    // 2. append datatype enum (4B LE)
    // 3. append size (4B LE) of serialized metadata

    std::vector<std::uint8_t> metadata;
    DatatypeEnum datatype;
    msg.serialize(metadata, datatype);
    uint32_t metadataSize = metadata.size();

    // 4B datatype & 4B metadata size
    std::uint8_t leDatatype[4];
    std::uint8_t leMetadataSize[4];
    for(int i = 0; i < 4; i++) leDatatype[i] = (static_cast<std::int32_t>(datatype) >> (i * 8)) & 0xFF;
    for(int i = 0; i < 4; i++) leMetadataSize[i] = (metadataSize >> i * 8) & 0xFF;

    ser.insert(ser.end(), metadata.begin(), metadata.end());
    ser.insert(ser.end(), leDatatype, leDatatype + sizeof(leDatatype));
    ser.insert(ser.end(), leMetadataSize, leMetadataSize + sizeof(leMetadataSize));
    return ser;
}


// }  // namespace spi
}  // namespace dai

