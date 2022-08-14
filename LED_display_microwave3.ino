//teensy LC with LED display from microwave
//3 modes controlled by SPDT switch in main loop
//1) clock, 2) random segments, and 3) Frogger micro-game
#include <NSegmentDisplay.h>
#include <RTClib.h>
RTC_PCF8523 rtc;
//common anode display with 9 segments
const int NUM_SEG_PINS = 9;
const int NUM_DIGIT_PINS = 5;
const int segments[] = {8, 9, 10, 11, 13, 15, 14, 12, 20};
const int digits[] = {17, 5, 16, 21, 23};
const int btnLeft = 26;
const int btnRight = 0;
const int btnDown = 25;
const int btnUp = 24;
const int buttons[] = {btnLeft, btnRight, btnDown, btnUp};
const int sw1 = 1;
const int sw2 = 2;
bool btnLeftPressed = true;
bool btnRightPressed = true;
bool btnDownPressed = true;
bool btnUpPressed = true;
bool sw1Closed = true;
bool sw2Closed = true;
//main state control
byte state = 0;
//clock tick data
const int tickTime = 1000;
double lastTick = 0;
const int ticks[14][2] = {
  {3, 8}, {2, 8}, {4, 8}, {1, 8}, {0, 8}, {4, 0}, {4, 5}, {4, 4}, {4, 3}, {0, 7}, {1, 7}, {4, 7}, {2, 7}, {3, 7}
};
//win message and game timer vars
const int waitTime = 250;
double lastTime = 0;
double winMessageStart = 0;
int timer = 0;
int timerStart = 0;

NSegmentDisplay disp(true, NUM_SEG_PINS, segments, NUM_DIGIT_PINS, digits);

void setup() {
  Serial.begin(9600);
  for (int i = 0; i < 4; i++) {
    pinMode(buttons[i], INPUT_PULLUP);
  }
  pinMode(sw1, INPUT_PULLUP);
  pinMode(sw2, INPUT_PULLUP);
  pinMode(3, INPUT);
  //**********RTC code begin***********
  if (! rtc.begin()) {
    Serial.println("Couldn't find RTC");
    Serial.flush();
    abort();
  }
  if (! rtc.initialized() || rtc.lostPower()) {
    Serial.println("RTC is NOT initialized, let's set the time!");
    // When time needs to be set on a new device, or after a power loss, the
    // following line sets the RTC to the date & time this sketch was compiled
    //    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    // This line sets the RTC with an explicit date & time, for example to set
    // January 21, 2014 at 3am you would call:
    // rtc.adjust(DateTime(2014, 1, 21, 3, 0, 0));
    //
    // Note: allow 2 seconds after inserting battery or applying external power
    // without battery before calling adjust(). This gives the PCF8523's
    // crystal oscillator time to stabilize. If you call adjust() very quickly
    // after the RTC is powered, lostPower() may still return true.
  }

  // When time needs to be re-set on a previously configured device, the
  // following line sets the RTC to the date & time this sketch was compiled
  //         rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  // This line sets the RTC with an explicit date & time, for example to set
  // January 21, 2014 at 3am you would call:
  // rtc.adjust(DateTime(2014, 1, 21, 3, 0, 0));

  // When the RTC was stopped and stays connected to the battery, it has
  // to be restarted by clearing the STOP bit. Let's do this to ensure
  // the RTC is running.
  rtc.start();

  // The PCF8523 can be calibrated for:
  //        - Aging adjustment
  //        - Temperature compensation
  //        - Accuracy tuning
  // The offset mode to use, once every two hours or once every minute.
  // The offset Offset value from -64 to +63. See the Application Note for calculation of offset values.
  // https://www.nxp.com/docs/en/application-note/AN11247.pdf
  // The deviation in parts per million can be calculated over a period of observation. Both the drift (which can be negative)
  // and the observation period must be in seconds. For accuracy the variation should be observed over about 1 week.
  // Note: any previous calibration should cancelled prior to any new observation period.
  // Example - RTC gaining 43 seconds in 1 week
  float drift = -30; // seconds plus or minus over oservation period - set to 0 to cancel previous calibration.
  float period_sec = (7 * 86400);  // total obsevation period in seconds (86400 = seconds in 1 day:  7 days = (7 * 86400) seconds )
  float deviation_ppm = (drift / period_sec * 1000000); //  deviation in parts per million (μs)
  float drift_unit = 4.34; // use with offset mode PCF8523_TwoHours
  // float drift_unit = 4.069; //For corrections every min the drift_unit is 4.069 ppm (use with offset mode PCF8523_OneMinute)
  int offset = round(deviation_ppm / drift_unit);
  // rtc.calibrate(PCF8523_TwoHours, offset); // Un-comment to perform calibration once drift (seconds) and observation period (seconds) are correct
  // rtc.calibrate(PCF8523_TwoHours, 0); // Un-comment to cancel previous calibration

  //Serial.print("Offset is "); Serial.println(offset); // Print to control offset
  //*************RTC code end************
  timerStart = millis() / 1000;
}

