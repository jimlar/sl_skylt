#include <Wire.h>
#include <Adafruit_GFX.h>
#include "Adafruit_LEDBackpack.h"

Adafruit_8x16minimatrix matrix = Adafruit_8x16minimatrix();

void setup() {
  matrix.begin(0x70);
  matrix.clear();
  matrix.setTextSize(1);
  matrix.setTextWrap(false);
  matrix.setTextColor(LED_ON);
  matrix.setRotation(1);
  matrix.writeDisplay();
  matrix.setBrightness(10);
}

void loop() {
  for (int8_t x=7; x>=-256; x--) {
    matrix.clear();
    matrix.setCursor(x,0);
    matrix.print("The world is yours!");
    matrix.writeDisplay();
    delay(30);
  }
  delay(2000);
}

