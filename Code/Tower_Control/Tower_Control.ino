// ===================== Libraries =====================
#define FASTLED_INTERNAL  // Suppress FastLED.h pragma compile messages

// --- Core Arduino / C standard ---
#include <Arduino.h>  // Core Arduino functions (digitalWrite, millis, etc.)
#include <stdio.h>    // Standard C I/O (sprintf, etc.)
#include <stdlib.h>   // Standard C utilities (atoi, malloc, etc.)

// --- Hardware communication ---
#include <Wire.h>  // I2C communication
#include <SPI.h>   // SPI bus

// --- Storage ---
#include <SD.h>           // SD card support
#include <SerialFlash.h>  // Serial flash memory support
#include <EEPROM.h>       // EEPROM read/write

// --- Input devices ---
#include <Encoder.h>  // Rotary encoder

// --- Output / display ---
#include <FastLED.h>                 // LED strip control
#include <Adafruit_LiquidCrystal.h>  // LCD display
#include <Adafruit_LEDBackpack.h>    // 7-segment or LED matrix backpacks
#include <Audio.h>                   // Audio library (Teensy or similar)


// ===================== Pin Assignments =====================
// --- LED strip ---
#define NUM_LEDS 16          // Number of LEDs in strip
#define DATA_PIN 39          // Pin to Control NeoPixels
#define LED_NOTIFICATION 13  // Notification LED

// --- SD / Audio ---
#define SDCARD_CS_PIN BUILTIN_SDCARD  // SD card chip select
#define SDCARD_MOSI_PIN 61            // MOSI pin for audio
#define SDCARD_SCK_PIN 60             // SCK pin for audio

// --- Buttons ---
#define PIN_BUTTON_RE 26     // Rotary Encoder Button
#define PIN_BUTTON_BACK 24   // Back Button
#define PIN_BUTTON_START 25  // Start Race Button
#define PIN_BUTTON_STOP 27   // Pause Button

// --- Rotary Encoder ---
#define ENCODER_INCREMENT 29  // Rotary Encoder Pin +
#define ENCODER_DECREMENT 28  // Rotary Encoder Pin -

// --- Lane Relays (track control) ---
#define RELAY_LANE_1 14  // Relay for Lane 1
#define RELAY_LANE_2 15  // Relay for Lane 2
#define RELAY_LANE_3 16  // Relay for Lane 3
#define RELAY_LANE_4 17  // Relay for Lane 4

// --- Lap Monitors (track switches) ---
#define MONITOR_LAP_LANE_1 36  // Lap counter switch Lane 1
#define MONITOR_LAP_LANE_2 35  // Lap counter switch Lane 2
#define MONITOR_LAP_LANE_3 34  // Lap counter switch Lane 3
#define MONITOR_LAP_LANE_4 33  // Lap counter switch Lane 4

// I2S Audio Assignments
AudioPlaySdWav playSdWav1;
AudioMixer4 mixer1;
AudioOutputI2S i2s1;
AudioConnection patchCord1(playSdWav1, 0, mixer1, 0);
AudioConnection patchCord2(playSdWav1, 1, mixer1, 1);
AudioConnection patchCord3(mixer1, 0, i2s1, 0);
AudioConnection patchCord4(mixer1, 0, i2s1, 1);

// Neopixel Arrays
CRGB leds[NUM_LEDS];                                                                 // Define the array of leds
int NP_Boot_Pattern[16] = { 13, 14, 12, 15, 0, 11, 1, 10, 2, 9, 3, 8, 4, 7, 5, 6 };  // LED Order for Boot Animation
int NP_Boot_Colors[4] = { 0, 64, 96, 160 };                                          // Colors Used in Boot Animation
int NP_Boot_Transitions = 4;                                                         // Color Transitions in Boot Animation
int NP_Race_Start_Red[4] = { 12, 13, 14, 15 };                                       // Red LEDS for Race Start

// 7 Seg LED Assignments
Adafruit_AlphaNum4 playerPolePositions[2] = { Adafruit_AlphaNum4(), Adafruit_AlphaNum4() };                                              // Pole Position Car Numbers 1 - 4 ((1 & 2) + (3 & 4))
Adafruit_AlphaNum4 playerTimes[4] = { Adafruit_AlphaNum4(), Adafruit_AlphaNum4(), Adafruit_AlphaNum4(), Adafruit_AlphaNum4() };          // Pole Position Time 1 - 4
Adafruit_AlphaNum4 lapRecNum = Adafruit_AlphaNum4();                                                                                      //Lap Counter and Lap Record Car Number
Adafruit_AlphaNum4 laptRecTime = Adafruit_AlphaNum4();                                                                                     //Lap Record Time

// LCD Backpack Setup
Adafruit_LiquidCrystal lcd(1);  //default address #0 (A0-A2 not jumpered)

//Encoder Setup
Encoder myEnc(ENCODER_INCREMENT, ENCODER_DECREMENT);

// Variables
// Menu Navigation - The variables trigger a menu change when set as the currentMenu
enum class MenuState {
  MENU_WELCOME,
  MENU_OPTIONS,
  MENU_OPTIONS_BACK,
  OPTIONS_END_RACE_RESET_DELAY,
  OPTIONS_RACE_PENALTY_TIMER,
  OPTIONS_TRACK_DEBOUNCE_TIMING,
  OPTIONS_CLEAR_LAP_RECORD,
  MENU_NUM_RACERS,
  MENU_NUM_LAPS,
  MENU_CAR_NUM_LANE_ASSIGN,
  MENU_CAR_NUM_LANE_ASSIGN_BACK,
  MENU_NONE
};
MenuState currentMenu = MenuState::MENU_WELCOME;

enum class RaceState {
  NONE,                 // Not racing; idle or in menu

  // --- Start Sequence ---
  START_PREPARE,        // Clear LEDs, show "Start Engines"
  START_LIGHTS_RED,     // Red lights + sound
  START_LIGHTS_YELLOW,  // Yellow flashing
  START_LIGHTS_GREEN,   // Green go signal
  START_COMPLETE,       // Potential for others pre-race steps if ever needed

  // --- Race Active ---
  ACTIVE,               // Main race loop (metrics, laps, penalties)
  PAUSED,               // Temporarily paused
  STOPPED,              // Manually aborted

  // --- Race End ---
  END_CHECK,            // Confirm all racers done
  END_RESULTS,          // Show leaderboard or animations
  END,                  // Generic end state
  CLEAR                 // Reset for next race
};
RaceState raceState = RaceState::NONE;
bool stateEntered = true;
unsigned long stateStartTime = 0;

struct MenuEntry {
  MenuState id;
  const char *label;
};

const MenuEntry mainMenu[] = {
  { MenuState::MENU_WELCOME, "Welcome!" },
  { MenuState::MENU_OPTIONS, "Options" },
  { MenuState::MENU_NUM_RACERS, "Number of Racers" },
  { MenuState::MENU_NUM_LAPS, "Number of Laps" },
  { MenuState::MENU_CAR_NUM_LANE_ASSIGN, "Car# & Ln Assign" },
};

const int mainMenuCount = sizeof(mainMenu) / sizeof(mainMenu[0]);

const MenuEntry optionsMenu[] = {
  { MenuState::MENU_NUM_RACERS, "Number of Racers" },
  { MenuState::OPTIONS_END_RACE_RESET_DELAY, "End Race Delay" },
  { MenuState::OPTIONS_RACE_PENALTY_TIMER, "Penalty Time" },
  { MenuState::OPTIONS_TRACK_DEBOUNCE_TIMING, "Track Debounce" },
  { MenuState::OPTIONS_CLEAR_LAP_RECORD, "Clear Lap Record" },
};

const int optionsMenuCount = sizeof(optionsMenu) / sizeof(optionsMenu[0]);

// Defines the types of input we can expect
enum class InputEvent {
  NONE,
  ENCODER_LEFT,
  ENCODER_RIGHT,
  BUTTON_BACK,
  BUTTON_START,
  BUTTON_STOP,
  BUTTON_RE
};

// Rotary Encoder - Logs the position of the Rotary Encoder
long encoderPositionNew;

// Timing
unsigned long pauseStartTime = 0;      // Time (Milliseconds) to ajdust in the event the race is paused

// Debounce timers
unsigned long debounceButton = 200;   // Debounce time (Milliseconds) for a Button Press
unsigned int debounceTrack = 1000;    // Default debounce time (Milliseconds) when a car passes the start line (Can be Modified in Options 500-10000 and saved to EEPROM)
unsigned long debounceTick = 150;     // Debounce time for preventing too many ticks when displaying new values selected via rotary encoder

// Race Identifiers
int numLaps = 5;             //Default number of laps in the Race (Can be Modified in Menu 5-99)
int MIN_LAPS = 5;            //Minimum number of laps in a race
int MAX_LAPS = 99;           //Maximum number of laps in a race
int numLanes = 4;            //Max number of lanes on the race track
int numRacers = 2;           //Default number of Racers in the Race (Can be Modified in Menu 1-4)
int configuredRacers = 0;    //How many cars have been configured
int carConfigIndex = 0;      //Tracks which car is having its number and lane assigned

// Race Information
int currentLapNum = 0;               //Lap Count in Current Race
unsigned long recordLapTime = 99999; //Default Lap Record Time (Actual is called from EEPROM)
int recordLapCarsIndex = -1;         //Array Identifer of the record setting car
int recordLapCarNumbersIndex;        //Lap Record Car Number (Value is called from EEPROM)
unsigned long soundBuffer;           //Time (Milliseconds) buffer to avoid sound stomping on eachother

// Struct (or class/object) that defines everything that a car needs to have
struct Car {
  int lane;                  // Lane the car is in
  struct Lane *p_lane;       // A pointer to the lane object the car is present in
  int number;                // The number that represents which car type is in use
  int currentLap;            // Current lap the car is on
  int position;              // Position the car is currently in
  unsigned long lapTime;     // The time (Milliseconds) of the last lap
  unsigned long priorLapMillis; // The timestamp of the millis() read at the prior lap
  unsigned long totalTime;   // Total race time (Milliseconds)
  unsigned long startTime;   // Start race time (millis at the race start)
  bool isOnLastLap;          // Flag to signal last lap Neopixel and Sound Events
  bool finished;             // Has the car finished the race
  bool totalDisplayed;       // Has the car had it's totalTime displayed
};

