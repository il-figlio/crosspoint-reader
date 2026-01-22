#pragma once
#include <cstdint>
#include <string>

class TimeManager {
 private:
  // Private constructor for singleton
  TimeManager() = default;

  // Static instance
  static TimeManager instance;

  bool timeValid = false;
  time_t syncedTime = 0;         // Unix timestamp at sync
  unsigned long syncedMillis = 0; // millis() at sync

 public:
  // Delete copy constructor and assignment
  TimeManager(const TimeManager&) = delete;
  TimeManager& operator=(const TimeManager&) = delete;

  // Singleton accessor
  static TimeManager& getInstance() { return instance; }

  // Call when WiFi is connected to sync time via NTP
  // Returns true if sync was successful
  bool syncFromNtp();

  // Get current time string "HH:MM"
  // Returns empty string if time is not available
  std::string getTimeString() const;

  // Check if time is available
  bool hasValidTime() const { return timeValid; }

  // Persistence - save/load time sync data to/from SD card
  bool saveToFile() const;
  bool loadFromFile();
};

// Global accessor macro
#define TIME_MANAGER TimeManager::getInstance()
