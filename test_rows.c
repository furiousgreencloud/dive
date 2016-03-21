#include <stdio.h>
#include <beaglebone.h>
#include <string.h>
#include <inttypes.h>
#include <assert.h>

#define DEBUG 4

#define PIN_INDEX unsigned

#define ROWS 50
#define COLS 50

#define GRAIN 1
#define DELAY (600/GRAIN)

uint64_t buffer[ROWS*300];

// LO RIGHT
PIN_INDEX loRightRowSelectPins[] = {
  P8_45,
  P8_46,
  P8_44,
  P8_43,
  P8_42, 
  P8_41,
  P8_40,
  P8_39
};
#define LO_RIGHT_CLOCK_PIN P8_38
#define LO_RIGHT_DATA_PIN  P8_37

// LO LEFT
PIN_INDEX loLeftRowSelectPins[] = {
  P8_36,
  P8_35,
  P8_34,
  P8_33,
  P8_32,
  P8_31,
  P8_30,
  P8_29
};
#define LO_LEFT_CLOCK_PIN P8_28
#define LO_LEFT_DATA_PIN  P8_27

// HI RIGHT
PIN_INDEX hiRightRowSelectPins[] = {
  P8_18,
  P8_17,
  P8_19,
  P8_20,
  P8_21,
  P8_22,
  P8_23,
  P8_24
};
#define HI_RIGHT_CLOCK_PIN P8_25
#define HI_RIGHT_DATA_PIN  P8_26

// HI LEFT
PIN_INDEX hiLeftRowSelectPins[] = {
  P8_16,
  P8_15,
  P8_14,
  P8_13,
  P8_12,
  P8_11,
  P8_10,
  P8_9
};
#define HI_LEFT_CLOCK_PIN P8_8
#define HI_LEFT_DATA_PIN  P8_7

#define LO_LATCH_PIN P9_42
#define HI_LATCH_PIN P9_27

#define HALL_EFFECT_PIN P9_12

#define BOOL unsigned

// Forwards
void CheckHallState();  

void shift_out(PIN data_pin, PIN clock_pin, BOOL msbfirst, unsigned char byte) {
  int i;
  if (msbfirst) {
    for(i = 7; i >= 0; i--) {
      unsigned bit = byte & (1 << i);
      digitalWrite(data_pin, bit);
      digitalWrite(clock_pin,1);
      digitalWrite(clock_pin,0);
    }
  } else {
    for(i = 0; i < 8; i++) {
      unsigned bit = byte & (1 << i);
      digitalWrite(data_pin, bit);
      digitalWrite(clock_pin,1);
      digitalWrite(clock_pin,0);
    }
  }
}

void lShiftOut(unsigned char byte, BOOL topbot) {
  if (topbot == LOW) {
    shift_out(pins[LO_LEFT_DATA_PIN],pins[LO_LEFT_CLOCK_PIN], 1, byte);
  } else {
    shift_out(pins[HI_LEFT_DATA_PIN],pins[HI_LEFT_CLOCK_PIN], 0, byte);
  }
}

void rShiftOut(unsigned char byte, BOOL topbot) {
  if (topbot == LOW) {
    shift_out(pins[LO_RIGHT_DATA_PIN],pins[LO_RIGHT_CLOCK_PIN], 1, byte);
  } else {
    shift_out(pins[HI_RIGHT_DATA_PIN],pins[HI_RIGHT_CLOCK_PIN], 0, byte);
  }
}

void Latch(BOOL hilo) {
    if (DEBUG <= 2) printf("Latch\n");
    if (hilo == LOW) {
      digitalWrite(pins[LO_LATCH_PIN],1);
      digitalWrite(pins[LO_LATCH_PIN],0);
    } else {
      digitalWrite(pins[HI_LATCH_PIN],1);
      digitalWrite(pins[HI_LATCH_PIN],0);
    }
}

