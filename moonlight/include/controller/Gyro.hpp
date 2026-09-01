#pragma once

#include <cstdint>

struct MotionSensorData
{
    // Gyroscope (deg/s)
    float gyroX = 0.0f, gyroY = 0.0f, gyroZ = 0.0f;
    // Accelerometer (m/s^2)
    float accelX = 0.0f, accelY = 0.0f, accelZ = 0.0f;
    // Orientation quaternion
    float quatW = 0.0f, quatX = 0.0f, quatY = 0.0f, quatZ = 0.0f;
    // Basic orientation (-1, 0, 1 per axis relative to gravity)
    float basicX = 0.0f, basicY = 0.0f, basicZ = 0.0f;
    // Magnetometer field stability
    uint8_t magStability = 0;
    // Timestamp
    uint64_t hostTimestamp = 0;
};

class GyroManager
{
  public:
    GyroManager();

    // Called every input poll to sample sensors and send motion events when needed
    void update();

    // Read current sensor data (for UI/test overlay)
    bool readMotionData(MotionSensorData& out);

    bool isSensorAvailable() const { return sensorAvailable; }

  private:
    uint64_t lastSendUs  = 0;
    bool initialized     = false;
    bool sensorAvailable = false;

    // Platform-specific sensor read implementation (gyro + accelerometer)
    bool readPlatformSensor(float& gx, float& gy, float& gz,
        float& ax, float& ay, float& az);
};
