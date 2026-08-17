/**********************************************************************************/
/* Copyright (c) 2025 Markus Geiger                                               */
/*                                                                                */
/* Permission is hereby granted, free of charge, to any person obtaining a copy   */
/* of this software and associated documentation files (the "Software"), to deal  */
/* in the Software without restriction, including without limitation the rights   */
/* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell      */
/* copies of the Software, and to permit persons to whom the Software is          */
/* furnished to do so, subject to the following conditions:                       */
/*                                                                                */
/* The above copyright notice and this permission notice shall be included in all */
/* copies or substantial portions of the Software.                                */
/*                                                                                */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR     */
/* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,       */
/* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE    */
/* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER         */
/* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,  */
/* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE  */
/* SOFTWARE.                                                                      */
/**********************************************************************************/

#include "BluetoothTransport.h"

#ifdef BT_SUPPORT_ENABLED

// Include platform-specific BLE libraries
#if defined(ARDUINO_ARCH_ESP32)
#include <BLEDevice.h>
#include <BLEClient.h>
#endif

// Debugging switches
#undef  BT_DEBUG_INFO
#define BT_DEBUG_WARNING

// Define the static buffer for BluetoothTransport
uint8_t BluetoothTransport::_resp_buffer[BluetoothTransport::MAX_RESP_SIZE];
#endif

BluetoothTransport::BluetoothTransport(const char* mac)
{
#ifdef BT_SUPPORT_ENABLED
    _resp_size = 0;
    _resp_received = 0;
    _response_ready = false;
    // Initialize the buffer to zeros
    memset(_resp_buffer, 0, MAX_RESP_SIZE);

    // Initialize BLE client - platform-specific code
#if defined(ARDUINO_ARCH_ESP32)
    // ESP32 BLE initialization
    BLEDevice::init("RadiaCode Client");
    BLEClient* pClient = BLEDevice::createClient();

    // Connect to the RadiaCode device.
    // PATCHED (2026-08-04): confirmed on our physical RC-110
    // (serial RC-110-000983) that BLE_ADDR_TYPE_PUBLIC is correct — the
    // original single-attempt connect(bleAddress) (implicit public) actually
    // failed intermittently for a different reason (another BLE central,
    // likely the phone app, was already connected to the device). Tried
    // public first (known-good, fast path); random kept as a fallback in
    // case some other RadiaCode model/firmware advertises a random address.
    BLEAddress bleAddress(mac);
    bool connected = pClient->connect(bleAddress, BLE_ADDR_TYPE_PUBLIC);
    if (!connected)
    {
        connected = pClient->connect(bleAddress, BLE_ADDR_TYPE_RANDOM);
    }
    if (connected)
    {
        _peripheral = (void*)pClient;

        // Get service and characteristics
        BLEUUID serviceUUID("e63215e5-7003-49d8-96b0-b024798fb901");
        BLERemoteService* pService = pClient->getService(serviceUUID);

        if (pService != nullptr)
        {
            // Get write characteristic
            BLEUUID writeUUID("e63215e6-7003-49d8-96b0-b024798fb901");
            BLERemoteCharacteristic* pWriteChar = pService->getCharacteristic(writeUUID);
            _write_handle = 0; // In ESP32, we use the characteristic directly

            // Get notify characteristic
            BLEUUID notifyUUID("e63215e7-7003-49d8-96b0-b024798fb901");
            BLERemoteCharacteristic* pNotifyChar = pService->getCharacteristic(notifyUUID);
            _notify_handle = 0; // In ESP32, we use the characteristic directly

            // Set up notification callback
            if (pNotifyChar != nullptr)
            {
                // Register for notifications
                pNotifyChar->registerForNotify([this](BLERemoteCharacteristic* pChar, uint8_t* data, size_t length, bool isNotify)
                {
                    // Handle notification data
                    if ((_resp_size == 0) && (length >= 4))
                    {
                        // First packet contains the response size
                        // Properly handle 4-byte size field to avoid alignment issues
                        uint32_t size_value = 0;
                        size_value = (uint32_t)data[0] | 
                                    ((uint32_t)data[1] << 8) | 
                                    ((uint32_t)data[2] << 16) | 
                                    ((uint32_t)data[3] << 24);

                        _resp_size = size_value + 4; // Include the size field itself

                        // Check if response size fits in our fixed buffer
                        if (_resp_size > MAX_RESP_SIZE)
                        {
#ifdef BT_DEBUG_WARNING
                            Serial.print("Warning: Response size too large (");
                            Serial.print(_resp_size);
                            Serial.print(" bytes), limiting to ");
                            Serial.println(MAX_RESP_SIZE);
#endif
                            _resp_size = MAX_RESP_SIZE;
                        }

                        // Reset buffer and copy initial data
                        memset(_resp_buffer, 0, MAX_RESP_SIZE);
                        memcpy(_resp_buffer, data, length);
                        _resp_received = length;
                    }
                    else
                    {
                        // Append data to buffer, ensuring we don't exceed buffer size
                        if ((_resp_received + length) <= MAX_RESP_SIZE)
                        {
                            // Calculate how much we can safely copy
                            size_t copyLength = length;
                            if ((_resp_received + copyLength) > MAX_RESP_SIZE)
                            {
                                copyLength = MAX_RESP_SIZE - _resp_received;
#ifdef BT_DEBUG_WARNING
                                Serial.println("Warning: Truncating BLE packet to fit buffer");
#endif
                            }

                            // Only copy if there's space
                            if (copyLength > 0)
                            {
                                memcpy(_resp_buffer + _resp_received, data, copyLength);
                                _resp_received += copyLength;
                            }
                        }
                    }

                    // Check if all data received
                    if (_resp_received >= _resp_size)
                    {
                        _response_ready = true;
#ifdef BT_DEBUG_INFO
                        Serial.print("Response complete: ");
                        Serial.print(_resp_received);
                        Serial.print(" of ");
                        Serial.print(_resp_size);
                        Serial.println(" bytes received");
#endif
                    }
                });

                // Enable notifications
                pNotifyChar->getDescriptor(BLEUUID((uint16_t)0x2902))->writeValue((uint8_t*)"\x01\x00", 2);
            }
        }
    }
    else
    {
        Serial.println("Failed to connect to BLE device");
        _peripheral = nullptr;
    }
#else
    _peripheral = nullptr; // Would be initialized with actual BLE connection
#endif
#else
    Serial.println("Bluetooth not supported on this platform");
#endif
}

