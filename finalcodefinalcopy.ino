/*
  RTOS Multi-Sensor Dashboard
  ESP32 + DHT11 + GY-6500 (MPU6500) + 0.96" OLED

  Three sensors run as independent FreeRTOS tasks, each reading its own
  hardware on its own schedule, while a shared display task reads their
  combined results and shows them live on an OLED screen.

  - Temperature/Humidity task (DHT11): updates every 2 seconds
  - Accelerometer task (GY-6500): updates every 500 ms
  - Display task (OLED): reads both results and refreshes the screen
    every second

  All shared sensor data is protected by a mutex, so tasks never read
  a half-updated value while another task is in the middle of writing it.

  Author: Muzammil Gaho
*/

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "FastIMU.h"
#include <DHT.h>

// ---- Pin definitions ----
const int DHT_PIN = 15;
const int IMU_ADDRESS = 0x68;
const int OLED_ADDRESS = 0x3C;

// ---- Display setup ----
const int SCREEN_WIDTH = 128;
const int SCREEN_HEIGHT = 64;
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// ---- Sensor objects ----
DHT dht(DHT_PIN, DHT11);
MPU6500 imu;
calData imuCalib = { 0 };

// ---- Shared state between tasks, protected by dataMutex ----
SemaphoreHandle_t dataMutex;
float sharedTemperature = 0;
float sharedHumidity = 0;
float sharedAccelZ = 0;

// Reads temperature and humidity from the DHT11 on its own schedule,
// then safely publishes the results for other tasks to read.
void TaskReadDHT(void *parameter) {
  while (true) {
    float temp = dht.readTemperature();
    float hum = dht.readHumidity();

    xSemaphoreTake(dataMutex, portMAX_DELAY);
    sharedTemperature = temp;
    sharedHumidity = hum;
    xSemaphoreGive(dataMutex);

    vTaskDelay(2000 / portTICK_PERIOD_MS);
    // DHT11 cannot reliably be read faster than about once per second
  }
}

// Reads the Z-axis acceleration from the GY-6500 on its own schedule,
// then safely publishes the result for other tasks to read.
void TaskReadIMU(void *parameter) {
  while (true) {
    imu.update();
    AccelData accelData;
    imu.getAccel(&accelData);

    xSemaphoreTake(dataMutex, portMAX_DELAY);
    sharedAccelZ = accelData.accelZ;
    xSemaphoreGive(dataMutex);

    vTaskDelay(500 / portTICK_PERIOD_MS);
  }
}

// Reads the latest shared sensor values and refreshes the OLED display.
// Copies values into local variables while holding the lock only briefly,
// so the (slower) screen-drawing work doesn't block the other tasks.
void TaskUpdateDisplay(void *parameter) {
  while (true) {
    float tempCopy, humCopy, accelCopy;

    xSemaphoreTake(dataMutex, portMAX_DELAY);
    tempCopy = sharedTemperature;
    humCopy = sharedHumidity;
    accelCopy = sharedAccelZ;
    xSemaphoreGive(dataMutex);

    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);

    display.print("Temp: ");
    display.print(tempCopy);
    display.println(" C");

    display.print("Humidity: ");
    display.print(humCopy);
    display.println(" %");

    display.print("AccelZ: ");
    display.println(accelCopy);

    display.display();

    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
}

void setup() {
  Serial.begin(115200);
  Wire.begin();

  dht.begin();

  dataMutex = xSemaphoreCreateMutex();

  int imuError = imu.init(imuCalib, IMU_ADDRESS);
  if (imuError != 0) {
    Serial.print("IMU init failed, error code: ");
    Serial.println(imuError);
  }

  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS)) {
    Serial.println("OLED not found!");
  }

  xTaskCreate(TaskReadDHT, "DHT Task", 2000, NULL, 1, NULL);
  xTaskCreate(TaskReadIMU, "IMU Task", 2000, NULL, 1, NULL);
  xTaskCreate(TaskUpdateDisplay, "Display Task", 2000, NULL, 1, NULL);
}

void loop() {
  // Intentionally empty. All real work happens in the three independent
  // FreeRTOS tasks created in setup() above.
}