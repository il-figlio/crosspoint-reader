#include "TimeManager.h"

#include <HardwareSerial.h>
#include <SDCardManager.h>
#include <Serialization.h>
#include <time.h>

// Initialize the static instance
TimeManager TimeManager::instance;

namespace {
constexpr uint8_t TIME_FILE_VERSION = 1;
constexpr char TIME_FILE[] = "/.crosspoint/time.bin";
// Maximum age of saved time before we consider it stale (24 hours in ms)
constexpr unsigned long MAX_TIME_AGE_MS = 24UL * 60 * 60 * 1000;
}  // namespace

bool TimeManager::syncFromNtp() {
  Serial.printf("[%lu] [TM] Starting NTP sync...\n", millis());

  // Configure time with UTC offset 0 - we'll show local time based on device location later
  // For now, just sync to UTC
  configTime(0, 0, "pool.ntp.org", "time.nist.gov", "time.google.com");

  struct tm timeinfo;
  if (!getLocalTime(&timeinfo, 10000)) {  // 10 second timeout
    Serial.printf("[%lu] [TM] NTP sync failed - timeout\n", millis());
    return false;
  }

  syncedTime = mktime(&timeinfo);
  syncedMillis = millis();
  timeValid = true;

  Serial.printf("[%lu] [TM] NTP sync successful: %04d-%02d-%02d %02d:%02d:%02d\n", millis(), timeinfo.tm_year + 1900,
                timeinfo.tm_mon + 1, timeinfo.tm_mday, timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);

  saveToFile();
  return true;
}

std::string TimeManager::getTimeString() const {
  if (!timeValid) {
    return "";
  }

  // Calculate current time from synced values
  const unsigned long elapsedMs = millis() - syncedMillis;
  const unsigned long elapsedSecs = elapsedMs / 1000;
  const time_t currentTime = syncedTime + static_cast<time_t>(elapsedSecs);

  struct tm* timeinfo = localtime(&currentTime);
  if (timeinfo == nullptr) {
    return "";
  }

  char buffer[6];
  snprintf(buffer, sizeof(buffer), "%02d:%02d", timeinfo->tm_hour, timeinfo->tm_min);
  return std::string(buffer);
}

bool TimeManager::saveToFile() const {
  // Make sure the directory exists
  SdMan.mkdir("/.crosspoint");

  FsFile outputFile;
  if (!SdMan.openFileForWrite("TM", TIME_FILE, outputFile)) {
    Serial.printf("[%lu] [TM] Failed to open time file for writing\n", millis());
    return false;
  }

  serialization::writePod(outputFile, TIME_FILE_VERSION);
  serialization::writePod(outputFile, syncedTime);
  serialization::writePod(outputFile, syncedMillis);
  outputFile.close();

  Serial.printf("[%lu] [TM] Time data saved to file\n", millis());
  return true;
}

bool TimeManager::loadFromFile() {
  FsFile inputFile;
  if (!SdMan.openFileForRead("TM", TIME_FILE, inputFile)) {
    Serial.printf("[%lu] [TM] No saved time file found\n", millis());
    return false;
  }

  uint8_t version;
  serialization::readPod(inputFile, version);
  if (version != TIME_FILE_VERSION) {
    Serial.printf("[%lu] [TM] Unknown time file version: %u\n", millis(), version);
    inputFile.close();
    return false;
  }

  time_t savedTime;
  unsigned long savedMillis;
  serialization::readPod(inputFile, savedTime);
  serialization::readPod(inputFile, savedMillis);
  inputFile.close();

  // Calculate how much time has passed since the save
  // Note: millis() resets on reboot, so we need to be careful here
  // If current millis() is less than savedMillis, we've rebooted
  const unsigned long currentMillis = millis();

  if (currentMillis < savedMillis) {
    // Device has rebooted - we can't reliably calculate elapsed time
    // The saved time is stale, but we can use it as a rough estimate
    // Assume minimal time has passed since save (just the boot time)
    syncedTime = savedTime;
    syncedMillis = currentMillis;  // Reset to current millis as baseline
    timeValid = true;
    Serial.printf("[%lu] [TM] Loaded time after reboot (approximate)\n", millis());
  } else {
    // No reboot - calculate elapsed time
    const unsigned long elapsed = currentMillis - savedMillis;
    if (elapsed > MAX_TIME_AGE_MS) {
      // Time is too old, don't use it
      Serial.printf("[%lu] [TM] Saved time too old (%lu ms), ignoring\n", millis(), elapsed);
      return false;
    }
    syncedTime = savedTime;
    syncedMillis = savedMillis;
    timeValid = true;
    Serial.printf("[%lu] [TM] Loaded time from file\n", millis());
  }

  return true;
}
