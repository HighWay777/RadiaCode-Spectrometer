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

#ifndef BluetoothTransport_h
#define BluetoothTransport_h

#include "RadiaCodeTransport.h"
#include "BytesBuffer.h"

#if defined(ARDUINO_ARCH_ESP32)
#define BT_SUPPORT_ENABLED
#endif

// PATCHED (2026-08-22): lets the application see whether the BLE link is
// still up without owning the client, so a dropped connection can be detected
// and auto-reconnected instead of silently returning empty data forever.
bool radiacodeBleLinkUp(void);

class BluetoothTransport : public RadiaCodeTransport
{
    public:
        BluetoothTransport(const char* mac);
        virtual ~BluetoothTransport(void);
        virtual BytesBuffer execute(const uint8_t* request, size_t length) override;

    private:
#ifdef BT_SUPPORT_ENABLED
        // Platform-specific Bluetooth variables
        void* _peripheral; // Pointer to Bluetooth peripheral object
        int _write_handle;
        int _notify_handle;

        // Use fixed buffer instead of dynamic allocation for response
        // Increased to 4K to handle larger responses
        static const size_t MAX_RESP_SIZE = 4096;
        // Use static buffer to avoid stack overflow
        static uint8_t _resp_buffer[MAX_RESP_SIZE];
        size_t _resp_received;
        size_t _resp_size;
        bool _response_ready;
#endif
};

#endif
