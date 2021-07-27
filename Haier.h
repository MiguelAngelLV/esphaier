/**
* Create by Miguel Ángel López on 20/07/19
**/

#ifndef HAIER_ESP_HAIER_H
#define HAIER_ESP_HAIER_H

#include "esphome.h"

using namespace esphome;
using namespace esphome::climate;

#define TEMPERATURE     13
#define COMMAND         17

#define MODE            23
#define MODE_SMART      0
#define MODE_COOL       1
#define MODE_HEAT       2
#define MODE_FAN_ONLY   3
#define MODE_DRY        4

#define FAN_SPEED       25
#define FAN_HIGH        0
#define FAN_MIDDLE      1
#define FAN_MEDIUM      2
#define FAN_AUTO        3

#define SWING           27
#define SWING_OFF       0
#define SWING_BOTH      1

#define LOCK            28
#define LOCK_ON         80
#define LOCK_OFF        00

#define POWER           29
// bit position  >>
#define POWER_BIT_ON             0
#define HEALTH_MODE_BIT_ON       3
#define COMPRESSOR_MODE_BIT_ON   4

#define SWING_POS               31
// bit position >>
#define SWING_UNDEFINED_BIT     0
#define TURBO_MODE_BIT_ON       1
#define SILENT_MODE_BIT_ON      2
#define SWING_HORIZONTAL_BIT    3
#define SWING_VERTICAL_BIT      4
#define LED_BIT_OFF             5

#define SET_TEMPERATURE 35

#define CRC 36

// temperatures supported by AC system
#define MIN_SET_TEMPERATURE 16
#define MAX_SET_TEMPERATURE 30
#define STEP_TEMPERATURE 1
#define TEMPERATURE_FOR_FAN_ONLY    8

//if internal temperature is outside of those boundaries, message will be discarded
#define MIN_VALID_INTERNAL_TEMP 10
#define MAX_VALID_INTERNAL_TEMP 50

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


        traits.set_supports_action(true);

        traits.set_supported_modes(
        {
            climate::CLIMATE_MODE_OFF,
            climate::CLIMATE_MODE_HEAT_COOL,
            climate::CLIMATE_MODE_COOL,
            climate::CLIMATE_MODE_HEAT,
            climate::CLIMATE_MODE_FAN_ONLY,
            climate::CLIMATE_MODE_DRY
        });

        traits.set_supported_fan_modes(
        {
            climate::CLIMATE_FAN_AUTO,
            climate::CLIMATE_FAN_LOW,
            climate::CLIMATE_FAN_MEDIUM,
            climate::CLIMATE_FAN_HIGH,
            climate::CLIMATE_FAN_MIDDLE,
        });

        traits.set_supported_swing_modes(
        {
            climate::CLIMATE_SWING_OFF,
            climate::CLIMATE_SWING_BOTH,
            climate::CLIMATE_SWING_VERTICAL,
            climate::CLIMATE_SWING_HORIZONTAL
        });

        traits.set_visual_min_temperature(MIN_SET_TEMPERATURE);
        traits.set_visual_max_temperature(MAX_SET_TEMPERATURE);
        traits.set_visual_temperature_step(STEP_TEMPERATURE);
        traits.set_supports_current_temperature(true);
        traits.set_supports_two_point_target_temperature(false);

        return traits;
    }

public:

    void readData() {


        auto raw = getHex(data, sizeof(data));
        ESP_LOGD("Haier", "Readed message: %s ", raw.c_str());


        byte check = data[CRC];

        getChecksum(data, sizeof(data));

        if (check != data[CRC]) {
            ESP_LOGW("Haier", "Invalid checksum");
            return;
        }


        lastCRC = check;

        current_temperature = data[TEMPERATURE];
        target_temperature = data[SET_TEMPERATURE] + 16;

        if(current_temperature < MIN_VALID_INTERNAL_TEMP || current_temperature > MAX_VALID_INTERNAL_TEMP 
            || target_temperature < MIN_SET_TEMPERATURE || target_temperature > MAX_SET_TEMPERATURE){
            ESP_LOGW("Haier", "Invalid temperatures");
            return;
        }


        if (data[POWER] & ( 1 << POWER_BIT_ON )) {

            switch (data[MODE]) {
                case MODE_COOL:
                    mode = CLIMATE_MODE_COOL;
                    break;
                case MODE_HEAT:
                    mode = CLIMATE_MODE_HEAT;
                    break;
                case MODE_DRY:
                    mode = CLIMATE_MODE_DRY;
                    break;
                case MODE_FAN_ONLY:
                    mode = CLIMATE_MODE_FAN_ONLY;
                    break;
                default:
                    mode = CLIMATE_MODE_HEAT_COOL;
            }

            if ( data[SWING_POS] & ( 1 << SILENT_MODE_BIT_ON )) {
                fan_mode = CLIMATE_FAN_LOW;
            } else {
                switch (data[FAN_SPEED]) {
                    case FAN_AUTO:
                        fan_mode = CLIMATE_FAN_AUTO;
                        break;
                    case FAN_MEDIUM:
                        fan_mode = CLIMATE_FAN_MEDIUM;
                        break;
                    case FAN_MIDDLE:
                        fan_mode = CLIMATE_FAN_MIDDLE;
                        break;
                    case FAN_HIGH:
                        fan_mode = CLIMATE_FAN_HIGH;
                        break;
                    default:
                        fan_mode = CLIMATE_FAN_AUTO;
                }
            }

            switch (data[SWING]) {
                case SWING_OFF: 
                    if ( data[SWING_POS] & ( 1 << SWING_VERTICAL_BIT )) {
                        swing_mode = CLIMATE_SWING_VERTICAL;
                    } else if ( data[SWING_POS] & ( 1 << SWING_HORIZONTAL_BIT )) {
                        swing_mode = CLIMATE_SWING_HORIZONTAL;
                    } else {
                        swing_mode = CLIMATE_SWING_OFF;
                    }
                    break;
                case SWING_BOTH:
                    swing_mode = CLIMATE_SWING_BOTH;
                    break;

            } 
        } else {
            mode = CLIMATE_MODE_OFF;
            //fan_mode = CLIMATE_FAN_OFF;
            //swing_mode = CLIMATE_SWING_OFF;
        }

        this->publish_state();

    }

