#include "HX710B.h"

HX710B::HX710B(byte dout, byte sck) : dataPin(dout), clockPin(sck) {}

void HX710B::begin() {
  pinMode(dataPin, INPUT);
  pinMode(clockPin, OUTPUT);
  digitalWrite(clockPin, HIGH);
  delayMicroseconds(100);
  digitalWrite(clockPin, LOW);
}

bool HX710B::is_ready() { 
  return digitalRead(dataPin) == LOW; 
}

long HX710B::read() {
  while (!is_ready());
  
  long value = 0;
  for (byte i = 0; i < 24; i++) {
    digitalWrite(clockPin, HIGH);
    delayMicroseconds(1);
    value = (value << 1) | digitalRead(dataPin);
    digitalWrite(clockPin, LOW);
    delayMicroseconds(1);
  }
  
  // Cycle clock for channel/gain selection
  for (byte i = 0; i < 1; i++) {
    digitalWrite(clockPin, HIGH);
    delayMicroseconds(1);
    digitalWrite(clockPin, LOW);
    delayMicroseconds(1);
  }
  
  return value;
}
