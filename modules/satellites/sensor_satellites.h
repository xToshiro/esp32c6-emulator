#ifndef SENSOR_SATELLITES_H
#define SENSOR_SATELLITES_H

#include "satellite_base.h"
#include "../scd41.h"
#include "../bme680.h"
#include "../dfrobot_pm.h"
#include "../hm3301.h"
#include "../dfrobot_ozone.h"
#include "../seeed_multigas.h"
#include "../battery_model.h"
#include "../sound_sensor.h"

// Satellite wrapper for Sensirion SCD41 (CO2, Temp, Hum)
class SCD41Satellite : public ISatelliteModule {
private:
    std::shared_ptr<SCD41Sensor> _device;

public:
    SCD41Satellite(std::shared_ptr<SCD41Sensor> dev)
        : ISatelliteModule("SCD41_CO2", "I2C_SENSOR", SatelliteType::SENSOR_I2C), _device(dev) {}

    void setPlugged(bool plugged) override {
        ISatelliteModule::setPlugged(plugged);
        _device->setEnabled(plugged);
    }

    void setPowered(bool powered) override {
        ISatelliteModule::setPowered(powered);
        _device->setPower(powered);
    }

    void step(uint32_t dtMs) override {
        (void)dtMs;
    }

    std::shared_ptr<SCD41Sensor> getDevice() const { return _device; }
};

// Satellite wrapper for Bosch BME680 (Pressure, Temp, Hum, Gas)
class BME680Satellite : public ISatelliteModule {
private:
    std::shared_ptr<BME680Sensor> _device;

public:
    BME680Satellite(std::shared_ptr<BME680Sensor> dev)
        : ISatelliteModule("BME680_ENV", "I2C_SENSOR", SatelliteType::SENSOR_I2C), _device(dev) {}

    void setPlugged(bool plugged) override {
        ISatelliteModule::setPlugged(plugged);
        _device->setEnabled(plugged);
    }

    void setPowered(bool powered) override {
        ISatelliteModule::setPowered(powered);
        _device->setPower(powered);
    }

    void step(uint32_t dtMs) override {
        (void)dtMs;
    }

    std::shared_ptr<BME680Sensor> getDevice() const { return _device; }
};

// Satellite wrapper for Particulate Matter Sensor (DFRobot / HM3301)
class PMSatellite : public ISatelliteModule {
private:
    std::shared_ptr<DFRobotPMSensor> _device;

public:
    PMSatellite(std::shared_ptr<DFRobotPMSensor> dev)
        : ISatelliteModule("DFROBOT_PM", "I2C_SENSOR", SatelliteType::SENSOR_I2C), _device(dev) {}

    void setPlugged(bool plugged) override {
        ISatelliteModule::setPlugged(plugged);
        _device->setEnabled(plugged);
    }

    void setPowered(bool powered) override {
        ISatelliteModule::setPowered(powered);
        _device->setPower(powered);
    }

    void step(uint32_t dtMs) override {
        (void)dtMs;
    }

    std::shared_ptr<DFRobotPMSensor> getDevice() const { return _device; }
};

// Satellite wrapper for DFRobot Ozone Sensor (O3)
class OzoneSatellite : public ISatelliteModule {
private:
    std::shared_ptr<DFRobotOzoneSensor> _device;

public:
    OzoneSatellite(std::shared_ptr<DFRobotOzoneSensor> dev)
        : ISatelliteModule("DFROBOT_OZONE", "I2C_SENSOR", SatelliteType::SENSOR_I2C), _device(dev) {}

    void setPlugged(bool plugged) override {
        ISatelliteModule::setPlugged(plugged);
        _device->setEnabled(plugged);
    }

    void setPowered(bool powered) override {
        ISatelliteModule::setPowered(powered);
        _device->setPower(powered);
    }

    void step(uint32_t dtMs) override {
        (void)dtMs;
    }

    std::shared_ptr<DFRobotOzoneSensor> getDevice() const { return _device; }
};

// Satellite wrapper for Seeed Multichannel Gas GMXXX (CO, NO2, VOC)
class MultigasSatellite : public ISatelliteModule {
private:
    std::shared_ptr<SeeedMultigasSensor> _device;

public:
    MultigasSatellite(std::shared_ptr<SeeedMultigasSensor> dev)
        : ISatelliteModule("SEEED_MULTIGAS", "I2C_SENSOR", SatelliteType::SENSOR_I2C), _device(dev) {}

    void setPlugged(bool plugged) override {
        ISatelliteModule::setPlugged(plugged);
        _device->setEnabled(plugged);
    }

    void setPowered(bool powered) override {
        ISatelliteModule::setPowered(powered);
        _device->setPower(powered);
    }

    void step(uint32_t dtMs) override {
        (void)dtMs;
    }

    std::shared_ptr<SeeedMultigasSensor> getDevice() const { return _device; }
};

// Satellite wrapper for Battery Divider (ADC Pino 5)
class BatterySatellite : public ISatelliteModule {
private:
    std::shared_ptr<BatteryModel> _device;

public:
    BatterySatellite(std::shared_ptr<BatteryModel> dev)
        : ISatelliteModule("BATTERY_ADC", "ANALOG_ADC", SatelliteType::ANALOG_ADC), _device(dev) {}

    void step(uint32_t dtMs) override {
        (void)dtMs;
        if (isOperational()) {
            _device->updateAdc();
        }
    }

    std::shared_ptr<BatteryModel> getDevice() const { return _device; }
};

// Satellite wrapper for Sound Sensor (ADC Pino 4)
class SoundSatellite : public ISatelliteModule {
private:
    std::shared_ptr<SoundSensorModel> _device;

public:
    SoundSatellite(std::shared_ptr<SoundSensorModel> dev)
        : ISatelliteModule("SOUND_ADC", "ANALOG_ADC", SatelliteType::ANALOG_ADC), _device(dev) {}

    void step(uint32_t dtMs) override {
        (void)dtMs;
        if (isOperational()) {
            _device->updateAdc();
        }
    }

    std::shared_ptr<SoundSensorModel> getDevice() const { return _device; }
};

#endif // SENSOR_SATELLITES_H
