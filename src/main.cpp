#include <Arduino.h>

// PlatformIO enables USB CDC on generic Serial in platformio.ini.
// No motor outputs or IMU peripherals are configured in this example.
void setup()
{
  Serial.begin(115200);
}

void loop()
{
  if (Serial) {
    Serial.print("MAMBA STM32F405 USB heartbeat; uptime_ms=");
    Serial.println(millis());
  }
  delay(1000);
}
