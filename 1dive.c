#include <stdio.h>
#include <beaglebone.h>
#include <string.h>

#define DEBUG 3

#define PIN_INDEX unsigned

#define ROWS 25
#define COLS 25

#define DELAY 750
#define GRAIN 1

unsigned long buffer[ROWS];

PIN_INDEX rowSelectPins[] = {
  P8_25,
  P8_27,
  P8_29,
  P8_31, 
  P8_33,
  P8_35,
  P8_37,
  P8_39
};

#define CLOCK_PIN P8_41
#define DATA_PIN  P8_43
#define LATCH_PIN P8_45

void ShiftOut(unsigned char byte) {
  int i;
  for(i = 7; i >= 0; i--) {
    unsigned bit = byte & (1 << i);
    digitalWrite(pins[DATA_PIN], bit);
    digitalWrite(pins[CLOCK_PIN],1);
    digitalWrite(pins[CLOCK_PIN],0);
    if (DEBUG <= 1) {
      if (bit) printf("1"); else printf("0");
    }
  }
  if (DEBUG <= 1) printf("\n");
}

void Latch() {
    if (DEBUG <= 2) printf("Latch\n");
    digitalWrite(pins[LATCH_PIN],1);
    digitalWrite(pins[LATCH_PIN],0);
}

void RefreshAll(unsigned cycles,unsigned long usdelay) {
  if (DEBUG <=3 ) printf("RefreshAll\n");
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


      unsigned long rowTmp = buffer[row - 1];

      ShiftOut(rowTmp >> 3*8);    // last cols
      ShiftOut(rowTmp >> 2*8);
      ShiftOut(rowTmp >>   8);
      ShiftOut(rowTmp);         // first cols

      for(pin = 0; pin < 8; pin++) {
        digitalWrite(pins[rowSelectPins[pin]], rowport & 1);
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
      if (DEBUG <= 1) {
        sleep(1);
      }

      ShiftOut(0);
      ShiftOut(0);
      ShiftOut(0);
      ShiftOut(0);
      Latch();   // Blank Row

      for(pin = 0; pin < 8; pin++) {
        digitalWrite(pins[rowSelectPins[pin]], 0);
      }
    }
  }
}

void Clear() {
  memset(buffer, 0, sizeof(buffer));
}

void WritePoint(unsigned xPos, unsigned  yPos, unsigned value) {
    if (value) {
        buffer[yPos] |=  0x1 << xPos;
    } else {
        buffer[yPos] &= ~(0x1 << xPos);
    }
}

void PeggySetup() {
  Clear();

  pinMode(pins[LATCH_PIN ],OUTPUT);
  pinMode(pins[CLOCK_PIN ],OUTPUT);
  pinMode(pins[DATA_PIN  ],OUTPUT);

  digitalWrite(pins[LATCH_PIN ],0);
  digitalWrite(pins[CLOCK_PIN ],0);
  digitalWrite(pins[DATA_PIN  ],0);

  PIN_INDEX pin;
  for(pin = 0; pin < sizeof(rowSelectPins)/sizeof(PIN_INDEX); pin++) {
    pinMode(pins[rowSelectPins[pin]],OUTPUT);
    digitalWrite(pins[rowSelectPins[pin]], 0);
  }

  ShiftOut(0);
  ShiftOut(0);
  ShiftOut(0);
  ShiftOut(0);
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
  while (y < COLS) {
    Clear();
    int x = 0;
    while (x < ROWS) {
      WritePoint(x, y,1);
      x++;
    }
    RefreshAll(GRAIN,DELAY); //Draw frame buffer
    y++;
  }


  y = 0;   
  while (y < COLS) {
    Clear();
    x = 0;
    while (x < ROWS) {
      WritePoint(y, x, 1);
      x++;
    }

    RefreshAll(GRAIN,DELAY); //Draw frame buffer
    y++;
  }
}


int main(int argc, char **argv) {
  run(&setup, &loop);
  // unmmap(gpio);
  // close(gpio_fd);
}

