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

#include "Decoders.h"
#include "BytesBuffer.h"
#include "RadiaCodeTypes.h"

// Debugging switches
#undef  DEC_DEBUG_INFO
#define DEC_DEBUG_WARNING
#define DEC_DEBUG_ERROR

// Helper function for spectrum decoding (version 0)
void decodeCountsV0(BytesBuffer& br, Spectrum& spectrum)
{
    // Clear the counts before starting
    spectrum.count_size = 0;

    // Safety check - make sure we have data available
    if (br.available() < sizeof(uint32_t))
    {
#ifdef DEC_DEBUG_WARNING
        Serial.println("Warning: Not enough data for spectrum decoding (V0)");
#endif
        return;
    }

    // Read in the counts until we run out of data or reach MAX_CHANNELS
    while ((br.available() > 0) && (br.available() >= sizeof(uint32_t)) && 
           (spectrum.count_size < Spectrum::MAX_CHANNELS))
    {
        uint32_t count;
        if (br.readUint32(&count))
        {
            // Double-check bounds before writing
            if (spectrum.count_size < Spectrum::MAX_CHANNELS)
            {
                spectrum.counts[spectrum.count_size++] = count;
            }
            else
            {
#ifdef DEC_DEBUG_WARNING
                Serial.println("Warning: Reached maximum spectrum channels");
#endif
                break;
            }
        }
        else
        {
#ifdef DEC_DEBUG_ERROR
            Serial.println("Error reading uint32 in decodeCountsV0");
#endif
            break;
        }
    }
}

// Helper function for spectrum decoding (version 1)
void decodeCountsV1(BytesBuffer& br, Spectrum& spectrum)
{
    // Clear the counts before starting
    spectrum.count_size = 0;
    uint32_t last = 0;

    // Safety check - make sure we have data available
    if (br.available() < sizeof(uint16_t))
    {
#ifdef DEC_DEBUG_WARNING
        Serial.println("Warning: Not enough data for spectrum decoding");
#endif
        return;
    }

    while ((br.available() >= sizeof(uint16_t)) && (spectrum.count_size < Spectrum::MAX_CHANNELS))
    {
        uint16_t u16;
        if (!br.readUint16(&u16))
        {
#ifdef DEC_DEBUG_ERROR
            Serial.println("Error reading u16 in decodeCountsV1");
#endif
            break;
        }

        uint16_t cnt = (u16 >> 4) & 0x0FFF;
        uint8_t vlen = u16 & 0x0F;

        // Add safety check for unreasonable count values
        if (cnt > 4096)
        {
#ifdef DEC_DEBUG_WARNING
            Serial.print("Warning: Suspicious count value in spectrum: ");
            Serial.println(cnt);
#endif
            cnt = 0; // Skip this block
        }

        for (uint16_t i = 0; i < cnt && spectrum.count_size < Spectrum::MAX_CHANNELS; i++)
        {
            uint32_t v = 0;

            if (vlen == 0)
            {
                v = 0;
            }
            else if (vlen == 1)
            {
                uint8_t val;
                if (!br.readUint8(&val))
                {
#ifdef DEC_DEBUG_ERROR
                    Serial.println("Error reading uint8 in decodeCountsV1");
#endif
                    goto end_decoding;
                }
                v = val;
            }
            else if (vlen == 2)
            {
                int8_t val;
                if (!br.readInt8(&val))
                {
#ifdef DEC_DEBUG_ERROR
                    Serial.println("Error reading int8 in decodeCountsV1");
#endif
                    goto end_decoding;
                }
                v = last + val;
            }
            else if (vlen == 3)
            {
                int16_t val;
                if (!br.readInt16(&val))
                {
#ifdef DEC_DEBUG_ERROR
                    Serial.println("Error reading int16 in decodeCountsV1");
#endif
                    goto end_decoding;
                }
                v = last + val;
            }
            else if (vlen == 4)
            {
                if (br.available() < 3)
                {
#ifdef DEC_DEBUG_ERROR
                    Serial.println("Error: Not enough data for vlen=4");
#endif
                    goto end_decoding;
                }
                uint8_t a, b;
                int8_t c;
                if ((!br.readUint8(&a)) || (!br.readUint8(&b)) || (!br.readInt8(&c)))
                {
#ifdef DEC_DEBUG_ERROR
                    Serial.println("Error reading 3-byte value in decodeCountsV1");
#endif
                    goto end_decoding;
                }
                v = last + ((c << 16) | (b << 8) | a);
            }
            else if (vlen == 5)
            {
                int32_t val;
                if (!br.readInt32(&val))
                {
#ifdef DEC_DEBUG_ERROR
                    Serial.println("Error reading int32 in decodeCountsV1");
#endif
                    goto end_decoding;
                }
                v = last + val;
            }
            else
            {
#ifdef DEC_DEBUG_ERROR
                Serial.print("Error: Unsupported vlen in decodeCountsV1: ");
                Serial.println(vlen);
#endif
                break;
            }

            last = v;

            // Extra safety check before adding to spectrum
            if (spectrum.count_size < Spectrum::MAX_CHANNELS)
            {
                spectrum.counts[spectrum.count_size++] = v;
            }
        }
    }

end_decoding:
    ; // Null statement to avoid syntax error
    // No additional cleanup needed
}

