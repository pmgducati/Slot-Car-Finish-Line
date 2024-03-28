// Libraries
#define FASTLED_INTERNAL // To disable FastLED.h pragma messages on compile include this before including FastLED.h
#include <FastLED.h>
#include <Encoder.h>
#include <SPI.h>
#include <SD.h>
#include <SerialFlash.h>
#include <Wire.h>
#include <Audio.h>
#include <Adafruit_LiquidCrystal.h>
#include <Adafruit_LEDBackpack.h>
#include <EEPROM.h>
#include <stdio.h>
#include <stdlib.h>

//NeoPixel Pin Assignments
#define NUM_LEDS 16  // Number of LEDs in strip
#define DATA_PIN 39  // Pin to Control NeoPixels

//I2S Audio Assignments
AudioPlaySdWav playSdWav1;
AudioMixer4 mixer1;
AudioOutputI2S i2s1;
AudioConnection patchCord1(playSdWav1, 0, mixer1, 0);
AudioConnection patchCord2(playSdWav1, 1, mixer1, 1);
AudioConnection patchCord3(mixer1, 0, i2s1, 0);
AudioConnection patchCord4(mixer1, 0, i2s1, 1);

//Neopixel Arrays
CRGB leds[NUM_LEDS];                                                        //Define the array of leds
int NP_Boot_Pattern[16] = { 13, 14, 12, 15, 0, 11, 1, 10, 2, 9, 3, 8, 4, 7, 5, 6 };  //LED Order for Boot Animation
int NP_Boot_Colors[4] = { 0, 64, 96, 160 };                                     //Colors Used in Boot Animation
int NP_Boot_Transitions = 4;                                                     //Color Transitions in Boot Animation
int NP_Race_Start_Red[4] = { 12, 13, 14, 15 };                                   //Red LEDS for Race Start
int NP_Lane_1[4] = { 11, 10, 9, 12 };                                         //Lane One LEDs
int NP_Lane_2[4] = { 8, 7, 6, 13 };                                           //Lane Two LEDs
int NP_Lane_3[4] = { 5, 4, 3, 14 };                                           //Lane Three LEDs
int NP_Lane_4[4] = { 2, 1, 0, 15 };                                           //Lane Four LEDs

// 7 Seg LED Assignments
Adafruit_AlphaNum4 Player_PolePositions[4] = { Adafruit_AlphaNum4(), Adafruit_AlphaNum4(), Adafruit_AlphaNum4(), Adafruit_AlphaNum4() }; // Pole Position Car Numbers 1 - 4 ((1 & 2) + (3 & 4))
Adafruit_AlphaNum4 Player_Times[4] = { Adafruit_AlphaNum4(), Adafruit_AlphaNum4(), Adafruit_AlphaNum4(), Adafruit_AlphaNum4() };         // Pole Position Time 1 - 4
Adafruit_AlphaNum4 LapRecNum = Adafruit_AlphaNum4();   //Lap Counter and Lap Record Car Number
Adafruit_AlphaNum4 LapTimeRec = Adafruit_AlphaNum4();  //Lap Record Time

//Pin Assignments
#define SDCARD_CS_PIN BUILTIN_SDCARD  //used for Audio Playback
#define SDCARD_MOSI_PIN 61            //used for Audio Playback 11
#define SDCARD_SCK_PIN 60             //used for Audio Playback 13
#define BUTTON_RE 26                  //Rotary Encoder Button
#define BUTTON_BACK 24                //Back Button
#define BUTTON_START 25               //Start Race Button
#define BUTTON_STOP 27                //Pause Button
#define RELAY_LANE_1 14              //Controls the Relay for Lane 1 in the Control Box
#define RELAY_LANE_2 15              //Controls the Relay for Lane 2 in the Control Box
#define RELAY_LANE_3 16              //Controls the Relay for Lane 3 in the Control Box
#define RELAY_LANE_4 17              //Controls the Relay for Lane 4 in the Control Box
#define MONITOR_LAP_LANE_1 36            //Lap counter switch in Lane 1
#define MONITOR_LAP_LANE_2 35            //Lap counter switch in Lane 2
#define MONITOR_LAP_LANE_3 34            //Lap counter switch in Lane 3
#define MONITOR_LAP_LANE_4 33            //Lap counter switch in Lane 4
#define LED_NOTIFICATION 13                    //Notification LED
#define ENCODER_INCREMENT 29               //Rotary Encoder Pin +
#define ENCODER_DECREMENT 28               //Rotary Encoder Pin -

// LCD Backpack Setup
Adafruit_LiquidCrystal lcd(1);  //default address #0 (A0-A2 not jumpered)

//Encoder Setup
Encoder myEnc(ENCODER_INCREMENT, ENCODER_DECREMENT);

//Variables
//Menu Navigation - The variables trigger a menu change when flagged as 1
int Menu_Welcome_Message = 1;
int Menu_Options = 0;
int Menu_Number_of_Racers = 0;
int Menu_Number_of_Laps = 0;
int Menu_Car_Num_Lane = 0;
int Menu_Start_Race = 0;
int Options_Stop_Race = 0;
int Options_Clear_Lap_Record = 0;
int Options_Debounce_Track = 0;
int Options_Penalty = 0;
int Array_Increment = 0;
int Toggle_Menu_Initialize = 1;
int Toggle_Race_Metrics = 0;
int Toggle_Race_Stop = 0;
int Toggle_Race_Pause = 0;
int Toggle_Race_Hazard = 0;

//Rotary Encoder - Logs the position of the Rotary Encoder
long Encoder_Position_Old = -999;
long Encoder_Position_New;

//Debouncing
unsigned long Time_Current;             //Current Overall time in milliseconds
unsigned long Time_Reference_Debounce;  //Millis reference when button is presses
unsigned long Time_Offset_Pause;  //Time (Milliseconds) to ajdust in the event the race is paused
int Debounce_Encoder = 125;       //Debounce time (Milliseconds) for the Rotary Encoder
int Debounce_Button = 200;   //Debounce time (Milliseconds) for a Button Press
unsigned int Debounce_Track = 1000;   //Default debounce time (Milliseconds) when a car passes the start line (Can be Modified in Options 500-10000 and saved to EEPROM)

//Race Identifiers
int Num_Laps = 5;    //Default number of laps in the Race (Can be Modified in Menu 5-99)
int MIN_LAPS = 5;    //Minimum number of laps in a race
int MAX_LAPS = 99;   //Maximum number of laps in a race
int Num_Racers = 4;  //Default number of Racers in the Race (Can be Modified in Menu 1-4)
int Num_Lane = 1;    //Used for Lane Assignment of Drivers/Car/Lap Times
int Num_Lanes = 4;   //Max number of lanes on the race track
int Configured_Racers = 0; //How many cars have been configured

//Button Status Monitors
int Monitor_Start = 0;  //Triggers an event when the Start Button is pressed
int Monitor_Back = 0;   //Triggers an event when the Back Button is pressed
int Monitor_Stop = 0;   //Triggers an event when the Stop Button is pressed
int Monitor_Last_Press_Back = 0;     //Flags an event when the Back Button is pressed
int Monitor_Last_Press_Start = 0;    //Flags an event when the Start Button is pressed

//Race Information
int Current_Lap_Num = 0;           //Lap Count in Current Race
int Race_Over = 0;                 //Race Complete Flag
int First_Car_Finish = 0;          //Flag to Play FINISH.WAV
int Last_Lap = 0;                  //Last Lap Flag
unsigned long Record_Lap = 99999;  //Default Lap Record Time (Actual is called from EEPROM)
int Record_Car_Num;                //Array Identifer of the record setting car
int Record_Car;                     //Lap Record Car Number (Value is called from EEPROM)
unsigned long sound_buffer;  //Time (Milliseconds) buffer to avoid sound stomping on eachother

// Struct (or class/object) that defines everything that a car needs to have
struct Car {
  int lane;                  // Lane the car is in
  struct Lane *lane_p;       // A pointer to the lane object the car is present in
  int number;                // The number that represents which car type is in use
  int cur_lap;               // Current lap the car is on
  int place;                 // Place the car is currently in
  unsigned long lap_time;    // The time (Milliseconds) of the last time
  unsigned long total_time;  // Total race time (Milliseconds)
  int last_lap;              // Flag to signal last lap Neopixel and Sound Events
  int finish;                // Has the car finished the race
};