// Declare our cars array and fill them in with default values in the loop
struct Car *cars = (Car *)malloc(numLanes * sizeof *cars);

// Struct (or class/object) that defines everything that a lane needs to have
struct Lane {
  int number;                  // Lane number
  struct Car *p_car;           // A pointer to the car object present in the lane
  int np[4];                   // Lane LEDs array
  int relay;                   // Controls the Relay for the lane in the Control Box
  int monitorLap;              // Lap counter switch for the lane
  int currentState;            // In Track Lap Counter Monitors State, per lane
  int previousState;           // In Track Lap Counter Monitors Previous State, per lane, prevents duplicate lap counting
  bool hasPenalty;             // Flag if car crosses start line before the green light, per lane
  unsigned long penaltyStartTime; // The race time the penalty occurred (Milliseconds)
};

// Declare our lanes array and fill them in with default values in the loop
struct Lane *lanes = (Lane *)malloc(numLanes * sizeof *lanes);

// Neopixel Variables
int NP_Brightness = 84;          // Set Neopixel Brightness

// Delay variables
int delayStartSequence = 100;  // Start Animation Speed (Higher = Slower)
int delayDim = 50;             // Dimming Speed (Higher = Slower)
unsigned int delayYellowLight = 750;  // Delay between Yellow Lights
unsigned int delayRedLight = 4250;    // Time for Red Lights
int delayStopRace = 10000;            // Default time (Milliseconds) for wait on race end before going back to Main Menu (Can be Modified in Options 1000-10000 and saved to EEPROM)
int delayFinalTimesDisplay = 5000;    // Time to wait (ms) befor displaying a finished racer's final total race time
unsigned long delayPenalty = 5000;    // Default time (Milliseconds) for Penalty duration if a car crosses the track before green (Can be Modified in Options 500-5000 and saved to EEPROM)

// Menu Arrays
// Car Names and Numbers Displayed on LCD
String carNames[10] = { "01 Skyline", "03 Ford Capri", "05 BMW 3.5 CSL", "05 Lancia LC2", "33 Audi RS5", "51 Porsche 935", "576 Lancia Beta", "80 BMW M1", "88 BTTF Delorean", "MM GT Falcon V8" };
// Car Numbers on Displayed on Pole Position and Lap Record 7 Segment
String carNumbers[11] = { "01", "03", "05", "05", "33", "51", "57", "80", "88", "MM", "--" };
// Menu selection for Erasing EEPROM
String recReset[20] = { "NO", "X", "XXX", "X", "XXX", "X", "XXX", "X", "XXX", "X", "YES", "X", "XXX", "X", "XXX", "X", "XXX", "X", "XXX", "X" };

// --- Function Declarations (Prototypes) ---
void munuOptions(bool reset);
void displayPolePosition(int lane_num = -1, int index = -1);
void saveLapRecord(int carNumber = (sizeof(carNumbers) / sizeof(carNumbers[0])) - 1);

// Function to help qsort cars in position order
int cmpLapAndTotalTime(const void *left, const void *right) {
  struct Car *a = (struct Car *)left;
  struct Car *b = (struct Car *)right;

  if (b->currentLap < a->currentLap) {
    return -1;
  } else if (a->currentLap < b->currentLap) {
    return 1;
  } else {
    return (b->totalTime < a->totalTime) - (a->totalTime < b->totalTime);
  }
}

// Function to help qsort cars in lane order
int cmpLaneOrder(const void *left, const void *right) {
  struct Car *a = (struct Car *)left;
  struct Car *b = (struct Car *)right;

  if (a->lane < b->lane) {
    return -1;
  }
  return 1;
}

// Read Long from EEPROM (Lap Time)
long readLongEEPROM(long address) {
  //Read the 4 bytes from the eeprom memory.
  long four = EEPROM.read(address);
  long three = EEPROM.read(address + 1);
  long two = EEPROM.read(address + 2);
  long one = EEPROM.read(address + 3);

  //Return the recomposed long by using bitshift.
  return ((four << 0) & 0xFF) + ((three << 8) & 0xFFFF) + ((two << 16) & 0xFFFFFF) + ((one << 24) & 0xFFFFFFFF);
}

// Read Int from EEPROM (Variables for Options)
int readIntEEPROM(int address) {
  long two = EEPROM.read(address);
  long one = EEPROM.read(address + 1);

  //Return the recomposed long by using bitshift
  return ((two << 0) & 0xFFFFFF) + ((one << 8) & 0xFFFFFFFF);
}

// Reads the start button and won't exit the function until the user stops pressing it, still takes into account debounce
int readButtonStart(bool waitForRelease = true) {
  int buttonPressed = 0;  // Was the button pressed at all
  int buttonStatus = 0;   // The current status of the button

  unsigned long currentPressTime = 0;
  static unsigned long previousPressTime = 0;  // Holds last press time for debounce

  do {
    buttonStatus = digitalRead(PIN_BUTTON_START);
    currentPressTime = millis();

    if (buttonStatus == HIGH && !buttonPressed) {
      // Only register if enough time passed since last valid press
      if ((currentPressTime - previousPressTime) > debounceButton) {
        buttonPressed = buttonStatus;  // If we saw the button pressed at any time, set the flag
        previousPressTime = currentPressTime;
        if (!waitForRelease) return buttonStatus;
      }
    }
  } while (waitForRelease && buttonStatus == HIGH);  // Still blocking until release

  return buttonPressed;
}

// Reads the back button and won't exit the function until the user stops pressing it, still takes into account debounce
int readButtonBack(bool waitForRelease = true) {
  int buttonPressed = 0;  // Was the button pressed at all
  int buttonStatus = 0;   // The current status of the button

  unsigned long currentPressTime = 0;
  static unsigned long previousPressTime = 0;  // Holds last press time for debounce

  do {
    buttonStatus = digitalRead(PIN_BUTTON_BACK);
    currentPressTime = millis();

    if (buttonStatus == HIGH && !buttonPressed) {
      // Only register if enough time passed since last valid press
      if ((currentPressTime - previousPressTime) > debounceButton) {
        buttonPressed = buttonStatus;  // If we saw the button pressed at any time, set the flag
        previousPressTime = currentPressTime;
        if (!waitForRelease) return buttonStatus;
      }
    }
  } while (waitForRelease && buttonStatus == HIGH);  // Still blocking until release

  return buttonPressed;
}

// Reads the stop button and won't exit the function until the user stops pressing it, still takes into account debounce
int readButtonStop(bool waitForRelease = true) {
  int buttonPressed = 0;  // Was the button pressed at all
  int buttonStatus = 0;   // The current status of the button

  unsigned long currentPressTime = 0;
  static unsigned long previousPressTime = 0;  // Holds last press time for debounce

  do {
    buttonStatus = digitalRead(PIN_BUTTON_STOP);
    currentPressTime = millis();

    if (buttonStatus == HIGH && !buttonPressed) {
      // Only register if enough time passed since last valid press
      if ((currentPressTime - previousPressTime) > debounceButton) {
        buttonPressed = buttonStatus;  // If we saw the button pressed at any time, set the flag
        previousPressTime = currentPressTime;
        if (!waitForRelease) return buttonStatus;
      }
    }
  } while (waitForRelease && buttonStatus == HIGH);  // Still blocking until release

  return buttonPressed;
}

// Reads the rotary encoder
void readRotaryEncoder() {
  static long lastPosition = 0;
  long position = myEnc.read() / 4;

  if (position != lastPosition) {
    lastPosition = position;
    encoderPositionNew = position;
  } else {
    lastPosition = position;
  }
}

// Generic Input reader
InputEvent readInputs(bool waitForRelease = true) {
  static long lastEncoderPos = 0;

  // --- 1️⃣ Handle Rotary Encoder ---
  readRotaryEncoder();  // Updates encoderPositionNew
  if (encoderPositionNew > lastEncoderPos) {
    lastEncoderPos = encoderPositionNew;
    return InputEvent::ENCODER_RIGHT;
  } else if (encoderPositionNew < lastEncoderPos) {
    lastEncoderPos = encoderPositionNew;
    return InputEvent::ENCODER_LEFT;
  }

  // --- 2️⃣ Handle Buttons ---
  if (readButtonStart(waitForRelease) == 1) {
    return InputEvent::BUTTON_START;
  }
  if (readButtonBack(waitForRelease) == 1) {
    return InputEvent::BUTTON_BACK;
  }

  if (readButtonStop(waitForRelease) == 1) {
    return InputEvent::BUTTON_STOP;
  }

  // --- 3️⃣ Default ---
  return InputEvent::NONE;
}

// ===================== Setup Helpers =====================
// Lane Helper mappings
const uint8_t RELAY_PINS[4] = { RELAY_LANE_1, RELAY_LANE_2, RELAY_LANE_3, RELAY_LANE_4 };
const uint8_t MONITOR_PINS[4] = { MONITOR_LAP_LANE_1, MONITOR_LAP_LANE_2, MONITOR_LAP_LANE_3, MONITOR_LAP_LANE_4 };

// Default NeoPixel indices per lane
const uint8_t NP_LANE_MAP[4][4] = {
  { 11, 10, 9, 12 },  // Lane 1
  { 8, 7, 6, 13 },    // Lane 2
  { 5, 4, 3, 14 },    // Lane 3
  { 2, 1, 0, 15 }     // Lane 4
};

// Initialize a given car object
void initCar(int i) {
  cars[i].lane = i + 90;     // Set lane to a non-existent lane
  cars[i].p_lane = nullptr;  // No lane-pointer by default (set below if desired)
  cars[i].number = 10;       // Default placeholder car number
  cars[i].currentLap = 0;
  cars[i].position = i + 1;
  cars[i].lapTime = 0;
  cars[i].priorLapMillis = 0; // millis() timestamp of the last completed lap to calculate lap times
  cars[i].totalTime = 0;
  cars[i].startTime = 0;
  cars[i].isOnLastLap = 0;
  cars[i].finished = 0;
  cars[i].totalDisplayed = false;
}

// Initialize all car objects to default starting values
void initCars() {
  for (int i = 0; i < numLanes; ++i) {
    initCar(i);
  }
}

