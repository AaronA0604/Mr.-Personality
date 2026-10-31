// 1. establish bluetooth connection with computer
// 2. read stats from computer
// 3. based on stats, display different emotion (animation on OLED)

#include "BluetoothSerial.h"
#include <U8g2lib.h>

BluetoothSerial SerialBT;
U8G2_SH1106_128X64_NONAME_F_4W_HW_SPI u8g2(U8G2_R0, /*cs=*/5, /*dc=*/16, /*reset=*/17);

bool firstReading = true;

int cpu = 0;
int cpuChange = 0;        // new cpu minus previous cpu (the "spike")
int battery = 0;
long uptime = 0;          // in minutes

unsigned long spikeStart = 0;
bool spikeActive = false;

String mood = "";

unsigned long lastFrame = 0;
unsigned long frameCount = 0;

float easeSpeed = 0.4;

// wake up timers
bool wakeActive = false;
unsigned long wakeStart = 0;
const unsigned long WAKE_LENGTH = 2000;

// blink timers
unsigned long nextBlink = 90;
int blinkFrame = 0;
float blinkScale = 1.0;      // 1.0 = open, ~0.1 = nearly closed

struct Face {
  float eyerx;
  float eyery;

  float pupily;
  float pupilr;

  float mouthy;
  float mouthrx;
  float mouthry;

  float browy;
  float browWidth;
  float browCurve;
  float browTilt;

  float lidy;
};

const Face CONTENT    = {10, 12, 26, 6, 46, 14, 10,  6, 14,  4,  0, 0};
const Face FOCUSED    = {10,  7, 24, 5, 46, 14,  2, 14, 14,  6,  2, 0};
const Face TIRED      = {10,  8, 24, 5, 46, 14,  3, 12, 14, -2, -2, 4};
const Face SUPRISED   = {12, 14, 23, 3, 50,  5,  8,  6, 11,  7,  0, 0};
const Face WAKE_SLEEP = {10,  0, 24, 1, 48,  8,  2, 10, 14,  0, -2, 0};
const Face WAKE_YAWN  = {11,  3, 24, 1, 44, 10, 13,  8, 14,  3,  0, 0};
const Face WAKE_OPEN  = {10,  9, 25, 5, 46, 12,  4,  9, 14,  1, -2, 2};

Face current = WAKE_SLEEP;
Face target  = CONTENT;

void parseLine(String line) {
  int c1 = line.indexOf(',');
  int c2 = line.indexOf(',', c1 + 1);

  if (c1 == -1 || c2 == -1) return;   // damaged line, ignore it

  int newCpu = constrain(line.substring(0, c1).toInt(), 0, 100);

  cpuChange = newCpu - cpu;
  cpu = newCpu;
  battery = constrain(line.substring(c1 + 1, c2).toInt(), 0, 100);
  uptime = line.substring(c2 + 1).toInt();

  if (firstReading) {
    cpuChange = 0;
  }

  firstReading = false;
}

float ease(float cur, float tgt, float speed) {
  return cur + (tgt - cur) * speed;
}

void easeFace(float speed) {
  current.eyerx     = ease(current.eyerx,     target.eyerx,     speed);
  current.eyery     = ease(current.eyery,     target.eyery,     speed);
  current.pupily    = ease(current.pupily,    target.pupily,    speed);
  current.pupilr    = ease(current.pupilr,    target.pupilr,    speed);
  current.mouthy    = ease(current.mouthy,    target.mouthy,    speed);
  current.mouthrx   = ease(current.mouthrx,   target.mouthrx,   speed);
  current.mouthry   = ease(current.mouthry,   target.mouthry,   speed);
  current.browy     = ease(current.browy,     target.browy,     speed);
  current.browWidth = ease(current.browWidth, target.browWidth, speed);
  current.browCurve = ease(current.browCurve, target.browCurve, speed);
  current.browTilt  = ease(current.browTilt,  target.browTilt,  speed);
  current.lidy      = ease(current.lidy,      target.lidy,      speed);
}

void setTargets() {
  if (mood == "focused") {
    target = FOCUSED;
    easeSpeed = 0.4;
  } else if (mood == "tired") {
    target = TIRED;
    easeSpeed = 0.4;
  } else if (mood == "suprised") {
    target = SUPRISED;
    easeSpeed = 0.4;
  } else if (mood == "wakeUp") {
    float p = (millis() - wakeStart) / (float)WAKE_LENGTH;
    if      (p < 0.20) { target = WAKE_SLEEP; easeSpeed = 0.25; }
    else if (p < 0.55) { target = WAKE_YAWN;  easeSpeed = 0.30; }
    else if (p < 0.78) { target = WAKE_OPEN;  easeSpeed = 0.35; }
    else               { target = CONTENT;    easeSpeed = 0.35; }
  } else {
    target = CONTENT;     // every other mood falls back to content for now
    easeSpeed = 0.4;
  }
}

