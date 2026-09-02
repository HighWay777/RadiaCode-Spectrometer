// Host shim for ESP32 NVS. Backed by an in-memory map so background.cpp
// compiles and runs unmodified; nothing persists between replay runs, which
// is what we want - each replay starts from a known state.
#pragma once

#include <Arduino.h>
#include <map>
#include <vector>

class Preferences {
 public:
  bool begin(const char* ns, bool /*readOnly*/ = false) { ns_ = ns; return true; }
  void end() {}

  size_t getBytesLength(const char* k) { return blob(k).size(); }
  size_t getBytes(const char* k, void* out, size_t len) {
    const std::vector<uint8_t>& b = blob(k);
    size_t n = b.size() < len ? b.size() : len;
    if (n) memcpy(out, b.data(), n);
    return n;
  }
  size_t putBytes(const char* k, const void* in, size_t len) {
    const uint8_t* p = (const uint8_t*)in;
    store()[key(k)].bytes.assign(p, p + len);
    return len;
  }

  bool     getBool (const char* k, bool d = false)          { return has(k) ? store()[key(k)].u != 0 : d; }
  uint32_t getULong(const char* k, uint32_t d = 0)          { return has(k) ? (uint32_t)store()[key(k)].u : d; }
  String   getString(const char* k, const char* d = "")     { return has(k) ? String(store()[key(k)].s) : String(d); }

  void putBool  (const char* k, bool v)        { store()[key(k)].u = v ? 1 : 0; }
  void putULong (const char* k, uint32_t v)    { store()[key(k)].u = v; }
  void putString(const char* k, const char* v) { store()[key(k)].s = v ? v : ""; }

  void remove(const char* k) { store().erase(key(k)); }

 private:
  struct Entry { unsigned long long u = 0; std::string s; std::vector<uint8_t> bytes; };
  std::string ns_;
  std::string key(const char* k) { return ns_ + "/" + k; }
  bool has(const char* k) { return store().count(key(k)) != 0; }
  const std::vector<uint8_t>& blob(const char* k) {
    static const std::vector<uint8_t> empty;
    return has(k) ? store()[key(k)].bytes : empty;
  }
  static std::map<std::string, Entry>& store() {
    static std::map<std::string, Entry> s;
    return s;
  }
};
