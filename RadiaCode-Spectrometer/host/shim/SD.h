// Host shim for the SD card, backed by ordinary stdio. Lets
// isotope_library.cpp load and export CSV libraries on the PC unmodified.
#pragma once

#include <Arduino.h>

#define FILE_READ  "rb"
#define FILE_WRITE "wb"

class File {
 public:
  File() {}
  explicit File(FILE* f) : f_(f) {}

  operator bool() const { return f_ != nullptr; }
  void close() { if (f_) { fclose(f_); f_ = nullptr; } }

  bool available() {
    if (!f_) return false;
    int c = fgetc(f_);
    if (c == EOF) return false;
    ungetc(c, f_);
    return true;
  }

  size_t readBytesUntil(char term, char* buf, size_t len) {
    if (!f_) return 0;
    size_t n = 0;
    int c;
    while (n < len && (c = fgetc(f_)) != EOF) {
      if (c == term) break;
      buf[n++] = (char)c;
    }
    return n;
  }

  int printf(const char* fmt, ...) {
    if (!f_) return 0;
    va_list ap; va_start(ap, fmt);
    int n = vfprintf(f_, fmt, ap);
    va_end(ap);
    return n;
  }
  void print(const char* s)   { if (f_) fputs(s, f_); }
  void println(const char* s) { if (f_) { fputs(s, f_); fputc('\n', f_); } }

 private:
  FILE* f_ = nullptr;
};

class SDClass {
 public:
  File open(const char* path, const char* mode = FILE_READ) {
    return File(fopen(path, mode));
  }
  bool exists(const char* path) {
    FILE* f = fopen(path, "rb");
    if (!f) return false;
    fclose(f);
    return true;
  }
};
extern SDClass SD;
