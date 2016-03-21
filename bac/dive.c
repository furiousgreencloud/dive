#include <stdio.h>
#include <beaglebone.h>
#include <string.h>
#include <inttypes.h>
#include <assert.h>
#include <sys/time.h>

#define DEBUG 4

#define PIN_INDEX unsigned

#define ROWS 50
#define COLS 50
#define SLICES 25

#define max(a,b) ((a < b) ? b : a)
#define min(a,b) ((a < b) ? a : b)
#define scale(x, in_min, in_max, out_min, out_max) ((x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min)

/* GRAIN: duration that each row/scan line is displayed 
 * a lower value provides a dimmer but more steady image */
#define GRAIN 100  /* millseconds (50-100) */

/* (milliseconds) durtion that the refresh shows the image */
#define DELAY 2000


typedef struct Slice { uint64_t rows[ROWS];} Slice;
Slice g_vol[SLICES];

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
      // usleep(5000);
      digitalWrite(clock_pin,0);
      // usleep(5000);
    }
  } else {
    for(i = 0; i < 8; i++) {
      unsigned bit = byte & (1 << i);
      digitalWrite(data_pin, bit);
      digitalWrite(clock_pin,1);
      // usleep(5000);
      digitalWrite(clock_pin,0);
      // usleep(5000);
    }
  }
}

void lShiftOut(unsigned char byte, BOOL topbot, BOOL msbfirst) {
  if (topbot == LOW) {
    shift_out(pins[LO_LEFT_DATA_PIN],pins[LO_LEFT_CLOCK_PIN], msbfirst, byte);
  } else {
    shift_out(pins[HI_LEFT_DATA_PIN],pins[HI_LEFT_CLOCK_PIN], msbfirst, byte);
  }
}

void rShiftOut(unsigned char byte, BOOL topbot, BOOL msbfirst) {
  if (topbot == LOW) {
    shift_out(pins[LO_RIGHT_DATA_PIN],pins[LO_RIGHT_CLOCK_PIN], msbfirst, byte);
  } else {
    shift_out(pins[HI_RIGHT_DATA_PIN],pins[HI_RIGHT_CLOCK_PIN], msbfirst, byte);
  }
}

void Latch(BOOL hilo) {
    if (DEBUG <= 2) printf("Latch\n");
    if (hilo == LOW) {
      digitalWrite(pins[LO_LATCH_PIN],1);
      //usleep(10000);
      digitalWrite(pins[LO_LATCH_PIN],0);
    } else {
      digitalWrite(pins[HI_LATCH_PIN],1);
      //usleep(10000);
      digitalWrite(pins[HI_LATCH_PIN],0);
    }
    
}


