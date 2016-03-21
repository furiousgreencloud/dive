#include <beaglebone.h>

void setup() {
  pinMode(pins[P8_3], OUTPUT);
}

void loop() {
  digitalWrite(pins[P8_3], LOW);
  digitalWrite(pins[P8_3], HIGH);
}

int main(int argc, char **argv) {
  run(&setup, &loop);
}
