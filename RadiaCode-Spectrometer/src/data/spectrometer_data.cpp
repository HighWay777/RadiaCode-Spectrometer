#include "spectrometer_data.h"

// Single canonical definition — both MockSource and BleSource (data/mock_source.*,
// data/ble_source.*) write into this same instance; only one of them should be
// ticked from main.cpp at a time.
SpectrometerData g_data;
