#include <stdio.h>
#include <beaglebone.h>
#include <string.h>
#include <inttypes.h>
#include <assert.h>
#include <sys/time.h>
#include <signal.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>

#define BUFLEN 1024
#define NPACK 10
#define PORT 6666

#define DEBUG 5

// CONSTANTS

#define ROWS 50
#define COLS 50
#define MAX_SLICES 60 
#define SNOW_FLAKE_COUNT 8

/* GRAIN: duration that each row/scan line is displayed 
 * a lower value provides a dimmer but more steady image */
#define ROW_DISPLAY_SLEEP 0 /* microseconds */
#define SLEEP_GRAIN 10  /* microseconds */
#define UPDATE_INTERVAL_USEC 150000

// MACROS

#define max(a,b) ((a < b) ? b : a)
#define min(a,b) ((a < b) ? a : b)
#define scale(x, in_min, in_max, out_min, out_max) ((x - (in_min)) * ((out_max) - (out_min)) / ((in_max) - (in_min)) + (out_min))

// GLOBALS

// each slice hase two yokes, to in effect this is double
unsigned g_sliceCount = 60;

struct sigaction g_sa;
struct itimerval g_timer;
struct timeval g_cycleStartTime;
struct timeval g_animateTime;
unsigned long g_T_usec = 250*1000; // 1/4 sec

typedef struct Slice { uint64_t rows[ROWS];} Slice;
Slice g_vol[MAX_SLICES];

// FORWARDS

void DrawCurrentSlice();
void CheckHallState();  
void AnimateVolume();
void udpRecv();

// TYPES

#define PIN_INDEX unsigned
#define BOOL unsigned

// PIN ASSIGNMENTS

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
GPIO loRightRowSelectGPIO[8];

#define LO_RIGHT_CLOCK_PIN P8_38
#define LO_RIGHT_DATA_PIN  P8_37
GPIO loRightClockGPIO;
GPIO loRightDataGPIO;

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
GPIO loLeftRowSelectGPIO[8];

#define LO_LEFT_CLOCK_PIN P8_28
#define LO_LEFT_DATA_PIN  P8_27
GPIO loLeftClockGPIO;
GPIO loLeftDataGPIO;

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
GPIO hiRightRowSelectGPIO[8];

#define HI_RIGHT_CLOCK_PIN P8_25
#define HI_RIGHT_DATA_PIN  P8_26
GPIO hiRightClockGPIO;
GPIO hiRightDataGPIO;

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
GPIO hiLeftRowSelectGPIO[8];

#define HI_LEFT_CLOCK_PIN P8_8
#define HI_LEFT_DATA_PIN  P8_7
GPIO hiLeftClockGPIO;
GPIO hiLeftDataGPIO;

#define LO_LATCH_PIN P9_42
#define HI_LATCH_PIN P9_27
GPIO loLatchGPIO;
GPIO hiLatchGPIO;

#define HALL_EFFECT_PIN P9_12
GPIO hallEffectGPIO;

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

inline void Sleep(unsigned long usec) {
  if (usec == 0) goto done;
  struct timeval sleepstart;
  gettimeofday(&sleepstart,NULL);
  while(TimeDiff(&sleepstart,NULL) <= usec) {
    //CheckHallState();
    usleep(SLEEP_GRAIN);
  }
done:
  (void)0;
  //CheckHallState();
}

inline void shift_out(GPIO* data_pin, GPIO* clock_pin, BOOL msbfirst, unsigned char byte) {
  unsigned lastDataBit = -1;
  int i;
  if (msbfirst) {
    for(i = 7; i >= 0; i--) {
      unsigned bit = byte & (1 << i);
      if (bit != lastDataBit)
        digitalWrite_raw(data_pin, bit);
      digitalLatch_raw(clock_pin);
      lastDataBit = bit;
    }
  } else {
    for(i = 0; i < 8; i++) {
      unsigned bit = byte & (1 << i);
      if (bit != lastDataBit)
        digitalWrite_raw(data_pin, bit);
      digitalLatch_raw(clock_pin);
      lastDataBit = bit;
    }
  }
}

