#include <Arduino.h>
#include <SPI.h>
#include <ICM42688.h>

// STM32duino constructor order: MOSI, MISO, SCK. CS is managed by the library.
SPIClass imuBus(PA7, PA6, PA5);
ICM42688 imu(imuBus, PA4, 1000000);

int initStatus = 0;
bool wasConnected = false;

void setup()
{
  Serial.begin(115200);
  // Wait for the monitor so startup calibration instructions remain visible.
  while (!Serial) {
    delay(10);
  }
  Serial.println("MAMBA SPI test: keep the FC still; calibrating gyro...");
  delay(2000);
  initStatus = imu.begin();
}

void loop()
{
  if (!Serial) {
    wasConnected = false;
    delay(100);
    return;
  }
  if (!wasConnected) {
    Serial.print("IMU.begin status=");
    Serial.println(initStatus);
    Serial.println("CW180 board axes: ax_g,ay_g,az_g,gx_dps,gy_dps,gz_dps,temp_C");
    wasConnected = true;
  }
  if (initStatus < 0) {
    Serial.print("IMU initialization failed; status=");
    Serial.println(initStatus);
    delay(1000);
    return;
  }
  if (imu.getAGT() < 0) {
    Serial.println("IMU sample read failed");
    delay(1000);
    return;
  }
  // Handoff specifies CW180: reverse X/Y for both sensors, preserve Z.
  Serial.print(-imu.accX(), 6); Serial.print(',');
  Serial.print(-imu.accY(), 6); Serial.print(',');
  Serial.print(imu.accZ(), 6); Serial.print(',');
  Serial.print(-imu.gyrX(), 6); Serial.print(',');
  Serial.print(-imu.gyrY(), 6); Serial.print(',');
  Serial.print(imu.gyrZ(), 6); Serial.print(',');
  Serial.println(imu.temp(), 2);
  delay(100);
}