void loop() {
  switchCheck();
  if (state > 0) {
    resetGame();
  }
  switch (state) {
    case 0:
      game();
      break;
    case 1:
      displayTime();
      break;
    case 2:
      randomSegments();
      break;
  }
}

void switchCheck() {
  sw1Closed = digitalRead(sw1);
  sw2Closed = digitalRead(sw2);
  if (!sw1Closed) {
    state = 0;
  } else if (!sw2Closed) {
    state = 1;
  } else if (sw1Closed && sw2Closed) {
    state = 2;
  }
}

//**********clock section************//
int minuteOffset = 0; //offset to add or subtract minutes w button
int hourOffset = 0; //used to adjust hours for different timezones
DateTime now;
void displayTime() {
  now = rtc.now();
  checkClockButtons();
  //convert to 12 hr
  int hr = now.hour() + hourOffset;
  if (hr > 12) {
    hr -= 12;
  } else if (hr == 0) {
    hr = 12;
  } 
  disp.multiDigitNumber(2, hr);
  makeTicks();
  int minut = (now.minute() + minuteOffset);
  if (minut < 10) disp.number(1, 0);
  disp.multiDigitNumber(minut);
}

int counter = 0;
void makeTicks() {
  //display colon
  disp.segment(4, 1, 3);
  disp.segment(4, 2, 3);
  //display ticks around the edge
  disp.segment(ticks[counter][0], ticks[counter][1], 3);
  if (millis() > lastTick + tickTime) {
    lastTick = millis();
    counter++;
    if (counter > 13) counter = 0;
  }
}

//change debounce for faster button response
int clockDebounce = 450;
double clockLastPress = 0;
void checkClockButtons() {
  for (int i = 0; i < 4; i++) {
    if (!digitalRead(buttons[i])) {
      if (millis() > clockLastPress + clockDebounce) {
        clockLastPress = millis();
        clockAction(i);
      }
    }
  }
}

void clockAction(int whichButton) {
  switch (whichButton) {
    case 0:
      subtractMin();
      break;
    case 1:
      addMin();
      break;
    case 2:
      subtractHour();
      break;
    case 3:
      addHour();
      break;
  }
}


void subtractMin() {
  if (now.minute() + minuteOffset > 0) {
    minuteOffset--;
  }
}

void addMin() {
  if (now.minute() + minuteOffset < 60) {
    minuteOffset++;
  }
}

void subtractHour() {
  if (now.hour() + hourOffset > 0) {
    hourOffset--;
  }
}

void addHour() {
  if (now.hour() + hourOffset < 24) {
    hourOffset++;
  }
}
//**********random segments section*******//
int choices[5][9] = {
  {0, 0, 0, 0, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0, 0, 0, 0}
};

void randomSegments() {
  for (int i = 0; i < 5; i++) {
    for (int j = 0; j < 9; j++) {
      if (choices[i][j]) disp.segment(i, j);
    }
    delay(3);
    disp.off(0);
  }
  if (millis() > lastTick + tickTime) {
    pickRandom();
    lastTick = millis();
  }
}

void pickRandom() {
  for (int i = 0; i < 5; i++) {
    for (int j = 0; j < 9; j++) {
      choices[i][j] = random(9) < 5 ? 0 : 1;
    }
  }
}
//**********game section************//

class Sprite {
  private:
    bool spriteOn = false; //for target sprite to blink
    double lastOn = 0;      //**
    int waitForNext = 500;  //**
    //middle section
    int grid[2][10][2] = {
      {
        {3, 5}, {3, 1}, {2, 5}, {2, 1}, {4, 1}, {1, 5}, {1, 1}, {0, 5}, {0, 1}, {4, 5}
      },
      {
        {3, 4}, {3, 2}, {2, 4}, {2, 2}, {4, 2}, {1, 4}, {1, 2}, {0, 4}, {0, 2}, {4, 4}
      }
    };

    int bottom[6][2] = {
      {3, 7}, {2, 7}, {4, 7}, {1, 7}, {0, 7}, {4, 3}
    };
    int top[6][2] = {
      {3, 8}, {2, 8}, {4, 8}, {1, 8}, {0, 8}, {4, 0}
    };

