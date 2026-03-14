#ifndef OTA_H
#define OTA_H

// OTA Firmware Update Module
// Uses FlasherX for Teensy 4.1 flash programming
// Receives Intel HEX files via WebSocket and programs flash

extern "C" {
  #include "FlashTxx.h"
}

#include "Debug.h"

// Hex record info structure (from FXUtil.cpp)
typedef struct {
  char data[32] __attribute__ ((aligned (8)));  // buffer for hex data
  unsigned int addr;    // address in intel hex record
  unsigned int code;    // intel hex record type (0=data, etc.)
  unsigned int num;     // number of data bytes in intel hex record
  uint32_t base;        // base address to be added to intel hex 16-bit addr
  uint32_t min;         // min address in hex file
  uint32_t max;         // max address in hex file
  int eof;              // set true on intel hex EOF (code = 1)
  int lines;            // number of hex records received
} hex_info_t;


class OTAUpdate {
public:
  uint32_t bytesReceived = 0;
  uint32_t linesProcessed = 0;
  String lastError = "";
  bool updateInProgress = false;
  bool pendingFlash = false;

  // Callback to poll network during flash operations
  void (*networkPollCallback)() = nullptr;

  void setNetworkPollCallback(void (*callback)()) {
    networkPollCallback = callback;
  }

  // Initialize flash buffer and start update
  bool startUpdate() {
    info("OTA: startUpdate() called");

    if (updateInProgress) {
      lastError = "Update already in progress";
      err("OTA: ", lastError.c_str());
      return false;
    }

    // Use RAM buffer instead of flash buffer to avoid flash-on-flash issues
    // Try progressively smaller buffers until one fits in available heap
    info("OTA: Free RAM before alloc: ", freeHeap() / 1024, "KB");

    const uint32_t sizes[] = {512 * 1024, 400 * 1024, 300 * 1024, 256 * 1024};
    bufferAddr = 0;
    for (uint32_t sz : sizes) {
      bufferSize = sz;
      bufferAddr = (uint32_t)malloc(bufferSize);
      if (bufferAddr != 0) break;
    }

    if (bufferAddr == 0) {
      lastError = String("Failed to allocate RAM buffer (free: ") + (freeHeap() / 1024) + "KB)";
      err("OTA: ", lastError.c_str());
      return false;
    }

    // Initialize buffer to 0xFF (like erased flash)
    memset((void*)bufferAddr, 0xFF, bufferSize);

    info("OTA: Using RAM buffer at 0x", String(bufferAddr, HEX).c_str(),
         " size ", bufferSize / 1024, "KB");

    usingRamBuffer = true;

    // Reset hex info
    hex.addr = 0;
    hex.code = 0;
    hex.num = 0;
    hex.base = 0;
    hex.min = 0xFFFFFFFF;
    hex.max = 0;
    hex.eof = 0;
    hex.lines = 0;

    bytesReceived = 0;
    linesProcessed = 0;
    lastError = "";
    updateInProgress = true;
    pendingFlash = false;

    return true;
  }

  // Parse and validate a single hex line, write to flash buffer
  // Returns true on success, false on error (check lastError)
  bool processHexLine(const char* line) {
    if (!updateInProgress) {
      lastError = "No update in progress";
      return false;
    }

    if (hex.eof) {
      // Already got EOF, ignore additional lines
      return true;
    }

    // Parse the hex line with checksum validation
    if (!parseHexLine(line, hex.data, &hex.addr, &hex.num, &hex.code)) {
      lastError = String("Bad hex line: ") + line;
      return false;
    }

    // Process the record type
    if (!processHexRecord()) {
      lastError = String("Invalid hex code: ") + hex.code;
      return false;
    }

    // If data record, write to flash buffer
    if (hex.code == 0) {
      uint32_t addr = bufferAddr + hex.base + hex.addr - FLASH_BASE_ADDR;

      // Check address bounds
      if (hex.max > (FLASH_BASE_ADDR + bufferSize)) {
        lastError = String("Address too large: 0x") + String(hex.max, HEX);
        return false;
      }

      // Write to RAM buffer (simple memcpy, no flash operations during upload)
      memcpy((void*)addr, (void*)hex.data, hex.num);

      bytesReceived += hex.num;
    }

    hex.lines++;
    linesProcessed = hex.lines;

    return true;
  }

  // Finalize update - verify firmware and prepare for flash
  // Returns true if ready to flash, false on error
  bool finishUpdate() {
    if (!updateInProgress) {
      lastError = "No update in progress";
      return false;
    }

    if (!hex.eof) {
      lastError = "No EOF record received";
      return false;
    }

    info("OTA: Received ", hex.lines, " lines, ",
         hex.max - hex.min, " bytes (0x", String(hex.min, HEX).c_str(),
         " - 0x", String(hex.max, HEX).c_str(), ")");

    // Check FLASH_ID in new code
    if (!check_flash_id(bufferAddr, hex.max - hex.min)) {
      lastError = String("Firmware missing target ID: ") + FLASH_ID;
      abortUpdate();
      return false;
    }

    info("OTA: Firmware verified for ", FLASH_ID);

    // Store values needed for flash_move
    firmwareSize = hex.max - hex.min;
    pendingFlash = true;

    return true;
  }