void PaintRow(uint64_t rowTmp, BOOL hilow, BOOL flip /*horizonal flip*/ ) {
    
    BOOL right_to_left = (hilow == LOW) ;
    if (flip) right_to_left = !right_to_left;;

    if (right_to_left) {
        // right to left for bottom rows
        BOOL msbfirst = 1;
        if (!flip) {
          rShiftOut(rowTmp >> 7*8, hilow, msbfirst);    // last cols
          rShiftOut(rowTmp >> 6*8, hilow, msbfirst);
          rShiftOut(rowTmp >> 5*8, hilow, msbfirst);
          rShiftOut(rowTmp >> 4*8, hilow, msbfirst);

          lShiftOut(rowTmp >> 3*8, hilow, msbfirst);    
          lShiftOut(rowTmp >> 2*8, hilow, msbfirst);
          lShiftOut(rowTmp >>   8, hilow, msbfirst);
          lShiftOut(rowTmp       , hilow, msbfirst);    // first cols
        } else {
          lShiftOut(rowTmp >> 7*8, hilow, msbfirst);    // last cols
          lShiftOut(rowTmp >> 6*8, hilow, msbfirst);
          lShiftOut(rowTmp >> 5*8, hilow, msbfirst);
          lShiftOut(rowTmp >> 4*8, hilow, msbfirst);

          rShiftOut(rowTmp >> 3*8, hilow, msbfirst);    
          rShiftOut(rowTmp >> 2*8, hilow, msbfirst);
          rShiftOut(rowTmp >>   8, hilow, msbfirst);
          rShiftOut(rowTmp       , hilow, msbfirst);    // first cols
        }
    } else {
        // left to right (backwards) for top (up side down) rows
        BOOL msbfirst = 0;
        rowTmp = rowTmp << 7;
        if (!flip)  {
          lShiftOut(rowTmp       , hilow, msbfirst);    // first cols
          lShiftOut(rowTmp >>   8, hilow, msbfirst);
          lShiftOut(rowTmp >> 2*8, hilow, msbfirst);
          lShiftOut(rowTmp >> 3*8, hilow, msbfirst);    

          rShiftOut(rowTmp >> 4*8, hilow, msbfirst);
          rShiftOut(rowTmp >> 5*8, hilow, msbfirst);
          rShiftOut(rowTmp >> 6*8, hilow, msbfirst);
          rShiftOut(rowTmp >> 7*8, hilow, msbfirst);    // last cols
        } else {
          rShiftOut(rowTmp       , hilow, msbfirst);    // first cols
          rShiftOut(rowTmp >>   8, hilow, msbfirst);
          rShiftOut(rowTmp >> 2*8, hilow, msbfirst);
          rShiftOut(rowTmp >> 3*8, hilow, msbfirst);    

          lShiftOut(rowTmp >> 4*8, hilow, msbfirst);
          lShiftOut(rowTmp >> 5*8, hilow, msbfirst);
          lShiftOut(rowTmp >> 6*8, hilow, msbfirst);
          lShiftOut(rowTmp >> 7*8, hilow, msbfirst);    // last cols
        }
    }
}