  public:
    int level = 0;  //is player on bottom, top or middle sections
    int ypos = 1; //vertical pos once in grid
    int xpos; //horiz pos of player
    Sprite(int level) {
      this->level = level;
      init();
    }

    void init() {
      this->xpos = random(6);
    }

    void displaySprite() {
      if (this->level == 0) {
        disp.segment(bottom[this->xpos][0], bottom[this->xpos][1], 3);
      } else if (this->level == 1) {
        disp.segment(grid[this->ypos][this->xpos][0], grid[this->ypos][this->xpos][1], 3);
      } else {
        disp.segment(top[this->xpos][0], top[this->xpos][1], 3);
      }
    }

    void blinkSprite() {
      if (millis() > lastOn + waitForNext) {
        lastOn = millis();
        spriteOn = !spriteOn;
      }
      if (spriteOn) {
        disp.segment(top[this->xpos][0], top[this->xpos][1], 3);
      }
    }

    void stepOnGrid() {
      switch (this->xpos) {
        case 0:
          random(2) > 0 ? this->xpos = 0 : this->xpos = 1;
          break;
        case 1:
          random(2) > 0 ? this->xpos = 2 : this->xpos = 3;
          break;
        case 2:
          this->xpos = 4;
          break;
        case 3:
          random(2) > 0 ? this->xpos = 5 : this->xpos = 6;
          break;
        case 4:
          random(2) > 0 ? this->xpos = 7 : this->xpos = 8;
          break;
        case 5:
          this->xpos = 9;
          break;
      }
    }

    void stepOffGrid() {
      switch (this->xpos) {
        case 0:
          this->xpos = 0;
        case 1:
          this->xpos = 0;
          break;
        case 2:
          this->xpos = 1;
        case 3:
          this->xpos = 1;
          break;
        case 4:
          this->xpos = 2;
          break;
        case 5:
          this->xpos = 3;
        case 6:
          this->xpos = 3;
          break;
        case 7:
          this->xpos = 4;
        case 8:
          this->xpos = 4;
          break;
        case 9:
          this->xpos = 5;
          break;
      }
    }

    void goBack() {
      stepOffGrid();
      this->level = 0;
      this->ypos = 1;
    }
};

class Car {
  private:
    int carXpos;
    int carYpos;
    double lastCarStep;
    int carChange;
    boolean carShowing = true;
    int carHideDelay;
    double startCarHide = 0;
    int carDelay = 400;
    int carGrid[3][6][2] = {
      {
        {3, 0}, {2, 0}, {4, 1}, {1, 0}, {0, 0}, {4, 5}
      },
      {
        {3, 6}, {2, 6}, {4, 2}, {1, 6}, {0, 6}, {4, 4}
      },
      {
        {3, 3}, {2, 3}, {4, 2}, {1, 3}, {0, 3}, {4, 4}
      }
    };

  public:
    Car(int carYpos) {
      this->carYpos = carYpos;
      init();
    }

    void init() {
      randomSeed(analogRead(3));
      carXpos = random(5);
      lastCarStep = millis();
      carChange = random(2) > 0 ? -1 : 1;
    }

    void displayCar() {
      if (carShowing) {
        disp.segment(carGrid[carYpos][carXpos][0], carGrid[carYpos][carXpos][1], 3);
      }
    }

    void moveCar() {
      if (carShowing) {
        if (millis() > lastCarStep + carDelay) {
          carXpos += carChange;
          if (carXpos > 5 && carChange > 0) {
            carXpos = 0;
            startCarHide = millis();
            carHideDelay = random(1000, 4000);
            carShowing = false;
          }
          if (carXpos < 0 && carChange < 0) {
            carXpos = 5;
            startCarHide = millis();
            carHideDelay = random(1000, 4000);
            carShowing = false;
          }
          lastCarStep = millis();
        }
      } else {
        if (millis() > startCarHide + carHideDelay) {
          carShowing = true;
          lastCarStep = millis();
        }
      }
    }

    void hit(Sprite &player) {
      if (player.level == 1 && this->carShowing) {
        if (player.ypos == this->carYpos || (this->carYpos == 2 && player.ypos == 1)) {
          if ((player.xpos == 0 || player.xpos == 1) && this->carXpos == 0 ||
              (player.xpos == 2 || player.xpos == 3) && this->carXpos == 1 ||
              player.xpos == 4 && this->carXpos == 2 ||
              (player.xpos == 5 || player.xpos == 6) && this->carXpos == 3 ||
              (player.xpos == 7 || player.xpos == 8) && this->carXpos == 4 ||
              player.xpos == 9 && this->carXpos == 5) {
            player.goBack();
            blinken();
          }
        }
      }
    }