// Initialize a given lane object
void initLane(int i){
  lanes[i].number = i + 1;   // Lane numbers are 1-based
  lanes[i].p_car = nullptr;  // Default to no car in a lane
  for (int j = 0; j < 4; ++j) {
    lanes[i].np[j] = NP_LANE_MAP[i][j];
  }
  lanes[i].relay = RELAY_PINS[i];
  lanes[i].monitorLap = MONITOR_PINS[i];
  lanes[i].currentState = -1;
  lanes[i].previousState = -1;
  lanes[i].hasPenalty = 0;
  lanes[i].penaltyStartTime = 0;
}

// Initialize all lane objects to default starting values
void initLanes() {
  for (int i = 0; i < numLanes; ++i) {
    initLane(i);
  }
}

void setup() {
  // Start Serial Monitor
  Serial.begin(9600);
  while (!Serial && millis() < 3000) ;  // wait up to 3 sec

  // I2S Audio / SD setup
  AudioMemory(8);
  SPI.setMOSI(SDCARD_MOSI_PIN);
  SPI.setSCK(SDCARD_SCK_PIN);
  while (!(SD.begin(SDCARD_CS_PIN))) {
    delay(10);  // Avoids tight busy-loop
  }

  // Initialize cars & lanes
  initCars();
  initLanes();

  // Read EEPROM Variables and replace default values
  recordLapTime = readLongEEPROM(0x02);
  recordLapCarNumbersIndex = EEPROM.read(0x00);
  delayPenalty = readIntEEPROM(0x08);
  delayStopRace = readIntEEPROM(0x06);
  debounceTrack = readIntEEPROM(0x10);

  // Set up the 7-Segment LED Panels
  playerPolePositions[0].begin(0x70);  // Lanes 1 & 2
  playerPolePositions[1].begin(0x71);  // Lanes 3 & 4

  lapRecNum.begin(0x72);               // Pass in the address for the Lap Counter and Lap Record Car Number
  laptRecTime.begin(0x77);             // Pass in the address for the Lap Record Time
  playerTimes[1].begin(0x73);          // Pass in the address for the Place 1 Lap Time
  playerTimes[0].begin(0x74);          // Pass in the address for the Place 2 Lap Time
  playerTimes[2].begin(0x75);          // Pass in the address for the Place 3 Lap Time
  playerTimes[3].begin(0x76);          // Pass in the address for the Place 4 Lap Time

  // Set up the LCD's number of rows and columns and enable the backlight
  lcd.begin(16, 2);
  lcd.setBacklight(HIGH);

  // Pin Mode Assignments
  pinMode(PIN_BUTTON_RE, INPUT);
  pinMode(PIN_BUTTON_BACK, INPUT);
  pinMode(PIN_BUTTON_START, INPUT);
  pinMode(PIN_BUTTON_STOP, INPUT);
  pinMode(LED_NOTIFICATION, OUTPUT);

  // Set the monitorLap and relay pin modes and disable the lanes before the race
  for (int l = 0; l < numLanes; l++) {
    pinMode(lanes[l].monitorLap, INPUT_PULLUP);
    pinMode(lanes[l].relay, OUTPUT);
    digitalWrite(lanes[l].relay, HIGH);
  }

  // Neopixel Setup
  LEDS.addLeds<WS2812, DATA_PIN, RGB>(leds, NUM_LEDS);
  LEDS.setBrightness(NP_Brightness);
}

// Main Loop
void loop() {
  switch (currentMenu) {
    case MenuState::MENU_WELCOME:
      welcomeMessage();
      break;

    case MenuState::MENU_OPTIONS:
      munuOptions(true);
      break;

    case MenuState::MENU_OPTIONS_BACK:
      munuOptions(false);
      break;

    case MenuState::OPTIONS_END_RACE_RESET_DELAY:
      optionEndRaceDelay();
      break;

    case MenuState::OPTIONS_RACE_PENALTY_TIMER:
      optionPenaltyTimer();
      break;

    case MenuState::OPTIONS_TRACK_DEBOUNCE_TIMING:
      optionTrackDebounce();
      break;

    case MenuState::OPTIONS_CLEAR_LAP_RECORD:
      optionClearLapRecord();
      break;

    case MenuState::MENU_NUM_RACERS:
      munuNumRacers();
      break;

    case MenuState::MENU_NUM_LAPS:
      munuNumLaps();
      break;

    case MenuState::MENU_CAR_NUM_LANE_ASSIGN:
    case MenuState::MENU_CAR_NUM_LANE_ASSIGN_BACK:
      munuCarNumLaneAssign();
      break;

    case MenuState::MENU_NONE:
      // Nothing to do, race system might be running
      break;

    default:
      // Fallback if something goes wrong
      // currentMenu = MenuState::MENU_WELCOME;
      break;
  }  // end - switch (currentMenu) {

  switch (raceState) {
    // --- START SEQUENCE ---
    case RaceState::START_PREPARE:
    case RaceState::START_LIGHTS_RED:
    case RaceState::START_LIGHTS_YELLOW:
    case RaceState::START_LIGHTS_GREEN:
    case RaceState::START_COMPLETE:
      startRace();
      break;

    // --- ACTIVE RACE LOGIC ---
    case RaceState::ACTIVE: {
      // --- Handle top-level control inputs ---
      if (readButtonStart(false)) {
        raceState = RaceState::PAUSED;
        stateEntered = true;
        break;
      }

      if (readButtonStop(false)) {
        raceState = RaceState::STOPPED;
        stateEntered = true;
        break;
      }

      // --- Core race logic tick ---
      raceMetrics();
      break;
    }

    // --- PAUSED STATE ---
    case RaceState::PAUSED:
      pauseRace();
      break;

    // --- STOPPED / ABORTED ---
    case RaceState::STOPPED:
      stopRace();
      break;

    // --- RACE END SEQUENCE ---
    case RaceState::END:
    case RaceState::END_CHECK:
    case RaceState::END_RESULTS:
      endRace();
      break;

    // --- CLEARING AND RESETTING ---
    case RaceState::CLEAR:
      clearRace();
      break;

    case RaceState::NONE:
    default:
      // No race active; waiting in menu or idle
      break;
  }  // end - switch (raceState) {
}

// Play Audio Files
void playFile(const char *filename) {
  playSdWav1.play(filename);        // Start playing the file.  The sketch continues to run while the file plays.
  delay(5);                         // A brief delay for the library read WAV info
  while (playSdWav1.isPlaying()) {  // Simply wait for the file to finish playing.
  }
}

// Initial LED Animation and Welcome Message
void welcomeMessage() {
  lcd.setCursor(3, 0);
  lcd.print("Welcome to");
  lcd.setCursor(3, 1);
  lcd.print("the Race!");
  playSdWav1.play("WELCOME.WAV");
  displayLapRecord();

  // Intro Green/Yellow/Red/Blue Neopixel Animation
  for (int j = 0; j < NP_Boot_Transitions; j++) {
    for (int i = 0; i < NUM_LEDS; i++) {  // Left Chase Animation
      leds[NP_Boot_Pattern[i]] = CHSV(NP_Boot_Colors[j], 255, 255);
      i++;
      leds[NP_Boot_Pattern[i]] = CHSV(NP_Boot_Colors[j], 255, 255);
      FastLED.show();
      delay(delayStartSequence);
    }
    j++;
    for (int i = 15; i > 0; i--) {  // Right Chase Animation
      leds[NP_Boot_Pattern[i]] = CHSV(NP_Boot_Colors[j], 255, 255);
      i--;
      leds[NP_Boot_Pattern[i]] = CHSV(NP_Boot_Colors[j], 255, 255);
      FastLED.show();
      delay(delayStartSequence);
    }
  }

  // Fade out Animation
  for (int b = 84; b >= 0; b -= 2) {
    LEDS.setBrightness(b);
    FastLED.show();
    delay(delayDim);
  }

  // Setup for next menu
  currentMenu = MenuState::MENU_NUM_RACERS;
  lcd.clear();
}

// --- Options Main Menu ---
void munuOptions(bool reset = false) {
  static int selectedIndex = 1;
  if (reset) selectedIndex = 1;  // Set to your preferred default

  auto updateDisplay = [](int index) {
    static bool firstRun = true;
    static unsigned long lastTick = 0;
    unsigned long now = millis();

    if (!firstRun && (now - lastTick) > debounceTick) {
      playSdWav1.play("TICK.WAV");
      lastTick = now;
    }
    firstRun = false;

    lcd.clear();
    lcd.setCursor(4, 0);
    lcd.print("Options");
    lcd.setCursor(0, 1);
    lcd.print("                ");  // Clear line
    int centerVal = (16 - strlen(optionsMenu[index].label)) / 2;
    lcd.setCursor(centerVal, 1);
    lcd.print(optionsMenu[index].label);
  };

  // Initial draw
  updateDisplay(selectedIndex);

  int lastIndex = -1;
  while (true) {
    InputEvent event = readInputs();

    switch (event) {
      case InputEvent::ENCODER_RIGHT:
        selectedIndex = min(selectedIndex + 1, optionsMenuCount - 1);
        break;

      case InputEvent::ENCODER_LEFT:
        selectedIndex = max(selectedIndex - 1, 0);
        break;

      case InputEvent::BUTTON_START:
        // Move to the selected submenu
        currentMenu = optionsMenu[selectedIndex].id;
        return;

      case InputEvent::BUTTON_BACK:
        // Return to previous main menu
        currentMenu = MenuState::MENU_NUM_RACERS;
        return;

      case InputEvent::NONE:
        continue;

      default:
        continue;
    }

    if (selectedIndex != lastIndex) {
      updateDisplay(selectedIndex);
      lastIndex = selectedIndex;
    }
  }
}

// Stop Race Timeout Value Selection and Set
void optionEndRaceDelay() {
  auto updateDisplay = [](int index) {
    static bool firstRun = true;
    static unsigned long lastTick = 0;
    unsigned long now = millis();

    if (!firstRun && (now - lastTick) > debounceTick) {
      playSdWav1.play("TICK.WAV");
      lastTick = now;
    }
    firstRun = false;

    lcd.setCursor(6, 1);
    lcd.print("        ");  // Clear area
    lcd.setCursor(6, 1);
    lcd.print(index);
  };

  lcd.clear();
  lcd.setCursor(2, 0);
  lcd.print("End Timeout");
  updateDisplay(delayStopRace);

  // --- Menu loop ---
  int lastValue = -1;
  int valueBackup = delayStopRace;
  while (true) {
    InputEvent event = readInputs();

    switch (event) {
      case InputEvent::ENCODER_RIGHT:
        delayStopRace = min(delayStopRace + 500, 10000);
        break;

      case InputEvent::ENCODER_LEFT:
        delayStopRace = max(delayStopRace - 500, 1000);
        break;

      case InputEvent::BUTTON_START:
        // Save and return to options menu
        writeIntEEPROM(0x06, delayStopRace);
        currentMenu = MenuState::MENU_OPTIONS;
        return;

      case InputEvent::BUTTON_BACK:
        // Cancel and return without saving
        delayStopRace = valueBackup;
        currentMenu = MenuState::MENU_OPTIONS_BACK;
        return;

      case InputEvent::NONE:
        continue;

      default:
        continue;
    }

    // --- Only update when changed ---
    if (delayStopRace != lastValue) {
      updateDisplay(delayStopRace);
      lastValue = delayStopRace;
    }
  }
}