BluetoothTransport::~BluetoothTransport(void)
{
#ifdef BT_SUPPORT_ENABLED
    // Clean up BLE resources - platform-specific code
#if defined(ARDUINO_ARCH_ESP32)
    if (_peripheral != nullptr)
    {
        BLEClient* pClient = (BLEClient*)_peripheral;
        pClient->disconnect();
    }
#endif

    // Reset state variables
    _resp_size = 0;
    _resp_received = 0;
#endif
}

BytesBuffer BluetoothTransport::execute(const uint8_t* request, size_t length)
{
    BytesBuffer response;

#ifdef BT_SUPPORT_ENABLED
    if (_peripheral == nullptr)
    {
        Serial.println("Bluetooth not connected");
        return response;
    }

    // Reset response tracking
    _resp_received = 0;
    _resp_size = 0;
    _response_ready = false;

    // Send request in chunks - platform-specific code
#if defined(ARDUINO_ARCH_ESP32)
    BLEClient* pClient = (BLEClient*)_peripheral;
    BLEUUID writeUUID("e63215e6-7003-49d8-96b0-b024798fb901");
    BLERemoteService* pService = pClient->getService(BLEUUID("e63215e5-7003-49d8-96b0-b024798fb901"));
    BLERemoteCharacteristic* pWriteChar = pService->getCharacteristic(writeUUID);

    // Send in chunks of 18 bytes (BLE MTU limitation)
    const size_t chunkSize = 18;
    for (size_t pos = 0; pos < length; pos += chunkSize)
    {
        size_t remaining = length - pos;
        size_t toSend = (remaining > chunkSize) ? chunkSize : remaining;

        uint8_t chunk[chunkSize];
        memcpy(chunk, request + pos, toSend);
        
        pWriteChar->writeValue(chunk, toSend);
        delay(5); // Small delay to avoid overwhelming the BLE stack
    }
#endif

    // Wait for response with timeout - extended to 30 seconds
    unsigned long startTime = millis();
    const unsigned long TIMEOUT_MS = 30000; // Increased timeout to 30 seconds
    unsigned long elapsedTime = 0;

    while ((!_response_ready) && ((elapsedTime = (millis() - startTime)) < TIMEOUT_MS))
    {
#ifdef BT_DEBUG_INFO
        // Print progress every 2 seconds
        if ((elapsedTime % 2000) < 20)
        {
            Serial.print("Waiting for BLE response: ");
            Serial.print(_resp_received);
            Serial.print("/");
            Serial.print(_resp_size > 0 ? _resp_size : '?');
            Serial.print(" bytes (");
            Serial.print(elapsedTime / 1000);
            Serial.println("s)");
        }
#endif
        delay(50); // Less frequent polling
        yield(); // Allow ESP32 background tasks to run
    }

    if (!_response_ready)
    {
#ifdef BT_DEBUG_WARNING
        Serial.print("Warning: Bluetooth response timeout after ");
        Serial.print(TIMEOUT_MS / 1000);
        Serial.print("s. Received ");
        Serial.print(_resp_received);
        Serial.print(" of ");
        Serial.print(_resp_size);
        Serial.println(" bytes");
#endif
        return response;
    }

    // Create BytesBuffer from response
    if (_resp_received >= 4) // Make sure we have at least the header
    {
        // Skip the size field (first 4 bytes)
        size_t dataSize = _resp_received - 4;

        // Additional safety check to ensure we have valid data
        if (dataSize > (MAX_RESP_SIZE - 4))
        {
#ifdef BT_DEBUG_WARNING
            Serial.println("Warning: Invalid response size detected");
#endif
            dataSize = MAX_RESP_SIZE - 4;
        }

        // Check BytesBuffer capacity limits
        if (dataSize > BytesBuffer::MAX_BUFFER_SIZE)
        {
            dataSize = BytesBuffer::MAX_BUFFER_SIZE;
#ifdef BT_DEBUG_WARNING
            Serial.println("Warning: Truncating response to fit BytesBuffer");
#endif
        }

        // Create a BytesBuffer with the response data
        response = BytesBuffer(_resp_buffer + 4, dataSize);
    }
#else
    Serial.println("Bluetooth not supported on this platform");
#endif

    return response;
}