void RefreshAll(unsigned cycles,unsigned long usdelay) {
  if (DEBUG <=2 ) printf("RefreshAll\n");
  int row; // 1 based
  PIN_INDEX pin;

  unsigned k = cycles;
  while(k) {
    k--;
    for (row = 1; row <= ROWS; row++) {
      uint64_t rowTmp = buffer[row - 1];

      /*
      if (!rowTmp) {
        CheckHallState();
        usleep(usdelay);
        CheckHallState();
        continue;
      }
      */

      BOOL low = (row > 25); // use lower pannels
      unsigned int rowportLo = 0;
      unsigned int rowportHi = 0;
      if (low) {
        int r = row - 25;
        if (r < 16) {
           rowportLo = r;
        } else if (r < 26) {
           rowportLo = (r - 15) << 4; // count up in the next mux
        }
      } else {
        // use high pannels
        int upSidedownRow = 26 - row; 
        // the above is becausel the top rows are upside down
        // so now we can use the name logic on the new 'upsidedown' row number
        if (upSidedownRow < 16) {
           rowportHi = upSidedownRow;
        } else if (upSidedownRow < 26) {
           rowportHi = (upSidedownRow - 15) << 4; // count up in the next mux
        }
      }


      if (low) { 
        rShiftOut(rowTmp >> 7*8, LOW);    // last cols
        rShiftOut(rowTmp >> 6*8, LOW);
        rShiftOut(rowTmp >> 5*8, LOW);
        rShiftOut(rowTmp >> 4*8, LOW);

        lShiftOut(rowTmp >> 3*8, LOW);    
        lShiftOut(rowTmp >> 2*8, LOW);
        lShiftOut(rowTmp >>   8, LOW);
        lShiftOut(rowTmp       , LOW);    // first cols
      } else {
        rowTmp = rowTmp << 7;
        lShiftOut(rowTmp       , low ? LOW : HIGH);    // first cols
        lShiftOut(rowTmp >>   8, low ? LOW : HIGH);
        lShiftOut(rowTmp >> 2*8, low ? LOW : HIGH);
        lShiftOut(rowTmp >> 3*8, low ? LOW : HIGH);    

        rShiftOut(rowTmp >> 4*8, low ? LOW : HIGH);
        rShiftOut(rowTmp >> 5*8, low ? LOW : HIGH);
        rShiftOut(rowTmp >> 6*8, low ? LOW : HIGH);
        rShiftOut(rowTmp >> 7*8, low ? LOW : HIGH);    // last cols
      }

      //if (DEBUG <= 4) printf("\n");

      assert(rowportHi || rowportLo); // one or the other need to be written

      if (rowportLo) {
        for(pin = 0; pin < 8; pin++) {
          digitalWrite(pins[loRightRowSelectPins[pin]], rowportLo & 1);
          digitalWrite(pins[loLeftRowSelectPins[pin]],  rowportLo & 1);
          rowportLo >>= 1;
        }
      } else if (rowportHi) {
        for(pin = 0; pin < 8; pin++) {
          digitalWrite(pins[hiRightRowSelectPins[pin]], rowportHi & 1);
          digitalWrite(pins[hiLeftRowSelectPins[pin]],  rowportHi & 1);
          rowportHi >>= 1;
        }
      }
      Latch(low ? LOW : HIGH);  // Light Row

      CheckHallState();
      usleep(usdelay);

      if (DEBUG <= 3) {
        usleep(5000);
      }

      lShiftOut(0, low ? LOW : HIGH);
      lShiftOut(0, low ? LOW : HIGH);
      lShiftOut(0, low ? LOW : HIGH);
      lShiftOut(0, low ? LOW : HIGH);
      rShiftOut(0, low ? LOW : HIGH);
      rShiftOut(0, low ? LOW : HIGH);
      rShiftOut(0, low ? LOW : HIGH);
      rShiftOut(0, low ? LOW : HIGH);

      Latch(low ? LOW : HIGH);   // Blank Row

      if (low) {
        for(pin = 0; pin < 8; pin++) {
          digitalWrite(pins[loRightRowSelectPins[pin]], 0);
          digitalWrite(pins[loLeftRowSelectPins[pin]] , 0);
        }
      } else {
        for(pin = 0; pin < 8; pin++) {
          digitalWrite(pins[hiRightRowSelectPins[pin]], 0);
          digitalWrite(pins[hiLeftRowSelectPins[pin]] , 0);
        }
      }
    }
  }
}

void Clear() {
  //if (DEBUG <=5) printf("Blanking %d Bytes\n",sizeof(buffer));
  memset(buffer, 0, sizeof(buffer));
}

void WritePoint(unsigned xPos, unsigned  yPos, unsigned value) {
    if (xPos >= 25) {
      xPos += (32-25);
    }
    if (value) {
        buffer[yPos] |=  1LLU << xPos;
    } else {
        buffer[yPos] &= ~(1LLU << xPos);
    }
}