// Penalty Value Selection and Set
void optionPenaltyTimer() {
  auto updateDisplay = [](int index) {
    static bool firstRun = true;
    static unsigned long lastTick = 0;
    unsigned long now = millis();

    if (!firstRun && (now - lastTick) > debounceTick) {
      playSdWav1.play("TICK.WAV");
      lastTick = now;
    }
    firstRun = false;

    lcd.setCursor(6, 1);
    lcd.print("        ");  // Clear field
    lcd.setCursor(6, 1);
    lcd.print(index);
  };

  // Initial draw
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Penalty Timeout");
  updateDisplay(delayPenalty);

  // --- Menu loop ---
  unsigned long lastValue = -1;
  unsigned long valueBackup = delayPenalty;
  while (true) {
    InputEvent event = readInputs();

    switch (event) {
      case InputEvent::ENCODER_RIGHT:
        delayPenalty = min(delayPenalty + 500u, 5000u);
        break;

      case InputEvent::ENCODER_LEFT:
        delayPenalty = max(delayPenalty - 500u, 500u);
        break;

      case InputEvent::BUTTON_START:
        // Save and return to main options
        writeIntEEPROM(0x10, delayPenalty);
        currentMenu = MenuState::MENU_OPTIONS;
        return;

      case InputEvent::BUTTON_BACK:
        // Return without saving
        delayPenalty = valueBackup;
        currentMenu = MenuState::MENU_OPTIONS_BACK;
        return;

      case InputEvent::NONE:
        continue;

      default:
        continue;
    }

    // --- Update LCD only on change ---
    if (delayPenalty != lastValue) {
      updateDisplay(delayPenalty);
      lastValue = delayPenalty;
    }
  }
}

// Track Debounce Value Selection and Set
void optionTrackDebounce() {
  auto updateDisplay = [](int index) {
    static bool firstRun = true;
    static unsigned long lastTick = 0;
    unsigned long now = millis();

    if (!firstRun && (now - lastTick) > debounceTick) {
      playSdWav1.play("TICK.WAV");
      lastTick = now;
    }
    firstRun = false;

    lcd.setCursor(6, 1);
    lcd.print("        ");  // Clear field
    lcd.setCursor(6, 1);
    lcd.print(index);
  };

  // Initial draw
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Lane Debouncing");
  updateDisplay(debounceTrack);

  // --- Menu loop ---
  unsigned int lastValue = 0;
  unsigned int valueBackup = debounceTrack;
  while (true) {
    InputEvent event = readInputs();

    switch (event) {
      case InputEvent::ENCODER_RIGHT:
        debounceTrack = min(debounceTrack + 500u, 5000u);
        break;

      case InputEvent::ENCODER_LEFT:
        debounceTrack = max(debounceTrack - 500u, 500u);
        break;

      case InputEvent::BUTTON_START:
        // Save and return to main options
        writeIntEEPROM(0x08, debounceTrack);
        currentMenu = MenuState::MENU_OPTIONS;
        return;

      case InputEvent::BUTTON_BACK:
        // Return without saving
        debounceTrack = valueBackup;
        currentMenu = MenuState::MENU_OPTIONS_BACK;
        return;

      case InputEvent::NONE:
        continue;

      default:
        continue;
    }

    // --- Update LCD only on change ---
    if (debounceTrack != lastValue) {
      updateDisplay(debounceTrack);
      lastValue = debounceTrack;
    }
  }
}

// Menu Section to Clear Lap Record from EEPROM
void optionClearLapRecord() {
  int selectedIndex = 0;
  const int maxIndex = (sizeof(recReset) / sizeof(recReset[0])) - 1;;

  auto updateDisplay = [](int index) {
    static bool firstRun = true;
    static unsigned long lastTick = 0;
    unsigned long now = millis();

    if (!firstRun && (now - lastTick) > debounceTick) {
      playSdWav1.play("TICK.WAV");
      lastTick = now;
    }
    firstRun = false;

    lcd.setCursor(1, 1);
    lcd.print("                ");
    centerTextEEPROM(index);
    lcd.print(recReset[index]);
  };

  // Initial display
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("ERASE LAP RECORD");
  updateDisplay(selectedIndex);

  int lastIndex = -1;
  while (true) {
    InputEvent event = readInputs();

    switch (event) {
      case InputEvent::ENCODER_RIGHT:
        selectedIndex = (selectedIndex + 1) > maxIndex ? 0 : selectedIndex + 1;
        break;

      case InputEvent::ENCODER_LEFT:
        selectedIndex = (selectedIndex - 1) < 0 ? maxIndex : selectedIndex - 1;
        break;

      case InputEvent::BUTTON_START:
        if (recReset[selectedIndex] == "YES") {  // "YES" — clear lap record
          recordLapTime = 99999;
          recordLapCarsIndex = -1;
          recordLapCarNumbersIndex = (sizeof(carNumbers) / sizeof(carNumbers[0])) - 1;
          saveLapRecord(recordLapCarNumbersIndex);
          displayLapRecord();
        }
        // In both YES/NO cases, return to Options
        currentMenu = MenuState::MENU_OPTIONS;
        return;

      case InputEvent::BUTTON_BACK:
        // Back out without clearing
        currentMenu = MenuState::MENU_OPTIONS_BACK;
        return;

      case InputEvent::NONE:
        continue;

      default:
        continue;
    }

    if (selectedIndex != lastIndex) {
      updateDisplay(selectedIndex);
      lastIndex = selectedIndex;
    }
  }
}

// Displays the car that's currently configured to a lane
void displayCarSelect() {
  // Write all the cars configured
  for (int i = 0; i < numLanes; i++) {
    // Blank out the display if the lane doesn't have a car in it
    if (lanes[i].p_car == nullptr) {
      playerTimes[i].clear();
      playerTimes[i].writeDisplay();
      continue;
    }

    // Write the car ascii buffer
    int carNum = (lanes[i].p_car - cars) + 1;
    playerTimes[i].writeDigitAscii(0, 'C');
    playerTimes[i].writeDigitAscii(1, 'A');
    playerTimes[i].writeDigitAscii(2, 'R');
    playerTimes[i].writeDigitAscii(3, '0' + carNum);

    // Write the to the display
    playerTimes[i].writeDisplay();
  }
}

// Menu Section to Specify Number of Racers
void munuNumRacers() {
  // --- Helper for updating the display ---
  auto updateDisplay = [](int index) {
    static bool firstRun = true;
    static unsigned long lastTick = 0;
    unsigned long now = millis();

    if (!firstRun && (now - lastTick > debounceTick)) {  // debounce tick sound
      playSdWav1.play("TICK.WAV");
      lastTick = now;
    }
    firstRun = false;

    lcd.setCursor(7, 1);
    lcd.print("   ");  // clear area in case of shorter numbers
    lcd.setCursor(7, 1);
    lcd.print(index);
  };

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Number of Racers");
  updateDisplay(numRacers);
  displayPolePosition();

  // --- Menu loop – user is locked here until Start or Back is pressed ---
  int lastRacers = -1;
  int existingNumRacers = numRacers;
  while (true) {
    InputEvent event = readInputs();

    switch (event) {
      case InputEvent::ENCODER_RIGHT:
        numRacers = min(numRacers + 1, numLanes);
        break;

      case InputEvent::ENCODER_LEFT:
        numRacers = max(numRacers - 1, 1);
        break;

      case InputEvent::BUTTON_START:
        currentMenu = MenuState::MENU_NUM_LAPS;
        if (numRacers < existingNumRacers) {
          // Re-init the extra cars and lanes that may have already been configured
          for (int i = numRacers; i < numLanes; ++i) {
            int laneToReinit = cars[i].lane - 1;
            if (cars[i].p_lane != nullptr) {
              configuredRacers--;
              initLane(laneToReinit);
            }
            initCar(i);
          }
          displayPolePosition();
          displayCarSelect();
        }
        return;

      case InputEvent::BUTTON_BACK:
        currentMenu = MenuState::MENU_OPTIONS;
        numRacers = existingNumRacers;
        return;

      case InputEvent::NONE:
        continue;

      default:
        continue;
    }

    if (numRacers != lastRacers) {
      updateDisplay(numRacers);
      lastRacers = numRacers;
    }
  }
}

// Menu Section to Specify Number of Laps
void munuNumLaps() {
  // --- Display helper ---
  auto updateDisplay = [](int index) {
    static bool firstRun = true;
    static unsigned long lastTick = 0;
    unsigned long now = millis();

    if (!firstRun && (now - lastTick > debounceTick)) {  // debounce tick sound
      playSdWav1.play("TICK.WAV");
      lastTick = now;
    }
    firstRun = false;

    lcd.setCursor(7, 1);
    lcd.print("   ");  // Clear area in case number shrinks
    lcd.setCursor(7, 1);
    lcd.print(index);
  };

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Number of Laps");
  updateDisplay(numLaps);

  int lastLaps = -1;
  int existingNumLaps = numLaps;
  while (true) {
    InputEvent event = readInputs();

    switch (event) {
      case InputEvent::ENCODER_RIGHT:
        numLaps = (numLaps + 1) > MAX_LAPS ? MIN_LAPS : numLaps + 1;
        break;

      case InputEvent::ENCODER_LEFT:
        numLaps = (numLaps - 1) < MIN_LAPS ? MAX_LAPS : numLaps - 1;
        break;

      case InputEvent::BUTTON_START:
        currentMenu = MenuState::MENU_CAR_NUM_LANE_ASSIGN;
        return;

      case InputEvent::BUTTON_BACK:
        currentMenu = MenuState::MENU_NUM_RACERS;
        numLaps = existingNumLaps;
        return;

      case InputEvent::NONE:
        continue;

      default:
        continue;
    }

    if (numLaps != lastLaps) {
      updateDisplay(numLaps);
      lastLaps = numLaps;
    }
  }
}