void pickMood() {
  // uptime
  if (wakeActive) {
    mood = "wakeUp";
    return;
  } else {
    mood = "content";
  }

  // cpu change
  if (spikeActive) {
    mood = "suprised";
    return;
  } else {
    mood = "content";
  }

  // battery
  if (battery <= 30) {
    mood = "tired";
    return;
  } else {
    mood = "content";
  }

  // cpu
  if (cpu > 50) {
    mood = "focused";
    return;
  } else {
    mood = "content";
  }
}

// side: -1 = left brow, +1 = right brow
// curve: height of the arch (negative = dip)
// tilt: positive = angry (inner end lower), negative = sad (inner end higher)
void drawBrow(int cx, int baseY, int halfW, float curve, float tilt, int side) {
  int prevX = 0, prevY = 0;

  for (int i = 0; i <= 8; i++) {
    float t = (i - 4) / 4.0;                          // -1 to +1 across the brow
    int x = cx + (int)(t * halfW);
    int y = baseY - (int)(curve * (1 - t * t))        // the arch
                  + (int)(tilt * -t * side);          // the slant, mirrored by side
    y = constrain(y, 0, 62);

    if (i > 0) {
      u8g2.drawLine(prevX, prevY, x, y);
      u8g2.drawLine(prevX, prevY + 1, x, y + 1);      // second copy = 2 px thick
    }
    prevX = x;
    prevY = y;
  }
}

void setup() {
  Serial.begin(115200);
  SerialBT.begin("ESP32-Personality");

  SerialBT.setTimeout(50);
  
  u8g2.begin();
  u8g2.setDrawColor(1);
}

void loop() {
  if (!wakeActive && uptime < 5) {
    wakeActive = true;
    wakeStart = millis();
  }

  if (wakeActive && millis() - wakeStart >= WAKE_LENGTH) {
    wakeActive = false;
  }

  if (spikeActive && millis() - spikeStart >= 3000) {
    spikeActive = false;
  }

  if (SerialBT.available() > 0) {
    String line = SerialBT.readStringUntil('\n');
    parseLine(line);

    if (cpuChange >= 30 && !spikeActive) {
      spikeActive = true;
      spikeStart = millis();
    }

    Serial.printf("CPU %d (%+d)  BAT %d UP %ld\n", cpu, cpuChange, battery, uptime);
  }

  pickMood();
  setTargets();

  if (millis() - lastFrame >= 33) {      // about 30 frames per second
    lastFrame = millis();
    frameCount ++;

    easeFace(easeSpeed);

    // blink
    if (frameCount >= nextBlink && blinkFrame < 4 && !wakeActive && !spikeActive) {         // closing: 0-3
      blinkFrame++;
      blinkScale -= 0.225;
    } else if (blinkFrame >= 4 && blinkFrame < 8) {          // opening: 4-7
      blinkFrame++;
      blinkScale += 0.225;
    } else if (blinkFrame >= 8) {                            // finished
      blinkFrame = 0;
      blinkScale = 1.0;                                     // snap, so float error can't build up
      nextBlink = frameCount + random(60, 154);
    }

    float eyeH   = max(1.0f, current.eyery * blinkScale);
    float pupilR = min(current.pupilr, eyeH - 1);

    u8g2.setDrawColor(1);
    u8g2.clearBuffer();

    // eyes
    u8g2.drawFilledEllipse(44, 23, current.eyerx, eyeH);
    u8g2.drawFilledEllipse(84, 23, current.eyerx, eyeH);

    // pupils
    u8g2.setDrawColor(0);
    if (pupilR >= 1) {
      u8g2.drawDisc(46, 26, round(current.pupily));
      u8g2.drawDisc(86, 26, round(current.pupily));
    }

    // eye lid
    if (current.lidy >= 1) {
      u8g2.drawBox(34, 15, 60, round(current.lidy));
    }

    // mouth
    u8g2.setDrawColor(1);
    u8g2.drawEllipse(64, current.mouthy, current.mouthrx, current.mouthry, U8G2_DRAW_LOWER_LEFT | U8G2_DRAW_LOWER_RIGHT);
    u8g2.drawEllipse(64, current.mouthy + 1, current.mouthrx, current.mouthry, U8G2_DRAW_LOWER_LEFT | U8G2_DRAW_LOWER_RIGHT);

    // eyebrows
    drawBrow(44, current.browy, current.browWidth, current.browCurve, current.browTilt, -1);
    drawBrow(84, current.browy, current.browWidth, current.browCurve, current.browTilt, +1);

    u8g2.sendBuffer();
  }
}
