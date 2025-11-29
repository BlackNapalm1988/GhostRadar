#include "Dictionary.h"
#include "config_core.h"
#include "Settings.h"
#include <SPIFFS.h>
#include <FS.h>
#include <vector>

static std::vector<String> dictionary;
static char letterBuffer[LETTER_BUFFER_SIZE];
static int letterCount = 0;
static uint8_t activeDictIndex = 0;
static bool spiffsMounted = false;

static const char* const DICT_FILES[] = {
  "/words.txt",
  "/paranormal.txt",
  "/short.txt"
};

static const char* const DICT_NAMES[] = {
  "Default",
  "Paranormal",
  "Short"
};

static const size_t DICT_FILE_COUNT = sizeof(DICT_FILES) / sizeof(DICT_FILES[0]);

// small fallback dict
static const char* FALLBACK_DICT[] = {
  "HELLO",
  "GHOST",
  "SPIRIT",
  "YES",
  "NO",
  "DEMON",
  "ANGEL",
  "LIGHT",
  "DARK",
  "COLD",
  "HOT",
};
static const int FALLBACK_DICT_SIZE = sizeof(FALLBACK_DICT) / sizeof(FALLBACK_DICT[0]);

static void loadFallbackDictionary() {
  dictionary.clear();
  dictionary.reserve(FALLBACK_DICT_SIZE);
  for (int i = 0; i < FALLBACK_DICT_SIZE; i++) {
    dictionary.push_back(String(FALLBACK_DICT[i]));
  }
  Serial.print("Loaded fallback dictionary, size=");
  Serial.println(dictionary.size());
}

static bool loadDictionaryFromSPIFFS(const char* path) {
  dictionary.clear();
  dictionary.reserve(512);  // reduce reallocations/fragmentation for moderate dictionaries

  if (!SPIFFS.exists(path)) {
    Serial.print("Dictionary file not found: ");
    Serial.println(path);
    return false;
  }

  File file = SPIFFS.open(path, "r");
  if (!file) {
    Serial.print("Failed to open dictionary file: ");
    Serial.println(path);
    return false;
  }

  Serial.print("Loading dictionary from ");
  Serial.println(path);

  while (file.available()) {
    String line = file.readStringUntil('\n');
    line.trim();
    if (line.length() == 0) continue;
    line.toUpperCase();
    if (line.length() > LETTER_BUFFER_SIZE) continue;
    dictionary.push_back(line);
  }
  file.close();

  Serial.print("Loaded ");
  Serial.print(dictionary.size());
  Serial.println(" words from SPIFFS.");

  return !dictionary.empty();
}

bool Dictionary_setActiveIndex(uint8_t idx) {
  if (DICT_FILE_COUNT == 0) return false;
  uint8_t clamped = Settings_clampDictionaryIndex(idx, DICT_FILE_COUNT - 1);
  activeDictIndex = clamped;
  Settings_get().dictionaryIndex = clamped;
  bool loaded = false;

  if (spiffsMounted) {
    loaded = loadDictionaryFromSPIFFS(DICT_FILES[clamped]);
  }
  if (!loaded) {
    Serial.println("Using fallback dictionary.");
    loadFallbackDictionary();
  }

  Dictionary_clearBufferAndWord();
  return loaded;
}

void Dictionary_begin() {
  letterCount = 0;
  activeDictIndex = Settings_clampDictionaryIndex(Settings_get().dictionaryIndex, DICT_FILE_COUNT - 1);

  spiffsMounted = SPIFFS.begin(false);
  if (!spiffsMounted) {
    Serial.println("SPIFFS mount failed (no auto-format). Using fallback dictionary.");
    loadFallbackDictionary();
    Settings_get().dictionaryIndex = 0;
    activeDictIndex = 0;
    return;
  }

  Serial.println("SPIFFS mounted.");
  // List files (debug)
  File root = SPIFFS.open("/");
  File f = root.openNextFile();
  while (f) {
    Serial.print("FILE: ");
    Serial.println(f.name());
    f = root.openNextFile();
  }

  if (!Dictionary_setActiveIndex(activeDictIndex)) {
    Serial.println("Failed to load requested dictionary, using fallback.");
  }
}

void Dictionary_appendLetter(char l) {
  if (letterCount < LETTER_BUFFER_SIZE) {
    letterBuffer[letterCount++] = l;
  } else {
    memmove(letterBuffer, letterBuffer + 1, LETTER_BUFFER_SIZE - 1);
    letterBuffer[LETTER_BUFFER_SIZE - 1] = l;
  }
}

bool Dictionary_checkForWord(String &foundWord) {
  if (letterCount == 0 || dictionary.empty()) return false;

  for (size_t i = 0; i < dictionary.size(); i++) {
    const String &word = dictionary[i];
    int len = word.length();
    if (len > letterCount) continue;

    bool match = true;
    for (int j = 0; j < len; j++) {
      char bufChar = letterBuffer[letterCount - len + j];
      char dictChar = word[j];
      if (bufChar != dictChar) {
        match = false;
        break;
      }
    }

    if (match) {
      foundWord = word;

      int keep = 2;
      if (keep > letterCount) keep = letterCount;
      char temp[LETTER_BUFFER_SIZE];
      memcpy(temp, letterBuffer + (letterCount - keep), keep);
      memcpy(letterBuffer, temp, keep);
      letterCount = keep;

      return true;
    }
  }
  return false;
}

void Dictionary_clearBufferAndWord() {
  letterCount = 0;
}

uint8_t Dictionary_getActiveIndex() {
  return activeDictIndex;
}

uint8_t Dictionary_getCount() {
  return (uint8_t)DICT_FILE_COUNT;
}

const char* Dictionary_getActiveName() {
  return Dictionary_getNameForIndex(activeDictIndex);
}

const char* Dictionary_getNameForIndex(uint8_t idx) {
  if (DICT_FILE_COUNT == 0) return nullptr;
  if (idx >= DICT_FILE_COUNT) idx = DICT_FILE_COUNT - 1;
  return DICT_NAMES[idx];
}