  // Abort update and clean up
  void abortUpdate() {
    if (updateInProgress && bufferAddr != 0) {
      if (usingRamBuffer) {
        free((void*)bufferAddr);
      } else {
        firmware_buffer_free(bufferAddr, bufferSize);
      }
    }
    updateInProgress = false;
    pendingFlash = false;
    usingRamBuffer = false;
    bufferAddr = 0;
    bufferSize = 0;
  }

  // Check if flash operation is pending
  bool hasPendingFlash() {
    return pendingFlash;
  }

  // Execute flash_move - MUST be called from main loop, not callback!
  // This will reboot the device
  void executeFlash() {
    if (!pendingFlash) return;

    info("OTA: Flashing ", firmwareSize, " bytes...");
    Serial.flush();  // Ensure message is sent before disabling interrupts
    delay(10);       // Give serial time to complete

    // Disable interrupts - flash_move is critical and cannot be interrupted
    __disable_irq();

    // This function does not return - it moves firmware and reboots
    flash_move(FLASH_BASE_ADDR, bufferAddr, firmwareSize);

    // Should not reach here, but just in case
    REBOOT;
  }

  // Get progress percentage (0-100)
  int getProgress() {
    if (!updateInProgress || bufferSize == 0) return 0;
    // Estimate based on typical firmware size (~500KB)
    return min(99, (int)(bytesReceived * 100 / 500000));
  }

private:
  uint32_t bufferAddr = 0;
  uint32_t bufferSize = 0;
  uint32_t firmwareSize = 0;
  bool usingRamBuffer = false;
  hex_info_t hex;

  // Estimate largest free heap block by probing with malloc
  uint32_t freeHeap() {
    uint32_t lo = 0, hi = 1024 * 1024;
    while (hi - lo > 1024) {
      uint32_t mid = (lo + hi) / 2;
      void* p = malloc(mid);
      if (p) { free(p); lo = mid; }
      else { hi = mid; }
    }
    return lo;
  }

  // Parse Intel HEX line with checksum validation
  // Returns true on valid line, false on error
  bool parseHexLine(const char* theline, char* bytes,
                    unsigned int* addr, unsigned int* num, unsigned int* code) {
    unsigned sum, len, cksum;
    const char* ptr;
    int temp;

    *num = 0;

    if (theline[0] != ':')
      return false;
    if (strlen(theline) < 11)
      return false;

    ptr = theline + 1;
    if (!sscanf(ptr, "%02x", &len))
      return false;

    ptr += 2;
    if (strlen(theline) < (11 + (len * 2)))
      return false;

    if (!sscanf(ptr, "%04x", addr))
      return false;

    ptr += 4;
    if (!sscanf(ptr, "%02x", code))
      return false;

    ptr += 2;

    // Calculate checksum as we go
    sum = (len & 255) + ((*addr >> 8) & 255) + (*addr & 255) + (*code & 255);

    while (*num != len) {
      if (!sscanf(ptr, "%02x", &temp))
        return false;
      bytes[*num] = temp;
      ptr += 2;
      sum += bytes[*num] & 255;
      (*num)++;
      if (*num >= 256)
        return false;
    }

    if (!sscanf(ptr, "%02x", &cksum))
      return false;

    // Verify checksum - sum of all bytes including checksum should be 0
    if (((sum & 255) + (cksum & 255)) & 255)
      return false;  // Checksum error

    return true;
  }

  // Process hex record type
  bool processHexRecord() {
    if (hex.code == 0) {
      // Data record - update min/max address
      if (hex.base + hex.addr + hex.num > hex.max)
        hex.max = hex.base + hex.addr + hex.num;
      if (hex.base + hex.addr < hex.min)
        hex.min = hex.base + hex.addr;
    }
    else if (hex.code == 1) {
      // EOF
      hex.eof = 1;
    }
    else if (hex.code == 2) {
      // Extended segment address (top 16 of 24-bit addr)
      hex.base = ((hex.data[0] << 8) | hex.data[1]) << 4;
    }
    else if (hex.code == 3) {
      // Start segment address (80x86 real mode only) - skip
      return true;
    }
    else if (hex.code == 4) {
      // Extended linear address (top 16 of 32-bit addr)
      hex.base = ((hex.data[0] << 8) | hex.data[1]) << 16;
    }
    else if (hex.code == 5) {
      // Start linear address (32-bit big endian addr)
      hex.base = (hex.data[0] << 24) | (hex.data[1] << 16)
               | (hex.data[2] << 8) | (hex.data[3] << 0);
    }
    else {
      return false;  // Unknown record type
    }

    return true;
  }
};

// Global OTA instance
OTAUpdate OTA;

#endif
