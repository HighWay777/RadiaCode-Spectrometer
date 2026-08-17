#pragma once

/**
 * ========================================
 * Boiler API Credentials Configuration
 * ========================================
 * 
 * EASY TO EDIT: Update these credentials to match your remote boiler API.
 * 
 * SECURITY NOTE: 
 * - These credentials are compiled into the firmware
 * - Consider implementing token-based auth for production
 */

// ========================================
// 🔧 EDIT YOUR BOILER API CREDENTIALS HERE 🔧
// ========================================

// Remote boiler API endpoint
static const char* const BOILER_API_BASE = "https://boiler101.vhelectronics.com";

// HTTP Basic Auth credentials
static const char* const BOILER_USER = "evg";
static const char* const BOILER_PASS = "***REMOVED***";

// API timeout (milliseconds)
static const int BOILER_API_TIMEOUT = 5000;  // 5 seconds
