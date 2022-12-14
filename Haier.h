/**
* Create by Miguel Ángel López on 20/07/19
**/

#ifndef HAIER_ESP_HAIER_H
#define HAIER_ESP_HAIER_H

#include "esphome.h"

using namespace esphome;
using namespace esphome::climate;


#define TEMPERATURE         13
#define HUMIDITY            15

#define COMMAND             17


#define MODE                23
#define MODE_SMART          0
#define MODE_COOL           1
#define MODE_HEAT           2
#define MODE_ONLY_FAN       3
#define MODE_DRY            4

#define FAN_SPEED           25
#define FAN_MIN             2
#define FAN_MIDDLE          1
#define FAN_MAX             0
#define FAN_AUTO            3

#define SWING               27
#define SWING_MODE          31
#define SWING_OFF           00
#define SWING_VERTICAL      01
#define SWING_HORIZONTAL    02
#define SWING_BOTH          01


#define SWING_HORIZONTAL_MASK   (1 << 3)
#define SWING_VERTICAL_MASK     (1 << 4)


#define LOCK                28
#define LOCK_ON             80
#define LOCK_OFF            0


#define POWER               29
#define POWER_MASK          1


#define FRESH               31
#define FRESH_ON            1
#define FRESH_OFF           0


#define SET_TEMPERATURE     35
#define DECIMAL_MASK        (1 << 5)


#define CRC                 36


#define CONFORT_PRESET_MASK     (1 << 3)


#define MIN_SET_TEMPERATURE 16
#define MAX_SET_TEMPERATURE 30
#define STEP_TEMPERATURE 0.5f


#define MIN_VALID_TEMPERATURE 10
#define MAX_VALID_TEMPERATURE 50


class Haier : public Climate, public PollingComponent {

private:

    byte lastCRC;
    byte data[37];
    byte poll[13] = {255, 255, 10, 0, 0, 0, 0, 0, 1, 1, 77, 1, 90};
    byte on[13] = {255, 255, 10, 0, 0, 0, 0, 0, 1, 1, 77, 2, 91};
    byte off[13] = {255, 255, 10, 0, 0, 0, 0, 0, 1, 1, 77, 3, 92};


    bool swing;


public:


    Haier(bool swing) : PollingComponent(5 * 1000) {
        lastCRC = 0;
        this->swing = swing;
    }


    void setup() override {

        Serial.begin(9600);


    }

    void loop() override {


        if (Serial.available() > 0) {

            if (Serial.read() != 255) return;
            if (Serial.read() != 255) return;

            data[0] = 255;
            data[1] = 255;

            Serial.readBytes(data + 2, sizeof(data) - 2);

            readData();

        }

    }

    void update() override {

        Serial.write(poll, sizeof(poll));
        auto raw = getHex(poll, sizeof(poll));
        ESP_LOGD("Haier", "POLL: %s ", raw.c_str());
    }

protected:
    ClimateTraits traits() override {
        auto traits = climate::ClimateTraits();


        traits.set_supported_modes({
                                           climate::CLIMATE_MODE_OFF,
                                           climate::CLIMATE_MODE_HEAT_COOL,
                                           climate::CLIMATE_MODE_COOL,
                                           climate::CLIMATE_MODE_HEAT,
                                           climate::CLIMATE_MODE_FAN_ONLY,
                                           climate::CLIMATE_MODE_DRY
                                   });

        traits.set_supported_fan_modes({
                                               climate::CLIMATE_FAN_AUTO,
                                               climate::CLIMATE_FAN_LOW,
                                               climate::CLIMATE_FAN_MEDIUM,
                                               climate::CLIMATE_FAN_HIGH,
                                               climate::CLIMATE_FAN_MIDDLE,
                                       });


        if (swing) {
            traits.set_supported_swing_modes({
                                                     climate::CLIMATE_SWING_OFF,
                                                     climate::CLIMATE_SWING_BOTH,
                                                     climate::CLIMATE_SWING_VERTICAL,
                                                     climate::CLIMATE_SWING_HORIZONTAL
                                             });
        }

        traits.set_visual_min_temperature(MIN_SET_TEMPERATURE);
        traits.set_visual_max_temperature(MAX_SET_TEMPERATURE);
        traits.set_visual_temperature_step(STEP_TEMPERATURE);
        traits.set_supports_current_temperature(true);
        traits.set_supports_two_point_target_temperature(false);


        traits.add_supported_preset(CLIMATE_PRESET_NONE);
        traits.add_supported_preset(CLIMATE_PRESET_COMFORT);


        return traits;
    }

public:

    void readData() {


        auto raw = getHex(data, sizeof(data));
        ESP_LOGD("Haier", "Readed message: %s ", raw.c_str());


        byte check = data[CRC];

        getChecksum(data, sizeof(data));

        if (check != data[CRC]) {
            ESP_LOGD("Haier", "Invalid checksum");
            return;
        }


        lastCRC = check;


        if (MIN_VALID_TEMPERATURE < data[TEMPERATURE] && data[TEMPERATURE] < MAX_VALID_TEMPERATURE)
            current_temperature = data[TEMPERATURE];


        target_temperature = data[SET_TEMPERATURE] + 16;


        if (data[POWER] & DECIMAL_MASK)
            target_temperature += 0.5f;


        switch (data[MODE]) {
            case MODE_SMART:
                mode = CLIMATE_MODE_HEAT_COOL;
                break;
            case MODE_COOL:
                mode = CLIMATE_MODE_COOL;
                break;
            case MODE_HEAT:
                mode = CLIMATE_MODE_HEAT;
                break;
            case MODE_ONLY_FAN:
                mode = CLIMATE_MODE_FAN_ONLY;
                break;
            case MODE_DRY:
                mode = CLIMATE_MODE_DRY;
                break;
            default:
                mode = CLIMATE_MODE_HEAT_COOL;
        }


        switch (data[FAN_SPEED]) {
            case FAN_AUTO:
                fan_mode = CLIMATE_FAN_AUTO;
                break;

            case FAN_MIN:
                fan_mode = CLIMATE_FAN_LOW;
                break;

            case FAN_MIDDLE:
                fan_mode = CLIMATE_FAN_MEDIUM;
                break;

            case FAN_MAX:
                fan_mode = CLIMATE_FAN_HIGH;
                break;
        }


        if (data[SWING] == SWING_OFF) {
            if (data[SWING_MODE] & SWING_HORIZONTAL_MASK)
                swing_mode = CLIMATE_SWING_HORIZONTAL;
            else if (data[SWING_MODE] & SWING_VERTICAL_MASK)
                swing_mode = CLIMATE_SWING_VERTICAL;
            else
                swing_mode = CLIMATE_SWING_OFF;
        } else if (data[SWING] == SWING_BOTH)
            swing_mode = CLIMATE_SWING_BOTH;


        if (data[POWER] & CONFORT_PRESET_MASK)
            preset = CLIMATE_PRESET_COMFORT;
        else
            preset = CLIMATE_PRESET_NONE;


        if ((data[POWER] & POWER_MASK) == 0) {
            mode = CLIMATE_MODE_OFF;
        }


        this->publish_state();

    }