// Given a current lane number, it will translate that to a starting search index and find the next available lane searching in increasing lane order
int nextLaneUp(int startIndex = 0) {
  if (startIndex < 0 || startIndex >= numLanes) { startIndex = 0; }

  int searches = 0;
  for (int l = startIndex; searches < numLanes; l++) {
    if (lanes[l].p_car == NULL) { return lanes[l].number; }
    if (l >= numLanes - 1) { l = -1; }
    searches++;
  }
  return 0;
}

// Given a current lane number, it will translate that to a starting search index and find the next available lane searching in decreasing lane order
int nextLaneDown(int startIndex = 0) {
  if (startIndex <= 1 || startIndex > numLanes) { startIndex = numLanes + 1; }

  int searches = 0;
  for (int l = (startIndex - 2); searches < numLanes; l--) {
    if (lanes[l].p_car == NULL) { return lanes[l].number; }
    if (l <= 0) { l = numLanes; }
    searches++;
  }
  return 0;
}

// Helps find the next possible slot available for configuring a new car
int findFirstUnconfiguredCar() {
  for (int i = 0; i < numRacers; i++) {
    if (cars[i].p_lane == nullptr) {
      return i;  // Found first unconfigured car (no lane assigned yet)
    }
  }
  return 0;  // No unconfigured cars, just start at index 0
}

// Select which car to configure
void selectCar() {
  int numCars = numRacers;  // Total racers available
  carConfigIndex = findFirstUnconfiguredCar();

  // --- Helper for updating the display ---
  auto updateDisplay = [](int index, const char* extra) {
    static bool firstRun = true;
    static unsigned long lastTick = 0;
    unsigned long now = millis();

    if (!firstRun && (now - lastTick > debounceTick)) {  // debounce tick sound
      playSdWav1.play("TICK.WAV");
      lastTick = now;
    }
    firstRun = false;

    lcd.setCursor(1, 1);        // Reset cursor to position (1, 1)
    lcd.print(index);           // Print car number
    lcd.print(" ");             // Space for clearing any previous star
    lcd.setCursor(1 + String(index).length(), 1); // Position after the number

    if (extra[0] != '\0') {  // If there's a star, print it
      lcd.print(extra);
    }
  };

  lcd.clear();
  lcd.setCursor(1, 0);
  lcd.print("Select Car");

  // Menu loop – user is locked here until Start or Back is pressed
  int lastCar = -1;
  while (true) {
    // Check if the current car has already been configured and set the '*' accordingly
    const char* configured = (cars[carConfigIndex].p_lane != nullptr) ? "*" : "";
    updateDisplay(carConfigIndex + 1, configured);  // Display car with '*' if configured

    InputEvent event = readInputs();

    switch (event) {
      case InputEvent::ENCODER_RIGHT:
        carConfigIndex = (carConfigIndex + 1) % numCars;  // Wrap around forward
        break;

      case InputEvent::ENCODER_LEFT:
        carConfigIndex = (carConfigIndex - 1 + numCars) % numCars;  // Wrap around backward
        break;

      case InputEvent::BUTTON_START:
        currentMenu = MenuState::MENU_CAR_NUM_LANE_ASSIGN;
        if (cars[carConfigIndex].p_lane != nullptr) {
          initLane(cars[carConfigIndex].lane - 1); // If we hit back we also need to put the car/lane configuration back in place
          initCar(carConfigIndex);
          configuredRacers--; // We need to increment this _BACK UP_ again if we back out of the configuration now
          displayPolePosition(); // Flush the display from the lane that the user was previously configured in
          displayCarSelect();
        }
        return;

      case InputEvent::BUTTON_BACK:
        currentMenu = MenuState::MENU_NUM_LAPS;
        return;

      case InputEvent::BUTTON_STOP:
        if (cars[carConfigIndex].p_lane != nullptr) {
            lcd.clear();
            lcd.setCursor(0, 0);
            lcd.print("Car ");
            lcd.print(carConfigIndex + 1);
            lcd.print(" Lane ");
            lcd.print(cars[carConfigIndex].lane);
            lcd.print(" Num");
            centerTextCar(cars[carConfigIndex].number);
            lcd.print(carNames[cars[carConfigIndex].number]);
            delay(3000);
            currentMenu = MenuState::MENU_CAR_NUM_LANE_ASSIGN_BACK;
            return;
        }
        continue;

      case InputEvent::NONE:
        continue;

      default:
        continue;
    }

    if (carConfigIndex != lastCar) {
      const char* configured = (cars[carConfigIndex].p_lane != nullptr) ? "*" : "";
      updateDisplay(carConfigIndex + 1, configured);  // Update display with correct car number and '*' if configured
      lastCar = carConfigIndex;
    }
  }
}

// Select a lane to assign to the car being configured
void selectCarLane() {
  int curLane = nextLaneUp(0);  // Start with the first available lane

  // --- Helper for updating the display ---
  auto updateDisplay = [](int index) {
    static bool firstRun = true;
    static unsigned long lastTick = 0;
    unsigned long now = millis();

    if (!firstRun && (now - lastTick) > debounceTick) {  // debounce tick sound
      playSdWav1.play("TICK.WAV");
      lastTick = now;
    }
    firstRun = false;

    lcd.setCursor(7, 1);
    lcd.print(index);
  };

  lcd.clear();
  lcd.setCursor(1, 0);
  lcd.print("Select Lane");
  updateDisplay(curLane);

  // Menu loop – user is locked here until Start or Back is pressed
  int lastLane = -1;
  while (true) {
    InputEvent event = readInputs();

    switch (event) {
      case InputEvent::ENCODER_RIGHT:
        curLane = nextLaneUp(curLane);
        break;

      case InputEvent::ENCODER_LEFT:
        curLane = nextLaneDown(curLane);
        break;

      case InputEvent::BUTTON_START:
        // Commit lane assignment
        cars[carConfigIndex].lane = curLane;
        cars[carConfigIndex].position = curLane;
        displayCarSelect();
        currentMenu = MenuState::MENU_CAR_NUM_LANE_ASSIGN;
        return;

      case InputEvent::BUTTON_BACK:
        // Go back without committing
        currentMenu = MenuState::MENU_CAR_NUM_LANE_ASSIGN_BACK;
        return;

      case InputEvent::NONE:
        continue;

      default:
        continue;
    }

    if (curLane != lastLane) {
      updateDisplay(curLane);
      lastLane = curLane;
    }
  }
}

// Select the car number to assign to the car being configured
void selectCarNum() {
  int selectedCarIndex = 0;
  int numCars = sizeof(carNames) / sizeof(carNames[0]);  // This calculates the size of the carNames array

  auto updateDisplay = [](int index) {
    static bool firstRun = true;
    static unsigned long lastTick = 0;
    unsigned long now = millis();

    if (!firstRun && (now - lastTick) > debounceTick) {  // Debounce tick sound
      playSdWav1.play("TICK.WAV");
      lastTick = now;
    }
    firstRun = false;

    lcd.setCursor(0, 1);
    lcd.print("                ");  // Clear line
    centerTextCar(index);
    lcd.print(carNames[index]);
    displayPolePosition(cars[carConfigIndex].lane, index);
  };

  lcd.clear();
  lcd.setCursor(1, 0);
  lcd.print("Select Car Num");
  updateDisplay(selectedCarIndex);

  int lastIndex = -1;
  while (true) {
    InputEvent event = readInputs();

    switch (event) {
      case InputEvent::ENCODER_RIGHT:
        selectedCarIndex = (selectedCarIndex + 1) % numCars;  // This wraps the curIndex back around to 0 if it becomes larger than numCars
        break;
      case InputEvent::ENCODER_LEFT:
        selectedCarIndex = (selectedCarIndex - 1 + numCars) % numCars;  // This wraps the curIndex back around to max value if it becomes less than 0
        break;

      case InputEvent::BUTTON_START:
        // Commit the selected car number before moving on
        cars[carConfigIndex].number = selectedCarIndex;
        currentMenu = MenuState::MENU_CAR_NUM_LANE_ASSIGN;
        return;

      case InputEvent::BUTTON_BACK:
        // Don’t commit — just go back to previous menu
        currentMenu = MenuState::MENU_CAR_NUM_LANE_ASSIGN_BACK;
        return;

      case InputEvent::NONE:
        continue;

      default:
        continue;
    }

    if (selectedCarIndex != lastIndex) {
      updateDisplay(selectedCarIndex);
      lastIndex = selectedCarIndex;
    }
  }
}

// Centers the car's number on the LCD
void centerTextCar(int index) {
  String Car_Name = carNames[index];
  int centerVal = (16 - Car_Name.length()) / 2;
  lcd.setCursor(centerVal, 1);
}

// Centers the EEPROM Menu text on the LCD
void centerTextEEPROM(int index) {
  String EEPROM_Name = recReset[index];
  int centerVal = (16 - EEPROM_Name.length()) / 2;
  lcd.setCursor(centerVal, 1);
}