// Struct (or class/object) that defines everything that a lane needs to have
struct Lane {
  int number;                // Lane number
  struct Car *car_p;         // A pointer to the car object present in the lane
  int np[4];                 // Lane LEDs array
  int relay;                 // Controls the Relay for the lane in the Control Box
  int monitor_lap;           // Lap counter switch for the lane
  int state;                 // In Track Lap Counter Monitors State, per lane
  int penalty;               // Flag if car crosses start line before the green light, per lane
};

// Declare our cars array and fill them in with default values in the loop
struct Car *cars = (Car *)malloc(Num_Lanes * sizeof *cars);

// Declare our lanes array and fill them in with default values in the loop
struct Lane *lanes = (Lane *)malloc(Num_Lanes * sizeof *lanes);

//Screen Variables
int Center_Value;  //Used to center the text on the 16x2 LCD screen

//Neopixel Variables
int NP_Brightness = 84;          //Set Neopixel Brightness
int Delay_Start_Sequence = 100;  //Start Animation Speed (Higher = Slower)
int Delay_Dim = 50;              //Dimming Speed (Higher = Slower)
int Delay_Yellow_Light = 750;    //Delay between Yellow Lights
int Delay_Red_Light = 4250;      //Time for Red Lights
int Delay_Stop_Race = 10000;     //Default time (Milliseconds) for wait on race end before going back to Main Menu (Can be Modified in Options 1000-10000 and saved to EEPROM)

//Relay Penality Variables
int Delay_Penality = 5000;  //Default time (Milliseconds) for Penalty duration if a car crosses the track before green (Can be Modified in Options 500-5000 and saved to EEPROM)

//Menu Arrays
//Car Names and Numbers Displayed on LCD
String Car_Names[10] = { "01 Skyline", "03 Ford Capri", "05 BMW 3.5 CSL", "05 Lancia LC2", "33 Audi RS5", "51 Porsche 935", "576 Lancia Beta", "80 BMW M1", "88 BTTF Delorean", "MM GT Falcon V8" };
//Car Numbers on Displayed on Pole Position and Lap Record 7 Segment
String Car_Numbers[11] = { "01", "03", "05", "05", "33", "51", "57", "80", "88", "MM", "--" };
//Options Menu
String Options_Selection[5] = { "Start Race", "End Race Time", "Penalty Time", "Track Debounce", "Erase Lap Record" };
//Menu selection for Erasing EEPROM
String Rec_Reset[20] = { "NO", "X", "XXX", "X", "XXX", "X", "XXX", "X", "XXX", "X", "YES", "X", "XXX", "X", "XXX", "X", "XXX", "X", "XXX", "X" };

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

//Read Long from EEPROM (Lap Time)
long EEPROMReadlong(long address) {
  //Read the 4 bytes from the eeprom memory.
  long four = EEPROM.read(address);
  long three = EEPROM.read(address + 1);
  long two = EEPROM.read(address + 2);
  long one = EEPROM.read(address + 3);

  //Return the recomposed long by using bitshift.
  return ((four << 0) & 0xFF) + ((three << 8) & 0xFFFF) + ((two << 16) & 0xFFFFFF) + ((one << 24) & 0xFFFFFFFF);
}
//Read Int from EEPROM (Variables for Options)
int EEPROMReadInt(int address) {
  long two = EEPROM.read(address);
  long one = EEPROM.read(address + 1);

  //Return the recomposed long by using bitshift
  return ((two << 0) & 0xFFFFFF) + ((one << 8) & 0xFFFFFFFF);
}

// Reads the start button and won't exit the function until the user stops pressing it
int Read_Buttons_Start(){
  int button_pressed = 0; // Was the button pressed at all
  int button_status = 0;  // The current status of the button

  do {
    button_status = digitalRead(BUTTON_START);
    if (button_status == 1 && button_pressed == 0) { button_pressed = button_status; } // If we saw the button pressed at any time, set the flag
  } while (button_status == 1);
  return button_pressed;
}

// Reads the back button and won't exit the function until the user stops pressing it
int Read_Buttons_Back(){
  int button_pressed = 0; // Was the button pressed at all
  int button_status = 0;  // The current status of the button

  do {
    button_status = digitalRead(BUTTON_BACK);
    if (button_status == 1 && button_pressed == 0) { button_pressed = button_status; } // If we saw the button pressed at any time, set the flag
  } while (button_status == 1);
  return button_pressed;
}

