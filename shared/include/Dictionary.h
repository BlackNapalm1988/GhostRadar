#pragma once
#include <Arduino.h>

void Dictionary_begin();
void Dictionary_appendLetter(char l);
bool Dictionary_checkForWord(String &foundWord);
void Dictionary_clearBufferAndWord();
bool Dictionary_setActiveIndex(uint8_t idx);
uint8_t Dictionary_getActiveIndex();
uint8_t Dictionary_getCount();
const char* Dictionary_getActiveName();
const char* Dictionary_getNameForIndex(uint8_t idx);