void RefreshAll(const Slice *slice, unsigned duration, BOOL flip /* horizontal */) {
  if (DEBUG <=2 ) printf("RefreshAll\n");
  int row; // 1 based
  PIN_INDEX pin;


  unsigned k = max(1,duration/GRAIN);
  while(k) {
    k--;
    for (row = 1; row <= ROWS; row++) {
      uint64_t rowTmp = slice->rows[row - 1];

      /*
      if (!rowTmp) {
        CheckHallState();
        usleep(GRAIN);
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
        PaintRow(rowTmp,LOW,flip);
      } else {
        PaintRow(rowTmp,HIGH,flip);
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
      usleep(GRAIN);

      if (DEBUG <= 3) {
        usleep(5000);
      }

      lShiftOut(0, low ? LOW : HIGH, low ? 1 : 0);
      lShiftOut(0, low ? LOW : HIGH, low ? 1 : 0);
      lShiftOut(0, low ? LOW : HIGH, low ? 1 : 0);
      lShiftOut(0, low ? LOW : HIGH, low ? 1 : 0);
      rShiftOut(0, low ? LOW : HIGH, low ? 1 : 0);
      rShiftOut(0, low ? LOW : HIGH, low ? 1 : 0);
      rShiftOut(0, low ? LOW : HIGH, low ? 1 : 0);
      rShiftOut(0, low ? LOW : HIGH, low ? 1 : 0);

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

void Clear(Slice *slice) {
  //if (DEBUG <=5) printf("Blanking %d Bytes\n",sizeof(buffer));
  memset(slice, 0, sizeof(Slice));
}

void ClearVolume() {
  //if (DEBUG <=5) printf("Blanking %d Bytes\n",sizeof(buffer));
  memset(g_vol, 0, sizeof(g_vol));
}

void WritePoint(Slice *slice, unsigned xPos, unsigned  yPos, unsigned value) {
    if (xPos >= 25) {
      xPos += (32-25);
    }
    if (value) {
        slice->rows[yPos] |=  1LLU << xPos;
    } else {
        slice->rows[yPos] &= ~(1LLU << xPos);
    }
}

void PeggySetup() {
  PIN_INDEX pin;

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

    rShiftOut(pat >> 7*8, topbot, topbot ? 1 : 0);    // last cols
    rShiftOut(pat >> 6*8, topbot, topbot ? 1 : 0);
    rShiftOut(pat >> 5*8, topbot, topbot ? 1 : 0);
    rShiftOut(pat >> 4*8, topbot, topbot ? 1 : 0);

    lShiftOut(pat >> 3*8, topbot, topbot ? 1 : 0);    
    lShiftOut(pat >> 2*8, topbot, topbot ? 1 : 0);
    lShiftOut(pat >>   8, topbot, topbot ? 1 : 0);
    lShiftOut(pat       , topbot, topbot ? 1 : 0);    // first cols

    Latch(topbot);
  }

}

struct timeval g_cycleStartTime;

void setup() {
  int s;
  PeggySetup();

  pinMode(pins[HALL_EFFECT_PIN],INPUT);

  ClearVolume();

  for(s = 0; s < SLICES; s++) {
    int x1 = scale(s,0,SLICES,(COLS-1),3*(COLS-1)/4);
    int x2 = scale(s,0,SLICES,(COLS-1)/4,(COLS-1)/2);
    printf("%d %d\n", x1, x2);
    int y = 0;
    while (y < ROWS) {
      WritePoint(&g_vol[s],x1, y, 1);
      WritePoint(&g_vol[s],x2, y, 1);
      y++;
    }
  }

  gettimeofday(&g_cycleStartTime,NULL);

  printf("Setup Complete\n");
  sleep(5);
}

unsigned long g_T = 10*1000*1000;

unsigned long TimeDiff(struct timeval* start, struct timeval* end) { 
  // NULL 'end' implies end is now
  // 'end' must be after 'start'
  struct timeval now;
  if (end == NULL) {
    gettimeofday(&now,NULL);
    end = & now;
  }
  time_t delay_sec = end->tv_sec - start->tv_sec;
  suseconds_t delay_uSec = end->tv_usec - start->tv_usec;
  return delay_sec*1000000 + delay_uSec;
}

void UpdatePeriod() {
  struct timeval now;
  gettimeofday(&now,NULL);

  unsigned long delta_usec = TimeDiff(&g_cycleStartTime,&now);
  memcpy(&g_cycleStartTime,&now,sizeof(now));

  g_T = (unsigned long) (( 0.5 * (double)g_T) +  (0.5 * (double) delta_usec));
  printf("Period %d usec\n", g_T);
}

void CheckHallState() {
    static int s_hall = -1; 
    
    int prev = s_hall;
    s_hall = digitalRead(pins[HALL_EFFECT_PIN]);
    if (prev != s_hall) {
      // printf("Hall State is %s\n", s_hall ? "HIGH" : "LOW");
      if (s_hall) UpdatePeriod();
    }
}

void dive() { 
  unsigned long usec = TimeDiff(&g_cycleStartTime,NULL);

  usec = usec % g_T;
  if (usec < g_T/2) {
    // draw first half of cylinder normally
    int slice = scale(usec,0,g_T/2,0,SLICES);
    RefreshAll(&g_vol[slice] // Draw Slice
        , 0 /* delay */
        , 0 /* horizontal flip */);
    printf("Draw Slice %d\n", slice);
  } else {
    // draw the slices again horizontally flipped
    int slice = scale(usec,g_T/2,g_T,0,SLICES);
    slice = min(slice,SLICES-1);
    RefreshAll(&g_vol[slice] // Draw Slice
        , 0 /* delay */
        , 1 /* horizontal flip */);
    printf("Draw Slice %d flipped\n", slice);
  }
  //CheckHallState(); 
}


void rows_and_cols() { 
  if (DEBUG <=2) printf("Loop Start\n");
  int x,y;

  y = 0;   
  while (y < ROWS) {
    Clear(&g_vol[0]);
    int x = 0;
    while (x < COLS) {
      WritePoint(&g_vol[0],x, y, 1);
      x++;
    }
    RefreshAll(&g_vol[0]
      , 0 /* delay */
      , 0 /* horizontal flip */);
    y++;
  }

  x = 0;   
  while (x < COLS) {
    Clear(&g_vol[0]);
    y = 0;
    while (y < ROWS) {
      WritePoint(&g_vol[0],x, y, 1);
      y++;
    }

    RefreshAll(&g_vol[0]
      , 0 /* delay */
      , 0 /* horizontal flip */);
    x++;
  }
  //CheckHallState(); 
}

int loop() {
	rows_and_cols();
  //dive();
}

int main(int argc, char **argv) {
  run(&setup, &loop);
  // unmmap(gpio);
  // close(gpio_fd);
}