void setup() {
  //Start Serial Monitor
  Serial.begin(9600);
  //I2S Audio Setup
  AudioMemory(8);
  SPI.setMOSI(SDCARD_MOSI_PIN);
  SPI.setSCK(SDCARD_SCK_PIN);
  if (!(SD.begin(SDCARD_CS_PIN))) {
    while (1) {
    }
  }

  // Set up each of our cars with default values
  cars[0] = (struct Car){ .lane = 96, .number = 10, .cur_lap = 0, .place = 1, .lap_time = 0, .total_time = 0, .last_lap = 0, .finish = 0 };
  cars[1] = (struct Car){ .lane = 97, .number = 10, .cur_lap = 0, .place = 2, .lap_time = 0, .total_time = 0, .last_lap = 0, .finish = 0 };
  cars[2] = (struct Car){ .lane = 98, .number = 10, .cur_lap = 0, .place = 3, .lap_time = 0, .total_time = 0, .last_lap = 0, .finish = 0 };
  cars[3] = (struct Car){ .lane = 99, .number = 10, .cur_lap = 0, .place = 4, .lap_time = 0, .total_time = 0, .last_lap = 0, .finish = 0 };

  // Set up each of our lanes with default values
  lanes[0] = (struct Lane){ .number = 1, .np = { 11, 10, 9, 12 }, .relay = RELAY_LANE_1, .monitor_lap = MONITOR_LAP_LANE_1, .state = 0, .penalty = 0 };
  lanes[1] = (struct Lane){ .number = 2, .np = { 8, 7, 6, 13 },   .relay = RELAY_LANE_2, .monitor_lap = MONITOR_LAP_LANE_2, .state = 0, .penalty = 0 };
  lanes[2] = (struct Lane){ .number = 3, .np = { 5, 4, 3, 14 },   .relay = RELAY_LANE_3, .monitor_lap = MONITOR_LAP_LANE_3, .state = 0, .penalty = 0 };
  lanes[3] = (struct Lane){ .number = 4, .np = { 2, 1, 0, 15 },   .relay = RELAY_LANE_4, .monitor_lap = MONITOR_LAP_LANE_4, .state = 0, .penalty = 0 };

  //Read EEPROM Variables and replace default values
  Record_Lap = EEPROMReadlong(0x02);
  Record_Car = EEPROM.read(0x00);
  Delay_Penality = EEPROMReadInt(0x08);
  Delay_Stop_Race = EEPROMReadInt(0x06);
  Debounce_Track = EEPROMReadInt(0x10);

  // Set up the 7-Segment LED Panels
  Player_PolePositions[0].begin(0x70);  // pass in the address for the Place 1 and 2 Car Numbers
  Player_PolePositions[1].begin(0x70);  // pass in the address for the Place 1 and 2 Car Numbers
  Player_PolePositions[2].begin(0x71);  // pass in the address for the Place 3 and 4 Car Numbers
  Player_PolePositions[3].begin(0x71);  // pass in the address for the Place 3 and 4 Car Numbers
  LapRecNum.begin(0x72);                // pass in the address for the Lap Counter and Lap Record Car Number
  LapTimeRec.begin(0x77);               // pass in the address for the Lap Record Time
  Player_Times[0].begin(0x73);          // pass in the address for the Place 1 Lap Time
  Player_Times[1].begin(0x74);          // pass in the address for the Place 2 Lap Time
  Player_Times[2].begin(0x75);          // pass in the address for the Place 3 Lap Time
  Player_Times[3].begin(0x76);          // pass in the address for the Place 4 Lap Time

  // Set up the LCD's number of rows and columns and enable the backlight
  lcd.begin(16, 2);
  lcd.setBacklight(HIGH);

  //Pin Mode Assignments
  pinMode(BUTTON_RE, INPUT);
  pinMode(BUTTON_BACK, INPUT);
  pinMode(BUTTON_START, INPUT);
  pinMode(BUTTON_STOP, INPUT);
  pinMode(LED_NOTIFICATION, OUTPUT);

  // Set the monitor_lap and relay pin modes and disable the lanes before the race
  for (int l = 0; l < Num_Lanes; l++) {
    pinMode(lanes[l].monitor_lap, INPUT_PULLUP);
    pinMode(lanes[l].relay, OUTPUT);
    digitalWrite(lanes[l].relay, HIGH);
  }

  //Neopixel Setup
  LEDS.addLeds<WS2812, DATA_PIN, RGB>(leds, NUM_LEDS);
  LEDS.setBrightness(NP_Brightness);
}
//Main Loop
void loop() {
  //Record the Current time
  Time_Current = millis();

  //Monitor Buttons
  Monitor_Back = digitalRead(BUTTON_BACK);
  Monitor_Stop = digitalRead(BUTTON_STOP);
  Monitor_Start = digitalRead(BUTTON_START);

  //Menu Navigation
  if (Menu_Welcome_Message == 1) {
    Welcome_Message();
  }
  if (Menu_Options == 1) {
    Options();
  }
  if (Menu_Number_of_Racers == 1) {
    Number_of_Racers();
  }
  if (Menu_Number_of_Laps == 1) {
    Number_of_Laps();
  }
  if (Toggle_Race_Metrics == 1) {
    Race_Metrics();
  }
  if (Menu_Start_Race == 1) {
    for (int l = 0; l < Num_Lanes; l++) {
     lanes[l].state = digitalRead(lanes[l].monitor_lap);
    }
    Start_Race();
  }
  if (Menu_Car_Num_Lane == 1) {
    Car_Num_Lane_Assign();
  }
  if (Toggle_Race_Pause == 1) {
    Pause_Race();
  }
  if (Toggle_Race_Stop == 1) {
    Stop_Race();
  }
  if (Monitor_Back == 1 && Monitor_Last_Press_Back == 0 && Time_Current > (Time_Reference_Debounce + Debounce_Encoder)) {
    Menu_Back();
  }
  if (Options_Stop_Race == 1) {
    Menu_Stop_Race();
  }
  if (Options_Clear_Lap_Record == 1) {
    Clear_Record_Lap();
  }
  if (Options_Debounce_Track == 1) {
    Menu_Debounce_Track();
  }
  if (Options_Penalty == 1) {
    Menu_Penalty();
  }
  //Record button presses to avoid rapid repeats
  Monitor_Last_Press_Back = Monitor_Back;
  Monitor_Last_Press_Start = Monitor_Start;

  if (Num_Laps <= Current_Lap_Num) {
    if (Last_Lap == 0) {
      Toggle_Menu_Initialize = 1;
    }
    End_Race();
  }
}
//Play Audio Files
void playFile(const char *filename) {
  playSdWav1.play(filename);        // Start playing the file.  The sketch continues to run while the file plays.
  delay(5);                         // A brief delay for the library read WAV info
  while (playSdWav1.isPlaying()) {  // Simply wait for the file to finish playing.
  }
}
//Navigate backwards through the race setup menu
void Menu_Back() {
  Time_Reference_Debounce = Time_Current;
  Toggle_Menu_Initialize = 1;                    //Enables menu intros
  if (Menu_Number_of_Racers == 1) {  //Accesses Options Menu
    Menu_Options = 1;
    Menu_Number_of_Racers = 0;
  }
  if (Menu_Number_of_Laps == 1) {  //Back to Number of Racers
    Menu_Number_of_Racers = 1;
    Menu_Number_of_Laps = 0;
  }
  if (Menu_Car_Num_Lane == 1) {  //Back to number of Laps
    Num_Lane = 1;
    Menu_Number_of_Laps = 1;
    Menu_Car_Num_Lane = 0;
  }
}
//Initial LED Animation and Welcome Message
void Welcome_Message() {
  //If the Race is Over Clear All Variables from Previous Race
  if (Race_Over == 1) {
    ClearRace();
  }
  //Welcome Message
  lcd.setCursor(3, 0);
  lcd.print("Welcome to");
  lcd.setCursor(3, 1);
  lcd.print("the Race!");
  playSdWav1.play("WELCOME.WAV");
  LapRecordDisplay();

  //Intro Green/Yellow/Red/Blue Neopixel Animation
  for (int j = 0; j < NP_Boot_Transitions; j++) {
    for (int i = 0; i < NUM_LEDS; i++) {  //Left Chase Animation
      leds[NP_Boot_Pattern[i]] = CHSV(NP_Boot_Colors[j], 255, 255);
      i++;
      leds[NP_Boot_Pattern[i]] = CHSV(NP_Boot_Colors[j], 255, 255);
      FastLED.show();
      delay(Delay_Start_Sequence);
    }
    j++;
    for (int i = 15; i > 0; i--) {  //Right Chase Animation
      leds[NP_Boot_Pattern[i]] = CHSV(NP_Boot_Colors[j], 255, 255);
      i--;
      leds[NP_Boot_Pattern[i]] = CHSV(NP_Boot_Colors[j], 255, 255);
      FastLED.show();
      delay(Delay_Start_Sequence);
    }
  }
  for (int i = 84; i >= 0; i--) {  //Fade out Animation
    if (i > 1) {
      i--;
    }
    LEDS.setBrightness(i);
    FastLED.show();
    delay(Delay_Dim);
  }

  //Setup for next menu
  Menu_Welcome_Message = 0;
  Menu_Number_of_Racers = 1;
  Toggle_Menu_Initialize = 1;
  lcd.clear();
  Encoder_Position_New = myEnc.read();
  Encoder_Position_Old = Encoder_Position_New;
  Time_Reference_Debounce = Time_Current;
}
//Rotary Encoder monitoring
void Rotary_Encoder() {
  if (Time_Current > (Time_Reference_Debounce + Debounce_Encoder)) {  //Buffer is 250
    Encoder_Position_New = myEnc.read();
  } else {
    Encoder_Position_New = myEnc.read();
    Encoder_Position_Old = Encoder_Position_New;
  }
}

// Options Main Menu Navigation
void Options() {
  int Options_Selection_Size = sizeof(Options_Selection) / sizeof(Options_Selection[0]);
  int Screen_Rotary_Update = 0;

  if (Toggle_Menu_Initialize == 1) {  // Menu Initialization
    lcd.clear();
    lcd.setCursor(4, 0);
    lcd.print("Options");
    lcd.setCursor(3, 1);
    lcd.print(Options_Selection[0]);
    Array_Increment = 0;
    Toggle_Menu_Initialize = 0;
  }

  Rotary_Encoder();
  if (Encoder_Position_New > Encoder_Position_Old) {  // Watch the Rotary Encoder and add
    Time_Reference_Debounce = Time_Current;
    Array_Increment++;
    if (Array_Increment >= Options_Selection_Size) {
      Array_Increment = Options_Selection_Size - 1;
    }
    Screen_Rotary_Update = 1;
  } else if (Encoder_Position_New < Encoder_Position_Old) {  // Watch the Rotary Encoder and subtract
    Time_Reference_Debounce = Time_Current;
    Array_Increment--;
    if (Array_Increment < 0) {
      Array_Increment = 0;
    }
    Screen_Rotary_Update = 1;
  }

  if (Screen_Rotary_Update == 1) {  // Scroll though the available options in the menu
    playSdWav1.play("TICK.WAV");
    String Option_Name = Options_Selection[Array_Increment];
    Center_Value = (16 - Option_Name.length()) / 2;
    lcd.clear();
    lcd.setCursor(4, 0);
    lcd.print("Options");
    lcd.setCursor(Center_Value, 1);
    lcd.print(Options_Selection[Array_Increment]);
    Time_Reference_Debounce = Time_Current;
    Encoder_Position_Old = Encoder_Position_New;
  }

  if (Monitor_Start != 1 || Monitor_Last_Press_Start != 0 || Time_Current <= (Time_Reference_Debounce + Debounce_Button)) { return; }

  Menu_Options = 0;
  Toggle_Menu_Initialize = 1;
  lcd.clear();
  Encoder_Position_New = myEnc.read();
  Encoder_Position_Old = Encoder_Position_New;
  Time_Reference_Debounce = Time_Current;

  // Determine which menu to move to
  switch (Array_Increment) {
    case 0: // Move on to Number of Racers Menu
      Menu_Number_of_Racers = 1;
      break;
    case 1: // Move on to End Race Time Setting
      Options_Stop_Race = 1;
      break;
    case 2: // Move on to Penalty Time Setting
      Options_Penalty = 1;
      break;
    case 3: // Move on to Track Debounce Setting
      Options_Debounce_Track = 1;
      break;
    case 4: // Move on to Erase Lap Record
      Options_Clear_Lap_Record = 1;
      break;
  }
}

