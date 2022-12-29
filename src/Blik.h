#pragma once

#include "mbed.h"
#include <stdint.h>
#include <chrono>

static const uint8_t BLIK_MESSAGE_MAXIMUM_PAYLOAD_SIZE = 118;

struct BlikMessage {
    uint32_t type;
    uint8_t data[BLIK_MESSAGE_MAXIMUM_PAYLOAD_SIZE];
    uint8_t size;
};

struct BlikReceiveBuffer {
    uint32_t canId;
    uint8_t data[BLIK_MESSAGE_MAXIMUM_PAYLOAD_SIZE];
    uint8_t size;
    int8_t lastframeindex;
    uint8_t bufferSize;
    std::chrono::time_point<mbed::internal::OsClock> timestamp;
};

class Blik {
    public:
        Blik(CAN* can, events::EventQueue* event_queue);
        void send(uint32_t id, uint8_t* data, uint8_t size);
        void onMessage(mbed::Callback<void(BlikMessage)> callback);

    private:
        CAN* can;
        void canWrite(uint32_t id, uint8_t* data, uint8_t size);
        void canRead();
        void canReadInterrupt();

        EventQueue queue;
        LowPowerTimer timer;

        static const uint32_t EMPTY = 0xFFFFFFFF;
        
        // TODO: this does not work yet....
        //static constexpr std::chrono::seconds BUFFER_TIMEOUT = 1s;
        std::array<BlikReceiveBuffer, MBED_CONF_BLIK_RECEIVE_BUFFER_SIZE> buffer;

        mbed::Callback<void(BlikMessage)> messageCallback;
        
        // Might be a better solution: https://github.com/ARMmbed/mbed-os/issues/6714#issuecomment-558863389
        static const uint32_t FMPIE0 = 0x01; // FIFO message pending interrupt enable
};