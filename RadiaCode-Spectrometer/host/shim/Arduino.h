// Host-build shim for the handful of Arduino symbols the analysis layer uses.
// Only enough surface to let src/analysis and src/data compile unmodified on
// a PC; nothing here is meant to emulate the ESP32.
#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdarg.h>
#include <string>
#include <chrono>

// --- String -----------------------------------------------------------------
// background.cpp holds a Preferences::getString() result before copying it out.
class String {
 public:
  String() {}
  String(const char* s) : s_(s ? s : "") {}
  String(const std::string& s) : s_(s) {}
  const char* c_str() const { return s_.c_str(); }
  size_t length() const { return s_.size(); }
 private:
  std::string s_;
};

// --- Serial -----------------------------------------------------------------
class HostSerial {
 public:
  int printf(const char* fmt, ...) {
    va_list ap; va_start(ap, fmt);
    int n = vfprintf(stdout, fmt, ap);
    va_end(ap);
    return n;
  }
  void print(const char* s)   { fputs(s, stdout); }
  void println(const char* s) { fputs(s, stdout); fputc('\n', stdout); }
  void println()              { fputc('\n', stdout); }
};
extern HostSerial Serial;

// --- misc -------------------------------------------------------------------
// PSRAM allocation degrades to plain malloc on the host.
inline void* ps_malloc(size_t n) { return malloc(n); }

inline unsigned long millis() {
  using namespace std::chrono;
  static const auto t0 = steady_clock::now();
  return (unsigned long)duration_cast<milliseconds>(steady_clock::now() - t0).count();
}
inline void delay(unsigned long) {}
