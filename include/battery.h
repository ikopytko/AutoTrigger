#ifndef BATTERY_H
#define BATTERY_H

#include <Arduino.h>

class Battery
{
public:
    Battery(uint8_t pin) : batteryPin{pin} {};

    int8_t checkBattery()
    {
        if (isMeasuring && lastMeasureTime + msReadingDelay < millis())
        {
            int a = analogRead(batteryPin);
            float voltage = a * (1.1 / 1023.0) * dividerRatio;
            digitalWrite(batteryPin, HIGH);
            int percent = sigmoidal((int)(voltage * 1000), 3000, 4200);
            lastMeasurement = percent;
            isMeasuring = false;
        }

        if (lastMeasureTime == 0 || lastMeasureTime + msBeteenMeasurement < millis())
        {
            digitalWrite(batteryPin, LOW);
            delayMicroseconds(10);
            analogRead(batteryPin); // discard fist reading
            isMeasuring = true;
            lastMeasureTime = millis();
        }

        return lastMeasurement;
    }

private:
    uint8_t batteryPin;

    bool isMeasuring;
    int lastMeasurement = 100;
    float dividerRatio = 5.7;

    uint16_t msBeteenMeasurement = 5000; // ms time between measurements
    uint16_t msReadingDelay = 2;         // ms stabilization time: how long to wait between first and second read
    uint64_t lastMeasureTime = 0;        // time of the last measurement

    /// @brief Li-Po characteristic curve approximation function
    /// @param voltage measured voltage, mV
    /// @param minVoltage fully discharged voltage, mV
    /// @param maxVoltage fully charged voltage, mV
    /// @return battery percentage
    static inline uint8_t sigmoidal(uint16_t voltage, uint16_t minVoltage, uint16_t maxVoltage)
    {
        // slow
        // uint8_t result = 110 - (110 / (1 + pow(1.468 * (voltage - minVoltage)/(maxVoltage - minVoltage), 6)));

        // steep
        // uint8_t result = 102 - (102 / (1 + pow(1.621 * (voltage - minVoltage)/(maxVoltage - minVoltage), 8.1)));

        // normal
        uint8_t result = 105 - (105 / (1 + pow(1.724 * (voltage - minVoltage) / (maxVoltage - minVoltage), 5.5)));

        return result >= 100 ? 100 : result;
    }
};
#endif