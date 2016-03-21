#include <stdio.h>
#include <beaglebone.h>
#include <string.h>
#include <inttypes.h>

#define DEBUG 4

#define PIN_INDEX unsigned

#define ROWS 25
#define COLS 50

#define GRAIN 1
#define DELAY (750/GRAIN)

uint64_t buffer[ROWS];

PIN_INDEX rightRowSelectPins[] = {
  P8_45,
  P8_46,
  P8_44,
  P8_43,
  P8_42, 
  P8_41,
  P8_40,
  P8_39
};

#define RIGHT_CLOCK_PIN P8_38
#define RIGHT_DATA_PIN  P8_37

PIN_INDEX leftRowSelectPins[] = {
  P8_36,
  P8_35,
  P8_34,
  P8_33,
  P8_32,
  P8_31,
  P8_30,
  P8_29
};

#define LEFT_CLOCK_PIN P8_28
#define LEFT_DATA_PIN  P8_27

#define LATCH_PIN P9_42

#define HALL_PIN P9_12

void shift_out(PIN data_pin, PIN clock_pin, unsigned char byte) {
  int i;
  for(i = 7; i >= 0; i--) {
    unsigned bit = byte & (1 << i);
    digitalWrite(data_pin, bit);
    digitalWrite(clock_pin,1);
    digitalWrite(clock_pin,0);
    /*
    if (DEBUG <= 4) {
      if (bit) printf("1"); else printf("0");
    }
    */
  }
}

void lShiftOut(unsigned char byte) {
  shift_out(pins[LEFT_DATA_PIN],pins[LEFT_CLOCK_PIN], byte);
}

void rShiftOut(unsigned char byte) {
  shift_out(pins[RIGHT_DATA_PIN],pins[RIGHT_CLOCK_PIN], byte);
}

void Latch() {
    if (DEBUG <= 2) printf("Latch\n");
    digitalWrite(pins[LATCH_PIN],1);
    digitalWrite(pins[LATCH_PIN],0);
}

void RefreshAll(unsigned cycles,unsigned long usdelay) {
  if (DEBUG <=2 ) printf("RefreshAll\n");
  int row;
  unsigned int rowport;
  PIN_INDEX pin;

  unsigned k = cycles;
  while(k) {
    k--;
    for (row = 1; row <= ROWS; row++) {
      if (row < 16) {
         rowport = row;
      } else if (row < 26) {
         rowport = (row - 15) << 4; // count up in the next mux
      } // TODO add more rows


      uint64_t rowTmp = buffer[row - 1];

      rShiftOut(rowTmp >> 7*8);    // last cols
      rShiftOut(rowTmp >> 6*8);
      rShiftOut(rowTmp >> 5*8);
      rShiftOut(rowTmp >> 4*8);
      lShiftOut(rowTmp >> 3*8);    
      lShiftOut(rowTmp >> 2*8);
      lShiftOut(rowTmp >>   8);
      lShiftOut(rowTmp       );    // first cols
      //if (DEBUG <= 4) printf("\n");

      for(pin = 0; pin < 8; pin++) {
        digitalWrite(pins[rightRowSelectPins[pin]], rowport & 1);
        digitalWrite(pins[leftRowSelectPins[pin]], rowport & 1);
        rowport >>= 1;
      }

      Latch();  // Light Row

      usleep(usdelay);
      /*
      int sleep = 50;
      while(sleep) {
        asm ( "NOP" );
        sleep--;
      }
      */
      if (DEBUG <= 3) {
        sleep(1);
      }

      lShiftOut(0);
      lShiftOut(0);
      lShiftOut(0);
      lShiftOut(0);
      rShiftOut(0);
      rShiftOut(0);
      rShiftOut(0);
      rShiftOut(0);
      Latch();   // Blank Row

      for(pin = 0; pin < 8; pin++) {
        digitalWrite(pins[rightRowSelectPins[pin]], 0);
        digitalWrite(pins[leftRowSelectPins[pin]], 0);
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
  Clear();

  pinMode(pins[LATCH_PIN ],OUTPUT);
  pinMode(pins[RIGHT_CLOCK_PIN ],OUTPUT);
  pinMode(pins[RIGHT_DATA_PIN  ],OUTPUT);

  //pinMode(pins[LEFT_LATCH_PIN ],OUTPUT);
  pinMode(pins[LEFT_CLOCK_PIN ],OUTPUT);
  pinMode(pins[LEFT_DATA_PIN  ],OUTPUT);

  digitalWrite(pins[LATCH_PIN ],0);
  digitalWrite(pins[RIGHT_CLOCK_PIN ],0);
  digitalWrite(pins[RIGHT_DATA_PIN  ],0);

  //digitalWrite(pins[LEFT_LATCH_PIN ],0);
  digitalWrite(pins[LEFT_CLOCK_PIN ],0);
  digitalWrite(pins[LEFT_DATA_PIN  ],0);

  PIN_INDEX pin;
  for(pin = 0; pin < sizeof(rightRowSelectPins)/sizeof(PIN_INDEX); pin++) {
    pinMode(pins[rightRowSelectPins[pin]],OUTPUT);
    pinMode(pins[leftRowSelectPins[pin]],OUTPUT);
    digitalWrite(pins[rightRowSelectPins[pin]], 0);
    digitalWrite(pins[leftRowSelectPins[pin]], 0);
  }

  lShiftOut(0xff);
  lShiftOut(0xff);
  lShiftOut(0xff);
  lShiftOut(0xff);
 
  rShiftOut(0xff);
  rShiftOut(0xff);
  rShiftOut(0xff);
  rShiftOut(0xff);
  Latch();    // Blank Row
}

void setup() {
  PeggySetup();
  printf("Setup Complete\n");
}

int loop() { 
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
}


int main(int argc, char **argv) {
  run(&setup, &loop);
  // unmmap(gpio);
  // close(gpio_fd);
}