// Menu Section to Select Car Numbers per Lane
void munuCarNumLaneAssign() {
  lcd.clear();

  // --- Create a backup copy of the car array ---
  Car carsBackup[numLanes];
  memcpy(carsBackup, cars, sizeof(carsBackup));

  // --- Create a backup copy of the lane array ---
  Car lanesBackup[numLanes];
  memcpy(lanesBackup, lanes, sizeof(lanesBackup));

  // --- Make a backup of the number of configured racers ---
  int configuredRacersBackup = configuredRacers;

  // --- Step 1: Select the car to configure ---
  selectCar();
  if (currentMenu == MenuState::MENU_NUM_LAPS) {
    memcpy(cars, carsBackup, sizeof(carsBackup));
    memcpy(lanes, lanesBackup, sizeof(lanesBackup));
    displayPolePosition(cars[carConfigIndex].lane, cars[carConfigIndex].number);
    configuredRacers = configuredRacersBackup;
    displayCarSelect();
    return;  // User backed out
  } else if (currentMenu == MenuState::MENU_CAR_NUM_LANE_ASSIGN_BACK) {
    currentMenu = MenuState::MENU_CAR_NUM_LANE_ASSIGN;
    return;
  }

  // --- Step 2: Select the lane ---
  selectCarLane();
  if (currentMenu == MenuState::MENU_CAR_NUM_LANE_ASSIGN_BACK) {
    memcpy(cars, carsBackup, sizeof(carsBackup));
    memcpy(lanes, lanesBackup, sizeof(lanesBackup));
    displayPolePosition(cars[carConfigIndex].lane, cars[carConfigIndex].number);
    configuredRacers = configuredRacersBackup;
    displayCarSelect();
    return;
  }

  // --- Step 3: Select the car number/type ---
  selectCarNum();
  if (currentMenu == MenuState::MENU_CAR_NUM_LANE_ASSIGN_BACK) {
    memcpy(cars, carsBackup, sizeof(carsBackup));
    memcpy(lanes, lanesBackup, sizeof(lanesBackup));
    displayPolePosition(cars[carConfigIndex].lane, cars[carConfigIndex].number);
    configuredRacers = configuredRacersBackup;
    displayCarSelect();
    return;
  }

  // --- Display summary for confirmation ---
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Car ");
  lcd.print(carConfigIndex + 1);
  lcd.print(" Lane ");
  lcd.print(cars[carConfigIndex].lane);
  lcd.print(" Num");
  centerTextCar(cars[carConfigIndex].number);
  lcd.print(carNames[cars[carConfigIndex].number]);
  displayPolePosition(cars[carConfigIndex].lane, cars[carConfigIndex].number);

  // --- Confirmation loop ---
  while (true) {
    InputEvent event = readInputs();

    switch (event) {
      case InputEvent::BUTTON_START:
        {
          // Determine if this car was newly configured
          bool newlyConfiguredCar = cars[carConfigIndex].p_lane == nullptr;

          // Look for the lane struct that matches the lane number selected
          for (int l = 0; l < numLanes; l++) {
            if (lanes[l].number != cars[carConfigIndex].lane) { continue; }
            lanes[l].p_car = &cars[carConfigIndex];   // Set up the 2 car & lane objects to reference each other
            cars[carConfigIndex].p_lane = &lanes[l];  // Set up the 2 car & lane objects to reference each other
            break;
          }

          displayCarSelect();

          if (newlyConfiguredCar) configuredRacers++;

          // If all racers configured, advance to race start
          if (configuredRacers == numRacers) {
            qsort(cars, numLanes, sizeof(struct Car), cmpLaneOrder);  // Ensure cars sorted by lane
            currentMenu = MenuState::MENU_NONE;
            raceState = RaceState::START_PREPARE;
            return;
          }

          // Otherwise, continue configuring remaining racers
          currentMenu = MenuState::MENU_CAR_NUM_LANE_ASSIGN;
          return;
        }

      case InputEvent::BUTTON_BACK:
        currentMenu = MenuState::MENU_CAR_NUM_LANE_ASSIGN_BACK;  // Back out to previous menu
        memcpy(cars, carsBackup, sizeof(carsBackup));
        memcpy(lanes, lanesBackup, sizeof(lanesBackup));
        displayPolePosition(cars[carConfigIndex].lane, cars[carConfigIndex].number);
        configuredRacers = configuredRacersBackup;
        displayCarSelect();
        return;

      case InputEvent::NONE:
        continue;

      default:
        continue;
    }
  }  // end - while (true) {
}

// Inital Display of Car Numbers on 7 Segment Displays
// Display looks like:
// Row 1: 2-3
// Row 2: 0-1
// Row 3: 2-3
// Row 4: 0-1
void displayPolePosition(int lane_num, int index) {
  int default_car_index = (sizeof(carNumbers) / sizeof(carNumbers[0])) - 1;
  int numbersToShow[4] = {default_car_index, default_car_index, default_car_index, default_car_index};

  for (int lane = 1; lane <= numLanes; lane++) {
    if (lane == lane_num && index >= 0) {
        // Show the number currently being selected
        numbersToShow[lane - 1] = index;
    } else {
      // Find the car assigned to this lane
      for (int i = 0; i < numLanes; i++) {
        if (cars[i].lane == lane) {
          numbersToShow[lane - 1] = cars[i].number;
          break;
        }
      }
    }
  } // end - for (int lane = 1; lane <= numLanes; lane++) {

  // Row 1
  playerPolePositions[0].writeDigitAscii(2, carNumbers[numbersToShow[0]][0]);
  playerPolePositions[0].writeDigitAscii(3, carNumbers[numbersToShow[0]][1]);
  // Row 2
  playerPolePositions[0].writeDigitAscii(0, carNumbers[numbersToShow[1]][0]);
  playerPolePositions[0].writeDigitAscii(1, carNumbers[numbersToShow[1]][1]);
  // Row 3
  playerPolePositions[1].writeDigitAscii(2, carNumbers[numbersToShow[2]][0]);
  playerPolePositions[1].writeDigitAscii(3, carNumbers[numbersToShow[2]][1]);
  // Row 4
  playerPolePositions[1].writeDigitAscii(0, carNumbers[numbersToShow[3]][0]);
  playerPolePositions[1].writeDigitAscii(1, carNumbers[numbersToShow[3]][1]);

  playerPolePositions[0].writeDisplay();
  playerPolePositions[1].writeDisplay();
}

// Race Start LED Animation/Sounds and Penalty Monitoring
void startRace() {
  static int loopCounter = 0;
  switch (raceState) {
    case RaceState::START_PREPARE:
      FastLED.clear();
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Gentlemen, Start");
      lcd.setCursor(2, 1);
      lcd.print("Your Engines");
      LEDS.setBrightness(84);

      // Baseline all lane sensores before the race begins
      for (int l = 0; l < numLanes; l++) {
        lanes[l].currentState = digitalRead(lanes[l].monitorLap);
      }

      raceState = RaceState::START_LIGHTS_RED;
      stateEntered = true;
      break;

    unsigned long now;
    case RaceState::START_LIGHTS_RED:
      if (stateEntered) {
        // Turn on red lights
        FastLED.clear();
        for (int i = 0; i < NP_Boot_Transitions; i++) {
          leds[NP_Race_Start_Red[i]] = CHSV(NP_Boot_Colors[2], 255, 255);
          // Optional: staggered animation if desired
          // FastLED.show();  // Update strip each iteration
          // delay(10);
        }
        FastLED.show();
        playSdWav1.play("RED.WAV");

        // --- Enable lane power (racers now live but must hold steady) ---
        for (int i = 0; i < numLanes; i++) {
          digitalWrite(lanes[i].relay, HIGH);  // track power ON
        }

        stateStartTime = millis();
        stateEntered = false;  // Mark initialization complete
      }

      // --- False Start Detection (live while red) ---
      for (int l = 0; l < numLanes; l++) {
        int currentState = digitalRead(lanes[l].monitorLap);

        // Car crossed early?
        if (!lanes[l].hasPenalty && currentState == LOW) {
          lanes[l].hasPenalty = 1;
          lanes[l].penaltyStartTime = millis();
          digitalWrite(lanes[l].relay, HIGH); // Cut power to the lane
          playSdWav1.play("PENALTY.WAV");

          // Flash that lane red
          for (int i = 0; i < 3; i++) {
            leds[lanes[l].np[i]] = CHSV(NP_Boot_Colors[2], 255, 255);
          }
          FastLED.show();
        }

        // Remember current lane sensor state
        lanes[l].currentState = currentState;
      }

      // --- Run every loop until the timer expires ---
      if (millis() - stateStartTime > delayRedLight) {
        raceState = RaceState::START_LIGHTS_YELLOW;
        stateEntered = true;  // signal that next state is new
      }
      break;

      case RaceState::START_LIGHTS_YELLOW:
        if (stateEntered) {
          loopCounter = 0;
          stateStartTime = millis();
          stateEntered = false;
        }

        // --- Animate Yellow Lights Sequentially ---
        if ((millis() - stateStartTime > delayYellowLight) && loopCounter < 3) {
          for (int l = 0; l < numLanes; l++) {
            // Only show yellow light if lane is NOT under penalty
            if (!lanes[l].hasPenalty) {
              leds[lanes[l].np[loopCounter]] = CHSV(NP_Boot_Colors[1], 255, 255);
            }
          }

          playSdWav1.play("YELLOW.WAV");
          FastLED.show();

          loopCounter++;
          stateStartTime = millis();  // reset timer for next increment
        }

        // --- Continue watching for false starts during yellow phase ---
        for (int l = 0; l < numLanes; l++) {
          int currentState = digitalRead(lanes[l].monitorLap);

          if (!lanes[l].hasPenalty && currentState == LOW) {
            lanes[l].hasPenalty = 1;
            lanes[l].penaltyStartTime = millis();
            digitalWrite(lanes[l].relay, HIGH); // Cut power to the lanes
            playSdWav1.play("PENALTY.WAV");

            // Flash that lane red
            for (int i = 0; i < 3; i++) {
              leds[lanes[l].np[i]] = CHSV(NP_Boot_Colors[2], 255, 255);
            }
            FastLED.show();
          }
          lanes[l].currentState = currentState;
        }

        // --- After full yellow animation, move to green ---
        if ((millis() - stateStartTime > delayYellowLight) && loopCounter >= 3) {
          raceState = RaceState::START_LIGHTS_GREEN;
          stateEntered = true;
        }
        break;

    case RaceState::START_LIGHTS_GREEN:
      // Turn on green
      for (int i = 0; i < NP_Boot_Transitions; i++) {
        for (int l = 0; l < numLanes; l++) {
          if (!lanes[l].hasPenalty) {
            leds[lanes[l].np[i]] = CHSV(NP_Boot_Colors[0], 255, 255);
          }
        }
        // Optional: staggered animation if desired
        // FastLED.show();  // Update strip each iteration
        // delay(10);
      }

      playSdWav1.play("GREEN.WAV");
      FastLED.show();

      // Transition to active race
      raceState = RaceState::ACTIVE;
      stateEntered = true;

      // Set start time of the race
      now = millis();
      for (int c = 0; c < numRacers; c++) {
        cars[c].startTime = now;
      }

      // Blank out the display of all lanes
      for (int i = 0; i < numLanes; i++) {
        playerTimes[i].clear();
        playerTimes[i].writeDisplay();
      }
      break;

    default:
      break;
  }
}