void decodeSpectrum(BytesBuffer& br, uint8_t format_version, Spectrum& spectrum)
{
    // Clear the spectrum to start with a clean state
    spectrum.clear();

    // Check if we have enough data for the spectrum header
    // We need 4 bytes for duration and 3x4 bytes for a0, a1, a2
    if (br.available() < (4 + 3*4))
    {
#ifdef DEC_DEBUG_ERROR
        Serial.println("Error: Not enough data for spectrum header");
#endif
        return;
    }

    // Read spectrum header with error checking
    uint32_t duration;
    if (!br.readUint32(&duration))
    {
#ifdef DEC_DEBUG_ERROR
        Serial.println("Error reading spectrum duration");
#endif
        return;
    }

    float a0, a1, a2;
    if ((!br.readFloat(&a0)) || (!br.readFloat(&a1)) || (!br.readFloat(&a2)))
    {
#ifdef DEC_DEBUG_ERROR
        Serial.println("Error reading spectrum calibration coefficients");
#endif
        return;
    }

    // Only assign values after we've successfully read them
    spectrum.duration_sec = duration;
    spectrum.a0 = a0;
    spectrum.a1 = a1;
    spectrum.a2 = a2;

    // Decode counts based on format version
    if (format_version == 0)
    {
        decodeCountsV0(br, spectrum);
    }
    else if (format_version == 1)
    {
        decodeCountsV1(br, spectrum);
    }
    else
    {
#ifdef DEC_DEBUG_WARNING
        Serial.print("Warning: Unsupported spectrum format version: ");
        Serial.println(format_version);
#endif
    }

    // Add a safety check on the count_size
    if (spectrum.count_size > Spectrum::MAX_CHANNELS)
    {
#ifdef DEC_DEBUG_ERROR
        Serial.println("Error: Spectrum count_size exceeds MAX_CHANNELS!");
#endif
        spectrum.count_size = Spectrum::MAX_CHANNELS;
    }
}

