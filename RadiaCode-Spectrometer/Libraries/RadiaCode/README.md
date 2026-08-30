# RadiaCode Arduino Library

![RadiaCode](/doc/radiacode.jpg)

[![Arduino](https://img.shields.io/badge/Arduino-Library-00979D.svg)](https://www.arduino.cc/)
[![ESP32](https://img.shields.io/badge/ESP32-Compatible-red.svg)](https://www.espressif.com/en/products/socs/esp32)
[![Version](https://img.shields.io/badge/Version-1.0.0-blue.svg)](https://github.com/mkgeiger/RadiaCode/releases)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)

A comprehensive Arduino library for interfacing with RadiaCode radiation detection devices via Bluetooth Low Energy (BLE). Designed for ESP32, but can easily ported to other BLE-capable microcontrollers.

## 📋 Features

- **Comprehensive Device Control**: Full access to RadiaCode device functionality
- **Real-time Data Acquisition**: Monitor radiation levels, dose rates, and more
- **Spectrum Analysis**: Analyze radiation energy spectrum data
- **Memory Optimized**: ESP32-friendly memory management for stable operation
- **Error Handling**: Robust error detection and reporting
- **User-friendly API**: Intuitive interface for device interaction

## 🧩 Supported Hardware

- **Devices**: RadiaCode-102, RadiaCode-103, RadiaCode-103G, RadiaCode-110
- **Microcontrollers**:
  - ESP32

## ⚡ Quick Start

### Installation

#### Using Arduino IDE Library Manager
1. Open Arduino IDE
2. Go to **Sketch → Include Library → Manage Libraries**
3. Search for "RadiaCode"
4. Click Install

#### Manual Installation
1. Download this repository as a ZIP file
2. In the Arduino IDE, go to **Sketch → Include Library → Add .ZIP Library**
3. Select the downloaded ZIP file
4. Restart the Arduino IDE

### Basic Usage

```cpp
#include <RadiaCode.h>

// Replace with your device's MAC address
const char* bluetoothMac = "11:22:33:44:55:66"; 

// Create RadiaCode instance
RadiaCode* radiacode = nullptr;

void setup(void)
{
  Serial.begin(115200);
  while (!Serial && millis() < 5000);

  Serial.println("Connecting to RadiaCode device...");
  radiacode = new RadiaCode(bluetoothMac);

  if (radiacode != nullptr)
  {
    String serialNum = radiacode->serialNumber();
    if (serialNum.length() > 0)
    {
      Serial.print("Connected to device: ");
      Serial.println(serialNum);
    }
  }
}

void loop(void)
{
  if (radiacode == nullptr) return;

  // Read radiation data
  std::vector<DataItem*> data = radiacode->dataBuf();

  // Process data
  for (DataItem* item : data)
  {
    // Check type without dynamic_cast for efficiency
    if (item->type == TYPE_REAL_TIME_DATA)
    {
      RealTimeData* rtData = static_cast<RealTimeData*>(item);
      Serial.print("Count rate: ");
      Serial.print(rtData->count_rate);
      Serial.println(" CPS");

      Serial.print("Dose rate: ");
      Serial.print(rtData->dose_rate * 10000.0f); // Convert to µSv/h
      Serial.println(" µSv/h");
    }
  }

  // Clean up data objects
  for (DataItem* item : data)
  {
    delete item;
  }
  data.clear();

  delay(1000);
}
```

## 📚 Examples

The library includes several examples to get you started:

- **[Basic](examples/Basic/Basic.ino)**: Simple connection and data reading
- **[Spectrum](examples/Spectrum/Spectrum.ino)**: Acquire and visualize radiation spectrum data
- **[Test](examples/Test/Test.ino)**: Test communication with the device

## 🛠️ API Reference

### Main Classes

| Class | Description |
|-------|-------------|
| `RadiaCode` | Core class for device interaction |
| `BytesBuffer` | Buffer management for protocol data |
| `Spectrum` | Energy spectrum data handling |

### Key Methods

| Method | Description |
|--------|-------------|
| `dataBuf()` | Get real-time radiation measurements |
| `spectrum()` | Get current radiation spectrum |
| `spectrumAccum()` | Get accumulated spectrum |
| `setAlarmLimits()` | Configure alarm thresholds |
| `getTemperature()` | Get device temperature |

### Full Documentation

For detailed API documentation, see the [RadiaCode API Reference](doc/radiacode.pdf).

## 📈 Spectrum Analysis

The library provides comprehensive tools for analyzing radiation spectrum data:

```cpp
// Get spectrum data
Spectrum spectrum = radiacode->spectrum();

// Access spectrum parameters
uint32_t duration = spectrum.duration_sec;
float a0 = spectrum.a0;
float a1 = spectrum.a1;
float a2 = spectrum.a2;

// Process spectrum data
for (int i = 0; i < spectrum.size(); i++)
{
  uint32_t counts = spectrum.at(i);
  float energy = spectrumChannelToEnergy(i, a0, a1, a2);
  Serial.printf("Channel %d: %d counts, %.2f keV\n", i, counts, energy);
}
```

## 🔄 Memory Optimization

The library is optimized for ESP32 and other microcontrollers with limited memory:

- Uses static shared buffers to minimize heap fragmentation
- Efficient data structures to reduce memory usage
- Stack-friendly implementations to prevent overflow
- Safe bounds checking throughout the codebase

## 📄 License

This library is released under the [MIT License](LICENSE).

## 👨‍💻 Author

Markus Geiger

## 🔗 Links

- [GitHub Repository](https://github.com/mkgeiger/RadiaCode)
- [RadiaCode Device Information](https://radiacode.com)
- [Report Issues](https://github.com/mkgeiger/RadiaCode/issues)

The implementation of this RadiaCode library in C++ was inspired by the cdump Python RadiaCode library (https://github.com/cdump/radiacode). I wanted to adapt it for Arduino platforms, mainly for the ESP32 (which supports BLE). Because RadiaCode does not publish the API documentation the only chance to implement this library was by studying the cdump Python library and reverse engineering the bluetooth protocol. Therefore are still some open points to be clarified, to be defined and to be done. So don't see the library implementation status as 100% complete and fully functional. But the main functions and beyond that work fine. Please feel free to improve the library and report also bugs. The usage of this library is therefore at your own risk. I assume no liability whatsoever.

