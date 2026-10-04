// USB CDC is selected in the build's board options, so Serial uses USB.
// This compile-only example does not configure motor outputs or the IMU.
void setup()
{
  Serial.begin(115200);
}

void loop()
{
  // Keep running even when no serial terminal is connected.
  if (Serial) {
    Serial.print("MAMBA STM32F405 USB heartbeat; uptime_ms=");
    Serial.println(millis());
  }
  delay(1000);
}