std::vector<DataItem*> decodeDataBuf(BytesBuffer& br, uint32_t base_time_sec)
{
    std::vector<DataItem*> ret;
    uint8_t next_seq = 0; // Initialize to invalid value
    bool first_packet = true;

    while (br.available() >= 7)
    {
        uint8_t seq, eid, gid;
        int32_t ts_offset;

        br.readUint8(&seq);
        br.readUint8(&eid);
        br.readUint8(&gid);
        br.readInt32(&ts_offset);

        // Calculate timestamp
        uint32_t timestamp = base_time_sec + (ts_offset * 10) / 1000;

        // Check sequence number
        if (!first_packet && (next_seq != seq))
        {
            // Only print sequence jump message occasionally to reduce spam
            static uint32_t last_seq_warning = 0;
            if ((millis() - last_seq_warning) > 10000) // Every 10 seconds max
            {
#ifdef DEC_DEBUG_ERROR
                Serial.print("Error: Sequence jump detected, expected: ");
                Serial.print(next_seq);
                Serial.print(", got: ");
                Serial.println(seq);
#endif
                last_seq_warning = millis();
            }
            // Continue processing instead of breaking - sequence jumps are not critical
            next_seq = seq; // Resync to current sequence
        }
        first_packet = false;
        next_seq = (seq + 1) % 256;

        if ((eid == 0) && (gid == 0)) // GRP_RealTimeData
        {
            RealTimeData* data = new RealTimeData();
            data->timestamp = timestamp;

            br.readFloat(&data->count_rate);
            // dose_rate should be read as float, not uint16_t
            br.readFloat(&data->dose_rate);  // Read as float directly, no conversion needed

            // count_rate_err and dose_rate_err are float in RealTimeData
            uint16_t tempCountRateErr, tempDoseRateErr;
            br.readUint16(&tempCountRateErr);
            br.readUint16(&tempDoseRateErr);
            data->count_rate_err = tempCountRateErr;
            data->dose_rate_err = tempDoseRateErr;

            br.readUint16(&data->flags);
            br.readUint8(&data->real_time_flags);

            // Convert errors from raw values
            data->count_rate_err /= 10.0f;
            data->dose_rate_err /= 10.0f;

            ret.push_back(data);
        }
        else if ((eid == 0) && (gid == 1)) // GRP_RawData
        {
            RawData* data = new RawData();
            data->timestamp = timestamp;

            br.readFloat(&data->count_rate);
            br.readFloat(&data->dose_rate);

            ret.push_back(data);
        }
        else if ((eid == 0) && (gid == 2)) // GRP_DoseRateDB
        {
            DoseRateDB* data = new DoseRateDB();
            data->timestamp = timestamp;

            br.readUint32(&data->count);
            br.readFloat(&data->count_rate);
            br.readFloat(&data->dose_rate);

            // dose_rate_err is float in DoseRateDB
            uint16_t tempDoseRateErr;
            br.readUint16(&tempDoseRateErr);
            data->dose_rate_err = tempDoseRateErr;

            br.readUint16(&data->flags);

            // Convert error from raw value
            data->dose_rate_err /= 10.0f;

            ret.push_back(data);
        }
        else if ((eid == 0) && (gid == 3)) // GRP_RareData
        {
            RareData* data = new RareData();
            data->timestamp = timestamp;

            br.readUint32(&data->duration);
            br.readFloat(&data->dose);

            uint16_t temperature, charge_level;
            br.readUint16(&temperature);
            br.readUint16(&charge_level);
            br.readUint16(&data->flags);

            // Convert raw values to actual values
            data->temperature = (temperature - 2000) / 100.0f;
            data->charge_level = charge_level / 100.0f;

            ret.push_back(data);
        }
        else if ((eid == 0) && (gid == 7)) // GRP_Event
        {
            Event* data = new Event();
            data->timestamp = timestamp;

            uint8_t event_id;
            br.readUint8(&event_id);
            data->event = static_cast<EventId>(event_id);

            br.readUint8(&data->event_param1);
            br.readUint16(&data->flags);

            ret.push_back(data);
        }
        else if ((eid == 0) && (gid == 9)) // GRP_RawDoseRate - might contain dose rate data
        {
            // Create a RawData object to store the dose rate
            RawData* data = new RawData();
            data->timestamp = timestamp;

            float dose_rate;
            uint16_t flags;
            br.readFloat(&dose_rate);
            br.readUint16(&flags);

            // Set count_rate to 0 since this packet only contains dose_rate
            data->count_rate = 0.0f;
            data->dose_rate = dose_rate;

#ifdef DEC_DEBUG_INFO
            // Debug: Print when we find dose rate data
            Serial.print("Found RawDoseRate: ");
            Serial.print(dose_rate);
            Serial.println(" µR/h");
#endif

            ret.push_back(data);
        }
        else
        {
            // Skip unknown data types - read and discard based on known patterns
            if ((eid == 0) && (gid == 4)) // GRP_UserData
            {
                uint32_t count;
                float count_rate, dose_rate;
                uint16_t dose_rate_err, flags;
                br.readUint32(&count);
                br.readFloat(&count_rate);
                br.readFloat(&dose_rate);
                br.readUint16(&dose_rate_err);
                br.readUint16(&flags);
            }
            else if ((eid == 0) && (gid == 5)) // GRP_ScheduleData
            {
                uint32_t count;
                float count_rate, dose_rate;
                uint16_t dose_rate_err, flags;
                br.readUint32(&count);
                br.readFloat(&count_rate);
                br.readFloat(&dose_rate);
                br.readUint16(&dose_rate_err);
                br.readUint16(&flags);
            }
            else if ((eid == 0) && (gid == 6)) // GRP_AccelData
            {
                uint16_t acc_x, acc_y, acc_z;
                br.readUint16(&acc_x);
                br.readUint16(&acc_y);
                br.readUint16(&acc_z);
            }
            else if ((eid == 0) && (gid == 8)) // GRP_RawCountRate
            {
                float count_rate;
                uint16_t flags;
                br.readFloat(&count_rate);
                br.readUint16(&flags);
            }
            else if ((eid == 1) && (gid == 1))
            {
                uint16_t samples_num;
                uint32_t smpl_time_ms;
                br.readUint16(&samples_num);
                br.readUint32(&smpl_time_ms);

                // Skip 8 bytes per sample
                br.setPosition(br.getPosition() + 8 * samples_num);
            }
            else if ((eid == 1) && (gid == 2))
            {
                uint16_t samples_num;
                uint32_t smpl_time_ms;
                br.readUint16(&samples_num);
                br.readUint32(&smpl_time_ms);

                // Skip 16 bytes per sample
                br.setPosition(br.getPosition() + 16 * samples_num);
            }
            else if ((eid == 1) && (gid == 3))
            {
                uint16_t samples_num;
                uint32_t smpl_time_ms;
                br.readUint16(&samples_num);
                br.readUint32(&smpl_time_ms);

                // Skip 14 bytes per sample
                br.setPosition(br.getPosition() + 14 * samples_num);
            }
            else
            {
#ifdef DEC_DEBUG_WARNING
                Serial.print("Warning: Unknown data type: eid=");
                Serial.print(eid);
                Serial.print(", gid=");
                Serial.println(gid);
#endif
                break; // Stop processing on unknown data type
            }
        }
    }

    return ret;
}
