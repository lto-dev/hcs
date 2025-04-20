#pragma once
#include <Arduino.h>
#include <Preferences.h>
#include <time.h>

// Growth Profile Stage structure
struct GrowthStage {
  int duration;           // Duration in days
  int waterDuration;      // Watering duration in minutes
  int waterInterval;      // Watering interval in minutes
  int lightHours;         // Light hours per day
  int lightStartHour;     // Hour of day when lights turn on (24h format)
  float phMin;            // Minimum pH value
  float phMax;            // Maximum pH value
};

// Growth Profile structure
struct GrowthProfile {
  char id[32];            // Unique identifier for the profile
  char name[64];          // Display name for the profile
  GrowthStage seedling;   // Seedling stage settings
  GrowthStage growing;    // Growing stage settings
  GrowthStage harvesting; // Harvesting stage settings
};

// Active Growth Cycle structure
struct GrowthCycle {
  char profileId[32];     // ID of the active profile
  long startTime;         // Start time as Unix timestamp
  bool active;            // Whether the cycle is active
};

// Maximum number of profiles to store
#define MAX_PROFILES 10

class GrowthManager {
private:
  Preferences& _preferences;
  GrowthProfile _profiles[MAX_PROFILES];
  int _profileCount = 0;
  GrowthCycle _activeCycle = {"", 0, false};
  
  // Default profile definitions
  static const GrowthProfile DEFAULT_PROFILES[3];
  
public:
  GrowthManager(Preferences& preferences) : _preferences(preferences) {}

  void begin() {
    loadProfiles();
    loadActiveCycle();
  }

  // Get all profiles and count
  const GrowthProfile* getProfiles() const { return _profiles; }
  int getProfileCount() const { return _profileCount; }
  const GrowthCycle& getActiveCycle() const { return _activeCycle; }

  // Functions for Growth Profile management
  void saveProfiles() {
    _preferences.begin("hydroGrowth", false);
    
    // Save profile count
    _preferences.putInt("profileCount", _profileCount);
    
    // Save each profile
    for (int i = 0; i < _profileCount; i++) {
      String prefix = "profile" + String(i);
      _preferences.putBytes(prefix.c_str(), &_profiles[i], sizeof(GrowthProfile));
    }
    
    _preferences.end();
    Serial.printf("Saved %d profiles\n", _profileCount);
  }

  void loadProfiles() {
    _preferences.begin("hydroGrowth", false);
    
    // Load profile count (default to 0 if not found)
    _profileCount = _preferences.getInt("profileCount", 0);
    
    // If no profiles exist, initialize with defaults
    if (_profileCount == 0) {
      Serial.println("No saved profiles found - initializing with defaults");
      
      // Copy default profiles
      for (int i = 0; i < 3; i++) { // 3 default profiles
        if (i < MAX_PROFILES) {
          memcpy(&_profiles[i], &DEFAULT_PROFILES[i], sizeof(GrowthProfile));
          _profileCount++;
        }
      }
      
      // Save the initialized profiles
      saveProfiles();
    } else {
      // Load existing profiles
      for (int i = 0; i < _profileCount && i < MAX_PROFILES; i++) {
        String prefix = "profile" + String(i);
        _preferences.getBytes(prefix.c_str(), &_profiles[i], sizeof(GrowthProfile));
      }
      Serial.printf("Loaded %d profiles\n", _profileCount);
    }
    
    _preferences.end();
  }

  void saveActiveCycle() {
    _preferences.begin("hydroGrowth", false);
    _preferences.putBytes("activeCycle", &_activeCycle, sizeof(GrowthCycle));
    _preferences.end();
    Serial.println("Saved active cycle");
  }

  void loadActiveCycle() {
    _preferences.begin("hydroGrowth", false);
    
    // Check if active cycle exists
    if (_preferences.getBytesLength("activeCycle") == sizeof(GrowthCycle)) {
      _preferences.getBytes("activeCycle", &_activeCycle, sizeof(GrowthCycle));
      Serial.println("Loaded active cycle");
      
      // Validate the loaded cycle
      bool validCycle = false;
      for (int i = 0; i < _profileCount; i++) {
        if (strcmp(_activeCycle.profileId, _profiles[i].id) == 0) {
          validCycle = true;
          break;
        }
      }
      
      if (!validCycle && _activeCycle.active) {
        Serial.println("Warning: Active cycle references non-existent profile, disabling");
        _activeCycle.active = false;
        saveActiveCycle();
      }
    } else {
      // Initialize a new inactive cycle
      strlcpy(_activeCycle.profileId, "", sizeof(_activeCycle.profileId));
      _activeCycle.startTime = 0;
      _activeCycle.active = false;
      Serial.println("No active cycle found");
    }
    
    _preferences.end();
  }

  // Find a profile by ID
  GrowthProfile* findProfileById(const char* id) {
    for (int i = 0; i < _profileCount; i++) {
      if (strcmp(_profiles[i].id, id) == 0) {
        return &_profiles[i];
      }
    }
    return nullptr;
  }