// Track Debounce Value Selection and Set
void Menu_Debounce_Track() {
  int Screen_Rotary_Update = 0;

  if (Toggle_Menu_Initialize == 1) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Lane Debouncing");
    lcd.setCursor(6, 1);
    lcd.print(Debounce_Track);
    Toggle_Menu_Initialize = 0;
  }

  Rotary_Encoder();
  if (Encoder_Position_New > Encoder_Position_Old) { // Watch the Rotary Encoder and add
    Time_Reference_Debounce = Time_Current;
    Debounce_Track = Debounce_Track + 500;
    if (Debounce_Track > 5000) {
      Debounce_Track = 5000;
    }
    Screen_Rotary_Update = 1;
  } else if (Encoder_Position_New < Encoder_Position_Old) { // Watch the Rotary Encoder and subtract
    Time_Reference_Debounce = Time_Current;
    Debounce_Track = Debounce_Track - 500;
    if (Debounce_Track < 500) {
      Debounce_Track = 500;
    }
    Screen_Rotary_Update = 1;
  }

  if (Screen_Rotary_Update == 1) { // Scroll though the available option in the menu
    playSdWav1.play("TICK.WAV");
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Lane Debouncing");
    lcd.setCursor(6, 1);
    lcd.print(Debounce_Track);
    Time_Reference_Debounce = Time_Current;
    Encoder_Position_Old = Encoder_Position_New;
  }

  if (Monitor_Start == 1 && Monitor_Last_Press_Start == 0 && Time_Current > (Time_Reference_Debounce + Debounce_Button)) { // Set the desired value for Track Debounce and save to EEPROM
    EEPROMWriteInt(0x08, Debounce_Track);
    Options_Debounce_Track = 0;
    Menu_Options = 1;
    Toggle_Menu_Initialize = 1;
    lcd.clear();
    Encoder_Position_New = myEnc.read();
    Encoder_Position_Old = Encoder_Position_New;
    Time_Reference_Debounce = Time_Current;
  }
}

// Penalty Value Selection and Set
void Menu_Penalty() {
  int Screen_Rotary_Update = 0;

  if (Toggle_Menu_Initialize == 1) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Penalty Timeout");
    lcd.setCursor(6, 1);
    lcd.print(Delay_Penality);
    Toggle_Menu_Initialize = 0;
  }

  Rotary_Encoder();
  if (Encoder_Position_New > Encoder_Position_Old) { // Watch the Rotary Encoder and add to the number of racers
    Time_Reference_Debounce = Time_Current;
    Delay_Penality = Delay_Penality + 500;
    if (Delay_Penality > 5000) {
      Delay_Penality = 5000;
    }
    Screen_Rotary_Update = 1;
  } else if (Encoder_Position_New < Encoder_Position_Old) { // Watch the Rotary Encoder and subtract from the number of racers
    Time_Reference_Debounce = Time_Current;
    Delay_Penality = Delay_Penality - 500;
    if (Delay_Penality < 500) {
      Delay_Penality = 500;
    }
    Screen_Rotary_Update = 1;
  }

  if (Screen_Rotary_Update == 1) { // Scroll though the available option in the menu
    playSdWav1.play("TICK.WAV");
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Penalty Timeout");
    lcd.setCursor(6, 1);
    lcd.print(Delay_Penality);
    Time_Reference_Debounce = Time_Current;
    Encoder_Position_Old = Encoder_Position_New;
  }

  if (Monitor_Start == 1 && Monitor_Last_Press_Start == 0 && Time_Current > (Time_Reference_Debounce + Debounce_Button)) { // Set the desired value for Penalty Duration and save to EEPROM
    EEPROMWriteInt(0x10, Delay_Penality);
    Options_Penalty = 0;
    Menu_Options = 1;
    Toggle_Menu_Initialize = 1;
    lcd.clear();
    Encoder_Position_New = myEnc.read();
    Encoder_Position_Old = Encoder_Position_New;
    Time_Reference_Debounce = Time_Current;
  }
}

// Stop Race Timeout Value Selection and Set
void Menu_Stop_Race() {
  int Screen_Rotary_Update = 0;

  if (Toggle_Menu_Initialize == 1) {
    lcd.clear();
    lcd.setCursor(2, 0);
    lcd.print("End Timeout");
    lcd.setCursor(6, 1);
    lcd.print(Delay_Stop_Race);
    Toggle_Menu_Initialize = 0;
  }

  Rotary_Encoder();
  if (Encoder_Position_New > Encoder_Position_Old) { // Watch the Rotary Encoder and add
    Time_Reference_Debounce = Time_Current;
    Delay_Stop_Race = Delay_Stop_Race + 500;
    if (Delay_Stop_Race > 10000) {
      Delay_Stop_Race = 10000;
    }
    Screen_Rotary_Update = 1;
  } else if (Encoder_Position_New < Encoder_Position_Old) { // Watch the Rotary Encoder and subtract
    Time_Reference_Debounce = Time_Current;
    Delay_Stop_Race = Delay_Stop_Race - 500;
    if (Delay_Stop_Race < 1000) {
      Delay_Stop_Race = 1000;
    }
    Screen_Rotary_Update = 1;
  }

  if (Screen_Rotary_Update == 1) { // Scroll though the available option in the menu
    playSdWav1.play("TICK.WAV");
    lcd.clear();
    lcd.setCursor(2, 0);
    lcd.print("End Timeout");
    lcd.setCursor(6, 1);
    lcd.print(Delay_Stop_Race);
    Time_Reference_Debounce = Time_Current;
    Encoder_Position_Old = Encoder_Position_New;
  }

  if (Monitor_Start == 1 && Monitor_Last_Press_Start == 0 && Time_Current > (Time_Reference_Debounce + Debounce_Button)) { // Set the desired value for Penalty Duration and save to EEPROM
    EEPROMWriteInt(0x06, Delay_Stop_Race);
    Options_Stop_Race = 0;
    Menu_Options = 1;
    Toggle_Menu_Initialize = 1;
    lcd.clear();
    Encoder_Position_New = myEnc.read();
    Encoder_Position_Old = Encoder_Position_New;
    Time_Reference_Debounce = Time_Current;
  }
}