    void control(const ClimateCall &call) override {



        if (call.get_mode().has_value()) {
            switch (call.get_mode().value()) {
                case CLIMATE_MODE_OFF:
                    sendData(off, sizeof(off));
                    break;

                case CLIMATE_MODE_HEAT_COOL:
                case CLIMATE_MODE_AUTO:
                    data[POWER] |= POWER_MASK;
                    data[MODE] = MODE_SMART;
                    break;
                case CLIMATE_MODE_HEAT:
                    data[POWER] |= POWER_MASK;
                    data[MODE] = MODE_HEAT;
                    break;
                case CLIMATE_MODE_COOL:
                    data[POWER] |= POWER_MASK;
                    data[MODE] = MODE_COOL;
                    break;

                case CLIMATE_MODE_FAN_ONLY:
                    data[POWER] |= POWER_MASK;
                    data[MODE] = MODE_ONLY_FAN;
                    break;

                case CLIMATE_MODE_DRY:
                    data[POWER] |= POWER_MASK;
                    data[MODE] = MODE_DRY;
                    break;

            }

        }


        if (call.get_preset().has_value()) {


            if (call.get_preset().value() == CLIMATE_PRESET_COMFORT)
                data[POWER] |= CONFORT_PRESET_MASK;
            else
                data[POWER] &= ~CONFORT_PRESET_MASK;


        }


        if (call.get_target_temperature().has_value()) {

            float target = call.get_target_temperature().value() - 16;

            data[SET_TEMPERATURE] = (uint16_t) target;

            if ((int) target == (int) (target + 0.5))
                data[POWER] &= ~DECIMAL_MASK;
            else
                data[POWER] |= DECIMAL_MASK;

        }

        if (call.get_fan_mode().has_value()) {
            switch (call.get_fan_mode().value()) {
                case CLIMATE_FAN_AUTO:
                    data[FAN_SPEED] = FAN_AUTO;
                    break;
                case CLIMATE_FAN_LOW:
                    data[FAN_SPEED] = FAN_MIN;
                    break;
                case CLIMATE_FAN_MEDIUM:
                    data[FAN_SPEED] = FAN_MIDDLE;
                    break;
                case CLIMATE_FAN_HIGH:
                    data[FAN_SPEED] = FAN_MAX;
                    break;

                case CLIMATE_FAN_ON:
                case CLIMATE_FAN_OFF:
                case CLIMATE_FAN_MIDDLE:
                case CLIMATE_FAN_FOCUS:
                case CLIMATE_FAN_DIFFUSE:
                    break;
            }

        }


        if (call.get_swing_mode().has_value())
            switch (call.get_swing_mode().value()) {
                case CLIMATE_SWING_OFF:
                    data[SWING] = SWING_OFF;
                    data[SWING_MODE] &= ~1;
                    break;
                case CLIMATE_SWING_VERTICAL:
                    data[SWING] = SWING_OFF;
                    data[SWING_MODE] |= SWING_VERTICAL_MASK;
                    data[SWING_MODE] &= ~SWING_HORIZONTAL_MASK;
                    break;
                case CLIMATE_SWING_HORIZONTAL:
                    data[SWING] = SWING_OFF;
                    data[SWING_MODE] |= SWING_HORIZONTAL_MASK;
                    data[SWING_MODE] &= ~SWING_VERTICAL_MASK;
                    break;
                case CLIMATE_SWING_BOTH:
                    data[SWING] = SWING_BOTH;
                    data[SWING_MODE] &= ~SWING_HORIZONTAL_MASK;
                    data[SWING_MODE] &= ~SWING_VERTICAL_MASK;
                    break;
            }





        //Values for "send"
        data[COMMAND] = 0;
        data[9] = 1;
        data[10] = 77;
        data[11] = 95;

        sendData(data, sizeof(data));


    }


    void sendData(byte *message, byte size) {

        byte crc = getChecksum(message, size);
        Serial.write(message, size - 1);
        Serial.write(crc);

        auto raw = getHex(message, size);
        ESP_LOGD("Haier", "Sended message: %s ", raw.c_str());

    }

    String getHex(byte *message, byte size) {

        String raw;

        for (int i = 0; i < size; i++) {
            raw += "\n" + String(i) + "-" + String(message[i]);

        }
        raw.toUpperCase();

        return raw;


    }

    byte getChecksum(const byte *message, size_t size) {
        byte position = size - 1;
        byte crc = 0;

        for (int i = 2; i < position; i++)
            crc += message[i];

        return crc;

    }


};


#endif //HAIER_ESP_HAIER_H
