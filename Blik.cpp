#include "Blik.h"
#include <algorithm>
#include "logger.h"

using namespace std::chrono;

#define LOGGER_MODULE_NAME "BLIK"
unsigned loggerLevel = LOGGER_LEVEL_DEBUG;

Blik::Blik(CAN* can): eventThread(osPriorityHigh), queue(32 * EVENTS_EVENT_SIZE) {
    this->can = can;
    NOTICE("*** Start Blik ***\r\n");

    timer.start();
    DEBUG("Create blik message buffer for %d message", BUFFER_SIZE);
    for(uint8_t i = 0; i < BUFFER_SIZE; i++) {
        buffer[i] = { EMPTY, { 0 }, 0, 0, 0, Kernel::Clock::now() };
    }

    eventThread.start(callback(&queue, &EventQueue::dispatch_forever));
    can->attach(callback(this, &Blik::canReadInterrupt), CAN::RxIrq);

    // TODO: set can filter() to receive only messages for this device
}

void Blik::send(uint32_t id, uint8_t* data, uint8_t size){
    NOTICE("Sending blik message")
    if(size <= 7) {
        INFO("Sending single frame of %d bytes", size);
        unsigned char payload[8] = { 0 };
        payload[0] = size & 0x0F;
        std::memcpy(payload + 1, data, size);
        canWrite(id, payload, size + 1);

    } else if(size <= BLIK_MESSAGE_MAXIMUM_PAYLOAD_SIZE) {
        uint8_t bytes_sent = 0;
        uint8_t frame_counter = 0;

        unsigned char payload[8] = { 0 };
        INFO("Sending first frame (of %d bytes total)", size);
        payload[0] = 0x10;
        payload[1] = size;
        std::memcpy(payload + 2, data, 6);
        canWrite(id, payload, 8);
        bytes_sent += 6;

        while( bytes_sent < size ) {
            payload[0] = 0x20 | frame_counter;
            uint8_t payload_size = std::min<uint8_t>(7, size - bytes_sent);
            INFO("Sending consecutive frame (%d) of size %d", frame_counter, payload_size);
            std::memcpy(payload + 1, data + bytes_sent, payload_size);
            canWrite(id, payload, payload_size + 1);
            bytes_sent += payload_size;
            frame_counter++;
        }
    } else {
        ERROR("Blik message of size %d is to large. The message should be less than %d bytes", size, BLIK_MESSAGE_MAXIMUM_PAYLOAD_SIZE);
    }
}

void Blik::canWrite(uint32_t id, uint8_t* data, uint8_t size) {
    CANMessage msg(
        id,
        data,
        size,
        CANType::CANData,
        CANFormat::CANExtended
    );
    can->write(msg);
}

void Blik::canRead() {
    CANMessage msg;
    if (!can->read(msg)) {
        ERROR("Blik::canRead interrupt without message");
        return;
    }
    CAN1->IER |= (1UL << FMPIE0);

    // single frame
    if((msg.data[0] & 0xF0) == 0x00) {
        BlikMessage message = {msg.id, {0}, msg.len};
        std::memcpy(message.data, msg.data + 1, msg.len - 1);

        messageCallback(message);
        NOTICE("Received blik message");
        INFO("Received single frame message of %d bytes", message.size);
    }

    // first frame
    if((msg.data[0] & 0xF0) == 0x10) {
        INFO("Received first frame");
        for(uint8_t i = 0; i < BUFFER_SIZE; i++) {
            BlikReceiveBuffer* buf = &buffer[i];
            if(buf->canId == EMPTY) {
                uint8_t payloadSize = msg.len - 2; // 2 == header size
                buf->canId = msg.id;
                std::memcpy(buf->data, msg.data + 2, payloadSize);
                buf->size = msg.data[1];
                buf->lastframeindex = -1;
                buf->bufferSize = 6;
                buf->timestamp = Kernel::Clock::now();
                INFO("Added first frame message to buffer for id %d", int(buf->canId));
                break;
            }
            ERROR("BlikReceiveBuffer FULL !");
        }
    }

    // consecutive frame
    if((msg.data[0] & 0xF0) == 0x20) {
        INFO("Received consecutive frame");
        for(uint8_t i = 0; i < BUFFER_SIZE; i++){
            BlikReceiveBuffer* buf = &buffer[i];
            if(buf->canId == msg.id){
                uint8_t frameindex = msg.data[0] & 0x0F;
                uint8_t payloadSize = msg.len - 1; // 1 == header size
                if(frameindex != buf->lastframeindex + 1) {
                    ERROR("BlikReceiveBuffer frame lost/out of sync!");
                    break;
                }
                std::memcpy(buf->data + buf->bufferSize, msg.data + 1, payloadSize);
                buf->lastframeindex = frameindex;
                buf->bufferSize += payloadSize;

                // last frame received, frame complete 
                if(buf->bufferSize == buf->size) {
                    BlikMessage message = {buf->canId, {0}, buf->size};
                    std::memcpy(message.data, buf->data, buf->size);
                    messageCallback(message);

                    NOTICE("Received blik message");
                    INFO("Received message of %d bytes", message.size);

                    buf->canId = EMPTY;
                }
                break;
            }
            ERROR("No buffered messages found for id %x", buf->canId);
        }
    } 

    INFO("Cleaning up old messages from buffer")
    for(uint8_t i = 0; i < BUFFER_SIZE; i++) {
        BlikReceiveBuffer* buf = &buffer[i];
        // TODO: use BUFFER_TIMEOUT from header file instead, does not work yet
        if(buf->canId != EMPTY && Kernel::Clock::now() - buf->timestamp > 1s) {
            DEBUG("Removed old message from BlikReceiveBuffer (id: %X)!", int(buf->canId));
            buf->canId = EMPTY;
        }
    }
}

void Blik::canReadInterrupt() {
    // FIFO message pending interrupt enable
    CAN1->IER &= ~(1UL << FMPIE0);
    queue.call(Callback<void()>(this, &Blik::canRead));
}

void Blik::onMessage(mbed::Callback<void(BlikMessage)> callback) {
    messageCallback = callback;
}