// Menu Section for selecting # of racers
void Number_of_Racers() {
  int Screen_Rotary_Update = 0;

  // If the Race is Over Clear All Variables from Previous Race
  if (Race_Over == 1) {
    ClearRace();
  }

  if (Toggle_Menu_Initialize == 1) { // Initialization of the Racers Menu
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Number of Racers");
    lcd.setCursor(7, 1);
    lcd.print(Num_Racers);
    Num_Racers = 4;
    Toggle_Menu_Initialize = 0;
  }

  Rotary_Encoder();
  if (Encoder_Position_New > Encoder_Position_Old) { // Watch the Rotary Encoder and add to the number of racers
    Time_Reference_Debounce = Time_Current;
    Num_Racers++;
    if (Num_Racers > Num_Lanes) {
      Num_Racers = Num_Lanes;
    }
    Screen_Rotary_Update = 1;
  } else if (Encoder_Position_New < Encoder_Position_Old) { // Watch the Rotary Encoder and subtract from the number of racers
    Time_Reference_Debounce = Time_Current;
    Num_Racers--;
    if (Num_Racers < 1) {
      Num_Racers = 1;
    }
    Screen_Rotary_Update = 1;
  }

  if (Screen_Rotary_Update == 1) { // Update the number of racers and display on LCD
    playSdWav1.play("TICK.WAV");
    lcd.setCursor(7, 1);
    lcd.print(Num_Racers);
    Time_Reference_Debounce = Time_Current;
    Encoder_Position_Old = Encoder_Position_New;
  }

  if (Monitor_Start == 1 && Monitor_Last_Press_Start == 0 && Time_Current > (Time_Reference_Debounce + Debounce_Button)) { // Move on to Number of Laps Menu
    Time_Reference_Debounce = Time_Current;
    Menu_Number_of_Racers = 0;
    Menu_Number_of_Laps = 1;
    Toggle_Menu_Initialize = 1;
  }
}

// Menu Section to Specify Number of Laps
void Number_of_Laps() {
  int Screen_Rotary_Update = 0;

  if (Toggle_Menu_Initialize == 1) { // Initialization of the Laps Menu
    lcd.clear();
    lcd.setCursor(1, 0);
    lcd.print("Number of Laps");
    lcd.setCursor(7, 1);
    lcd.print(Num_Laps);
    Num_Laps = 5;
    Monitor_Start = 0;
    Toggle_Menu_Initialize = 0;
  }

  Rotary_Encoder();
  if (Encoder_Position_New > Encoder_Position_Old) { // Watch the Rotary Encoder and add to the number of Laps
    Num_Laps++;
    Time_Reference_Debounce = Time_Current;
    if (Num_Laps > MAX_LAPS) {
      Num_Laps = MIN_LAPS;
    }
    Screen_Rotary_Update = 1;
  } else if (Encoder_Position_New < Encoder_Position_Old) { // Watch the Rotary Encoder and subtract from the number of Laps
    Time_Reference_Debounce = Time_Current;
    Num_Laps--;
    if (Num_Laps < MIN_LAPS) {
      Num_Laps = MAX_LAPS;
    }
    Screen_Rotary_Update = 1;
  }

  if (Screen_Rotary_Update == 1) { // Update the number of laps and display on LCD
    playSdWav1.play("TICK.WAV");
    lcd.setCursor(7, 1);
    lcd.print(Num_Laps);
    if (Num_Laps < 10) {
      lcd.setCursor(8, 1);
      lcd.print(" ");
    }
    Time_Reference_Debounce = Time_Current;
    Encoder_Position_Old = Encoder_Position_New;
  }

  if (Monitor_Start == 1 && Monitor_Last_Press_Start == 0 && Time_Current > (Time_Reference_Debounce + Debounce_Button)) { // Move on to Car Number Select Menu
    Time_Reference_Debounce = Time_Current;
    Menu_Number_of_Laps = 0;
    Menu_Car_Num_Lane = 1;
    Toggle_Menu_Initialize = 1;
  }
}

// Given a current lane number, it will translate that to a starting search index and find the next available lane searching in increasing lane order
int next_lane_up(int start_index) {
  if (start_index < 0 || start_index >= Num_Lanes) { start_index = 0; }

  int searches = 0;
  for (int l = start_index; searches < Num_Lanes; l++){
    if (lanes[l].car_p == NULL) { return lanes[l].number; }
    if (l >= Num_Lanes - 1) { l = -1; }
    searches++;
  }
  return 0;
}

// Given a current lane number, it will translate that to a starting search index and find the next available lane searching in decreasing lane order
int next_lane_down(int start_index) {
  if (start_index <= 1 || start_index > Num_Lanes) { start_index = Num_Lanes + 1; }

  int searches = 0;
  for (int l = (start_index - 2); searches < Num_Lanes; l--){
    if (lanes[l].car_p == NULL) { return lanes[l].number; }
    if (l <= 0) { l = Num_Lanes; }
    searches++;
  }
  return 0;
}

// Select a lane to assign to a car directly afterwards
int Car_Lane_Select() {
  int cur_lane = next_lane_up(0);
  lcd.clear();
  lcd.setCursor(1, 0);
  lcd.print("Select Lane");
  lcd.setCursor(6, 0);
  lcd.print(cur_lane);

  int start_pressed = Read_Buttons_Start();
  while(start_pressed == 0){
    Rotary_Encoder();
    if (Encoder_Position_New > Encoder_Position_Old) {  // Watch the Rotary Encoder and display the next car number
      cur_lane = next_lane_up(cur_lane);
    } else if (Encoder_Position_New < Encoder_Position_Old) {  // Watch the Rotary Encoder and display the previous car number
      cur_lane = next_lane_down(cur_lane);
    }

    if (Encoder_Position_New != Encoder_Position_Old) { // Update the lane number and display on LCD
      playSdWav1.play("TICK.WAV");
      lcd.setCursor(6, 0);
      lcd.print(cur_lane);
      Encoder_Position_Old = Encoder_Position_New;
    }

    start_pressed = Read_Buttons_Start();
    if (Read_Buttons_Back() == 1) { return BUTTON_BACK; } // If the player presses back get out of this function
  }

  return cur_lane;
}

// Centers the car's number on the LCD
void Center_Text_Car() {
  String Car_Name = Car_Names[Array_Increment];
  Center_Value = (16 - Car_Name.length()) / 2;
  lcd.setCursor(Center_Value, 1);
}

// Menu Section to Select Car Numbers per Lane
void Car_Num_Lane_Assign() {
  int Car_Names_Size = sizeof(Car_Names) / sizeof(Car_Names[0]);

  if (Toggle_Menu_Initialize == 1) {  // Initialization of the Cars Menu
    Array_Increment = 0;
    Time_Reference_Debounce = Time_Current;
    Monitor_Start = 0;
    Toggle_Menu_Initialize = 0;

    // Select the lane the user wants to put their car in
    int selected_lane = Car_Lane_Select();
    if (selected_lane == BUTTON_BACK) { return; } // If the user wanted out of the lane select don't configure anything else and exit
    Num_Lane = selected_lane;

    lcd.clear();
    lcd.setCursor(1, 0);
    lcd.print("Lane");
    lcd.setCursor(6, 0);
    lcd.print(Num_Lane);
    lcd.setCursor(8, 0);
    lcd.print("Car Num");
    Center_Text_Car();
    lcd.print(Car_Names[Array_Increment]);
    Pole_Pos_Display(Num_Lane);
  }

  Rotary_Encoder();
  if (Encoder_Position_New > Encoder_Position_Old) {  // Watch the Rotary Encoder and display the next car number
    Time_Reference_Debounce = Time_Current;
    Array_Increment++;
    if (Array_Increment >= Car_Names_Size) {
      Array_Increment = 0;
    }
  } else if (Encoder_Position_New < Encoder_Position_Old) {  // Watch the Rotary Encoder and display the previous car number
    Time_Reference_Debounce = Time_Current;
    Array_Increment--;
    if (Array_Increment < 0) {
      Array_Increment = Car_Names_Size - 1;
    }
  }

  if (Encoder_Position_New != Encoder_Position_Old) { // Update the car number and display on LCD
    playSdWav1.play("TICK.WAV");
    lcd.setCursor(0, 1);
    lcd.print("                ");
    Center_Text_Car();
    lcd.print(Car_Names[Array_Increment]);
    Pole_Pos_Display(Num_Lane);
    Encoder_Position_Old = Encoder_Position_New;
    Time_Reference_Debounce = Time_Current;
  }

  if (Monitor_Start != 1 || Monitor_Last_Press_Start != 0 || Time_Current <= (Time_Reference_Debounce + Debounce_Button)) { return; }

  Toggle_Menu_Initialize = 1;
  cars[Configured_Racers].number = Array_Increment;
  cars[Configured_Racers].lane = Num_Lane;
  cars[Configured_Racers].place = Num_Lane;

  // Look for the lane struct that matches the lane number selected
  for (int l = 0; l < Num_Lanes; l++) {
    if (lanes[l].number != Num_Lane) { continue; }
    lanes[l].car_p = &cars[Configured_Racers];  // Set up the 2 car & lane objects to reference each other
    cars[Configured_Racers].lane_p = &lanes[l]; // Set up the 2 car & lane objects to reference each other
    break;
  }

  Configured_Racers++;
  if (Configured_Racers == Num_Racers) {
    // We want to ensure all cars start in lane order
    qsort(cars, Num_Lanes, sizeof(struct Car), lane_order);
    Menu_Start_Race = 1;
    Menu_Car_Num_Lane = 0;
    return;
  }
}