// Climate control
    void control(const ClimateCall &call) override {

        if (call.get_mode().has_value())
            switch (call.get_mode().value()) {
                case CLIMATE_MODE_OFF:
                    data[POWER] &= ~(1 << POWER_BIT_ON);
                    break;
                case CLIMATE_MODE_COOL:
                    data[POWER] |= (1 << POWER_BIT_ON);
                    data[MODE] = MODE_COOL;
                    break;
                case CLIMATE_MODE_HEAT:
                    data[POWER] |= (1 << POWER_BIT_ON);
                    data[MODE] = MODE_HEAT;
                    break;
                case CLIMATE_MODE_DRY:
                    data[POWER] |= (1 << POWER_BIT_ON);
                    data[MODE] = MODE_DRY;
                    break;
                case CLIMATE_MODE_FAN_ONLY:
                    if (data[POWER] & ( 1 << POWER_BIT_ON )) {
                        //ESP_LOGW("Haier", "Cold start");
                    } else {
                        data[SET_TEMPERATURE] = TEMPERATURE_FOR_FAN_ONLY;
                    }
                    data[POWER] |= (1 << POWER_BIT_ON);
                    data[FAN_SPEED] = FAN_MIDDLE; // set fan speed for change work mode
                    data[MODE] = MODE_FAN_ONLY;
                    break;
                case CLIMATE_MODE_HEAT_COOL:
                    data[POWER] |= (1 << POWER_BIT_ON);
                    data[MODE] = MODE_SMART;
                    break;
            }

        //Set fan speed
        if (call.get_fan_mode().has_value())
            switch(call.get_fan_mode().value()) {
                case CLIMATE_FAN_LOW:
                    data[POWER] |= (1 << POWER_BIT_ON);
                    data[SWING_POS] |= (1 << SILENT_MODE_BIT_ON);
                    data[SWING_POS] &= ~(1 << TURBO_MODE_BIT_ON);
                    break;
                case CLIMATE_FAN_MIDDLE:
                    data[POWER] |= (1 << POWER_BIT_ON);
                    data[FAN_SPEED] = FAN_MIDDLE;
                    data[SWING_POS] &= ~(1 << SILENT_MODE_BIT_ON);
                    data[SWING_POS] &= ~(1 << TURBO_MODE_BIT_ON);
                    break;
                case CLIMATE_FAN_MEDIUM:
                    data[POWER] |= (1 << POWER_BIT_ON);
                    data[FAN_SPEED] = FAN_MEDIUM;
                    data[SWING_POS] &= ~(1 << SILENT_MODE_BIT_ON);
                    data[SWING_POS] &= ~(1 << TURBO_MODE_BIT_ON);
                    break;
                case CLIMATE_FAN_HIGH:
                    data[POWER] |= (1 << POWER_BIT_ON);
                    data[FAN_SPEED] = FAN_HIGH;
                    data[SWING_POS] &= ~(1 << SILENT_MODE_BIT_ON);
                    data[SWING_POS] &= ~(1 << TURBO_MODE_BIT_ON);
                    break;
                case CLIMATE_FAN_AUTO:
                    data[POWER] |= (1 << POWER_BIT_ON);
                    data[FAN_SPEED] = FAN_AUTO;
                    data[SWING_POS] &= ~(1 << SILENT_MODE_BIT_ON);
                    data[SWING_POS] &= ~(1 << TURBO_MODE_BIT_ON);
                    break;
        }


        //Set swing mode
        if (call.get_swing_mode().has_value())
            switch(call.get_swing_mode().value()) {
                case CLIMATE_SWING_OFF:
                    data[POWER] |= (1 << POWER_BIT_ON);
                    data[SWING] = SWING_OFF;
                    data[SWING_POS] &= ~(1 << SWING_UNDEFINED_BIT);
                    break;
                case CLIMATE_SWING_VERTICAL:
                    data[POWER] |= (1 << POWER_BIT_ON);
                    data[SWING] = SWING_OFF;
                    data[SWING_POS] |= (1 << SWING_VERTICAL_BIT);
                    data[SWING_POS] &= ~(1 << SWING_HORIZONTAL_BIT);
                    break;
                case CLIMATE_SWING_HORIZONTAL:
                    data[POWER] |= (1 << POWER_BIT_ON);
                    data[SWING] = SWING_OFF;
                    data[SWING_POS] |= (1 << SWING_HORIZONTAL_BIT);
                    data[SWING_POS] &= ~(1 << SWING_VERTICAL_BIT);
                    break;
                case CLIMATE_SWING_BOTH:
                    data[POWER] |= (1 << POWER_BIT_ON);
                    data[SWING] = SWING_BOTH;
                    data[SWING_POS] &= ~(1 << SWING_UNDEFINED_BIT);
                    data[SWING_POS] &= ~(1 << SWING_VERTICAL_BIT);
                    data[SWING_POS] &= ~(1 << SWING_HORIZONTAL_BIT);
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
