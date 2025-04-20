#ifndef HX710B_H
#define HX710B_H

#include <Arduino.h>

class HX710B {
  private:
    byte dataPin;
    byte clockPin;
    
  public:
    HX710B(byte dout, byte sck);
    void begin();
    bool is_ready();
    long read();
};

#endif