// Menu Section to Clear Lap Record from EEPROM
void Clear_Record_Lap() {
  int Screen_Rotary_Update = 0;

  if (Toggle_Menu_Initialize == 1) { // Initialization of the EEPROM Menu
    Array_Increment = 0;
    Time_Reference_Debounce = Time_Current;

    Monitor_Start = 0;
    Toggle_Menu_Initialize = 0;
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("ERASE LAP RECORD");
    lcd.setCursor(7, 1);
    lcd.print(Rec_Reset[Array_Increment]);
  }

  Rotary_Encoder();
  if (Encoder_Position_New > Encoder_Position_Old) { // Watch the Rotary Encoder and display the next Next Option
    Time_Reference_Debounce = Time_Current;
    Array_Increment++;
    if (Array_Increment > 19) {
      Array_Increment = 0;
    }
    Screen_Rotary_Update = 1;
  } else if (Encoder_Position_New < Encoder_Position_Old) { // Watch the Rotary Encoder and display the previous Option
    Time_Reference_Debounce = Time_Current;
    Array_Increment--;
    if (Array_Increment < 0) {
      Array_Increment = 19;
    }
    Screen_Rotary_Update = 1;
  }

  if (Screen_Rotary_Update == 1) { // Update the Next Option and display on LCD
    playSdWav1.play("TICK.WAV");
    lcd.setCursor(1, 1);
    lcd.print("                ");
    Center_Text_EEPROM();
    lcd.print(Rec_Reset[Array_Increment]);
    Encoder_Position_Old = Encoder_Position_New;
    Time_Reference_Debounce = Time_Current;
  }

  if (Monitor_Start != 1 || Monitor_Last_Press_Start != 0 || Time_Current <= (Time_Reference_Debounce + Debounce_Button)) { return; }

  if (Array_Increment == 10) { // Reset Lap Record Variables and move on to Race Start
    Record_Lap = 99999;
    Record_Car_Num = 10;
    Record_Car = 10;
    LapRecord();
    LapRecordDisplay();
  } else { // Do Not erase EEPROM and move on to Race Start
    Toggle_Menu_Initialize = 1;
    Menu_Options = 1;
    Options_Clear_Lap_Record = 0;
  }
}

// Centers the EEPROM Menu text on the LCD
void Center_Text_EEPROM() {
  String EEPROM_Name = Rec_Reset[Array_Increment];
  Center_Value = (16 - EEPROM_Name.length()) / 2;
  lcd.setCursor(Center_Value, 1);
}

// Inital Display of Car Numbers on 7 Segment Displays
void Pole_Pos_Display(int lane_num) {
  // If it's an odd numbered Num_Lane, display the car number to the left, even numbered player to the right
  if (lane_num % 2 != 0) {
    Player_PolePositions[lane_num - 1].writeDigitAscii(2, Car_Numbers[Array_Increment][0]);
    Player_PolePositions[lane_num - 1].writeDigitAscii(3, Car_Numbers[Array_Increment][1]);
  } else {
    Player_PolePositions[lane_num - 1].writeDigitAscii(0, Car_Numbers[Array_Increment][0]);
    Player_PolePositions[lane_num - 1].writeDigitAscii(1, Car_Numbers[Array_Increment][1]);
  }
  Player_PolePositions[lane_num - 1].writeDisplay();
}

// Race Start LED Animation/Sounds and Penalty Monitoring
void Start_Race() {
  if (Toggle_Menu_Initialize == 1) {  // Initialization of the Race Start Sequence
    FastLED.clear();
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Gentlemen, Start");
    lcd.setCursor(2, 1);
    lcd.print("Your Engines");
    LEDS.setBrightness(84);
    Toggle_Menu_Initialize = 0;

    //Turn on the Red Lights
    for (int i = 0; i < NP_Boot_Transitions; i++) {
      leds[NP_Race_Start_Red[i]] = CHSV(NP_Boot_Colors[2], 255, 255);
      delay(10);
    }

    FastLED.show();
    playSdWav1.play("RED.WAV");
    delay(Delay_Red_Light);

    // Enable Power to all Lanes
    for (int l = 0; l < Num_Lanes; l++) {
      digitalWrite(lanes[l].relay, LOW);
    }

    Time_Reference_Debounce = Time_Current;
    Array_Increment = 0;
  }

  // Yellow light flashing sequence, unless a penalty has occurred in a lane, see below
  if (Time_Current > (Time_Reference_Debounce + Delay_Yellow_Light) && Array_Increment < 3) {
    for (int l = 0; l < Num_Lanes; l++) {
      if (lanes[l].penalty == 1) { continue; }
      leds[lanes[l].np[Array_Increment]] = CHSV(NP_Boot_Colors[1], 255, 255);
    }

    Time_Reference_Debounce = Time_Current;
    playSdWav1.play("YELLOW.WAV");
    FastLED.show();
    Array_Increment++;
  }

  // Watches the Lanes for a premature start line cross and flags with a Penalty
  int penalty = 0;
  for (int l = 0; l < Num_Lanes; l++) {
    if (lanes[l].state != 0) { continue; }
    lanes[l].penalty = lanes[l].state == 0;
    penalty = 1;
  }

  // If a car crosses the start line before the green light, they are flagged with a penalty and will be part of this function below
  if (penalty == 1) { Penality_Start(); }

  // Green Light for non-penalty cars for the Start of the Race
  if (Time_Current > (Time_Reference_Debounce + Delay_Yellow_Light) && Array_Increment == 3) {
    for (int i = 0; i < 4; i++) {
      for (int l = 0; l < Num_Lanes; l++) {
        if (lanes[l].penalty == 0) {
          leds[lanes[l].np[i]] = CHSV(NP_Boot_Colors[0], 255, 255);
        }
      }
      delay(10);
    }

    playSdWav1.play("GREEN.WAV");
    FastLED.show();
    Menu_Start_Race = 0;
    Toggle_Race_Metrics = 1;
    Toggle_Menu_Initialize = 1;
    Time_Reference_Debounce = Time_Current;
  }
}

// If a car crosses the start line before the green light, they are flagged with a penality and the red lights turn on over the lane and power is cut for the penality duration
void Penality_Start() {
  for (int l = 0; l < Num_Lanes; l++) {
    if (lanes[l].penalty == 0) { continue; }
    digitalWrite(lanes[l].relay, HIGH);
    for (int i = 0; i < 3; i++) {
      leds[lanes[l].np[i]] = CHSV(NP_Boot_Colors[2], 255, 255);
    }
  }
  FastLED.show();
}