// Temorarpy Pause of the Race
void pauseRace() {
  static bool showingPausedMsg = false;    // replaces Toggle_Race_Hazard
  static unsigned long blinkTimer = 0;     // replaces Time_Reference_Debounce

  unsigned long now = millis();

  // --- INITIAL ENTRY ---
  if (stateEntered) {
    // Cut power to all lanes
    for (int l = 0; l < numLanes; l++) {
      digitalWrite(lanes[l].relay, HIGH);
    }

    playSdWav1.play("PAUSE.WAV");
    pauseStartTime = now;  // This is the time the race was paused so it can be adjusted for the current lap
    blinkTimer = now;
    showingPausedMsg = false;

    lcd.clear();
    lcd.setCursor(2, 0);
    lcd.print("Race Paused!");

    // Set initial yellow LED flash
    FastLED.clear();
    for (int i = 0; i < NUM_LEDS; i++) {
      leds[i] = CHSV(NP_Boot_Colors[1], 255, 255);
    }
    FastLED.show();

    stateEntered = false;
  }

  // Flash Yellow Lights and display message on LCD
  if (now - blinkTimer > delayYellowLight) {
    FastLED.clear();
    lcd.clear();

    if (showingPausedMsg) {
      lcd.setCursor(2, 0);
      lcd.print("Press Start    "); // pad to overwrite old text
      lcd.setCursor(2, 1);
      lcd.print("to Continue   ");
    } else {
      lcd.setCursor(2, 0);
      lcd.print("Race Paused!   ");
      for (int i = 0; i < NUM_LEDS; i++) {
        leds[i] = CHSV(NP_Boot_Colors[1], 255, 255);
      }
    }

    FastLED.show();
    showingPausedMsg = !showingPausedMsg;
    blinkTimer = now;
  }

  // --- CHECK FOR RESUME BUTTON ---
  if (readButtonStart(false)) {
    lcd.clear();
    lcd.setCursor(2, 0);
    lcd.print("Resuming Race");
    FastLED.clear();

    // --- Replay start light sequence ---
    // Red lights
    for (int i = 0; i < NP_Boot_Transitions; i++) {
      leds[NP_Race_Start_Red[i]] = CHSV(NP_Boot_Colors[2], 255, 255);
      delay(10);
    }
    playSdWav1.play("PAUSE.WAV");
    FastLED.show();
    delay(delayRedLight);

    // Yellow sequence
    for (int i = 0; i < 3; i++) {
      playSdWav1.play("YELLOW.WAV");
      for (int l = 0; l < numLanes; l++) {
        leds[lanes[l].np[i]] = CHSV(NP_Boot_Colors[1], 255, 255);
      }
      FastLED.show();
      delay(delayYellowLight);
    }

    // Power restored + green lights
    for (int l = 0; l < numLanes; l++) {
      digitalWrite(lanes[l].relay, LOW);
    }

    for (int i = 0; i < NP_Boot_Transitions; i++) {
      playSdWav1.play("GREEN.WAV");
      for (int l = 0; l < numLanes; l++) {
        leds[lanes[l].np[i]] = CHSV(NP_Boot_Colors[0], 255, 255);
      }
    }

    FastLED.show();

    unsigned long pauseDuration = millis() - pauseStartTime;

    // Adjust each car's time forward so their times baseline stays aligned
    for (int c = 0; c < numRacers; c++) {
      cars[c].startTime += pauseDuration;
      cars[c].priorLapMillis += pauseDuration;
    }

    showingPausedMsg = false;
    blinkTimer = 0;

    // --- Transition back to active race ---
    raceState = RaceState::ACTIVE;
    stateEntered = true;
  }

  if (readButtonStop(false)) {
    raceState = RaceState::STOPPED;
    stateEntered = true;
    return;
  }
}

// Stops race completely, kills power to all lanes and resets unit for new race
void stopRace() {
  // Cut power to all Lanes
  for (int l = 0; l < numLanes; l++) {
    digitalWrite(lanes[l].relay, HIGH);
  }

  // Set all lights to Red
  for (int led = 0; led < NUM_LEDS; led++) {
    leds[led] = CHSV(NP_Boot_Colors[2], 255, 255);
    // Optional: staggered animation if desired
    // FastLED.show();  // Update strip each iteration
    // delay(10);
  }
  FastLED.show();

  lcd.clear();
  lcd.setCursor(3, 0);
  lcd.print("Race Ended");

  delay(delayStopRace/2); // Make this half as long as a standard race ending as the race was aborted

  // Fade out animation
  for (int b = 84; b >= 0; b -= 2) {
    FastLED.setBrightness(b);
    FastLED.show();
    delay(delayDim);
  }

  raceState = RaceState::CLEAR;  // Clear All Variables from previous race to prep for another
}

// Monitors All Race Attributes (called continuously during ACTIVE state)
void raceMetrics() {
  unsigned long now = millis();

  // --- One-time initialization when entering ACTIVE state ---
  if (stateEntered) {
    LEDS.setBrightness(NP_Brightness);
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Race In Progress");

    stateEntered = false;
  }

  // --- Handle penalties: restore power after delay ---
  bool penaltyRestored = false;
  for (int l = 0; l < numLanes; l++) {
    if (!lanes[l].hasPenalty) continue;
    if ((now - lanes[l].p_car->startTime) <= delayPenalty) continue;

    // Restore lane power
    digitalWrite(lanes[l].relay, LOW);
    lanes[l].hasPenalty = 0;

    // Restore LEDs to green
    for (int i = 0; i < NP_Boot_Transitions; i++) {
      leds[lanes[l].np[i]] = CHSV(NP_Boot_Colors[0], 255, 255);
    }

    penaltyRestored = true;
  }
  if (penaltyRestored) FastLED.show();

  // --- Sample lane sensors (lap counters) ---
  for (int l = 0; l < numLanes; l++) {
    lanes[l].previousState = lanes[l].currentState;
    lanes[l].currentState = digitalRead(lanes[l].monitorLap);
  }

  // --- Update racer metrics ---
  now = millis(); // Constant millis() for all the checks on all racers
  for (int c = 0; c < numRacers; c++) {
    const bool lapCrossed = (
      cars[c].p_lane->currentState == LOW &&
      cars[c].p_lane->currentState != cars[c].p_lane->previousState &&
      (
        cars[c].currentLap == 0 || // We need to ignore debounce when starting the race
        now > (cars[c].priorLapMillis + debounceTrack)
      )
    );

    // Calculate timings
    if (lapCrossed) {
      if (cars[c].currentLap == 1) {
        cars[c].lapTime = now - cars[c].startTime;
      } else if (cars[c].currentLap > 1) {
        cars[c].lapTime = now - cars[c].priorLapMillis;
      }

      cars[c].priorLapMillis = now;
      cars[c].totalTime = now - cars[c].startTime;
      cars[c].currentLap++;

      updateLapCounter();
      updateRacePositions();
    }

    // --- Check for new lap record ---
    if (cars[c].lapTime < recordLapTime && (cars[c].lapTime) > debounceTrack && currentLapNum > 1) { // We need to have completed at least the first lap
      recordLapTime = cars[c].lapTime;
      recordLapCarsIndex = c;
      recordLapCarNumbersIndex = cars[c].number;
      displayLapRecord();
    }

    if (cars[c].currentLap >= numLaps) {
      raceState = RaceState::END;
    }
  }
}

// Reads Lap Record from EEPROM and Displays on 7 Sgement Displays
void displayLapRecord() {
  char LapTimeRec_String[5];
  unsigned short LapTimeRec_Display = (recordLapTime > 99999 ? 99999 : recordLapTime) / 10;  // Limit the lap time we'll display to ##.## seconds from milliseconds
  sprintf(LapTimeRec_String, "%4hu", LapTimeRec_Display);

  // Write the car who has the lap record
  lapRecNum.writeDigitAscii(0, carNumbers[recordLapCarNumbersIndex][0]);
  lapRecNum.writeDigitAscii(1, carNumbers[recordLapCarNumbersIndex][1]);

  // Write the lap time record
  laptRecTime.writeDigitAscii(0, LapTimeRec_String[0]);
  laptRecTime.writeDigitAscii(1, LapTimeRec_String[1], true);
  laptRecTime.writeDigitAscii(2, LapTimeRec_String[2]);
  laptRecTime.writeDigitAscii(3, LapTimeRec_String[3]);

  // Display the lap time record and car number
  laptRecTime.writeDisplay();
  lapRecNum.writeDisplay();
}

// When new Lap Record is achieved it is written to EEPROM
void saveLapRecord(int carNumber) {
  writeLongEEPROM(0x02, recordLapTime);
  EEPROM.write(0x00, carNumber);
}

//Write Long to EEPROM
void writeLongEEPROM(int address, long value) {
  //Decomposition from a long to 4 bytes by using bitshift.
  //One = Most significant -> Four = Least significant byte
  byte four = (value & 0xFF);
  byte three = ((value >> 8) & 0xFF);
  byte two = ((value >> 16) & 0xFF);
  byte one = ((value >> 24) & 0xFF);

  //Write the 4 bytes into the eeprom memory.
  EEPROM.write(address, four);
  EEPROM.write(address + 1, three);
  EEPROM.write(address + 2, two);
  EEPROM.write(address + 3, one);
}

//Write Int to EEPROM (Used for Values from Options)
void writeIntEEPROM(int address, int value) {
  byte two = (value & 0xFF);
  byte one = ((value >> 8) & 0xFF);

  EEPROM.update(address, two);
  EEPROM.update(address + 1, one);
}

// Monitor Lap Number and Display on 7 Segment Display
void updateLapCounter() {
  if (numLaps >= currentLapNum) {
    int max_lap = 0;
    for (int c = 0; c < numRacers; c++) {
      max_lap = max(max_lap, cars[c].currentLap);
    }

    // If the current lap counter is less than the highest lap, clear the display
    if (currentLapNum < max_lap) {
      currentLapNum = max_lap;  // Update and the Lap counter
      char LapBuffer[2];
      dtostrf(currentLapNum, 2, 0, LapBuffer);  // Convert the Lap number individual char in an array and update lap count 7 segment displays
      if (currentLapNum >= 10) { lapRecNum.writeDigitAscii(2, LapBuffer[0]); }
      lapRecNum.writeDigitAscii(3, LapBuffer[1]);
      lapRecNum.writeDisplay();
    }
  }

  if (numLaps <= currentLapNum) {
    String finalLap = "FL";
    lapRecNum.clear();
    lapRecNum.writeDisplay();
    lapRecNum.writeDigitAscii(2, finalLap[0]);
    lapRecNum.writeDigitAscii(3, finalLap[1]);
    lapRecNum.writeDisplay();
    displayLapRecord();
  }
}