inline void lShiftOut(unsigned char byte, BOOL topbot, BOOL msbfirst) {
  if (topbot == LOW) {
    shift_out(&loLeftDataGPIO,&loLeftClockGPIO, msbfirst, byte);
  } else {
    shift_out(&hiLeftDataGPIO,&hiLeftClockGPIO, msbfirst, byte);
  }
}

inline void rShiftOut(unsigned char byte, BOOL topbot, BOOL msbfirst) {
  if (topbot == LOW) {
    shift_out(&loRightDataGPIO,&loRightClockGPIO, msbfirst, byte);
  } else {
    shift_out(&hiRightDataGPIO,&hiRightClockGPIO, msbfirst, byte);
  }
}

inline void Latch(BOOL hilo) {
    if (hilo == LOW) {
      digitalLatch_raw(&loLatchGPIO);
    } else {
      digitalLatch_raw(&hiLatchGPIO);
    }
}

void PaintRow(uint64_t rowTmp, BOOL hilow, BOOL flip /*horizonal flip*/ ) {
    BOOL right_to_left = (hilow == LOW) ;
    if (flip) right_to_left = !right_to_left;;

    if (right_to_left) {
        // right to left for bottom rows
        BOOL msbfirst = 1;
        if (!flip) {

          if (rowTmp & 0xFFFFFFFF00000000) {
            rShiftOut(rowTmp >> 7*8, hilow, msbfirst);    // last cols
            rShiftOut(rowTmp >> 6*8, hilow, msbfirst);
            rShiftOut(rowTmp >> 5*8, hilow, msbfirst);
            rShiftOut(rowTmp >> 4*8, hilow, msbfirst);
          }

          if (rowTmp & 0xFFFFFFFF) {
            lShiftOut(rowTmp >> 3*8, hilow, msbfirst);    
            lShiftOut(rowTmp >> 2*8, hilow, msbfirst);
            lShiftOut(rowTmp >>   8, hilow, msbfirst);
            lShiftOut(rowTmp       , hilow, msbfirst);    // first cols
          }
        } else {
          if (rowTmp & 0xFFFFFFFF00000000) {
            lShiftOut(rowTmp >> 7*8, hilow, msbfirst);    // last cols
            lShiftOut(rowTmp >> 6*8, hilow, msbfirst);
            lShiftOut(rowTmp >> 5*8, hilow, msbfirst);
            lShiftOut(rowTmp >> 4*8, hilow, msbfirst);
          }

          if (rowTmp & 0xFFFFFFFF) {
            rShiftOut(rowTmp >> 3*8, hilow, msbfirst);    
            rShiftOut(rowTmp >> 2*8, hilow, msbfirst);
            rShiftOut(rowTmp >>   8, hilow, msbfirst);
            rShiftOut(rowTmp       , hilow, msbfirst);    // first cols
          }
        }
    } else {
        // left to right (backwards) for top (up side down) rows
        BOOL msbfirst = 0;
        rowTmp = rowTmp << 7;
        if (!flip)  {
          if (rowTmp & 0xFFFFFFFF) {
            lShiftOut(rowTmp       , hilow, msbfirst);    // first cols
            lShiftOut(rowTmp >>   8, hilow, msbfirst);
            lShiftOut(rowTmp >> 2*8, hilow, msbfirst);
            lShiftOut(rowTmp >> 3*8, hilow, msbfirst);    
          }

          if (rowTmp & 0xFFFFFFFF00000000) {
            rShiftOut(rowTmp >> 4*8, hilow, msbfirst);
            rShiftOut(rowTmp >> 5*8, hilow, msbfirst);
            rShiftOut(rowTmp >> 6*8, hilow, msbfirst);
            rShiftOut(rowTmp >> 7*8, hilow, msbfirst);    // last cols
          }
        } else {
          if (rowTmp & 0xFFFFFFFF) {
            rShiftOut(rowTmp       , hilow, msbfirst);    // first cols
            rShiftOut(rowTmp >>   8, hilow, msbfirst);
            rShiftOut(rowTmp >> 2*8, hilow, msbfirst);
            rShiftOut(rowTmp >> 3*8, hilow, msbfirst);    
          }

          if (rowTmp & 0xFFFFFFFF00000000) {
            lShiftOut(rowTmp >> 4*8, hilow, msbfirst);
            lShiftOut(rowTmp >> 5*8, hilow, msbfirst);
            lShiftOut(rowTmp >> 6*8, hilow, msbfirst);
            lShiftOut(rowTmp >> 7*8, hilow, msbfirst);    // last cols
          }
        }
    }
}