// Temorarpy Pause of the Race
void Pause_Race() {
  if (Toggle_Menu_Initialize == 1) { // Cut power to all lanes
    for (int l = 0; l < Num_Lanes; l++) {
      digitalWrite(lanes[l].relay, HIGH);
    }

    playSdWav1.play("PAUSE.WAV");
    Time_Offset_Pause = Time_Current; // This is the time the race was paused so it can be adjusted for the current lap
    Toggle_Menu_Initialize = 0;
  }

  // Flash Yellow Lights and display message on LCD
  if (Time_Current > (Time_Reference_Debounce + Delay_Yellow_Light) && Toggle_Race_Hazard == 0) {
    for (int i = 0; i < NUM_LEDS; i++) {
      leds[i] = CHSV(NP_Boot_Colors[1], 255, 255);
      delay(10);
    }
    lcd.clear();
    lcd.setCursor(2, 0);
    lcd.print("Race Paused!");
    Toggle_Race_Hazard = 1;
    Time_Reference_Debounce = Time_Current;
  }

  // Flash Yellow Lights and display message on LCD
  if (Time_Current > (Time_Reference_Debounce + Delay_Yellow_Light) && Toggle_Race_Hazard == 1) {
    FastLED.clear();
    lcd.clear();
    lcd.setCursor(2, 0);
    lcd.print("Press Start");
    lcd.setCursor(2, 1);
    lcd.print("to Continue");
    Toggle_Race_Hazard = 0;
    Time_Reference_Debounce = Time_Current;
  }
  FastLED.show();

  // Resumes Race
  if (Monitor_Start == 1 && Monitor_Last_Press_Start == 0 && Time_Current > (Time_Reference_Debounce + Debounce_Button)) {
    lcd.clear();
    lcd.setCursor(2, 0);
    lcd.print("Resuming Race");
    FastLED.clear();
    for (int i = 0; i < NP_Boot_Transitions; i++) {
      leds[NP_Race_Start_Red[i]] = CHSV(NP_Boot_Colors[2], 255, 255);
      delay(10);
    }
    playSdWav1.play("PAUSE.WAV"); // Neopixels go through Start sequence again and power is restores to all Lanes
    FastLED.show();
    delay(Delay_Red_Light);
    for (int i = 0; i < 3; i++) {
      playSdWav1.play("YELLOW.WAV");
      for (int l = 0; l < Num_Lanes; l++) {
        leds[lanes[l].np[i]] = CHSV(NP_Boot_Colors[1], 255, 255);
      }
      FastLED.show();
      delay(Delay_Yellow_Light);
    }
    for (int l = 0; l < Num_Lanes; l++) {
      digitalWrite(lanes[l].relay, LOW);
    }
    for (int i = 0; i < 4; i++) {
      playSdWav1.play("GREEN.WAV");
      for (int l = 0; l < Num_Lanes; l++) {
        leds[lanes[l].np[i]] = CHSV(NP_Boot_Colors[0], 255, 255);
      }
    }
    FastLED.show();
    Toggle_Race_Pause = 0;
    Toggle_Race_Metrics = 1;
    Toggle_Menu_Initialize = 1;
  }
}

// Stops race completely, Kills power to all lanes and resets unit for new race
void Stop_Race() {
  for (int i = 0; i < NUM_LEDS; i++) {  //Set all Lights to Red
    leds[i] = CHSV(NP_Boot_Colors[2], 255, 255);
    delay(10);
  }
  FastLED.show();
  lcd.clear();
  lcd.setCursor(3, 0);
  lcd.print("Race Ended");
  for (int l = 0; l < Num_Lanes; l++) { // Cut Power to all Lanes
    digitalWrite(lanes[l].relay, HIGH);
  }
  delay(Delay_Stop_Race);
  for (int i = 84; i >= 0; i--) {  //Fade out Animation
    if (i > 1) {
      i--;
    }
    LEDS.setBrightness(i);
    FastLED.show();
    delay(Delay_Dim);
  }
  Menu_Number_of_Racers = 1;
  Toggle_Menu_Initialize = 1;
  Toggle_Race_Stop = 0;
  Race_Over = 1;
}

// Monitors All Race Attributes
void Race_Metrics() {
  if (Toggle_Menu_Initialize == 1 && Race_Over == 0) {
    LEDS.setBrightness(NP_Brightness);  // Initialize Settings for race
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Race In Progress");
    Toggle_Menu_Initialize = 0;
    Time_Current = millis();
    for (int c = 0; c < Num_Racers; c++) {
      cars[c].total_time = Time_Current;
    }
  }

  // Monitor the Stop Button to cancel Race
  if (Monitor_Stop == 1) {
    Toggle_Race_Stop = 1;
    Toggle_Race_Metrics = 0;
    Stop_Race();
  }

  // Monitor the Start Button to Pause Race
  if (Monitor_Start == 1 && Toggle_Race_Metrics == 1) {
    Toggle_Menu_Initialize = 1;
    Toggle_Race_Metrics = 0;
    Toggle_Race_Pause = 1;
  }

  // Watches for penalty flags on any lane, restores power to the lane(s) after penalty duration
  int penalty = 0;
  for (int l = 0; l < Num_Lanes; l++) {
    if (lanes[l].penalty == 0 || Time_Current <= (Time_Reference_Debounce + Delay_Penality)) { continue; }

    // If there was a penalty on the lane and the penalty delay has expired
    digitalWrite(lanes[l].relay, LOW);
    lanes[l].penalty = 0;

    for (int i = 0; i < 4; i++) {
      leds[lanes[l].np[i]] = CHSV(NP_Boot_Colors[0], 255, 255);
    }

    penalty = 1;
  }

  if (penalty == 1) { FastLED.show(); }

  // Reads the lap counter pins for car crossings
  for (int l = 0; l < Num_Lanes; l++) {
    lanes[l].state = digitalRead(lanes[l].monitor_lap);
  }

  // When a car crosses the pin, record the current lap time and normalize for the display, mark time for Last Lap and increment the Lap counter
  for (int c = 0; c < Num_Racers; c++) {
    if (cars[c].lane_p->state == LOW && Time_Current > (cars[c].total_time + Debounce_Track)) {
      cars[c].lap_time = (Time_Current - cars[c].total_time);
      cars[c].total_time = Time_Current;
      cars[c].cur_lap = cars[c].cur_lap + 1;

      Lap_Counter();
      DetermineRacePlace();
    }
  }

  // Monitor for Lap Record
  for (int c = 0; c < Num_Racers; c++) {
    if (cars[c].lap_time < Record_Lap && (cars[c].lap_time) > Debounce_Track) {
      Record_Lap = cars[c].lap_time;
      Record_Car_Num = c;
      Record_Car = cars[Record_Car_Num].number;
      LapRecordDisplay();
    }
  }
}

// Reads Lap Record from EEPROM and Displays on 7 Sgement Displays
void LapRecordDisplay() {
  char LapTimeRec_String[5];
  unsigned short LapTimeRec_Display = (Record_Lap > 999999 ? 999999 : Record_Lap) / 1000; // Limit the lap time we'll display to ###.# seconds from milliseconds
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
      Current_Lap_Num = max_lap; // Update and the Lap counter
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
  unsigned short PlayerLapTimes_Display[4];
  char PlayerLapTimes_Strings[4][5];

  // Clear out the digital displays to prepare for writing the new data
  int Player_Index;
  for (int Player = 1; Player <= Num_Racers; Player++) {
    Player_Index = Player - 1;
    Player_Times[Player_Index].clear();
    Player_Times[Player_Index].writeDisplay();
    Player_PolePositions[Player_Index].clear();
    Player_PolePositions[Player_Index].writeDisplay();
  }

  // Write all the player lap times and pole positions
  for (int Player = 1; Player <= Num_Racers; Player++) {
    Player_Index = Player - 1;
    // Limit the lap time we'll display to ###.# seconds from milliseconds
    PlayerLapTimes_Display[Player_Index] = (cars[Player_Index].lap_time > 999999 ? 999999 : cars[Player_Index].lap_time) / 1000;
    sprintf(PlayerLapTimes_Strings[Player_Index], "%4hu", PlayerLapTimes_Display[Player_Index]);

    // Write the player lap time to the ascii buffer
    Player_Times[Player_Index].writeDigitAscii(0, PlayerLapTimes_Strings[Player_Index][0]);
    Player_Times[Player_Index].writeDigitAscii(1, PlayerLapTimes_Strings[Player_Index][1], true);
    Player_Times[Player_Index].writeDigitAscii(2, PlayerLapTimes_Strings[Player_Index][2]);
    Player_Times[Player_Index].writeDigitAscii(3, PlayerLapTimes_Strings[Player_Index][3]);

    // If it's an odd numbered player, display the car number to the left, even numbered player to the right
    if (Player % 2 != 0) {
      Player_PolePositions[Player_Index].writeDigitAscii(2, Car_Numbers[cars[Player_Index].number][0]);
      Player_PolePositions[Player_Index].writeDigitAscii(3, Car_Numbers[cars[Player_Index].number][1]);
    } else {
      Player_PolePositions[Player_Index].writeDigitAscii(0, Car_Numbers[cars[Player_Index].number][0]);
      Player_PolePositions[Player_Index].writeDigitAscii(1, Car_Numbers[cars[Player_Index].number][1]);
    }

    // Write the player lap time and pole position to the display
    Player_Times[Player_Index].writeDisplay();
    Player_PolePositions[Player_Index].writeDisplay();
  }

  // Now we need to get back into lane order
  qsort(cars, Num_Racers, sizeof(struct Car), lane_order);

  LapCountdown();
}