    void resetCar() {
      this->carXpos = random(6);
      this->carChange = random(2) > 0 ? -1 : 1;
    }
};

Car car1(0);
Car car2(1);
Car car3(2);
Sprite player(0);
Sprite target(2);

int score = 0;
void game() {
  //score thresholds
  int winScore = 30;
  int lvlTwoScore = 9;
  int lvlThreeScore = 19;
  if (score < winScore) {
    checkButtons();
    player.displaySprite();
    car1.displayCar();
    car1.moveCar();
    car1.hit(player);
    if (score > lvlTwoScore) {
      car2.displayCar();
      car2.moveCar();
      car2.hit(player);
    }
    if (score > lvlThreeScore) {
      car3.displayCar();
      car3.moveCar();
      car3.hit(player);
    }
    target.blinkSprite();
    success();
  } else {
    char buffer[15];
    int finalTime = setFinishTime();
    sprintf(buffer, "%d seconds", finalTime);
    while (millis() < winMessageStart + 12000) {
      scrollingMessage(buffer);
    }
    car1.resetCar();
    car2.resetCar();
    car3.resetCar();
    resetGame();
  }
}

//change debounce for faster button response
int debounce = 450;
double lastPress = 0;
void checkButtons() {
  for (int i = 0; i < 4; i++) {
    if (!digitalRead(buttons[i])) {
      if (millis() > lastPress + debounce) {
        lastPress = millis();
        btnAction(i);
      }
    }
  }
}

void btnAction(int btn) {
  //buttons[] = {btnLeft, btnRight, btnDown, btnUp};
  switch (btn) {
    case 0:                                          //move left
      if (player.xpos > 0 && player.level == 1) {
        player.xpos--;
      }
      break;
    case 1:                                           //move right
      if (player.xpos < 9 && player.level == 1) {
        player.xpos++;
      }
      break;
    case 2:                                         //move down
      if (player.level == 2) {
        player.stepOnGrid();
        player.level = 1;
      } else if (player.level == 1) {
        if (player.ypos == 0) {
          player.ypos = 1;
        } else {
          player.level = 0;
          player.stepOffGrid();
        }
      }
      break;
    case 3:                                       //move up
      if (player.level == 0) {
        player.stepOnGrid();
        player.level = 1;
      } else if (player.level == 1) {
        if (player.ypos == 1) {
          player.ypos = 0;
        } else {
          player.level = 2;
          player.stepOffGrid();
        }
      }
      break;
  }
}

// if reach the target sprite
void success() {
  if (target.xpos == player.xpos && player.level == 2) {
    loopAll();
    resetStage();
    car1.resetCar();
    car2.resetCar();
    score++;
    if (score == 30) winMessageStart = millis();
  }
}

//animation for getting hit
void blinken() {
  for (int i = 0; i < 2; i++) {
    disp.off(0);
    for (int k = 0; k < 5; k++) {
      for (int j = 0; j < 14; j++) {
        disp.segment(ticks[j][0], ticks[j][1], 3);
      }
    }
    disp.off(0);
    for (int k = 0; k < 12; k++) {
      disp.multiDigitNumber(8888);
    }
  }
}

//animation for scoring
void loopAll() {
  for (int i = 0; i < 14; i++) {
    disp.segment(ticks[i][0], ticks[i][1], 50);
  }
  disp.crazyEights(100);
}

