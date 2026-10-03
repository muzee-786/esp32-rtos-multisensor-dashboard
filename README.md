# ESP32 RTOS Multi-Sensor Dashboard

A real-time multi-sensor dashboard built on an ESP32 using FreeRTOS, where three independent tasks each manage their own hardware simultaneously, sharing results safely through a mutex-protected data structure.

Built as a personal project to apply real-time operating system (RTOS) concepts to embedded hardware.

## Demo Video

[Watch the demo on YouTube](https://youtube.com/watch?v=Ja4FrEaiSzc)

## How it works

Three FreeRTOS tasks run independently and concurrently:

| Task | Hardware | Update rate | Role |
|---|---|---|---|
| DHT Task | DHT11 temperature/humidity sensor | Every 2 seconds | Reads temperature and humidity, publishes results |
| IMU Task | GY-6500 (MPU6500) accelerometer | Every 500 ms | Reads Z-axis acceleration, publishes result |
| Display Task | 0.96" OLED screen | Every 1 second | Reads both published results and refreshes the display |

Unlike a traditional single-loop Arduino program, none of these tasks block each other — the DHT11's relatively slow reads don't hold up the faster accelerometer readings, and the display refreshes on its own independent schedule.

## Why a mutex is needed

All three sensors share a small set of variables (temperature, humidity, and acceleration) that multiple tasks read and write. Without protection, one task could read a value while another task is in the middle of updating it, resulting in a "torn" or inconsistent read.

A FreeRTOS mutex solves this: before any task reads or writes the shared data, it must acquire the mutex (`xSemaphoreTake`), and release it immediately after (`xSemaphoreGive`). This guarantees only one task touches the shared data at any given moment, while still allowing each task to run independently the rest of the time.

## Hardware

- ESP32 Dev Module
- DHT11 temperature/humidity sensor module
- GY-6500 (MPU6500) accelerometer/gyroscope module
- 0.96" I2C OLED display (SSD1306)
- Breadboard + jumper wires

## Wiring

| Component | Pin | Connects to |
|---|---|---|
| DHT11 | Data | ESP32 GPIO 15 |
| DHT11 | VCC | ESP32 5V |
| DHT11 | GND | ESP32 GND |
| GY-6500 | SDA | ESP32 GPIO 21 |
| GY-6500 | SCL | ESP32 GPIO 22 |
| GY-6500 | VCC | ESP32 3.3V |
| GY-6500 | GND | ESP32 GND |
| OLED | SDA | ESP32 GPIO 21 (shared I2C bus) |
| OLED | SCL | ESP32 GPIO 22 (shared I2C bus) |
| OLED | VCC | ESP32 3.3V |
| OLED | GND | ESP32 GND |

The GY-6500 and OLED share the same I2C bus (SDA/SCL), each identified by its own unique I2C address (0x68 for the accelerometer, 0x3C for the display).

## A real debugging note

The GY-6500 module uses an MPU6500 chip, which several common Arduino libraries (including Adafruit's MPU6050 library) failed to recognize, despite the sensor responding correctly on the I2C bus (confirmed with an I2C scanner). The fix was switching to the FastIMU library, which explicitly supports the MPU6500 chip variant. This is a good example of how a hardware connection can be completely correct while a library/chip mismatch still causes failures.

## What I'd improve next

- Add a fourth task to log sensor data to the ESP32's internal flash storage over time
- Experiment with task priorities under heavier load to observe real scheduling behavior, rather than the subtle differences seen with these lightweight tasks
- Add WiFi connectivity as a separate task, sending sensor data to a simple web dashboard