  // Get current growth stage based on active cycle
  String getCurrentGrowthStage(unsigned long currentTime) {
    if (!_activeCycle.active) {
      return "None";
    }
    
    // Find the profile
    GrowthProfile* profile = findProfileById(_activeCycle.profileId);
    if (!profile) {
      return "Invalid";
    }
    
    // Calculate elapsed days
    unsigned long elapsedDays = (currentTime - _activeCycle.startTime) / (24 * 60 * 60);
    
    // Determine current stage - harvesting stage continues indefinitely
    if (elapsedDays < profile->seedling.duration) {
      return "Seedling";
    } else if (elapsedDays < (profile->seedling.duration + profile->growing.duration)) {
      return "Growing";
    } else {
      // Harvesting stage continues until cycle is stopped or changed
      return "Harvesting";
    }
  }

  // Get current growth stage settings
  GrowthStage* getCurrentStageSettings() {
    if (!_activeCycle.active) {
      return nullptr;
    }
    
    // Find the profile
    GrowthProfile* profile = findProfileById(_activeCycle.profileId);
    if (!profile) {
      return nullptr;
    }
    
    // Calculate elapsed days
    unsigned long currentTime = time(nullptr);
    unsigned long elapsedDays = (currentTime - _activeCycle.startTime) / (24 * 60 * 60);
    
    // Determine current stage and return appropriate settings
    // Harvesting stage continues indefinitely
    if (elapsedDays < profile->seedling.duration) {
      return &profile->seedling;
    } else if (elapsedDays < (profile->seedling.duration + profile->growing.duration)) {
      return &profile->growing;
    } else {
      // Harvesting stage continues until cycle is stopped or changed
      return &profile->harvesting;
    }
  }

  // Add a new profile
  bool addProfile(GrowthProfile* newProfile) {
    if (_profileCount >= MAX_PROFILES) {
      return false;
    }
    
    // Check if ID exists
    for (int i = 0; i < _profileCount; i++) {
      if (strcmp(_profiles[i].id, newProfile->id) == 0) {
        // Update existing profile
        memcpy(&_profiles[i], newProfile, sizeof(GrowthProfile));
        saveProfiles();
        return true;
      }
    }
    
    // Add new profile
    memcpy(&_profiles[_profileCount], newProfile, sizeof(GrowthProfile));
    _profileCount++;
    saveProfiles();
    return true;
  }

  // Update a profile
  bool updateProfile(const char* id, GrowthProfile* updatedProfile) {
    // Find and update the profile
    for (int i = 0; i < _profileCount; i++) {
      if (strcmp(_profiles[i].id, id) == 0) {
        // Copy content but preserve the original ID
        strlcpy(updatedProfile->id, id, sizeof(updatedProfile->id));
        memcpy(&_profiles[i], updatedProfile, sizeof(GrowthProfile));
        saveProfiles();
        return true;
      }
    }
    
    return false;
  }

  // Start a growth cycle
  bool startGrowthCycle(const char* profileId, unsigned long startTime) {
    // Find the profile
    bool profileFound = false;
    for (int i = 0; i < _profileCount; i++) {
      if (strcmp(_profiles[i].id, profileId) == 0) {
        profileFound = true;
        break;
      }
    }
    
    if (!profileFound) {
      return false;
    }
    
    // Update active cycle
    strlcpy(_activeCycle.profileId, profileId, sizeof(_activeCycle.profileId));
    _activeCycle.startTime = startTime;
    _activeCycle.active = true;
    
    // Save to persistent storage
    saveActiveCycle();
    return true;
  }

  // Stop the active growth cycle
  void stopGrowthCycle() {
    _activeCycle.active = false;
    saveActiveCycle();
  }
};

// Initialize default profiles
const GrowthProfile GrowthManager::DEFAULT_PROFILES[3] = {
  {
    "tomatoes",
    "Tomatoes",
    {14, 5, 60, 8, 6, 5.5, 6.5},   // Seedling - 6AM light start
    {35, 5, 30, 12, 6, 5.8, 6.2},  // Growing - 6AM light start
    {21, 5, 45, 10, 6, 6.0, 6.5}   // Harvesting - 6AM light start
  },
  {
    "peppers",
    "Peppers",
    {14, 5, 120, 10, 6, 5.5, 6.5}, // Seedling - 6AM light start
    {30, 5, 45, 14, 6, 5.8, 6.3},  // Growing - 6AM light start
    {14, 5, 60, 12, 6, 5.8, 6.5}   // Harvesting - 6AM light start
  },
  {
    "lettuce",
    "Lettuce",
    {7, 5, 90, 10, 6, 5.6, 6.2},   // Seedling - 6AM light start
    {21, 5, 40, 12, 6, 5.6, 6.2},  // Growing - 6AM light start
    {7, 5, 30, 12, 6, 5.8, 6.0}    // Harvesting - 6AM light start
  }
};
