/**
* Create by Miguel Ángel López on 20/07/19
**/

#ifndef HAIER_ESP_HAIER_H
#define HAIER_ESP_HAIER_H

#include "esphome.h"

using namespace esphome;
using namespace esphome::climate;


#define TEMPERATURE   13
#define COMMAND       17


#define MODE 23
#define MODE_SMART 00
#define MODE_COOL 01
#define MODE_HEAT 02
#define MODE_ONLY_FAN 03
#define MODE_DRY 04

#define FAN_SPEED   25
#define FAN_MIN     02
#define FAN_MIDDLE  01
#define FAN_MAX     02
#define FAN_AUTO    03

#define SWING        27
#define SWING_OFF          00
#define SWING_VERTICAL     01
#define SWING_HORIZONTAL   02
#define SWING_BOTH

#define LOCK        28
#define LOCK_ON     80
#define LOCK_OFF    00

#define POWER       29
#define POWER_ON    9
#define POWER_OFF   8


#define FRESH       31
#define FRESH_ON    01
#define FRESH_OFF   00

#define SET_TEMPERATURE 35

#define CRC 36

class Haier : public Climate, public PollingComponent {

private:

    byte lastCRC;
    byte data[37];
    byte poll[13] = {255,255,10,0,0,0,0,0,1,1,77,1,90};
    byte on[13] = {255,255,10,0,0,0,0,0,1,1,77,2,91};



public:

    Haier() : PollingComponent(5 * 1000) {
        lastCRC = 0;
    }


    
    void setup() override {
        
        Serial.begin(9600);
        
        
    }

    void loop() override  {


        if (Serial.available() > 0) {
            
			if (Serial.read() != 255) return;
			if (Serial.read() != 255) return;
			
			data[0] = 255;
			data[1] = 255;
			
            Serial.readBytes(data+2, sizeof(data)-2);

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
        traits.set_supports_away(false);
        traits.set_supports_auto_mode(true);
        traits.set_supports_heat_mode(true);
        traits.set_supports_cool_mode(true);
        traits.set_visual_max_temperature(10);
        traits.set_visual_max_temperature(50);
        traits.set_visual_temperature_step(1.0f);
        traits.set_supports_current_temperature(true);

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

        current_temperature = data[TEMPERATURE];
        target_temperature = data[SET_TEMPERATURE] + 16;


        if (data[POWER] == POWER_OFF) {
            mode = CLIMATE_MODE_OFF;

        } else {

            switch (data[MODE]) {
                case MODE_COOL:
                    mode = CLIMATE_MODE_COOL;
                    break;
                case MODE_HEAT:
                    mode = CLIMATE_MODE_HEAT;
                    break;
                default:
                    mode = CLIMATE_MODE_AUTO;
            }


        }

        this->publish_state();

    }


    void control(const ClimateCall &call) override {

        if (call.get_mode().has_value())
            switch (call.get_mode().value()) {
                case CLIMATE_MODE_OFF:
                    data[POWER] = POWER_OFF;
                    break;
                case CLIMATE_MODE_AUTO:
                    data[POWER] = POWER_ON;
                    data[MODE] = MODE_SMART;
                    break;
                case CLIMATE_MODE_HEAT:
                    data[POWER] = POWER_ON;
                    data[MODE] = MODE_HEAT;
                    break;
                case CLIMATE_MODE_COOL:
                    data[POWER] = POWER_ON;
                    data[MODE] = MODE_COOL;
                    break;
            }


        if (call.get_target_temperature().has_value()) {
            data[SET_TEMPERATURE] = (uint16) call.get_target_temperature().value() - 16;
        }

        //Values for "send"
        data[COMMAND] = 0;
        data[9] = 1;
        data[10] = 77;
        data[11] = 95;

        sendData(data, sizeof(data));


    }


    void sendData(byte * message, byte size) {

        byte crc = getChecksum(message, size);
        Serial.write(message, size-1);
        Serial.write(crc);

        auto raw = getHex(message, size);
        ESP_LOGD("Haier", "Sended message: %s ", raw.c_str());

    }

    String getHex(byte * message, byte size) {

        String raw;

        for (int i=0; i < size; i++){
            raw += "\n" + String(i) + "-" + String(message[i]);

        }
        raw.toUpperCase();

        return raw;


    }

    byte getChecksum(const byte * message, size_t size) {
        byte position = size - 1;
        byte crc = 0;

        for (int i = 2; i < position; i++)
            crc += message[i];

        return crc;

    }




};


#endif //HAIER_ESP_HAIER_H