// Determine what place each car is in
void updateRacePositions() {
  // Sorts the cars based on how many laps completed and lowest total race time
  qsort(cars, numRacers, sizeof(struct Car), cmpLapAndTotalTime);

  // By those metrics we can now determine what place each car is in
  for (int i = 0; i < numRacers; i++) {
    cars[i].position = i + 1;
  }

  // Now we need to get back into lane order
  qsort(cars, numRacers, sizeof(struct Car), cmpLaneOrder);

  displayLeaderboard();
}

// Display Sorted Car Numbers and Lap times on Pole Position 7 Segmet Displays
void displayLeaderboard() {
  // Nothing to update if we're only on the first lap
  if (currentLapNum < 2) { return; }
  // Sorts the cars based on how many laps completed and lowest total race time
  qsort(cars, numRacers, sizeof(struct Car), cmpLapAndTotalTime);

  // Declare our lap time variables
  char playerLapTimesStrings[4][5];
  bool dp1 = false;
  bool dp2 = false;
  unsigned int lapDisplay = 0;

  // Write all the player lap times and pole positions
  for (int player_i = 0; player_i < numRacers; player_i++) {
    if (!cars[player_i].lapTime) {
      playerTimes[player_i].clear();
      playerTimes[player_i].writeDisplay();
      continue;
    }

    // Limit the lap time we'll display to ##.## or ###.# seconds from milliseconds
    if (cars[player_i].finished == 1) { // End of the race, display last final times before the reset
      lapDisplay = (cars[player_i].totalTime > 9999999 ? 9999999 : cars[player_i].totalTime);
    } else {
      lapDisplay = (cars[player_i].lapTime > 999999 ? 999999 : cars[player_i].lapTime);
    }

    if (lapDisplay < 100000) {         // < 100 sec → ##.##
      lapDisplay = lapDisplay / 10;
      dp1 = true;
      dp2 = false;
    } else if (lapDisplay < 1000000) { // < 1000 sec → ###.#
      lapDisplay = lapDisplay / 100;
      dp1 = false;
      dp2 = true;
    } else {                           // >= 1000 sec → ####
      lapDisplay = lapDisplay / 1000;
      dp1 = false;
      dp2 = false;
    }

    sprintf(playerLapTimesStrings[player_i], "%4hu", lapDisplay);

    // Write the player lap time to the ascii buffer
    playerTimes[player_i].writeDigitAscii(0, playerLapTimesStrings[player_i][0]);
    playerTimes[player_i].writeDigitAscii(1, playerLapTimesStrings[player_i][1], dp1);
    playerTimes[player_i].writeDigitAscii(2, playerLapTimesStrings[player_i][2], dp2);
    playerTimes[player_i].writeDigitAscii(3, playerLapTimesStrings[player_i][3]);

    // Write the player lap time and pole position to the display
    playerTimes[player_i].writeDisplay();
  }

  // Row 1
  playerPolePositions[0].writeDigitAscii(2, carNumbers[cars[0].number][0]);
  playerPolePositions[0].writeDigitAscii(3, carNumbers[cars[0].number][1]);
  // Row 2
  playerPolePositions[0].writeDigitAscii(0, carNumbers[cars[1].number][0]);
  playerPolePositions[0].writeDigitAscii(1, carNumbers[cars[1].number][1]);
  // Row 3
  playerPolePositions[1].writeDigitAscii(2, carNumbers[cars[2].number][0]);
  playerPolePositions[1].writeDigitAscii(3, carNumbers[cars[2].number][1]);
  // Row 4
  playerPolePositions[1].writeDigitAscii(0, carNumbers[cars[3].number][0]);
  playerPolePositions[1].writeDigitAscii(1, carNumbers[cars[3].number][1]);

  playerPolePositions[0].writeDisplay();
  playerPolePositions[1].writeDisplay();

  // Now we need to get back into lane order
  qsort(cars, numRacers, sizeof(struct Car), cmpLaneOrder);

  displayLapCountdown();
}

// Display The Correct Number of Laps LED Pattern
void displayLapCountdown() {
  for (int c = 0; c < numRacers; c++) {
    switch (numLaps - cars[c].currentLap) {
      case 1:  // 1 lap remaining
        leds[cars[c].p_lane->np[0]] = CRGB(0, 0, 0);
        leds[cars[c].p_lane->np[1]] = CHSV(NP_Boot_Colors[0], 255, 255);
        leds[cars[c].p_lane->np[2]] = CRGB(0, 0, 0);
        leds[cars[c].p_lane->np[3]] = CRGB(0, 0, 0);
        break;
      case 2:  // 2 laps remaining
        leds[cars[c].p_lane->np[0]] = CHSV(NP_Boot_Colors[0], 255, 255);
        leds[cars[c].p_lane->np[1]] = CRGB(0, 0, 0);
        leds[cars[c].p_lane->np[2]] = CHSV(NP_Boot_Colors[0], 255, 255);
        leds[cars[c].p_lane->np[3]] = CRGB(0, 0, 0);
        break;
      case 3:  // 3 laps remaining
        leds[cars[c].p_lane->np[0]] = CHSV(NP_Boot_Colors[0], 255, 255);
        leds[cars[c].p_lane->np[1]] = CHSV(NP_Boot_Colors[0], 255, 255);
        leds[cars[c].p_lane->np[2]] = CHSV(NP_Boot_Colors[0], 255, 255);
        leds[cars[c].p_lane->np[3]] = CRGB(0, 0, 0);
        break;
    }
  }

  FastLED.show();
}

// Final Lap and Finish Actions
void endRace() {
  static bool finishSoundPlayed = false;
  unsigned long now = millis();

  // Determine when a car is on its last lap
  for (int c = 0; c < numRacers; c++) {
    if (numLaps <= cars[c].currentLap && !cars[c].isOnLastLap) {
      if ((now - soundBuffer) >= 3500) {
        soundBuffer = now;
        playSdWav1.play("LASTLAP.WAV");
      }
      cars[c].isOnLastLap = 1;
      for (int i = 0; i < numLanes; i++) {
        leds[cars[c].p_lane->np[i]] = CRGB(255, 255, 255);
      }
    }
  }

  FastLED.show();

  // Action When a Car Finishes the Race
  for (int c = 0; c < numRacers; c++) {
    if (cars[c].currentLap <= numLaps || cars[c].finished != 0) { continue; }

    digitalWrite(cars[c].p_lane->relay, HIGH); // Cut Power to the Lane
    cars[c].finished = 1;

    // Quick blackout
    for (int i = 0; i < numLanes; i++) {
      leds[cars[c].p_lane->np[i]] = CRGB(0, 0, 0);
    }
    FastLED.show();
    delay(50);

    // Display The Correct Position LED Pattern
    switch (cars[c].position) {
      case 1:
        soundBuffer = now;
        leds[cars[c].p_lane->np[1]] = CRGB(0, 255, 255);
        break;
      case 2:
        if ((now - soundBuffer) >= 8500) {
          playSdWav1.play("RECORD.WAV");
          soundBuffer = now;
        }
        leds[cars[c].p_lane->np[0]] = CRGB(0, 255, 255);
        leds[cars[c].p_lane->np[2]] = CRGB(0, 255, 255);
        break;
      case 3:
      case 4:
        if ((now - soundBuffer) >= 8500) {
          playSdWav1.play("RECORD.WAV");
          soundBuffer = now;
        }
        for (int i = 0; i < cars[c].position; i++) {
          leds[cars[c].p_lane->np[i]] = CRGB(0, 255, 255);
        }
        break;
    }
  
    FastLED.show();
  }

  // Count all finished cars
  int carsFinished = 0;
  for (int c = 0; c < numRacers; c++) {
    if (cars[c].finished == 1) carsFinished++;
    if (cars[c].finished == 1 && now > (cars[c].startTime + cars[c].totalTime + delayFinalTimesDisplay) && !cars[c].totalDisplayed) {
      cars[c].totalDisplayed = true;
      displayLeaderboard();
    }
  }

  // When the first car crosses the finish line play the finish Song
  if (!finishSoundPlayed && carsFinished > 0) {
    playSdWav1.play("FINISH.WAV");
    finishSoundPlayed = true;
    soundBuffer = now;
  }

  // End Race After all Cars Cross the Finish Line
  if (carsFinished == numRacers) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Race Finished!  ");
    finishSoundPlayed = false;
    raceState = RaceState::CLEAR;
    stateEntered = true;

    delay(delayFinalTimesDisplay);
    displayLeaderboard();

    delay(delayStopRace);
  } else {
    raceState = RaceState::ACTIVE;
  }
}

// Reset all Variables and 7 Segment Displays from Previous Race and Record Lap Record
void clearRace() {
  // --- Save Race Record to EEPROM ---
  if (recordLapCarsIndex >= 0) { // Only if we actually completed a lap where a record was set
    saveLapRecord(cars[recordLapCarsIndex].number);
  }

  // --- Reset State Variables ---
  currentLapNum = 0;
  numLaps = 5;
  numRacers = 2;
  configuredRacers = 0;
  carConfigIndex = 0;
  recordLapCarsIndex = -1;
  pauseStartTime = 0;

  // Reset the rotary encoder values back to 0, including the static variables storing current encoder positional information
  myEnc.write(0);
  readInputs();

  // --- Clear LEDs ---
  FastLED.clear();
  FastLED.setBrightness(NP_Brightness);
  FastLED.show();

  // --- Re-initialize race entities ---
  initCars();
  initLanes();

  // --- Clear Leaderboard Display ---
  for (int player = 0; player < numLanes; player++) {
    playerTimes[player].clear();
    playerTimes[player].writeDisplay();
  }
  playerPolePositions[0].clear();
  playerPolePositions[0].writeDisplay();
  playerPolePositions[1].clear();
  playerPolePositions[1].writeDisplay();

  laptRecTime.clear();
  lapRecNum.clear();
  laptRecTime.writeDisplay();
  lapRecNum.writeDisplay();

  // --- Re-display lap record ---
  displayLapRecord();

  // --- Transition to configuration menu ---
  currentMenu = MenuState::MENU_NUM_RACERS;
  raceState = RaceState::NONE;
}
