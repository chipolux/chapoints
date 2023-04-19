#ifndef SHOCK_H
#define SHOCK_H

#include <vector>

namespace shock
{

/* Message Format:
 *      always 57 bits
 *      4 bits: message type? seems to differ for mode switch, power toggle, etc.
 *          0111 = shock/vibrate message
 *          1000 = mode switch message
 *          0100 = power off message
 *          0010 = power on message
 *      4 bits: mode number (0-5) for mode switch message, otherwise all 0.
 *      40 bits: unknown but does vary!
 *      9 bits: trailer? always identical for every known message 001100100
 */

enum MessageType {
    // this works, same as setting mode 0 and blowing into mic, vibration and
    // then a shock
    TEST_SHOCK = 0,
    // mode selection only shows number on display, mainboard mcu has the
    // real mode, and pressing the button will just get back in order
    MODE_0,
    MODE_1,
    MODE_2,
    MODE_3,
    MODE_4,
    MODE_5,
    // power on makes a nice short vibration and maybe beeps
    POWER_ON,
    // power off does nothing, maybe it just beeps
    POWER_OFF,

    // there are likely a few more types:
    //      vibrate, beep, and shock as triggered by barking
    //      vibrate, beep due to low battery
    //      variations of those with different vibe/shock/beep levels
};

void sendBit(const uint8_t &pin, const bool &bit)
{
    // A 1 bit is 480us of 3.3v and then 160us 0v/ground.
    // A 0 bit is 160us of 3.3v and then 480us 0v/ground.
    // Always leave line pulled to ground in between bits.
    if (bit) {
        digitalWrite(pin, HIGH);
        delayMicroseconds(480);
        digitalWrite(pin, LOW);
        delayMicroseconds(160);
    } else {
        digitalWrite(pin, HIGH);
        delayMicroseconds(160);
        digitalWrite(pin, LOW);
        delayMicroseconds(480);
    }
}

void sendMessage(const uint8_t &pin, const MessageType &messageType)
{
    // setup message based on type
    std::vector<bool> message;
    switch (messageType) {
    case TEST_SHOCK: {
        message = {
            // clang-format off
            0, 1, 1, 1, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 1, 1, 0,
            0, 0, 0, 0, 0, 1, 1, 0,
            0, 0, 0, 0, 0, 0, 1, 1,
            0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 1, 0, 1,
            0, 0, 1, 1, 0, 0, 1, 0,
            0
            // clang-format on
        };
        break;
    }
    case MODE_0: {
        message = {
            // clang-format off
            1, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 
            0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 1, 1, 1, 1, 0,
            0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 1, 1, 0, 0, 1, 0,
            0,
            // clang-format on
        };
        break;
    }
    case MODE_1: {
        message = {
            // clang-format off
            1, 0, 0, 0, 0, 0, 0, 1,
            0, 0, 0, 0, 0, 0, 0, 0, 
            0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 1, 1, 1, 1, 0,
            0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 1, 1, 0, 0, 1, 0,
            0,
            // clang-format on
        };
        break;
    }
    case MODE_2: {
        message = {
            // clang-format off
            1, 0, 0, 0, 0, 0, 1, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 
            0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 1, 1, 1, 1, 0,
            0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 1, 1, 0, 0, 1, 0,
            0,
            // clang-format on
        };
        break;
    }
    case MODE_3: {
        message = {
            // clang-format off
            1, 0, 0, 0, 0, 0, 1, 1,
            0, 0, 0, 0, 0, 0, 0, 0, 
            0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 1, 1, 1, 1, 0,
            0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 1, 1, 0, 0, 1, 0,
            0,
            // clang-format on
        };
        break;
    }
    case MODE_4: {
        message = {
            // clang-format off
            1, 0, 0, 0, 0, 1, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 
            0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 1, 1, 1, 1, 0,
            0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 1, 1, 0, 0, 1, 0,
            0,
            // clang-format on
        };
        break;
    }
    case MODE_5: {
        message = {
            // clang-format off
            1, 0, 0, 0, 0, 1, 0, 1,
            0, 0, 0, 0, 0, 0, 0, 0, 
            0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 1, 1, 1, 1, 0,
            0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 1, 1, 0, 0, 1, 0,
            0,
            // clang-format on
        };
        break;
    }
    case POWER_ON: {
        message = {
            // clang-format off
            0, 0, 1, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 1,
            0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 1, 1, 0, 0, 1, 0,
            0,
            // clang-format on
        };
        break;
    }
    case POWER_OFF: {
        message = {
            // clang-format off
            0, 1, 0, 0, 0, 0, 0, 0,
            1, 1, 1, 1, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 1, 1, 0, 0, 1, 0,
            0,
            // clang-format on
        };
        break;
    }
    }

    // send message
    pinMode(pin, OUTPUT);
    digitalWrite(pin, LOW);
    const auto size = message.size();
    auto i = size;
    for (i = 0; i < size; ++i) {
        sendBit(pin, message[i]);
    }
    delayMicroseconds(2240); // TODO: how sensitive is this gap?
    for (i = 0; i < size; ++i) {
        sendBit(pin, message[i]);
    }
    digitalWrite(pin, LOW);
    pinMode(pin, INPUT);
}

void setupPin(const uint8_t &pin)
{
    // pin should be floating low initially
    pinMode(pin, OUTPUT);
    digitalWrite(pin, LOW);
    pinMode(pin, INPUT);
}
}; // namespace shock

#endif // SHOCK_H
