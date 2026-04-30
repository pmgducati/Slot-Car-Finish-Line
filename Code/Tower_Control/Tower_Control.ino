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
Adafruit_AlphaNum4 Player_PolePositions[2] = { Adafruit_AlphaNum4(), Adafruit_AlphaNum4() };                                              // Pole Position Car Numbers 1 - 4 ((1 & 2) + (3 & 4))
Adafruit_AlphaNum4 Player_Times[4] = { Adafruit_AlphaNum4(), Adafruit_AlphaNum4(), Adafruit_AlphaNum4(), Adafruit_AlphaNum4() };          // Pole Position Time 1 - 4
Adafruit_AlphaNum4 LapRecNum = Adafruit_AlphaNum4();                                                                                      //Lap Counter and Lap Record Car Number
Adafruit_AlphaNum4 LapTimeRec = Adafruit_AlphaNum4();                                                                                     //Lap Record Time

// LCD Backpack Setup
Adafruit_LiquidCrystal lcd(1);  //default address #0 (A0-A2 not jumpered)

//Encoder Setup
Encoder myEnc(ENCODER_INCREMENT, ENCODER_DECREMENT);

// Variables
// Menu Navigation - The variables trigger a menu change when set as the currentMenu
enum class MenuState {
  MENU_WELCOME,
  MENU_OPTIONS,
  OPTIONS_END_RACE_RESET_DELAY,
  OPTIONS_RACE_PENALTY_TIMER,
  OPTIONS_TRACK_DEBOUNCE_TIMING,
  OPTIONS_CLEAR_LAP_RECORD,
  MENU_NUM_RACERS,
  MENU_NUM_LAPS,
  MENU_CAR_NUM_LANE_ASSIGN,
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
long Encoder_Position_Old = -999;
long Encoder_Position_New;

// Timing
unsigned long pauseStartTime = 0;      // Time (Milliseconds) to ajdust in the event the race is paused
unsigned long totalPauseDuration = 0;  // Total time paused for when multiple pauses happen

// Debounce timers
unsigned long debounceEncoder = 125;  // Debounce time (Milliseconds) for the Rotary Encoder
unsigned long debounceButton = 200;   // Debounce time (Milliseconds) for a Button Press
unsigned int debounceTrack = 1000;    // Default debounce time (Milliseconds) when a car passes the start line (Can be Modified in Options 500-10000 and saved to EEPROM)
unsigned long debounceTick = 150;     // Debounce time for preventing too many ticks when displaying new values selected via rotary encoder

// Race Identifiers
int Num_Laps = 5;            //Default number of laps in the Race (Can be Modified in Menu 5-99)
int MIN_LAPS = 5;            //Minimum number of laps in a race
int MAX_LAPS = 99;           //Maximum number of laps in a race
int Num_Lanes = 4;           //Max number of lanes on the race track
int Num_Racers = Num_Lanes;  //Default number of Racers in the Race (Can be Modified in Menu 1-4)
int Configured_Racers = 0;   //How many cars have been configured
int Car_Config_Index = 0;    //Tracks which car is having its number and lane assigned

// Race Information
int Current_Lap_Num = 0;           //Lap Count in Current Race
unsigned long Record_Lap = 99999;  //Default Lap Record Time (Actual is called from EEPROM)
int Record_Car_Num;                //Array Identifer of the record setting car
int Record_Car;                    //Lap Record Car Number (Value is called from EEPROM)
unsigned long sound_buffer;        //Time (Milliseconds) buffer to avoid sound stomping on eachother

// Struct (or class/object) that defines everything that a car needs to have
struct Car {
  int lane;                  // Lane the car is in
  struct Lane *p_lane;       // A pointer to the lane object the car is present in
  int number;                // The number that represents which car type is in use
  int cur_lap;               // Current lap the car is on
  int place;                 // Place the car is currently in
  unsigned long lap_time;    // The time (Milliseconds) of the last time
  unsigned long total_time;  // Total race time (Milliseconds)
  int last_lap;              // Flag to signal last lap Neopixel and Sound Events
  int finish;                // Has the car finished the race
};

// Declare our cars array and fill them in with default values in the loop
struct Car *cars = (Car *)malloc(Num_Lanes * sizeof *cars);

// Struct (or class/object) that defines everything that a lane needs to have
struct Lane {
  int number;                  // Lane number
  struct Car *p_car;           // A pointer to the car object present in the lane
  int np[4];                   // Lane LEDs array
  int relay;                   // Controls the Relay for the lane in the Control Box
  int monitor_lap;             // Lap counter switch for the lane
  int state;                   // In Track Lap Counter Monitors State, per lane
  int prev_state;              // In Track Lap Counter Monitors Previous State, per lane, prevents duplicate lap counting
  int penalty;                 // Flag if car crosses start line before the green light, per lane
  unsigned long penalty_time;  // The race time the penalty occurred (Milliseconds)
};

// Declare our lanes array and fill them in with default values in the loop
struct Lane *lanes = (Lane *)malloc(Num_Lanes * sizeof *lanes);

// Neopixel Variables
int NP_Brightness = 84;          // Set Neopixel Brightness

// Delay variables
int Delay_Start_Sequence = 100;  // Start Animation Speed (Higher = Slower)
int Delay_Dim = 50;              // Dimming Speed (Higher = Slower)
unsigned int Delay_Yellow_Light = 750;  // Delay between Yellow Lights
unsigned int Delay_Red_Light = 4250;    // Time for Red Lights
int Delay_Stop_Race = 10000;     // Default time (Milliseconds) for wait on race end before going back to Main Menu (Can be Modified in Options 1000-10000 and saved to EEPROM)
unsigned long Delay_Penalty = 5000;        // Default time (Milliseconds) for Penalty duration if a car crosses the track before green (Can be Modified in Options 500-5000 and saved to EEPROM)

// Menu Arrays
// Car Names and Numbers Displayed on LCD
String Car_Names[10] = { "01 Skyline", "03 Ford Capri", "05 BMW 3.5 CSL", "05 Lancia LC2", "33 Audi RS5", "51 Porsche 935", "576 Lancia Beta", "80 BMW M1", "88 BTTF Delorean", "MM GT Falcon V8" };
// Car Numbers on Displayed on Pole Position and Lap Record 7 Segment
String Car_Numbers[11] = { "01", "03", "05", "05", "33", "51", "57", "80", "88", "MM", "--" };
// Menu selection for Erasing EEPROM
String Rec_Reset[20] = { "NO", "X", "XXX", "X", "XXX", "X", "XXX", "X", "XXX", "X", "YES", "X", "XXX", "X", "XXX", "X", "XXX", "X", "XXX", "X" };

// --- Function Declarations (Prototypes) ---
void Options(bool reset);
void Pole_Pos_Display(int lane_num = -1, int index = -1);
void LapRecord(int carNumber = (sizeof(Car_Numbers) / sizeof(Car_Numbers[0])) - 1);

// Function to help qsort cars in place order
int cmp_lap_and_total_time(const void *left, const void *right) {
  struct Car *a = (struct Car *)left;
  struct Car *b = (struct Car *)right;

  if (b->cur_lap < a->cur_lap) {
    return -1;
  } else if (a->cur_lap < b->cur_lap) {
    return 1;
  } else {
    return (b->total_time < a->total_time) - (a->total_time < b->total_time);
  }
}

// Function to help qsort cars in lane order
int lane_order(const void *left, const void *right) {
  struct Car *a = (struct Car *)left;
  struct Car *b = (struct Car *)right;

  if (a->lane < b->lane) {
    return -1;
  }
  return 1;
}

// Read Long from EEPROM (Lap Time)
long EEPROMReadlong(long address) {
  //Read the 4 bytes from the eeprom memory.
  long four = EEPROM.read(address);
  long three = EEPROM.read(address + 1);
  long two = EEPROM.read(address + 2);
  long one = EEPROM.read(address + 3);

  //Return the recomposed long by using bitshift.
  return ((four << 0) & 0xFF) + ((three << 8) & 0xFFFF) + ((two << 16) & 0xFFFFFF) + ((one << 24) & 0xFFFFFFFF);
}

// Read Int from EEPROM (Variables for Options)
int EEPROMReadInt(int address) {
  long two = EEPROM.read(address);
  long one = EEPROM.read(address + 1);

  //Return the recomposed long by using bitshift
  return ((two << 0) & 0xFFFFFF) + ((one << 8) & 0xFFFFFFFF);
}

// Reads the start button and won't exit the function until the user stops pressing it, still takes into account debounce
int ReadButtonStart(bool waitForRelease = true) {
  int buttonPressed = 0;  // Was the button pressed at all
  int buttonStatus = 0;   // The current status of the button

  unsigned long currentPressTime = 0;
  static unsigned long previousPressTime = 0;  // Holds last press time for debounce

  do {
    buttonStatus = digitalRead(PIN_BUTTON_START);
    currentPressTime = millis();

    if (buttonStatus == HIGH && buttonPressed == 0) {
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
int ReadButtonBack(bool waitForRelease = true) {
  int buttonPressed = 0;  // Was the button pressed at all
  int buttonStatus = 0;   // The current status of the button

  unsigned long currentPressTime = 0;
  static unsigned long previousPressTime = 0;  // Holds last press time for debounce

  do {
    buttonStatus = digitalRead(PIN_BUTTON_BACK);
    currentPressTime = millis();

    if (buttonStatus == HIGH && buttonPressed == 0) {
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
int ReadButtonStop(bool waitForRelease = true) {
  int buttonPressed = 0;  // Was the button pressed at all
  int buttonStatus = 0;   // The current status of the button

  unsigned long currentPressTime = 0;
  static unsigned long previousPressTime = 0;  // Holds last press time for debounce

  do {
    buttonStatus = digitalRead(PIN_BUTTON_STOP);
    currentPressTime = millis();

    if (buttonStatus == HIGH && buttonPressed == 0) {
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

void Rotary_Encoder() {
  static long lastPosition = 0;
  long position = myEnc.read() / 4;

  if (position != lastPosition) {
    lastPosition = position;
    Encoder_Position_New = position;
  } else {
    lastPosition = position;
  }
}

// Generic Input reader
InputEvent readInputs(bool waitForRelease = true) {
  static long lastEncoderPos = 0;

  // --- 1️⃣ Handle Rotary Encoder ---
  Rotary_Encoder();  // Updates Encoder_Position_New
  if (Encoder_Position_New > lastEncoderPos) {
    lastEncoderPos = Encoder_Position_New;
    return InputEvent::ENCODER_RIGHT;
  } else if (Encoder_Position_New < lastEncoderPos) {
    lastEncoderPos = Encoder_Position_New;
    return InputEvent::ENCODER_LEFT;
  }

  // --- 2️⃣ Handle Buttons ---
  if (ReadButtonStart(waitForRelease) == 1) {
    return InputEvent::BUTTON_START;
  }
  if (ReadButtonBack(waitForRelease) == 1) {
    return InputEvent::BUTTON_BACK;
  }

  if (ReadButtonStop(waitForRelease) == 1) {
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
  cars[i].cur_lap = 0;
  cars[i].place = i + 1;
  cars[i].lap_time = 0;
  cars[i].prior_lap_ms = 0;  // millis() timestamp of the last completed lap to calculate lap times
  cars[i].total_time = 0;
  cars[i].start_time = 0;
  cars[i].last_lap = 0;
  cars[i].finish = 0;
  cars[i].total_displayed = false;
}

// Initialize all car objects to default starting values
void initCars() {
  for (int i = 0; i < Num_Lanes; ++i) {
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
  lanes[i].monitor_lap = MONITOR_PINS[i];
  lanes[i].state = -1;
  lanes[i].prev_state = -1;
  lanes[i].penalty = 0;
  lanes[i].penalty_time = 0;
}

// Initialize all lane objects to default starting values
void initLanes() {
  for (int i = 0; i < Num_Lanes; ++i) {
    initLane(i);
  }
}

void setup() {
  // Start Serial Monitor
  Serial.begin(9600);

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
  Record_Lap = EEPROMReadlong(0x02);
  Record_Car = EEPROM.read(0x00);
  Delay_Penalty = EEPROMReadInt(0x08);
  Delay_Stop_Race = EEPROMReadInt(0x06);
  debounceTrack = EEPROMReadInt(0x10);

  // Set up the 7-Segment LED Panels
  Player_PolePositions[0].begin(0x70);  // Lanes 1 & 2
  Player_PolePositions[1].begin(0x71);  // Lanes 3 & 4
  LapRecNum.begin(0x72);                // Pass in the address for the Lap Counter and Lap Record Car Number
  LapTimeRec.begin(0x77);               // Pass in the address for the Lap Record Time
  Player_Times[0].begin(0x73);          // Pass in the address for the Place 1 Lap Time
  Player_Times[1].begin(0x74);          // Pass in the address for the Place 2 Lap Time
  Player_Times[2].begin(0x75);          // Pass in the address for the Place 3 Lap Time
  Player_Times[3].begin(0x76);          // Pass in the address for the Place 4 Lap Time

  // Set up the LCD's number of rows and columns and enable the backlight
  lcd.begin(16, 2);
  lcd.setBacklight(HIGH);

  // Pin Mode Assignments
  pinMode(PIN_BUTTON_RE, INPUT);
  pinMode(PIN_BUTTON_BACK, INPUT);
  pinMode(PIN_BUTTON_START, INPUT);
  pinMode(PIN_BUTTON_STOP, INPUT);
  pinMode(LED_NOTIFICATION, OUTPUT);

  // Set the monitor_lap and relay pin modes and disable the lanes before the race
  for (int l = 0; l < Num_Lanes; l++) {
    pinMode(lanes[l].monitor_lap, INPUT_PULLUP);
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
      Welcome_Message();
      break;

    case MenuState::MENU_OPTIONS:
      Options(true);
      break;

    case MenuState::OPTIONS_END_RACE_RESET_DELAY:
      Option_Stop_Race();
      break;

    case MenuState::OPTIONS_RACE_PENALTY_TIMER:
      Option_Penalty();
      break;

    case MenuState::OPTIONS_TRACK_DEBOUNCE_TIMING:
      Option_Debounce_Track();
      break;

    case MenuState::OPTIONS_CLEAR_LAP_RECORD:
      Option_Clear_Record_Lap();
      break;

    case MenuState::MENU_NUM_RACERS:
      Number_of_Racers();
      break;

    case MenuState::MENU_NUM_LAPS:
      Number_of_Laps();
      break;

    case MenuState::MENU_CAR_NUM_LANE_ASSIGN:
      Car_Num_Lane_Assign();
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
      Start_Race();
      break;

    // --- ACTIVE RACE LOGIC ---
    case RaceState::ACTIVE: {
      // --- Handle top-level control inputs ---
      if (ReadButtonStart(false)) {
        raceState = RaceState::PAUSED;
        stateEntered = true;
        break;
      }

      if (ReadButtonStop(false)) {
        raceState = RaceState::STOPPED;
        stateEntered = true;
        break;
      }

      // --- Core race logic tick ---
      Race_Metrics();
      break;
    }

    // --- PAUSED STATE ---
    case RaceState::PAUSED:
      Pause_Race();
      break;

    // --- STOPPED / ABORTED ---
    case RaceState::STOPPED:
      Stop_Race();
      break;

    // --- RACE END SEQUENCE ---
    case RaceState::END:
    case RaceState::END_CHECK:
    case RaceState::END_RESULTS:
      End_Race();
      break;

    // --- CLEARING AND RESETTING ---
    case RaceState::CLEAR:
      Clear_Race();
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
void Welcome_Message() {
  lcd.setCursor(3, 0);
  lcd.print("Welcome to");
  lcd.setCursor(3, 1);
  lcd.print("the Race!");
  playSdWav1.play("WELCOME.WAV");
  LapRecordDisplay();

  // Intro Green/Yellow/Red/Blue Neopixel Animation
  for (int j = 0; j < NP_Boot_Transitions; j++) {
    for (int i = 0; i < NUM_LEDS; i++) {  // Left Chase Animation
      leds[NP_Boot_Pattern[i]] = CHSV(NP_Boot_Colors[j], 255, 255);
      i++;
      leds[NP_Boot_Pattern[i]] = CHSV(NP_Boot_Colors[j], 255, 255);
      FastLED.show();
      delay(Delay_Start_Sequence);
    }
    j++;
    for (int i = 15; i > 0; i--) {  // Right Chase Animation
      leds[NP_Boot_Pattern[i]] = CHSV(NP_Boot_Colors[j], 255, 255);
      i--;
      leds[NP_Boot_Pattern[i]] = CHSV(NP_Boot_Colors[j], 255, 255);
      FastLED.show();
      delay(Delay_Start_Sequence);
    }
  }

  // Fade out Animation
  for (int b = 84; b >= 0; b -= 2) {
    LEDS.setBrightness(b);
    FastLED.show();
    delay(Delay_Dim);
  }

  // Setup for next menu
  currentMenu = MenuState::MENU_NUM_RACERS;
  lcd.clear();
}

// --- Options Main Menu ---
void Options(bool reset = false) {
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
void Option_Stop_Race() {
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
  updateDisplay(Delay_Stop_Race);

  // --- Menu loop ---
  int lastValue = -1;
  while (true) {
    InputEvent event = readInputs();

    switch (event) {
      case InputEvent::ENCODER_RIGHT:
        Delay_Stop_Race = min(Delay_Stop_Race + 500, 10000);
        break;

      case InputEvent::ENCODER_LEFT:
        Delay_Stop_Race = max(Delay_Stop_Race - 500, 1000);
        break;

      case InputEvent::BUTTON_START:
        // Save and return to options menu
        EEPROMWriteInt(0x06, Delay_Stop_Race);
        currentMenu = MenuState::MENU_OPTIONS;
        return;

      case InputEvent::BUTTON_BACK:
        // Cancel and return without saving
        currentMenu = MenuState::MENU_OPTIONS;
        return;

      case InputEvent::NONE:
        continue;

      default:
        continue;
    }

    // --- Only update when changed ---
    if (Delay_Stop_Race != lastValue) {
      updateDisplay(Delay_Stop_Race);
      lastValue = Delay_Stop_Race;
    }
  }
}

// Penalty Value Selection and Set
void Option_Penalty() {
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
  updateDisplay(Delay_Penalty);

  // --- Menu loop ---
  unsigned long lastValue = -1;
  while (true) {
    InputEvent event = readInputs();

    switch (event) {
      case InputEvent::ENCODER_RIGHT:
        Delay_Penalty = min(Delay_Penalty + 500u, 5000u);
        break;

      case InputEvent::ENCODER_LEFT:
        Delay_Penalty = max(Delay_Penalty - 500u, 500u);
        break;

      case InputEvent::BUTTON_START:
        // Save and return to main options
        EEPROMWriteInt(0x10, Delay_Penalty);
        currentMenu = MenuState::MENU_OPTIONS;
        return;

      case InputEvent::BUTTON_BACK:
        // Return without saving
        currentMenu = MenuState::MENU_OPTIONS;
        return;

      case InputEvent::NONE:
        continue;

      default:
        continue;
    }

    // --- Update LCD only on change ---
    if (Delay_Penalty != lastValue) {
      updateDisplay(Delay_Penalty);
      lastValue = Delay_Penalty;
    }
  }
}

// Track Debounce Value Selection and Set
void Option_Debounce_Track() {
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
        EEPROMWriteInt(0x08, debounceTrack);
        currentMenu = MenuState::MENU_OPTIONS;
        return;

      case InputEvent::BUTTON_BACK:
        // Return without saving
        currentMenu = MenuState::MENU_OPTIONS;
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
void Option_Clear_Record_Lap() {
  static int selectedIndex = 0;
  const int maxIndex = 19;

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
    Center_Text_EEPROM(index);
    lcd.print(Rec_Reset[index]);
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
        if (selectedIndex == 10) {  // “YES” — clear lap record
          Record_Lap = 99999;
          Record_Car_Num = 10;
          Record_Car = 10;
          LapRecord();
          LapRecordDisplay();
        }
        // In both YES/NO cases, return to Options
        currentMenu = MenuState::MENU_OPTIONS;
        return;

      case InputEvent::BUTTON_BACK:
        // Back out without clearing
        currentMenu = MenuState::MENU_OPTIONS;
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

// Menu Section to Specify Number of Racers
void Number_of_Racers() {
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
  updateDisplay(Num_Racers);

  // --- Menu loop – user is locked here until Start or Back is pressed ---
  int lastRacers = -1;
  while (true) {
    InputEvent event = readInputs();

    switch (event) {
      case InputEvent::ENCODER_RIGHT:
        Num_Racers = min(Num_Racers + 1, Num_Lanes);
        break;

      case InputEvent::ENCODER_LEFT:
        Num_Racers = max(Num_Racers - 1, 1);
        break;

      case InputEvent::BUTTON_START:
        currentMenu = MenuState::MENU_NUM_LAPS;
        return;

      case InputEvent::BUTTON_BACK:
        currentMenu = MenuState::MENU_OPTIONS;
        return;

      case InputEvent::NONE:
        continue;

      default:
        continue;
    }

    if (Num_Racers != lastRacers) {
      updateDisplay(Num_Racers);
      lastRacers = Num_Racers;
    }
  }
}

// Menu Section to Specify Number of Laps
void Number_of_Laps() {
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
  updateDisplay(Num_Laps);

  int lastLaps = -1;
  while (true) {
    InputEvent event = readInputs();

    switch (event) {
      case InputEvent::ENCODER_RIGHT:
        Num_Laps = (Num_Laps + 1) > MAX_LAPS ? MIN_LAPS : Num_Laps + 1;
        break;

      case InputEvent::ENCODER_LEFT:
        Num_Laps = (Num_Laps - 1) < MIN_LAPS ? MAX_LAPS : Num_Laps - 1;
        break;

      case InputEvent::BUTTON_START:
        currentMenu = MenuState::MENU_CAR_NUM_LANE_ASSIGN;
        return;

      case InputEvent::BUTTON_BACK:
        currentMenu = MenuState::MENU_NUM_RACERS;
        return;

      case InputEvent::NONE:
        continue;

      default:
        continue;
    }

    if (Num_Laps != lastLaps) {
      updateDisplay(Num_Laps);
      lastLaps = Num_Laps;
    }
  }
}

// Given a current lane number, it will translate that to a starting search index and find the next available lane searching in increasing lane order
int next_lane_up(int start_index = 0) {
  if (start_index < 0 || start_index >= Num_Lanes) { start_index = 0; }

  int searches = 0;
  for (int l = start_index; searches < Num_Lanes; l++) {
    if (lanes[l].p_car == NULL) { return lanes[l].number; }
    if (l >= Num_Lanes - 1) { l = -1; }
    searches++;
  }
  return 0;
}

// Given a current lane number, it will translate that to a starting search index and find the next available lane searching in decreasing lane order
int next_lane_down(int start_index = 0) {
  if (start_index <= 1 || start_index > Num_Lanes) { start_index = Num_Lanes + 1; }

  int searches = 0;
  for (int l = (start_index - 2); searches < Num_Lanes; l--) {
    if (lanes[l].p_car == NULL) { return lanes[l].number; }
    if (l <= 0) { l = Num_Lanes; }
    searches++;
  }
  return 0;
}

// Helps find the next possible slot available for configuring a new car
int findFirstUnconfiguredCar() {
  for (int i = 0; i < Num_Lanes; i++) {
    if (cars[i].p_lane == nullptr) {
      return i;  // Found first unconfigured car (no lane assigned yet)
    }
  }
  return 0;  // No unconfigured cars, just start at index 0
}

// Select which car to configure
void Select_Car() {
  int numCars = Num_Racers;  // Total racers available
  Car_Config_Index = findFirstUnconfiguredCar();

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
    const char* configured = (cars[Car_Config_Index].p_lane != nullptr) ? "*" : "";
    updateDisplay(Car_Config_Index + 1, configured);  // Display car with '*' if configured

    InputEvent event = readInputs();

    switch (event) {
      case InputEvent::ENCODER_RIGHT:
        Car_Config_Index = (Car_Config_Index + 1) % numCars;  // Wrap around forward
        break;

      case InputEvent::ENCODER_LEFT:
        Car_Config_Index = (Car_Config_Index - 1 + numCars) % numCars;  // Wrap around backward
        break;

      case InputEvent::BUTTON_START:
        currentMenu = MenuState::MENU_CAR_NUM_LANE_ASSIGN;
        return;

      case InputEvent::BUTTON_BACK:
        currentMenu = MenuState::MENU_NUM_LAPS;
        return;

      case InputEvent::BUTTON_STOP:
        if (cars[Car_Config_Index].p_lane != nullptr) {
            lcd.clear();
            lcd.setCursor(0, 0);
            lcd.print("Car ");
            lcd.print(Car_Config_Index + 1);
            lcd.print(" Lane ");
            lcd.print(cars[Car_Config_Index].lane);
            lcd.print(" Num");
            Center_Text_Car(cars[Car_Config_Index].number);
            lcd.print(Car_Names[cars[Car_Config_Index].number]);
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

    if (Car_Config_Index != lastCar) {
      const char* configured = (cars[Car_Config_Index].p_lane != nullptr) ? "*" : "";
      updateDisplay(Car_Config_Index + 1, configured);  // Update display with correct car number and '*' if configured
      lastCar = Car_Config_Index;
    }
  }
}

// Select a lane to assign to the car being configured
void Select_Car_Lane() {
  int curLane = next_lane_up(0);  // Start with the first available lane

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
        curLane = next_lane_up(curLane);
        break;

      case InputEvent::ENCODER_LEFT:
        curLane = next_lane_down(curLane);
        break;

      case InputEvent::BUTTON_START:
        // Commit lane assignment
        cars[Car_Config_Index].lane = curLane;
        cars[Car_Config_Index].place = curLane;
        currentMenu = MenuState::MENU_CAR_NUM_LANE_ASSIGN;
        return;

      case InputEvent::BUTTON_BACK:
        // Go back without committing
        currentMenu = MenuState::MENU_NUM_LAPS;
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
void Select_Car_Num() {
  int selectedCarIndex = 0;
  int numCars = sizeof(Car_Names) / sizeof(Car_Names[0]);  // This calculates the size of the Car_Names array

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
    Center_Text_Car(index);
    lcd.print(Car_Names[index]);
    Pole_Pos_Display(cars[Car_Config_Index].lane, index);
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
        cars[Car_Config_Index].number = selectedCarIndex;
        currentMenu = MenuState::MENU_CAR_NUM_LANE_ASSIGN;
        return;

      case InputEvent::BUTTON_BACK:
        // Don’t commit — just go back to previous menu
        currentMenu = MenuState::MENU_NUM_LAPS;
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
void Center_Text_Car(int index) {
  String Car_Name = Car_Names[index];
  int centerVal = (16 - Car_Name.length()) / 2;
  lcd.setCursor(centerVal, 1);
}

// Centers the EEPROM Menu text on the LCD
void Center_Text_EEPROM(int index) {
  String EEPROM_Name = Rec_Reset[index];
  int centerVal = (16 - EEPROM_Name.length()) / 2;
  lcd.setCursor(centerVal, 1);
}

// Menu Section to Select Car Numbers per Lane
void Car_Num_Lane_Assign() {
  lcd.clear();

  // --- Create a backup copy of the car array ---
  Car cars_backup[Num_Lanes];
  memcpy(cars_backup, cars, sizeof(cars_backup));

  // --- Step 1: Select the car to configure ---
  Select_Car();
  if (currentMenu == MenuState::MENU_NUM_LAPS) {
    memcpy(cars, cars_backup, sizeof(cars_backup));
    return;  // User backed out
  }

  // --- Step 2: Select the lane ---
  Select_Car_Lane();
  if (currentMenu == MenuState::MENU_NUM_LAPS) {
    memcpy(cars, cars_backup, sizeof(cars_backup));
    return;
  }

  // --- Step 3: Select the car number/type ---
  Select_Car_Num();
  if (currentMenu == MenuState::MENU_NUM_LAPS) {
    memcpy(cars, cars_backup, sizeof(cars_backup));
    return;
  }

  // --- Display summary for confirmation ---
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Car ");
  lcd.print(Car_Config_Index + 1);
  lcd.print(" Lane ");
  lcd.print(cars[Car_Config_Index].lane);
  lcd.print(" Num");
  Center_Text_Car(cars[Car_Config_Index].number);
  lcd.print(Car_Names[cars[Car_Config_Index].number]);
  Pole_Pos_Display(cars[Car_Config_Index].lane, cars[Car_Config_Index].number);

  // --- Confirmation loop ---
  while (true) {
    InputEvent event = readInputs();

    switch (event) {
      case InputEvent::BUTTON_START:
        {
          // Determine if this car was newly configured
          bool newly_configured_car = cars[Car_Config_Index].p_lane == nullptr;

          // Look for the lane struct that matches the lane number selected
          for (int l = 0; l < Num_Lanes; l++) {
            if (lanes[l].number != cars[Car_Config_Index].lane) { continue; }
            lanes[l].p_car = &cars[Car_Config_Index];   // Set up the 2 car & lane objects to reference each other
            cars[Car_Config_Index].p_lane = &lanes[l];  // Set up the 2 car & lane objects to reference each other
            break;
          }

          if (newly_configured_car) {
            Configured_Racers++;
          }

          // If all racers configured, advance to race start
          if (Configured_Racers == Num_Racers) {
            qsort(cars, Num_Lanes, sizeof(struct Car), lane_order);  // Ensure cars sorted by lane
            currentMenu = MenuState::MENU_NONE;
            raceState = RaceState::START_PREPARE;
            return;
          }

          // Otherwise, continue configuring remaining racers
          currentMenu = MenuState::MENU_CAR_NUM_LANE_ASSIGN;
          return;
        }

      case InputEvent::BUTTON_BACK:
        currentMenu = MenuState::MENU_NUM_LAPS;  // Back out to previous menu
        memcpy(cars, cars_backup, sizeof(cars_backup));
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
void Pole_Pos_Display(int lane_num, int index) {
  int default_car_index = (sizeof(Car_Numbers) / sizeof(Car_Numbers[0])) - 1;
  int numbersToShow[4] = {default_car_index, default_car_index, default_car_index, default_car_index};

  for (int lane = 1; lane <= Num_Lanes; lane++) {
    if (lane == lane_num && index >= 0) {
        // Show the number currently being selected
        numbersToShow[lane - 1] = index;
    } else {
      // Find the car assigned to this lane
      for (int i = 0; i < Num_Lanes; i++) {
        if (cars[i].lane == lane) {
          numbersToShow[lane - 1] = cars[i].number;
          break;
        }
      }
    }
  } // end - for (int lane = 1; lane <= Num_Lanes; lane++) {

  // Row 1
  Player_PolePositions[0].writeDigitAscii(2, Car_Numbers[numbersToShow[0]][0]);
  Player_PolePositions[0].writeDigitAscii(3, Car_Numbers[numbersToShow[0]][1]);
  // Row 2
  Player_PolePositions[0].writeDigitAscii(0, Car_Numbers[numbersToShow[1]][0]);
  Player_PolePositions[0].writeDigitAscii(1, Car_Numbers[numbersToShow[1]][1]);
  // Row 3
  Player_PolePositions[1].writeDigitAscii(2, Car_Numbers[numbersToShow[2]][0]);
  Player_PolePositions[1].writeDigitAscii(3, Car_Numbers[numbersToShow[2]][1]);
  // Row 4
  Player_PolePositions[1].writeDigitAscii(0, Car_Numbers[numbersToShow[3]][0]);
  Player_PolePositions[1].writeDigitAscii(1, Car_Numbers[numbersToShow[3]][1]);

  Player_PolePositions[0].writeDisplay();
  Player_PolePositions[1].writeDisplay();
}

// Race Start LED Animation/Sounds and Penalty Monitoring
void Start_Race() {
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
      for (int l = 0; l < Num_Lanes; l++) {
        lanes[l].state = digitalRead(lanes[l].monitor_lap);
      }

      raceState = RaceState::START_LIGHTS_RED;
      stateEntered = true;
      break;

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
        for (int i = 0; i < Num_Lanes; i++) {
          digitalWrite(lanes[i].relay, HIGH);  // track power ON
        }

        stateStartTime = millis();
        stateEntered = false;  // Mark initialization complete
      }

      // --- False Start Detection (live while red) ---
      for (int l = 0; l < Num_Lanes; l++) {
        int currentState = digitalRead(lanes[l].monitor_lap);

        // Car crossed early?
        if (lanes[l].penalty == 0 && currentState == LOW) {
          lanes[l].penalty = 1;
          lanes[l].penalty_time = millis();
          digitalWrite(lanes[l].relay, HIGH); // Cut power to the lane
          playSdWav1.play("PENALTY.WAV");

          // Flash that lane red
          for (int i = 0; i < 3; i++) {
            leds[lanes[l].np[i]] = CHSV(NP_Boot_Colors[2], 255, 255);
          }
          FastLED.show();
        }

        // Remember current lane sensor state
        lanes[l].state = currentState;
      }

      // --- Run every loop until the timer expires ---
      if (millis() - stateStartTime > Delay_Red_Light) {
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
        if ((millis() - stateStartTime > Delay_Yellow_Light) && loopCounter < 3) {
          for (int l = 0; l < Num_Lanes; l++) {
            // Only show yellow light if lane is NOT under penalty
            if (lanes[l].penalty == 0) {
              leds[lanes[l].np[loopCounter]] = CHSV(NP_Boot_Colors[1], 255, 255);
            }
          }

          playSdWav1.play("YELLOW.WAV");
          FastLED.show();

          loopCounter++;
          stateStartTime = millis();  // reset timer for next increment
        }

        // --- Continue watching for false starts during yellow phase ---
        for (int l = 0; l < Num_Lanes; l++) {
          int currentState = digitalRead(lanes[l].monitor_lap);

          if (lanes[l].penalty == 0 && currentState == LOW) {
            lanes[l].penalty = 1;
            lanes[l].penalty_time = millis();
            digitalWrite(lanes[l].relay, HIGH); // Cut power to the lanes
            playSdWav1.play("PENALTY.WAV");

            // Flash that lane red
            for (int i = 0; i < 3; i++) {
              leds[lanes[l].np[i]] = CHSV(NP_Boot_Colors[2], 255, 255);
            }
            FastLED.show();
          }
          lanes[l].state = currentState;
        }

        // --- After full yellow animation, move to green ---
        if ((millis() - stateStartTime > Delay_Yellow_Light) && loopCounter >= 3) {
          raceState = RaceState::START_LIGHTS_GREEN;
          stateEntered = true;
        }
        break;

    case RaceState::START_LIGHTS_GREEN:
      // Turn on green
      for (int i = 0; i < NP_Boot_Transitions; i++) {
        for (int l = 0; l < Num_Lanes; l++) {
          if (lanes[l].penalty == 0) {
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
      break;

    default:
      break;
  }
}

// Temorarpy Pause of the Race
void Pause_Race() {
  static bool showingPausedMsg = false;    // replaces Toggle_Race_Hazard
  static unsigned long blinkTimer = 0;     // replaces Time_Reference_Debounce

  unsigned long now = millis();

  // --- INITIAL ENTRY ---
  if (stateEntered) {
    // Cut power to all lanes
    for (int l = 0; l < Num_Lanes; l++) {
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
  if (now - blinkTimer > Delay_Yellow_Light) {
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
  if (ReadButtonStart(false)) {
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
    delay(Delay_Red_Light);

    // Yellow sequence
    for (int i = 0; i < 3; i++) {
      playSdWav1.play("YELLOW.WAV");
      for (int l = 0; l < Num_Lanes; l++) {
        leds[lanes[l].np[i]] = CHSV(NP_Boot_Colors[1], 255, 255);
      }
      FastLED.show();
      delay(Delay_Yellow_Light);
    }

    // Power restored + green lights
    for (int l = 0; l < Num_Lanes; l++) {
      digitalWrite(lanes[l].relay, LOW);
    }

    for (int i = 0; i < NP_Boot_Transitions; i++) {
      playSdWav1.play("GREEN.WAV");
      for (int l = 0; l < Num_Lanes; l++) {
        leds[lanes[l].np[i]] = CHSV(NP_Boot_Colors[0], 255, 255);
      }
    }

    FastLED.show();

    unsigned long pauseDuration = millis() - pauseStartTime;
    totalPauseDuration += pauseDuration;

    // Adjust each car's time forward so their total_time baseline stays aligned
    for (int c = 0; c < Num_Racers; c++) {
      cars[c].total_time += pauseDuration;
    }

    showingPausedMsg = false;
    blinkTimer = 0;

    // --- Transition back to active race ---
    raceState = RaceState::ACTIVE;
    stateEntered = true;
  }
}

// Stops race completely, kills power to all lanes and resets unit for new race
void Stop_Race() {
  // Cut power to all Lanes
  for (int l = 0; l < Num_Lanes; l++) {
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

  delay(Delay_Stop_Race);

  // Fade out animation
  for (int b = 84; b >= 0; b -= 2) {
    FastLED.setBrightness(b);
    FastLED.show();
    delay(Delay_Dim);
  }

  raceState = RaceState::CLEAR;  // Clear All Variables from previous race to prep for another
}

// Monitors All Race Attributes (called continuously during ACTIVE state)
void Race_Metrics() {
  unsigned long now = millis();

  // --- One-time initialization when entering ACTIVE state ---
  if (stateEntered) {
    LEDS.setBrightness(NP_Brightness);
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Race In Progress");

    for (int c = 0; c < Num_Racers; c++) {
      cars[c].total_time = now; // Set start time reference for all cars
    }

    stateEntered = false;
  }

  // --- Handle penalties: restore power after delay ---
  bool penaltyRestored = false;
  for (int l = 0; l < Num_Lanes; l++) {
    if (lanes[l].penalty == 0) continue;
    if ((now - lanes[l].penalty_time) <= Delay_Penalty) continue;

    // Restore lane power
    digitalWrite(lanes[l].relay, LOW);
    lanes[l].penalty = 0;

    // Restore LEDs to green
    for (int i = 0; i < NP_Boot_Transitions; i++) {
      leds[lanes[l].np[i]] = CHSV(NP_Boot_Colors[0], 255, 255);
    }

    penaltyRestored = true;
  }
  if (penaltyRestored) FastLED.show();

  // --- Sample lane sensors (lap counters) ---
  for (int l = 0; l < Num_Lanes; l++) {
    lanes[l].prev_state = lanes[l].state;
    lanes[l].state = digitalRead(lanes[l].monitor_lap);
  }

  // --- Update racer metrics ---
  for (int c = 0; c < Num_Racers; c++) {
    const bool lapCrossed = (
      cars[c].p_lane->state == LOW &&
      cars[c].p_lane->state != cars[c].p_lane->prev_state &&
      now > (cars[c].total_time + debounceTrack)
    );

    if (lapCrossed) {
      cars[c].lap_time = now - cars[c].total_time;
      cars[c].total_time = now;
      cars[c].cur_lap++;

      Lap_Counter();
      DetermineRacePlace();
    }

    // --- Check for new lap record ---
    if (cars[c].lap_time < Record_Lap && (cars[c].lap_time) > debounceTrack) {
      Record_Lap = cars[c].lap_time;
      Record_Car_Num = c;
      Record_Car = cars[c].number;
      LapRecordDisplay();
    }

    if (cars[c].cur_lap >= Num_Laps) {
      raceState = RaceState::END;
    }
  }
}

// Reads Lap Record from EEPROM and Displays on 7 Sgement Displays
void LapRecordDisplay() {
  char LapTimeRec_String[5];
  unsigned short LapTimeRec_Display = (Record_Lap > 999999 ? 999999 : Record_Lap) / 1000;  // Limit the lap time we'll display to ###.# seconds from milliseconds
  sprintf(LapTimeRec_String, "%4hu", LapTimeRec_Display);

  // Write the car who has the lap record
  LapRecNum.writeDigitAscii(0, Car_Numbers[Record_Car][0]);
  LapRecNum.writeDigitAscii(1, Car_Numbers[Record_Car][1]);

  // Write the lap time record
  LapTimeRec.writeDigitAscii(0, LapTimeRec_String[0]);
  LapTimeRec.writeDigitAscii(1, LapTimeRec_String[1], true);
  LapTimeRec.writeDigitAscii(2, LapTimeRec_String[2]);
  LapTimeRec.writeDigitAscii(3, LapTimeRec_String[3]);

  // Display the lap time record and car number
  LapTimeRec.writeDisplay();
  LapRecNum.writeDisplay();
}

//When new Lap Record is achieved it is written to EEPROM
void LapRecord() {
  EEPROM_writelong(0x02, Record_Lap);
  EEPROM.write(0x00, cars[Record_Car_Num].number);
}

//Write Long to EEPROM
void EEPROM_writelong(int address, long value) {
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
void EEPROMWriteInt(int address, int value) {
  byte two = (value & 0xFF);
  byte one = ((value >> 8) & 0xFF);

  EEPROM.update(address, two);
  EEPROM.update(address + 1, one);
}

// Monitor Lap Number and Display on 7 Segment Display
void Lap_Counter() {
  if (Num_Laps >= Current_Lap_Num) {
    int max_lap = 0;
    for (int c = 0; c < Num_Racers; c++) {
      max_lap = max(max_lap, cars[c].cur_lap);
    }

    // If the current lap counter is less than the highest lap, clear the display
    if (Current_Lap_Num < max_lap) {
      Current_Lap_Num = max_lap;  // Update and the Lap counter
      char LapBuffer[2];
      dtostrf(Current_Lap_Num, 2, 0, LapBuffer);  // Convert the Lap number individual char in an array and update lap count 7 segment displays
      if (Current_Lap_Num >= 10) { LapRecNum.writeDigitAscii(2, LapBuffer[0]); }
      LapRecNum.writeDigitAscii(3, LapBuffer[1]);
      LapRecNum.writeDisplay();
    }
  }

  if (Num_Laps <= Current_Lap_Num) {
    String Final_Lap = "FL";
    LapRecNum.clear();
    LapRecNum.writeDisplay();
    LapRecNum.writeDigitAscii(2, Final_Lap[0]);
    LapRecNum.writeDigitAscii(3, Final_Lap[1]);
    LapRecNum.writeDisplay();
    LapRecordDisplay();
  }
}

// Determine what place each car is in
void DetermineRacePlace() {
  // Sorts the cars based on how many laps completed and lowest total race time
  qsort(cars, Num_Racers, sizeof(struct Car), cmp_lap_and_total_time);

  // By those metrics we can now determine what place each car is in
  for (int i = 0; i < Num_Racers; i++) {
    cars[i].place = i + 1;
  }

  // Now we need to get back into lane order
  qsort(cars, Num_Racers, sizeof(struct Car), lane_order);

  Display_Leaderboard();
}

// Display Sorted Car Numbers and Lap times on Pole Position 7 Segmet Displays
void Display_Leaderboard() {
  // Nothing to update if we're only on the first lap
  if (Current_Lap_Num < 2) { return; }
  // Sorts the cars based on how many laps completed and lowest total race time
  qsort(cars, Num_Racers, sizeof(struct Car), cmp_lap_and_total_time);

  // Declare our lap time variables
  char PlayerLapTimes_Strings[4][5];
  bool dp1 = false;
  bool dp2 = false;
  unsigned int lapDisplay = 0;

  // Write all the player lap times and pole positions
  for (int player_i = 0; player_i < Num_Racers; player_i++) {
    if (cars[player_i].lap_time == 0) {
      Player_Times[player_i].clear();
      Player_Times[player_i].writeDisplay();
      continue;
    }

    // Limit the lap time we'll display to ##.## or ###.# seconds from milliseconds
    if (cars[player_i].finish == 1) { // End of the race, display last final times before the reset
      lapDisplay = (cars[player_i].total_time > 9999999 ? 9999999 : cars[player_i].total_time);
    } else {
      lapDisplay = (cars[player_i].lap_time > 999999 ? 999999 : cars[player_i].lap_time);
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

    sprintf(PlayerLapTimes_Strings[player_i], "%4hu", lapDisplay);

    // Write the player lap time to the ascii buffer
    Player_Times[player_i].writeDigitAscii(0, PlayerLapTimes_Strings[player_i][0]);
    Player_Times[player_i].writeDigitAscii(1, PlayerLapTimes_Strings[player_i][1], dp1);
    Player_Times[player_i].writeDigitAscii(2, PlayerLapTimes_Strings[player_i][2], dp2);
    Player_Times[player_i].writeDigitAscii(3, PlayerLapTimes_Strings[player_i][3]);

    // Write the player lap time and pole position to the display
    Player_Times[player_i].writeDisplay();
  }

  // Row 1
  Player_PolePositions[0].writeDigitAscii(2, Car_Numbers[cars[0].number][0]);
  Player_PolePositions[0].writeDigitAscii(3, Car_Numbers[cars[0].number][1]);
  // Row 2
  Player_PolePositions[0].writeDigitAscii(0, Car_Numbers[cars[1].number][0]);
  Player_PolePositions[0].writeDigitAscii(1, Car_Numbers[cars[1].number][1]);
  // Row 3
  Player_PolePositions[1].writeDigitAscii(2, Car_Numbers[cars[2].number][0]);
  Player_PolePositions[1].writeDigitAscii(3, Car_Numbers[cars[2].number][1]);
  // Row 4
  Player_PolePositions[1].writeDigitAscii(0, Car_Numbers[cars[3].number][0]);
  Player_PolePositions[1].writeDigitAscii(1, Car_Numbers[cars[3].number][1]);

  Player_PolePositions[0].writeDisplay();
  Player_PolePositions[1].writeDisplay();

  // Now we need to get back into lane order
  qsort(cars, Num_Racers, sizeof(struct Car), lane_order);

  LapCountdown();
}

// Display The Correct Number of Laps LED Pattern
void LapCountdown() {
  for (int c = 0; c < Num_Racers; c++) {
    switch (Num_Laps - cars[c].cur_lap) {
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
void End_Race() {
  static bool finishSoundPlayed = false;
  unsigned long now = millis();

  // Determine when a car is on its last lap
  for (int c = 0; c < Num_Racers; c++) {
    if (Num_Laps <= cars[c].cur_lap && cars[c].last_lap == 0) {
      if ((now - sound_buffer) >= 3500) {
        sound_buffer = now;
        playSdWav1.play("LASTLAP.WAV");
      }
      cars[c].last_lap = 1;
      for (int i = 0; i < NUM_LANES; i++) {
        leds[cars[c].p_lane->np[i]] = CRGB(255, 255, 255);
      }
    }
  }

  FastLED.show();

  // Action When a Car Finishes the Race
  for (int c = 0; c < Num_Racers; c++) {
    if (cars[c].cur_lap <= Num_Laps || cars[c].finish != 0) { continue; }

    digitalWrite(cars[c].p_lane->relay, HIGH); // Cut Power to the Lane
    cars[c].finish = 1;

    // Quick blackout
    for (int i = 0; i < NUM_LANES; i++) {
      leds[cars[c].p_lane->np[i]] = CRGB(0, 0, 0);
    }
    FastLED.show();
    delay(50);

    // Display The Correct Position LED Pattern
    switch (cars[c].place) {
      case 1:
        sound_buffer = now;
        leds[cars[c].p_lane->np[1]] = CRGB(0, 255, 255);
        break;
      case 2:
        if ((now - sound_buffer) >= 8500) {
          playSdWav1.play("RECORD.WAV");
          sound_buffer = now;
        }
        leds[cars[c].p_lane->np[0]] = CRGB(0, 255, 255);
        leds[cars[c].p_lane->np[2]] = CRGB(0, 255, 255);
        break;
      case 3:
      case 4:
        if ((now - sound_buffer) >= 8500) {
          playSdWav1.play("RECORD.WAV");
          sound_buffer = now;
        }
        for (int i = 0; i < cars[c].place; i++) {
          leds[cars[c].p_lane->np[i]] = CRGB(0, 255, 255);
        }
        break;
    }
  
    FastLED.show();
  }

  // Count all finished cars
  int carsFinished = 0;
  for (int c = 0; c < Num_Racers; c++) {
    if (cars[c].finish == 1) carsFinished++;
  }

  // When the first car crosses the finish line play the finish Song
  if (!finishSoundPlayed && carsFinished > 0) {
    playSdWav1.play("FINISH.WAV");
    finishSoundPlayed = true;
    sound_buffer = now;
  }

  // End Race After all Cars Cross the Finish Line
  if (carsFinished == Num_Racers) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Race Finished!  ");
    finishSoundPlayed = false;
    delay(Delay_Stop_Race);
    raceState = RaceState::CLEAR;
    stateEntered = true;
  } else {
    raceState = RaceState::ACTIVE;
  }
}

// Reset all Variables and 7 Segment Displays from Previous Race and Record Lap Record
void Clear_Race() {
  // --- Save Race Record to EEPROM ---
  LapRecord();

  // --- Reset State Variables ---
  Current_Lap_Num = 0;
  Num_Laps = 5;
  Num_Racers = Num_Lanes;
  Configured_Racers = 0;
  Car_Config_Index = 0;
  pauseStartTime = 0;
  totalPauseDuration = 0;
  myEnc.write(0);

  // --- Clear LEDs ---
  FastLED.clear();
  FastLED.setBrightness(NP_Brightness);
  FastLED.show();

  // --- Re-initialize race entities ---
  initCars();
  initLanes();

  // --- Clear Leaderboard Display ---
  for (int player = 0; player < Num_Racers; player++) {
    Player_Times[player].clear();
    Player_Times[player].writeDisplay();
    Player_PolePositions[player].clear();
    Player_PolePositions[player].writeDisplay();
  }
  Player_PolePositions[0].clear();
  Player_PolePositions[0].writeDisplay();
  Player_PolePositions[1].clear();
  Player_PolePositions[1].writeDisplay();

  LapTimeRec.clear();
  LapRecNum.clear();
  LapTimeRec.writeDisplay();
  LapRecNum.writeDisplay();

  // --- Re-display lap record ---
  LapRecordDisplay();

  // --- Transition to configuration menu ---
  currentMenu = MenuState::MENU_NUM_RACERS;
  raceState = RaceState::NONE;
}
