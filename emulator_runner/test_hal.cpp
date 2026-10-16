#include <iostream>
#include "../hal/Arduino.h"
#include "../hal/Wire.h"
#include "../hal/WiFi.h"
#include "../hal/Preferences.h"
#include "../hal/HTTPClient.h"
#include "../hal/Adafruit_NeoPixel.h"
#include "../hal/SensirionI2cScd4x.h"
#include "../hal/Adafruit_BME680.h"
#include "../hal/DFRobot_AirQualitySensor.h"
#include "../hal/DFRobot_OzoneSensor.h"
#include "../hal/Multichannel_Gas_GMXXX.h"
#include "../hal/TinyGPS++.h"
#include "../modules/tca9548a.h"
#include "../modules/mcp23008.h"
#include "../modules/battery_model.h"
#include "../modules/sound_sensor.h"
#include "../modules/gps_model.h"

int main() {
    std::cout << "[TEST] Initializing Virtual ESP32-C6 HAL..." << std::endl;

    // 1. Setup Virtual I2C bus and devices
    auto tca = std::make_shared<TCA9548AMux>(0x70);
    auto mcp = std::make_shared<MCP23008Expander>(0x20);
    auto scd = std::make_shared<SCD41Sensor>(0x62);
    auto bme = std::make_shared<BME680Sensor>(0x77);
    auto pm  = std::make_shared<DFRobotPMSensor>(0x19);
    auto o3  = std::make_shared<DFRobotOzoneSensor>(0x73);
    auto gas = std::make_shared<SeeedMultigasSensor>(0x08);

    VirtualI2CBus::getInstance().registerDevice(tca);
    VirtualI2CBus::getInstance().registerDevice(mcp);
    VirtualI2CBus::getInstance().registerDevice(scd);
    VirtualI2CBus::getInstance().registerDevice(bme);
    VirtualI2CBus::getInstance().registerDevice(pm);
    VirtualI2CBus::getInstance().registerDevice(o3);
    VirtualI2CBus::getInstance().registerDevice(gas);

    // 2. Scan I2C
    auto found = VirtualI2CBus::getInstance().scanBus();
    std::cout << "[TEST] Found " << found.size() << " I2C devices on bus:" << std::endl;
    for (auto addr : found) {
        std::cout << "  - 0x" << std::hex << (int)addr << std::dec << std::endl;
    }

    // 3. Test SCD41 reading
    SensirionI2cScd4x scdAdapter;
    uint16_t co2;
    float temp, hum;
    if (scdAdapter.readMeasurement(co2, temp, hum) == 0) {
        std::cout << "[TEST] SCD41 OK -> CO2: " << co2 << " ppm, Temp: " << temp << " C, Hum: " << hum << " %" << std::endl;
    } else {
        std::cerr << "[TEST] SCD41 Read Failed!" << std::endl;
        return 1;
    }

    // 4. Test Battery Model
    BatteryModel bat(5);
    bat.setVoltage(3.85f);
    std::cout << "[TEST] Battery set to 3.85V, ADC read: " << analogRead(5) << std::endl;

    // 5. Test NeoPixel callback
    Adafruit_NeoPixel::setOnColorChangedCallback([](uint8_t r, uint8_t g, uint8_t b) {
        std::cout << "[LED EVENT] RGB Changed to: R=" << (int)r << ", G=" << (int)g << ", B=" << (int)b << std::endl;
    });

    Adafruit_NeoPixel led(1, 8, 0);
    led.setPixelColor(0, led.Color(0, 255, 0));
    led.show();

    // 6. Test GPS Fix Injection
    GPSModel gpsModel;
    gpsModel.setCoordinates(-3.7442, -38.5748);
    gpsModel.injectFix();

    TinyGPSPlus gps;
    while (SerialGPS.available()) {
        gps.encode(SerialGPS.read());
    }
    if (gps.location.isValid()) {
        std::cout << "[TEST] GPS Decoded Fix: Lat=" << gps.location.lat() << ", Lon=" << gps.location.lng() << std::endl;
    } else {
        std::cerr << "[TEST] GPS Fix Failed!" << std::endl;
        return 1;
    }

    // 7. Test HTTP Client
    HTTPClient http;
    http.begin("https://api-vitae.infrasa.gov.br/telemetry");
    int code = http.POST("{\"test\": true}");
    std::cout << "[TEST] HTTP POST returned code: " << code << std::endl;

    std::cout << "\n>>> ALL HAL & SENSOR MODULE TESTS PASSED SUCCESSFULLY! <<<" << std::endl;
    return 0;
}
