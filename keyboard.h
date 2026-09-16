#ifndef KEYBOARD_H
#define KEYBOARD_H

#include <stdint.h>
#include "main.h"

void initKeyboard(void);
char readKey(void);
char scanKeyboard(void);   // возвращает новую нажатую клавишу (edge-detect) или '\0'

extern char lastKey;
extern uint32_t lastScanTime;

#endif
