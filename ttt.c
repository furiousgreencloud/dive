int rows_and_cols() { 
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