// Display The Correct Number of Laps LED Pattern
void LapCountdown() {
  for (int c = 0; c < Num_Racers; c++) {
    switch (Num_Laps - cars[c].cur_lap) {
      case 1: // 1 lap remaining
        leds[cars[c].lane_p->np[0]] = CRGB(0, 0, 0);
        leds[cars[c].lane_p->np[1]] = CHSV(NP_Boot_Colors[0], 255, 255);
        leds[cars[c].lane_p->np[2]] = CRGB(0, 0, 0);
        leds[cars[c].lane_p->np[3]] = CRGB(0, 0, 0);
        break;
      case 2: // 2 laps remaining
        leds[cars[c].lane_p->np[0]] = CHSV(NP_Boot_Colors[0], 255, 255);
        leds[cars[c].lane_p->np[1]] = CRGB(0, 0, 0);
        leds[cars[c].lane_p->np[2]] = CHSV(NP_Boot_Colors[0], 255, 255);
        leds[cars[c].lane_p->np[3]] = CRGB(0, 0, 0);
        break;
      case 3: // 3 laps remaining
        leds[cars[c].lane_p->np[0]] = CHSV(NP_Boot_Colors[0], 255, 255);
        leds[cars[c].lane_p->np[1]] = CHSV(NP_Boot_Colors[0], 255, 255);
        leds[cars[c].lane_p->np[2]] = CHSV(NP_Boot_Colors[0], 255, 255);
        leds[cars[c].lane_p->np[3]] = CRGB(0, 0, 0);
        break;
    }
  }

  FastLED.show();
}

// Final Lap and Finish Actions
void End_Race() {
  // At Last Lap Turn all LEDs white per lane and play the Last Lap song
  if (Num_Laps == Current_Lap_Num && Toggle_Menu_Initialize == 1) {
    Toggle_Menu_Initialize = 0;
    Last_Lap = 1;
  }

  // Determine when a car is on its last lap
  for (int c = 0; c < Num_Racers; c++) {
    if (Num_Laps <= cars[c].cur_lap && cars[c].last_lap == 0) {
      if (sound_buffer <= (Time_Current - 3500)) {
        sound_buffer = Time_Current;
        playSdWav1.play("LASTLAP.WAV");
      }
      cars[c].last_lap = 1;
      for (int i = 0; i < 4; i++) {
        leds[cars[c].lane_p->np[i]] = CRGB(255, 255, 255);
      }
    }
  }

  FastLED.show();

  // Action When a Car Finishes the Race
  for (int c = 0; c < Num_Racers; c++) {
    if (cars[c].cur_lap <= Num_Laps || cars[c].finish != 0) { continue; }
    digitalWrite(cars[c].lane_p->relay, HIGH); // Cut Power to the Lane
    cars[c].finish = 1;
    for (int i = 0; i < 4; i++) {
      leds[cars[c].lane_p->np[i]] = CRGB(0, 0, 0);
    }
    FastLED.show();
    delay(10);

    // Display The Correct Position LED Pattern
    switch (cars[c].place) {
      case 1:
        sound_buffer = Time_Current;
        leds[cars[c].lane_p->np[1]] = CRGB(0, 255, 255);
        break;
      case 2:
        if (sound_buffer <= (Time_Current - 8500)) {
          playSdWav1.play("RECORD.WAV");
        }
        leds[cars[c].lane_p->np[0]] = CRGB(0, 255, 255);
        leds[cars[c].lane_p->np[2]] = CRGB(0, 255, 255);
        break;
      case 3:
        if (sound_buffer <= (Time_Current - 8500)) {
          playSdWav1.play("RECORD.WAV");
        }
        for (int i = 0; i < 3; i++) {
          leds[cars[c].lane_p->np[i]] = CRGB(0, 255, 255);
        }
        break;
      case 4:
        if (sound_buffer <= (Time_Current - 8500)) {
          playSdWav1.play("RECORD.WAV");
        }
        for (int i = 0; i < 4; i++) {
          leds[cars[c].lane_p->np[i]] = CRGB(0, 255, 255);
        }
        break;
    }
  
    FastLED.show();
  }

  // Determine the number of cars that have finished the race
  int cars_finished = 0;
  for (int c = 0; c < Num_Racers; c++) {
    if (cars[c].finish == 1) {
      cars_finished++;
    }
  }

  // When the first car crosses the finish line play the finish Song
  if (First_Car_Finish == 0 && cars_finished >= 1) {
    playSdWav1.play("FINISH.WAV");
    First_Car_Finish = 1;
  }

  // End Race After all Cars Cross the Finish Line
  if (Num_Racers == cars_finished && Race_Over == 0) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Race Finished!!");
    Race_Over = 1;
  }
}

//Reset all Variables and 7 Segment Displays from Previous Race and Record Lap Record
void ClearRace() {
  // Write Lap Record to EEPROM
  LapRecord();
  // Set All Race Values Back to Defaults
  Current_Lap_Num = 0;
  Race_Over = 0;
  First_Car_Finish = 0;
  Last_Lap = 0;
  Num_Laps = 5;
  Num_Racers = 4;
  Num_Lane = 1;
  Configured_Racers = 0;

  // Set all cars back to their default values
  cars[0] = (struct Car){ .lane = 96, .number = 10, .cur_lap = 0, .place = 1, .lap_time = 0, .total_time = 0, .last_lap = 0, .finish = 0 };
  cars[1] = (struct Car){ .lane = 97, .number = 10, .cur_lap = 0, .place = 2, .lap_time = 0, .total_time = 0, .last_lap = 0, .finish = 0 };
  cars[2] = (struct Car){ .lane = 98, .number = 10, .cur_lap = 0, .place = 3, .lap_time = 0, .total_time = 0, .last_lap = 0, .finish = 0 };
  cars[3] = (struct Car){ .lane = 99, .number = 10, .cur_lap = 0, .place = 4, .lap_time = 0, .total_time = 0, .last_lap = 0, .finish = 0 };

  // Set up each of our lanes with default values
  lanes[0] = (struct Lane){ .number = 1, .np = { 11, 10, 9, 12 }, .relay = RELAY_LANE_1, .monitor_lap = MONITOR_LAP_LANE_1, .state = 0, .penalty = 0 };
  lanes[1] = (struct Lane){ .number = 2, .np = { 8, 7, 6, 13 },   .relay = RELAY_LANE_2, .monitor_lap = MONITOR_LAP_LANE_2, .state = 0, .penalty = 0 };
  lanes[2] = (struct Lane){ .number = 3, .np = { 5, 4, 3, 14 },   .relay = RELAY_LANE_3, .monitor_lap = MONITOR_LAP_LANE_3, .state = 0, .penalty = 0 };
  lanes[3] = (struct Lane){ .number = 4, .np = { 2, 1, 0, 15 },   .relay = RELAY_LANE_4, .monitor_lap = MONITOR_LAP_LANE_4, .state = 0, .penalty = 0 };

  // Clear Leaderboard Display
  int Player_Index;
  for (int Player = 1; Player <= Num_Racers; Player++) {
    Player_Index = Player - 1;
    Player_Times[Player_Index].clear();
    Player_Times[Player_Index].writeDisplay();
    Player_PolePositions[Player_Index].clear();
    Player_PolePositions[Player_Index].writeDisplay();
  }

  LapTimeRec.clear();
  LapRecNum.clear();
  LapTimeRec.writeDisplay();
  LapRecNum.writeDisplay();
  FastLED.show();
  LapRecordDisplay();
}