// following for displaying win message
const uint8_t alpha[37][7] {
  {0, 0, 1, 1, 1, 0, 1}, //a
  {0, 0, 1, 1, 1, 1, 1}, //b
  {0, 0, 0, 1, 1, 0, 1}, //c
  {0, 1, 1, 1, 1, 0, 1}, //d
  {1, 0, 0, 1, 1, 1, 1}, //e
  {1, 0, 0, 0, 1, 1, 1}, //f
  {1, 1, 1, 1, 0, 1, 1}, //g
  {0, 0, 1, 0, 1, 1, 1}, //h
  {0, 0, 1, 0, 0, 0, 0}, //i
  {0, 1, 1, 1, 0, 0, 0}, //j
  {0, 1, 1, 0, 1, 1, 1}, //k
  {0, 1, 1, 0, 0, 0, 0}, //l
  {1, 0, 0, 1, 1, 1, 1}, //m
  {0, 0, 1, 0, 1, 0, 1}, //n
  {0, 0, 1, 1, 1, 0, 1}, //o
  {1, 1, 0, 0, 1, 1, 1}, //p
  {1, 1, 1, 0, 0, 1, 1}, //q
  {0, 0, 0, 0, 1, 0, 1}, //r
  {1, 0, 1, 1, 0, 1, 1}, //s
  {0, 0, 0, 1, 1, 1, 1}, //t
  {0, 0, 1, 1, 1, 0, 0}, //u
  {0, 0, 1, 1, 1, 0, 0}, //v
  {1, 1, 1, 1, 0, 0, 1}, //w
  {0, 1, 1, 0, 1, 1, 1}, //x
  {0, 1, 1, 1, 0, 1, 1}, //y
  {1, 0, 1, 1, 0, 1, 1}, //z
  {0, 0, 0, 0, 0, 0, 0}, // space
  {1, 1, 1, 1, 1, 1, 0},//0
  {0, 1, 1, 0, 0, 0, 0},//1
  {1, 1, 0, 1, 1, 0, 1},//2
  {1, 1, 1, 1, 0, 0, 1},//3
  {0, 1, 1, 0, 0, 1, 1},//4
  {1, 0, 1, 1, 0, 1, 1},//5
  {1, 0, 1, 1, 1, 1, 1},//6
  {1, 1, 1, 0, 0, 0, 0},//7
  {1, 1, 1, 1, 1, 1, 1},//8
  {1, 1, 1, 1, 0, 1, 1}//9
};

byte segment = 0;
void scrollingMessage(String text) {
  String newText = "     " + text;
  int len = newText.length();
  if (millis() > lastTime + waitTime) {
    segment++;
    lastTime = millis();
  }
  if (segment > len) segment = 0;
  messageSegment(newText.substring(segment, segment + 4));
}
// input substring and display over 4 digits
void messageSegment(String mesg) {
  mesg.toLowerCase();
  letter(0, mesg[3]);
  letter(1, mesg[2]);
  letter(2, mesg[1]);
  letter(3, mesg[0]);
}

void letter(int whichDigit, char whichLetter) {
  int tempLetter;
  switch (whichLetter) {
    case 'a':
      tempLetter = 0;
      break;
    case 'b':
      tempLetter = 1;
      break;
    case 'c':
      tempLetter = 2;
      break;
    case 'd':
      tempLetter = 3;
      break;
    case 'e':
      tempLetter = 4;
      break;
    case 'f':
      tempLetter = 5;
      break;
    case 'g':
      tempLetter = 6;
      break;
    case 'h':
      tempLetter = 7;
      break;
    case 'i':
      tempLetter = 8;
      break;
    case 'j':
      tempLetter = 9;
      break;
    case 'k':
      tempLetter = 10;
      break;
    case 'l':
      tempLetter = 11;
      break;
    case 'm':
      tempLetter = 12;
      break;
    case 'n':
      tempLetter = 13;
      break;
    case 'o':
      tempLetter = 14;
      break;
    case 'p':
      tempLetter = 15;
      break;
    case 'q':
      tempLetter = 16;
      break;
    case 'r':
      tempLetter = 17;
      break;
    case 's':
      tempLetter = 18;
      break;
    case 't':
      tempLetter = 19;
      break;
    case 'u':
      tempLetter = 20;
      break;
    case 'v':
      tempLetter = 21;
      break;
    case 'w':
      tempLetter = 22;
      break;
    case 'x':
      tempLetter = 23;
      break;
    case 'y':
      tempLetter = 24;
      break;
    case 'z':
      tempLetter = 25;
      break;
    case ' ':
      tempLetter = 26;
      break;
    case '0':
      tempLetter = 27;
      break;
    case '1':
      tempLetter = 28;
      break;
    case '2':
      tempLetter = 29;
      break;
    case '3':
      tempLetter = 30;
      break;
    case '4':
      tempLetter = 31;
      break;
    case '5':
      tempLetter = 32;
      break;
    case '6':
      tempLetter = 33;
      break;
    case '7':
      tempLetter = 34;
      break;
    case '8':
      tempLetter = 35;
      break;
    case '9':
      tempLetter = 36;
      break;
    default:
      tempLetter = 26;
      break;
  }
  for (int i = 0; i < 7; i++) {
    if (alpha[tempLetter][i]) disp.segment(whichDigit, i);
  }
  delay(3);
  disp.off(0);
}

int setFinishTime() {
  timer = millis() / 1000;
  timer -= timerStart;
  return timer;
}

// called when reach the target or knocked down
void resetStage() {
  player.xpos = random(6);
  target.xpos = random(6);
  player.level = 0;
  player.ypos = 1;
}

void resetGame() {
  resetStage();
  score = 0;
  timerStart = millis() / 1000;
}