void RefreshAll(const Slice *slice, BOOL flip /* horizontal */) {
  struct timeval metric;
  gettimeofday(&metric,NULL);
  int row; // 1 based
  PIN_INDEX pin;

  static int s_count = 0;
  static unsigned long s_accumTime = 0;


  for (row = 1; row <= ROWS; row++) {
    uint64_t rowTmp = slice->rows[row - 1];
    if ( !rowTmp ) {
      CheckHallState();
      continue;
    }

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

    //assert(rowportHi || rowportLo); // one or the other need to be written

    if (rowportLo) {
      for(pin = 0; pin < 8; pin++) {
        digitalWrite_raw(&loRightRowSelectGPIO[pin], rowportLo & 1);
        digitalWrite_raw(&loLeftRowSelectGPIO[pin],  rowportLo & 1);
        rowportLo >>= 1;
      }
    } else if (rowportHi) {
      for(pin = 0; pin < 8; pin++) {
        digitalWrite_raw(&hiRightRowSelectGPIO[pin], rowportHi & 1);
        digitalWrite_raw(&hiLeftRowSelectGPIO[pin],  rowportHi & 1);
        rowportHi >>= 1;
      }
    }
    Latch(low ? LOW : HIGH);  // Light Row

    CheckHallState();

    lShiftOut(0, low ? LOW : HIGH, low ? 1 : 0);
    lShiftOut(0, low ? LOW : HIGH, low ? 1 : 0);
    lShiftOut(0, low ? LOW : HIGH, low ? 1 : 0);
    lShiftOut(0, low ? LOW : HIGH, low ? 1 : 0);
    rShiftOut(0, low ? LOW : HIGH, low ? 1 : 0);
    rShiftOut(0, low ? LOW : HIGH, low ? 1 : 0);
    rShiftOut(0, low ? LOW : HIGH, low ? 1 : 0);
    rShiftOut(0, low ? LOW : HIGH, low ? 1 : 0);

    Latch(low ? LOW : HIGH);   // Blank Row

    // Blank COLS
    if (low) {
      for(pin = 0; pin < 8; pin++) {
        digitalWrite_raw(&loRightRowSelectGPIO[pin], 0);
        digitalWrite_raw(&loLeftRowSelectGPIO[pin] , 0);
      }
    } else {
      for(pin = 0; pin < 8; pin++) {
        digitalWrite_raw(&hiRightRowSelectGPIO[pin], 0);
        digitalWrite_raw(&hiLeftRowSelectGPIO[pin] , 0);
      }
    }
  }
  s_accumTime += TimeDiff(&metric,NULL);
  s_count++;
  if (s_count == 1000) {
    unsigned long avePaintTime = s_accumTime/1000;
    printf("Average Draw Screen Time: %f ms\n", avePaintTime/1000.0);
    s_count = 0;
    s_accumTime = 0;
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
  printf("Peggy Setup\n");
  PIN_INDEX pin;

  /* Setup Pin Modes */
  pinMode_raw(pins[LO_LATCH_PIN ],OUTPUT, &loLatchGPIO);
  pinMode_raw(pins[HI_LATCH_PIN ],OUTPUT, &hiLatchGPIO);

  pinMode_raw(pins[LO_RIGHT_CLOCK_PIN ],OUTPUT, &loRightClockGPIO);
  pinMode_raw(pins[LO_RIGHT_DATA_PIN  ],OUTPUT, &loRightDataGPIO);

  pinMode_raw(pins[LO_LEFT_CLOCK_PIN ],OUTPUT, &loLeftClockGPIO);
  pinMode_raw(pins[LO_LEFT_DATA_PIN  ],OUTPUT, &loLeftDataGPIO);


  pinMode_raw(pins[HI_RIGHT_CLOCK_PIN ],OUTPUT, &hiRightClockGPIO);
  pinMode_raw(pins[HI_RIGHT_DATA_PIN  ],OUTPUT, &hiRightDataGPIO);

  pinMode_raw(pins[HI_LEFT_CLOCK_PIN ],OUTPUT, &hiLeftClockGPIO);
  pinMode_raw(pins[HI_LEFT_DATA_PIN  ],OUTPUT, &hiLeftDataGPIO);

  int pin_count = 8 * sizeof(PIN_INDEX);
  assert( sizeof(loRightRowSelectPins) == pin_count );
  assert( sizeof(hiRightRowSelectPins) == pin_count );
  assert( sizeof(loLeftRowSelectPins) == pin_count );
  assert( sizeof(hiLeftRowSelectPins) == pin_count );

  for(pin = 0; pin < sizeof(loRightRowSelectPins)/sizeof(PIN_INDEX); pin++) {
    pinMode_raw(pins[hiRightRowSelectPins[pin]],OUTPUT, &hiRightRowSelectGPIO[pin]);
    pinMode_raw(pins[hiLeftRowSelectPins[pin]] ,OUTPUT, &hiLeftRowSelectGPIO[pin]);
    pinMode_raw(pins[loRightRowSelectPins[pin]],OUTPUT, &loRightRowSelectGPIO[pin]);
    pinMode_raw(pins[loLeftRowSelectPins[pin]] ,OUTPUT, &loLeftRowSelectGPIO[pin]);
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
    
    //uint64_t pat = 0x1555555ll + (0x1555555ll << 32); //  101010 pattern
    uint64_t pat = 0;
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

void timer_handler (int signum)
{
  DrawCurrentSlice();
}

void UpdateTimer() {
  unsigned long paintInterval = (g_T_usec/2) / g_sliceCount;
  
  g_timer.it_value.tv_sec = 0;
  g_timer.it_value.tv_usec = paintInterval;
  g_timer.it_interval.tv_sec = 0;
  g_timer.it_interval.tv_usec = paintInterval;
  setitimer(ITIMER_REAL, &g_timer, NULL);

  // printf("Timer Interval: %d usec\n",paintInterval);
}

void pol2vol(
    unsigned theta,  // degrees
    unsigned radius, // 0-24
    unsigned height, // 0-49

    unsigned *ret_slice,
    unsigned *ret_x,
    unsigned *ret_y) {

  assert(theta >= 0);
  assert(radius >= 0);
  assert(height >= 0);

  assert(theta < 360);
  assert(radius < COLS/2);
  assert(height < ROWS);

  if (theta < 180) {
    *ret_slice = scale(theta,0,180,0,g_sliceCount);
    *ret_x = COLS/2 + radius;
  } else {
    *ret_slice = scale(theta-180,0,180,0,g_sliceCount);
    *ret_x = COLS/2 - radius - 1;
  }
  *ret_y = ROWS - 1 - height;
}

void SetupSpiral() {
  ClearVolume();
  unsigned theta;
  for(theta = 0; theta < 360; theta++) {

      unsigned radius = scale(theta,0,360,0,25);
      unsigned height = 0;
      printf("[%d deg] (%d,%d)\n",theta,radius,height);

      unsigned s,y,x;
      pol2vol(theta,radius,height,&s,&x,&y);
      printf("[%d] (%d,%d)\n",s,x,y);

      WritePoint(&g_vol[s],x, y, 1);
  }
}

typedef struct {
  unsigned radius;
  unsigned theta;
  unsigned height;
} FLAKE;

FLAKE g_snow[SNOW_FLAKE_COUNT];

void SetupSnow() {
   unsigned f;
   for(f = 0; f < sizeof(g_snow)/sizeof(FLAKE); f++) {
     g_snow[f].theta = rand() % 360;
     g_snow[f].radius = rand() % COLS/2;
     g_snow[f].height = rand() % ROWS;
     unsigned s,y,x;
     pol2vol(
         g_snow[f].theta,
         g_snow[f].radius,
         g_snow[f].height,
         &s,&x,&y);
     WritePoint(&g_vol[s],x, y, 1);
   }

}

void UpdateSnow() {
  ClearVolume();
  unsigned f;
  for(f = 0; f < sizeof(g_snow)/sizeof(FLAKE); f++) {
    if (g_snow[f].height == 0) {
      g_snow[f].height = ROWS-1;
      g_snow[f].theta = rand() % 360;
      g_snow[f].height = rand() % ROWS;
    } else {
      g_snow[f].height--;
    }

    unsigned s,y,x;
    pol2vol(
        g_snow[f].theta,
        g_snow[f].radius,
        g_snow[f].height,
        &s,&x,&y);
    WritePoint(&g_vol[s],x, y, 1);
  }
  /* Robs Test
  int r,s;
  for(s = 0; s < g_sliceCount; s++) {
    for(r = 0; r < ROWS; r++) {
      WritePoint(&g_vol[s],24, r, 1);
      WritePoint(&g_vol[s],25, r, 1);
    } 
  }
  */
}

void UpdatePeriod() {
  static unsigned s_stable = 0;
  static unsigned long s_runningAvePeriod = 250*1000;;
  struct timeval now;
  gettimeofday(&now,NULL);

  unsigned long newPeriod = TimeDiff(&g_cycleStartTime,&now);
  if (s_stable) {
    if (newPeriod < s_runningAvePeriod / 2) {
      printf("SPIRIOUS Hall (T=%d)\n",newPeriod);
      s_stable = 0;
      memcpy(&g_cycleStartTime,&now,sizeof(now));
      return;
    } else if (newPeriod > s_runningAvePeriod + s_runningAvePeriod /2 ) {
      printf("MISSED Hall (T=%d)\n",newPeriod);
      s_stable = 0;
      memcpy(&g_cycleStartTime,&now,sizeof(now));
      return;
    }
  }

  // remember now at start of next cycle
  memcpy(&g_cycleStartTime,&now,sizeof(now));

  unsigned long new_T = 
    (unsigned long) 
        (( 0.9 * (double) s_runningAvePeriod) 
        +  (0.1 * (double) newPeriod));
  //printf("Candidate Period %d usec\n", new_T);

  unsigned long slack = 20;
  if (g_T_usec  <  (new_T - slack) || g_T_usec > (new_T + slack) ) {
    s_stable /= 2;
    printf("New Period %d usec\n", new_T);
    printf("\tTimer Delta %d usec\n", (long)new_T - (long)g_T_usec);
    g_T_usec = new_T;
    UpdateTimer();
  } else {
    s_stable++;
  }
  s_runningAvePeriod = new_T;
}

void CheckHallState() {
    static int s_hall = HIGH; 
    
    int prev = s_hall;
    s_hall = digitalRead_raw(&hallEffectGPIO);
    if (prev != s_hall) {
      //if (DEBUG <= 5) 
      //printf("Hall State is %s\n", s_hall ? "HIGH" : "LOW");
      //printf("%s", s_hall ? "+" : "-");
      if (s_hall) {
        //printf("\n");
        UpdatePeriod();
        udpRecv();
      }
    }
}

void DrawCurrentSlice() { // Draw a slice based on the current slice
    unsigned long usec = TimeDiff(&g_cycleStartTime,NULL) % g_T_usec;
    if (usec < g_T_usec/2) {
      // draw first half of cylinder normally
      int slice = scale(usec,0,g_T_usec/2,0,g_sliceCount);
      RefreshAll(&g_vol[slice] // Draw Slice
          , 0 /* horizontal flip */);
      //printf("Draw Slice %d\n", slice);
    } else {
      // draw the slices again horizontally flipped
      int slice = scale(usec,g_T_usec/2,g_T_usec,0,g_sliceCount);
      RefreshAll(&g_vol[slice] // Draw Slice
          , 1 /* horizontal flip */);
      //printf("Draw Slice %d flipped\n", slice);
    }
    //CheckHallState();
}


void RowsAndCols() { 
  while(1) {
    int x,y;
    y = 0;   
    int paintTime = 0; // usec
    while (y < ROWS) {
      Clear(&g_vol[0]);
      int x = 0;
      while (x < COLS) {
        WritePoint(&g_vol[0],x, y, 1);
        x++;
      }
      RefreshAll(&g_vol[0]
        //, paintTime /* delay */
        , 1 /* horizontal flip */);
        y++;
    }

    x = 0;   
    while (x < COLS) {
      y = 0;
      Clear(&g_vol[0]);
      while (y < ROWS) {
        WritePoint(&g_vol[0],x, y, 1);
        y++;
      }
      RefreshAll(&g_vol[0]
        //, paintTime /* delay */
        , 1 /* horizontal flip */);
      x++;
    }
  }
}

void AllPixels() {
  int i;
  while(1) {
    for(i = 0; i < ROWS*COLS; i++) {
      WritePoint(&g_vol[0],i % COLS, i / ROWS, 1);
      RefreshAll(&g_vol[0]
        //, 100000 /* usec delay */
        , 0 /* horizontal flip */);
      Clear(&g_vol[0]);
      //WritePoint(&g_vol[0],i % COLS, i / ROWS, 0);
    }
  }
}

void AnimateVolume() {
  printf("AnimateVolume\n");
  UpdateSnow();
}

void diep(char *s)
{
  perror(s);
  exit(1);
}

struct sockaddr_in g_si_me;
int g_socket = -1;

void udpSetup() {
  if ((g_socket=socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP))==-1)
    diep("socket");

  memset((char *) &g_si_me, 0, sizeof(g_si_me));
  g_si_me.sin_family = AF_INET;
  g_si_me.sin_port = htons(PORT);
  g_si_me.sin_addr.s_addr = htonl(INADDR_ANY);
  if (bind(g_socket, (struct sockaddr *)&g_si_me, sizeof(g_si_me))==-1)
    diep("bind");
  int opts = fcntl(g_socket,F_GETFL,0);
  fcntl(g_socket,F_SETFL, opts | O_NONBLOCK);
}


void udpRecv() {
  static s_expectedSlice = 0;
  struct sockaddr_in si_other;
  int slen=sizeof(si_other);


  int byteCount = recvfrom(g_socket, g_vol, sizeof(Slice)*g_sliceCount, 0 , (struct sockaddr *)&si_other, &slen);

  if (byteCount == -1 ) {
    if (errno == 11) return;  // Resource Tmp Unavail
    diep("recvfrom()");
  }
  if (byteCount == 0) return;

  int want = sizeof(Slice)*g_sliceCount;
  if(byteCount != want ) {
    printf("Warning: Short slice buffer\n");
    return;
  }

  /*
  if (slice != s_expectedSlice) {
    printf("Warning: Slice (%d) Dropped\n",s_expectedSlice);
  }
  */
  //assert(sizeof(Slice) >=  byteCount-2);
  /*
  if(g_buf[byteCount-1] != 0) {
    printf("Warning: Slice buffer has no terminator\n");
  }
  */

  //memcpy((void*)g_vol,(void*)g_buf,byteCount-1);
      /* -1: one for slice index, one for terminator */ 
  /*
  s_expectedSlice = ++slice;
  if (s_expectedSlice == g_sliceCount) {
    s_expectedSlice = 0;
  }
  */
}

void udpCleanUp() {
  close(g_socket);
  g_socket = -1;
}

setupTimer() {
  /* Install timer_handler as the signal handler for SIGVTALRM. */
  memset (&g_sa, 0, sizeof (g_sa));
  g_sa.sa_handler = &timer_handler;
  sigaction (SIGALRM, &g_sa, NULL);
}

void setup() {
  assert( g_sliceCount <= MAX_SLICES);
  PeggySetup();

  pinMode_raw(pins[HALL_EFFECT_PIN],INPUT,&hallEffectGPIO);

  ClearVolume();

  //SetupSpiral();
  //SetupSnow();
  udpSetup();

  printf("Setup Complete\n");
  sleep(1);

  gettimeofday(&g_cycleStartTime,NULL);
  gettimeofday(&g_animateTime,NULL);

  setupTimer();
  UpdateTimer();
}


int loop() {
  printf("Main Loop Start\n");
  //DrawSlices();
  //RowsAndCols();
  //AllPixels();
  while(1) {
    /*
    if (TimeDiff(&g_animateTime,NULL) >= UPDATE_INTERVAL_USEC) {
      gettimeofday(&g_animateTime,NULL);
      AnimateVolume();
    }
    */
    udpRecv();
    CheckHallState();
    usleep(SLEEP_GRAIN);
    CheckHallState();
  }
}

int main(int argc, char **argv) {
  run(&setup, &loop);
  //unmmap(gpio);
  //close(gpio_fd);
  udpCleanUp();
}