void PeggySetup() {
  PIN_INDEX pin;
  Clear();

  /* Setup Pin Modes */
  pinMode(pins[LO_LATCH_PIN ],OUTPUT);
  pinMode(pins[HI_LATCH_PIN ],OUTPUT);

  pinMode(pins[LO_RIGHT_CLOCK_PIN ],OUTPUT);
  pinMode(pins[LO_RIGHT_DATA_PIN  ],OUTPUT);

  pinMode(pins[LO_LEFT_CLOCK_PIN ],OUTPUT);
  pinMode(pins[LO_LEFT_DATA_PIN  ],OUTPUT);


  pinMode(pins[HI_RIGHT_CLOCK_PIN ],OUTPUT);
  pinMode(pins[HI_RIGHT_DATA_PIN  ],OUTPUT);

  pinMode(pins[HI_LEFT_CLOCK_PIN ],OUTPUT);
  pinMode(pins[HI_LEFT_DATA_PIN  ],OUTPUT);


  int pin_count = 8 * sizeof(PIN_INDEX);
  assert( sizeof(loRightRowSelectPins) == pin_count );
  assert( sizeof(hiRightRowSelectPins) == pin_count );
  assert( sizeof(loLeftRowSelectPins) == pin_count );
  assert( sizeof(hiLeftRowSelectPins) == pin_count );

  for(pin = 0; pin < sizeof(loRightRowSelectPins)/sizeof(PIN_INDEX); pin++) {
    pinMode(pins[hiRightRowSelectPins[pin]],OUTPUT);
    pinMode(pins[hiLeftRowSelectPins[pin]] ,OUTPUT);
    pinMode(pins[loRightRowSelectPins[pin]],OUTPUT);
    pinMode(pins[loLeftRowSelectPins[pin]] ,OUTPUT);
  }


  /* Initial Values */

  digitalWrite(pins[LO_RIGHT_CLOCK_PIN ],0);
  digitalWrite(pins[LO_RIGHT_DATA_PIN  ],0);

  digitalWrite(pins[LO_LEFT_CLOCK_PIN ],0);
  digitalWrite(pins[LO_LEFT_DATA_PIN  ],0);

  digitalWrite(pins[HI_RIGHT_CLOCK_PIN ],0);
  digitalWrite(pins[HI_RIGHT_DATA_PIN  ],0);

  digitalWrite(pins[HI_LEFT_CLOCK_PIN ],0);
  digitalWrite(pins[HI_LEFT_DATA_PIN  ],0);


  for(pin = 0; pin < sizeof(loRightRowSelectPins)/sizeof(PIN_INDEX); pin++) {
    digitalWrite(pins[hiRightRowSelectPins[pin]], (pin == 0) ? HIGH : LOW);
    digitalWrite(pins[hiLeftRowSelectPins[pin ]], (pin == 0) ? HIGH : LOW);
    digitalWrite(pins[loRightRowSelectPins[pin]], (pin == 0) ? HIGH : LOW);
    digitalWrite(pins[loLeftRowSelectPins[pin ]], (pin == 0) ? HIGH : LOW);
  }

  int topbot;
  for (topbot = 0; topbot < 2; topbot++) {
      
      uint64_t pat = 0x1555555ll + (0x1555555ll << 32); 
      Latch(topbot);

      rShiftOut(pat >> 7*8, topbot);    // last cols
      rShiftOut(pat >> 6*8, topbot);
      rShiftOut(pat >> 5*8, topbot);
      rShiftOut(pat >> 4*8, topbot);

      lShiftOut(pat >> 3*8, topbot);    
      lShiftOut(pat >> 2*8, topbot);
      lShiftOut(pat >>   8, topbot);
      lShiftOut(pat       , topbot);    // first cols

      Latch(topbot);
  }


  sleep(10);
}

void setup() {
  PeggySetup();

  pinMode(pins[HALL_EFFECT_PIN],INPUT);

  printf("Setup Complete\n");
}

void CheckHallState() {
    static int s_hall = -1; 
    
    int prev = s_hall;
    s_hall = digitalRead(pins[HALL_EFFECT_PIN]);
    if (prev != s_hall) {
      printf("Hall State is %s\n", s_hall ? "HIGH" : "LOW");
    }
}

int loop() { 
  /*
  sleep(1000);
  return;
  */

  if (DEBUG <=2) printf("Loop Start\n");
  int x,y;

  y = 0;   
  while (y < ROWS) {
    Clear();
    int x = 0;
    while (x < COLS) {
      WritePoint(x, y, 1);
      x++;
    }
    RefreshAll(GRAIN,DELAY); //Draw frame buffer
    y++;
  }

  x = 0;   
  while (x < COLS) {
    Clear();
    y = 0;
    while (y < ROWS) {
      WritePoint(x, y, 1);
      y++;
    }

    RefreshAll(GRAIN,DELAY); //Draw frame buffer
    x++;
  }
  //CheckHallState(); 
}


int main(int argc, char **argv) {
  run(&setup, &loop);
  // unmmap(gpio);
  // close(gpio_fd);
}

