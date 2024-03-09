// Libraries
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

//7 Seg LED Assignments
Adafruit_AlphaNum4 P1P2Num = Adafruit_AlphaNum4();     //Pole Position Car Numbers 1 & 2
Adafruit_AlphaNum4 P3P4Num = Adafruit_AlphaNum4();     //Pole Position Car Numbers 3 & 4
Adafruit_AlphaNum4 LapRecNum = Adafruit_AlphaNum4();   //Lap Counter and Lap Record Car Number
Adafruit_AlphaNum4 P1Time = Adafruit_AlphaNum4();      //Pole Position Time 1
Adafruit_AlphaNum4 P2Time = Adafruit_AlphaNum4();      //Pole Position Time 2
Adafruit_AlphaNum4 P3Time = Adafruit_AlphaNum4();      //Pole Position Time 3
Adafruit_AlphaNum4 P4Time = Adafruit_AlphaNum4();      //Pole Position Time 4
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
int Screen_Rotary_Update = 0;
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
int Debounce_Track = 1000;   //Default debounce time (Milliseconds) when a car passes the start line (Can be Modified in Options 500-10000 and saved to EEPROM)

//Race Identifiers
int Num_Laps = 5;    //Default number of laps in the Race (Can be Modified in Menu 5-99)
int Num_Racers = 4;  //Default number of Racers in the Race (Can be Modified in Menu 1-4)
int Num_Lane = 1;       //Used for Lane Assignment of Drivers/Car/Lap Times

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
unsigned long Record_Lap = 10000;  //Default Lap Record Time (Actual is called from EEPROM)
int Record_Car_Num;                //Array Identifer of the record setting car
int Record_Car;                     //Lap Record Car Number (Value is called from EEPROM)
int L1_State = 0;                  //In Track Lap Counter Monitors State, per lane
int L2_State = 0;
int L3_State = 0;
int L4_State = 0;
int L1_Last_Lap = 0;  //Flag to signal last Lap Neopixel and Sound Events, per lane
int L2_Last_Lap = 0;
int L3_Last_Lap = 0;
int L4_Last_Lap = 0;
unsigned long sound_buffer;  //Time (Milliseconds) buffer to avoid sound stomping on eachother

// Struct (or class/object) that defines everything that a car needs to have
struct Car {
  int lane;                  // Lane the car is in
  int car_number;            // The number that represents which car type is in use
  int cur_lap;               // Current lap the car is on
  int place;                 // Place the car is currently in
  unsigned long lap_time;    // The time (Milliseconds) of the last time
  unsigned long total_time;  // Total race time (Milliseconds)
  int finish;                // Has the car finished the race
};

// Declare our cars array and fill them in with default values in the loop
struct Car *cars = (Car *)malloc(Num_Racers * sizeof *cars);

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
int Penality_Lane1 = 1;  //Flag if car crosses start line before the green light, per lane
int Penality_Lane2 = 1;
int Penality_Lane3 = 1;
int Penality_Lane4 = 1;
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

  //Read EEPROM Variables and replace default values
  Record_Lap = EEPROMReadlong(0x02);
  Record_Car = EEPROM.read(0x00);
  Delay_Penality = EEPROMReadInt(0x08);
  Delay_Stop_Race = EEPROMReadInt(0x06);
  Debounce_Track = EEPROMReadInt(0x10);

  //Setup the 7-Segment LED Panels
  P1P2Num.begin(0x70);     // pass in the address for the Place 1 and 2 Car Numbers
  P3P4Num.begin(0x71);     // pass in the address for the Place 3 and 4 Car Numbers
  LapRecNum.begin(0x72);   // pass in the address for the Lap Counter and Lap Record Car Number
  LapTimeRec.begin(0x77);  // pass in the address for the Lap Record Time
  P1Time.begin(0x73);      // pass in the address for the Place 1 Lap Time
  P2Time.begin(0x74);      // pass in the address for the Place 2 Lap Time
  P3Time.begin(0x75);      // pass in the address for the Place 3 Lap Time
  P4Time.begin(0x76);      // pass in the address for the Place 4 Lap Time

  // set up the LCD's number of rows and columns and enable the backlight
  lcd.begin(16, 2);
  lcd.setBacklight(HIGH);

  //Pin Mode Assignments
  pinMode(BUTTON_RE, INPUT);
  pinMode(BUTTON_BACK, INPUT);
  pinMode(BUTTON_START, INPUT);
  pinMode(BUTTON_STOP, INPUT);
  pinMode(LED_NOTIFICATION, OUTPUT);
  pinMode(MONITOR_LAP_LANE_1, INPUT_PULLUP);
  pinMode(MONITOR_LAP_LANE_2, INPUT_PULLUP);
  pinMode(MONITOR_LAP_LANE_3, INPUT_PULLUP);
  pinMode(MONITOR_LAP_LANE_4, INPUT_PULLUP);
  pinMode(RELAY_LANE_1, OUTPUT);
  pinMode(RELAY_LANE_2, OUTPUT);
  pinMode(RELAY_LANE_3, OUTPUT);
  pinMode(RELAY_LANE_4, OUTPUT);

  //Neopixel Setup
  LEDS.addLeds<WS2812, DATA_PIN, RGB>(leds, NUM_LEDS);
  LEDS.setBrightness(NP_Brightness);

  //Disable Lanes before Race
  digitalWrite(RELAY_LANE_1, HIGH);
  digitalWrite(RELAY_LANE_2, HIGH);
  digitalWrite(RELAY_LANE_3, HIGH);
  digitalWrite(RELAY_LANE_4, HIGH);

  // Set up each of our cars with default values
  cars[0] = (struct Car){ .lane = 1, .car_number = 10, .cur_lap = 0, .place = 1, .lap_time = 0, .total_time = 0, .finish = 0 };
  cars[1] = (struct Car){ .lane = 2, .car_number = 10, .cur_lap = 0, .place = 2, .lap_time = 0, .total_time = 0, .finish = 0 };
  cars[2] = (struct Car){ .lane = 3, .car_number = 10, .cur_lap = 0, .place = 3, .lap_time = 0, .total_time = 0, .finish = 0 };
  cars[3] = (struct Car){ .lane = 4, .car_number = 10, .cur_lap = 0, .place = 4, .lap_time = 0, .total_time = 0, .finish = 0 };
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
    L1_State = digitalRead(MONITOR_LAP_LANE_1);
    L2_State = digitalRead(MONITOR_LAP_LANE_2);
    L3_State = digitalRead(MONITOR_LAP_LANE_3);
    L4_State = digitalRead(MONITOR_LAP_LANE_4);
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
    Screen_Rotary_Update = 0;
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
    Screen_Rotary_Update = 0;
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
    Screen_Rotary_Update = 0;
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

//Stop Race Timeout Value Selection and Set
void Menu_Stop_Race() {
  if (Toggle_Menu_Initialize == 1) {
    lcd.clear();
    lcd.setCursor(2, 0);
    lcd.print("End Timeout");
    lcd.setCursor(6, 1);
    lcd.print(Delay_Stop_Race);
    Toggle_Menu_Initialize = 0;
  }
  Rotary_Encoder();
  if (Encoder_Position_New > Encoder_Position_Old) {  //Watch the Rotary Encoder and add
    Time_Reference_Debounce = Time_Current;
    Delay_Stop_Race = Delay_Stop_Race + 500;
    if (Delay_Stop_Race > 10000) {
      Delay_Stop_Race = 10000;
    }
    Screen_Rotary_Update = 1;
  }
  if (Encoder_Position_New < Encoder_Position_Old) {  //Watch the Rotary Encoder and subtract
    Time_Reference_Debounce = Time_Current;
    Delay_Stop_Race = Delay_Stop_Race - 500;
    if (Delay_Stop_Race < 1000) {
      Delay_Stop_Race = 1000;
    }
    Screen_Rotary_Update = 1;
  }
  if (Screen_Rotary_Update == 1) {  //Scroll though the available option in the menu
    playSdWav1.play("TICK.WAV");
    lcd.clear();
    lcd.setCursor(2, 0);
    lcd.print("End Timeout");
    lcd.setCursor(6, 1);
    lcd.print(Delay_Stop_Race);
    Time_Reference_Debounce = Time_Current;
    Encoder_Position_Old = Encoder_Position_New;
    Screen_Rotary_Update = 0;
  }
  if (Monitor_Start == 1 && Monitor_Last_Press_Start == 0 && Time_Current > (Time_Reference_Debounce + Debounce_Button)) {  //Set the desired value for Penalty Duration and save to EEPROM
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
//Menu Section for selecting # of racers
void Number_of_Racers() {
  //If the Race is Over Clear All Variables from Previous Race
  if (Race_Over == 1) {
    ClearRace();
  }
  if (Toggle_Menu_Initialize == 1) {  //Initialization of the Racers Menu
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Number of Racers");
    lcd.setCursor(7, 1);
    lcd.print(Num_Racers);
    Num_Racers = 4;
    Toggle_Menu_Initialize = 0;
  }
  Rotary_Encoder();
  if (Encoder_Position_New > Encoder_Position_Old) {  //Watch the Rotary Encoder and add to the number of racers
    Time_Reference_Debounce = Time_Current;
    Num_Racers++;
    if (Num_Racers > 4) {
      Num_Racers = 4;
    }
    Screen_Rotary_Update = 1;
  }
  if (Encoder_Position_New < Encoder_Position_Old) {  //Watch the Rotary Encoder and subtract from the number of racers
    Time_Reference_Debounce = Time_Current;
    Num_Racers--;
    if (Num_Racers < 1) {
      Num_Racers = 1;
    }
    Screen_Rotary_Update = 1;
  }
  if (Screen_Rotary_Update == 1) {  //Update the number of racers and display on LCD
    playSdWav1.play("TICK.WAV");
    lcd.setCursor(7, 1);
    lcd.print(Num_Racers);
    Time_Reference_Debounce = Time_Current;
    Encoder_Position_Old = Encoder_Position_New;
    Screen_Rotary_Update = 0;
  }
  if (Monitor_Start == 1 && Monitor_Last_Press_Start == 0 && Time_Current > (Time_Reference_Debounce + Debounce_Button)) {  //Move on to Number of Laps Menu
    Time_Reference_Debounce = Time_Current;
    Menu_Number_of_Racers = 0;
    Menu_Number_of_Laps = 1;
    Toggle_Menu_Initialize = 1;
  }
}
//Menu Section to Specify Number of Laps
void Number_of_Laps() {
  if (Toggle_Menu_Initialize == 1) {  //Initialization of the Laps Menu
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
  if (Encoder_Position_New > Encoder_Position_Old) {  //Watch the Rotary Encoder and add to the number of Laps
    Num_Laps++;
    Time_Reference_Debounce = Time_Current;
    if (Num_Laps > 99) {
      Num_Laps = 5;
    }
    Screen_Rotary_Update = 1;
  }
  if (Encoder_Position_New < Encoder_Position_Old) {  //Watch the Rotary Encoder and subtract from the number of Laps
    Time_Reference_Debounce = Time_Current;
    Num_Laps--;
    if (Num_Laps < 5) {
      Num_Laps = 99;
    }
    Screen_Rotary_Update = 1;
  }
  if (Screen_Rotary_Update == 1) {  //Update the number of laps and display on LCD
    playSdWav1.play("TICK.WAV");
    lcd.setCursor(7, 1);
    lcd.print(Num_Laps);
    if (Num_Laps < 10) {
      lcd.setCursor(8, 1);
      lcd.print(" ");
    }
    Time_Reference_Debounce = Time_Current;
    Encoder_Position_Old = Encoder_Position_New;
    Screen_Rotary_Update = 0;
  }
  if (Monitor_Start == 1 && Monitor_Last_Press_Start == 0 && Time_Current > (Time_Reference_Debounce + Debounce_Button)) {  //Move on to Car Number Select Menu
    Time_Reference_Debounce = Time_Current;
    Menu_Number_of_Laps = 0;
    Menu_Car_Num_Lane = 1;
    Toggle_Menu_Initialize = 1;
  }
}
//Centers the car's number on the LCD
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
    lcd.clear();
    lcd.setCursor(1, 0);
    lcd.print("Lane");
    lcd.setCursor(6, 0);
    lcd.print(Num_Lane);
    lcd.setCursor(8, 0);
    lcd.print("Car Num");
    Center_Text_Car();
    lcd.print(Car_Names[Array_Increment]);
    Pole_Pos_Display();
  }

  Rotary_Encoder();
  if (Encoder_Position_New > Encoder_Position_Old) {  // Watch the Rotary Encoder and display the next car number
    Time_Reference_Debounce = Time_Current;
    Array_Increment++;
    if (Array_Increment >= Car_Names_Size) {
      Array_Increment = 0;
    }
    Screen_Rotary_Update = 1;
  } else if (Encoder_Position_New < Encoder_Position_Old) {  // Watch the Rotary Encoder and display the previous car number
    Time_Reference_Debounce = Time_Current;
    Array_Increment--;
    if (Array_Increment < 0) {
      Array_Increment = Car_Names_Size - 1;
    }
    Screen_Rotary_Update = 1;
  }

  if (Screen_Rotary_Update == 1) { // Update the car number and display on LCD
    playSdWav1.play("TICK.WAV");
    lcd.setCursor(0, 1);
    lcd.print("                ");
    Center_Text_Car();
    lcd.print(Car_Names[Array_Increment]);
    Pole_Pos_Display();
    Encoder_Position_Old = Encoder_Position_New;
    Time_Reference_Debounce = Time_Current;
    Screen_Rotary_Update = 0;
  }

  if (Monitor_Start != 1 || Monitor_Last_Press_Start != 0 || Time_Current <= (Time_Reference_Debounce + Debounce_Button)) { return; }

  Toggle_Menu_Initialize = 1;
  cars[Num_Lane - 1].car_number = Array_Increment;
  if (Num_Lane == Num_Racers) {
    Menu_Start_Race = 1;
    Menu_Car_Num_Lane = 0;
    return;
  }

  Num_Lane++;
}

//Menu Section to Clear Lap Record from EEPROM
void Clear_Record_Lap() {
  if (Toggle_Menu_Initialize == 1) {  //Initialization of the EEPROM Menu
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
  if (Encoder_Position_New > Encoder_Position_Old) {  //Watch the Rotary Encoder and display the next Next Option
    Time_Reference_Debounce = Time_Current;
    Array_Increment++;
    if (Array_Increment > 19) {
      Array_Increment = 0;
    }
    Screen_Rotary_Update = 1;
  }
  if (Encoder_Position_New < Encoder_Position_Old) {  //Watch the Rotary Encoder and display the previous Option
    Time_Reference_Debounce = Time_Current;
    Array_Increment--;
    if (Array_Increment < 0) {
      Array_Increment = 19;
    }
    Screen_Rotary_Update = 1;
  }
  if (Screen_Rotary_Update == 1) {  //Update the Next Option and display on LCD
    playSdWav1.play("TICK.WAV");
    lcd.setCursor(1, 1);
    lcd.print("                ");
    Center_Text_EEPROM();
    lcd.print(Rec_Reset[Array_Increment]);
    Encoder_Position_Old = Encoder_Position_New;
    Time_Reference_Debounce = Time_Current;
    Screen_Rotary_Update = 0;
  }
  if (Monitor_Start == 1 && Monitor_Last_Press_Start == 0 && Array_Increment == 10 && Time_Current > (Time_Reference_Debounce + Debounce_Button)) {  //Reset Lap Record Variables and move on to Race Start
    Record_Lap = 99999;
    Record_Car_Num = 10;
    Record_Car = 10;
    LapRecord();
    LapRecordDisplay();
  }
  if (Monitor_Start == 1 && Monitor_Last_Press_Start == 0 && Array_Increment != 10 && Time_Current > (Time_Reference_Debounce + Debounce_Button)) {  //Do Not erase EEPROM and move on to Race Start
    Toggle_Menu_Initialize = 1;
    Menu_Options = 1;
    Options_Clear_Lap_Record = 0;
  }
}
//Centers the EEPROM Menu text on the LCD
void Center_Text_EEPROM() {
  String EEPROM_Name = Rec_Reset[Array_Increment];
  Center_Value = (16 - EEPROM_Name.length()) / 2;
  lcd.setCursor(Center_Value, 1);
}
//Inital Display of Car Numbers on 7 Segment Displays
void Pole_Pos_Display() {
  if (Num_Lane == 4) {
    P3P4Num.writeDigitAscii(0, Car_Numbers[Array_Increment][0]);
    P3P4Num.writeDigitAscii(1, Car_Numbers[Array_Increment][1]);
    P3P4Num.writeDisplay();
  }
  if (Num_Lane == 3) {
    P3P4Num.writeDigitAscii(2, Car_Numbers[Array_Increment][0]);
    P3P4Num.writeDigitAscii(3, Car_Numbers[Array_Increment][1]);
    P3P4Num.writeDisplay();
  }
  if (Num_Lane == 2) {
    P1P2Num.writeDigitAscii(0, Car_Numbers[Array_Increment][0]);
    P1P2Num.writeDigitAscii(1, Car_Numbers[Array_Increment][1]);
    P1P2Num.writeDisplay();
  }
  if (Num_Lane == 1) {
    P1P2Num.writeDigitAscii(2, Car_Numbers[Array_Increment][0]);
    P1P2Num.writeDigitAscii(3, Car_Numbers[Array_Increment][1]);
    P1P2Num.writeDisplay();
  }
}
//Race Start LED Animation/Sounds and Penalty Monitoring
void Start_Race() {
  if (Toggle_Menu_Initialize == 1) {  //Initialization of the Race Start Sequence
    FastLED.clear();
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Gentlemen, Start");
    lcd.setCursor(2, 1);
    lcd.print("Your Engines");
    LEDS.setBrightness(84);
    Toggle_Menu_Initialize = 0;
    for (int i = 0; i < NP_Boot_Transitions; i++) {  //Turn on the Red Lights
      leds[NP_Race_Start_Red[i]] = CHSV(NP_Boot_Colors[2], 255, 255);
      delay(10);
    }
    FastLED.show();
    playSdWav1.play("RED.WAV");
    delay(Delay_Red_Light);
    digitalWrite(RELAY_LANE_1, LOW);  //Enable Power to all Lanes
    digitalWrite(RELAY_LANE_2, LOW);
    digitalWrite(RELAY_LANE_3, LOW);
    digitalWrite(RELAY_LANE_4, LOW);
    Time_Reference_Debounce = Time_Current;
    Array_Increment = 0;
  }
  if (Time_Current > (Time_Reference_Debounce + Delay_Yellow_Light) && Array_Increment < 3) {  //Yellow light flashing sequence, unless a penality has occurred in a lane, see below
    if (Penality_Lane1 != 0) {
      leds[NP_Lane_1[Array_Increment]] = CHSV(NP_Boot_Colors[1], 255, 255);
    }
    if (Penality_Lane2 != 0) {
      leds[NP_Lane_2[Array_Increment]] = CHSV(NP_Boot_Colors[1], 255, 255);
    }
    if (Penality_Lane3 != 0) {
      leds[NP_Lane_3[Array_Increment]] = CHSV(NP_Boot_Colors[1], 255, 255);
    }
    if (Penality_Lane4 != 0) {
      leds[NP_Lane_4[Array_Increment]] = CHSV(NP_Boot_Colors[1], 255, 255);
    }
    Time_Reference_Debounce = Time_Current;
    playSdWav1.play("YELLOW.WAV");
    FastLED.show();
    Array_Increment++;
  }
  if (L1_State == 0 || L2_State == 0 || L3_State == 0 || L4_State == 0) {  //Watches the Lanes for a premature start line cross and flags with a Penality
    if (L1_State == 0) {
      Penality_Lane1 = L1_State;
    }
    if (L2_State == 0) {
      Penality_Lane2 = L2_State;
    }
    if (L3_State == 0) {
      Penality_Lane3 = L3_State;
    }
    if (L4_State == 0) {
      Penality_Lane4 = L4_State;
    }
    Penality_Start();  //If a car crosses the start line before the green light, they are flagged with a penality and will be part of this function below
  }
  if (Time_Current > (Time_Reference_Debounce + Delay_Yellow_Light) && Array_Increment == 3) {  //Green Light for the Start of the Race
    for (int i = 0; i < 4; i++) {
      if (Penality_Lane1 != 0) {
        leds[NP_Lane_1[i]] = CHSV(NP_Boot_Colors[0], 255, 255);
      }
      if (Penality_Lane2 != 0) {
        leds[NP_Lane_2[i]] = CHSV(NP_Boot_Colors[0], 255, 255);
      }
      if (Penality_Lane3 != 0) {
        leds[NP_Lane_3[i]] = CHSV(NP_Boot_Colors[0], 255, 255);
      }
      if (Penality_Lane4 != 0) {
        leds[NP_Lane_4[i]] = CHSV(NP_Boot_Colors[0], 255, 255);
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
//If a car crosses the start line before the green light, they are flagged with a penality and the red lights turn on over the lane and power is cut for the penality duration
void Penality_Start() {
  if (Penality_Lane1 == 0) {
    digitalWrite(RELAY_LANE_1, HIGH);
    for (int i = 0; i < 3; i++) {
      leds[NP_Lane_1[i]] = CHSV(NP_Boot_Colors[2], 255, 255);
    }
  }
  if (Penality_Lane2 == 0) {
    digitalWrite(RELAY_LANE_2, HIGH);
    for (int i = 0; i < 3; i++) {
      leds[NP_Lane_2[i]] = CHSV(NP_Boot_Colors[2], 255, 255);
    }
  }
  if (Penality_Lane3 == 0) {
    digitalWrite(RELAY_LANE_3, HIGH);
    for (int i = 0; i < 3; i++) {
      leds[NP_Lane_3[i]] = CHSV(NP_Boot_Colors[2], 255, 255);
    }
  }
  if (Penality_Lane4 == 0) {
    digitalWrite(RELAY_LANE_4, HIGH);
    for (int i = 0; i < 3; i++) {
      leds[NP_Lane_4[i]] = CHSV(NP_Boot_Colors[2], 255, 255);
    }
  }
  FastLED.show();
}
//Temorarpy Pause of the Race
void Pause_Race() {
  if (Toggle_Menu_Initialize == 1) {  //Cut power to all lanes
    digitalWrite(RELAY_LANE_1, HIGH);
    digitalWrite(RELAY_LANE_2, HIGH);
    digitalWrite(RELAY_LANE_3, HIGH);
    digitalWrite(RELAY_LANE_4, HIGH);
    playSdWav1.play("PAUSE.WAV");
    Time_Offset_Pause = Time_Current;  //This is the time the race was paused so it can be adjusted for the current lap
    Toggle_Menu_Initialize = 0;
  }
  if (Time_Current > (Time_Reference_Debounce + Delay_Yellow_Light) && Toggle_Race_Hazard == 0) {  //Flash Yellow Lights and display message on LCD
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
  if (Time_Current > (Time_Reference_Debounce + Delay_Yellow_Light) && Toggle_Race_Hazard == 1) {  //Flash Yellow Lights and display message on LCD
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

  if (Monitor_Start == 1 && Monitor_Last_Press_Start == 0 && Time_Current > (Time_Reference_Debounce + Debounce_Button)) {  //Resumes Race
    lcd.clear();
    lcd.setCursor(2, 0);
    lcd.print("Resuming Race");
    FastLED.clear();
    for (int i = 0; i < NP_Boot_Transitions; i++) {
      leds[NP_Race_Start_Red[i]] = CHSV(NP_Boot_Colors[2], 255, 255);
      delay(10);
    }
    playSdWav1.play("PAUSE.WAV");  //Neopixels go through Start sequence again and power is restores to all Lanes
    FastLED.show();
    delay(Delay_Red_Light);
    for (int i = 0; i < 3; i++) {
      playSdWav1.play("YELLOW.WAV");
      leds[NP_Lane_1[i]] = CHSV(NP_Boot_Colors[1], 255, 255);
      leds[NP_Lane_2[i]] = CHSV(NP_Boot_Colors[1], 255, 255);
      leds[NP_Lane_3[i]] = CHSV(NP_Boot_Colors[1], 255, 255);
      leds[NP_Lane_4[i]] = CHSV(NP_Boot_Colors[1], 255, 255);
      FastLED.show();
      delay(Delay_Yellow_Light);
    }
    digitalWrite(RELAY_LANE_1, LOW);
    digitalWrite(RELAY_LANE_2, LOW);
    digitalWrite(RELAY_LANE_3, LOW);
    digitalWrite(RELAY_LANE_4, LOW);
    for (int i = 0; i < 4; i++) {
      playSdWav1.play("GREEN.WAV");
      leds[NP_Lane_1[i]] = CHSV(NP_Boot_Colors[0], 255, 255);
      leds[NP_Lane_2[i]] = CHSV(NP_Boot_Colors[0], 255, 255);
      leds[NP_Lane_3[i]] = CHSV(NP_Boot_Colors[0], 255, 255);
      leds[NP_Lane_4[i]] = CHSV(NP_Boot_Colors[0], 255, 255);
    }
    FastLED.show();
    Toggle_Race_Pause = 0;
    Toggle_Race_Metrics = 1;
    Toggle_Menu_Initialize = 1;
  }
}
//Stops race completely, Kills power to all lanes and resets unit for new race
void Stop_Race() {
  for (int i = 0; i < NUM_LEDS; i++) {  //Set all Lights to Red
    leds[i] = CHSV(NP_Boot_Colors[2], 255, 255);
    delay(10);
  }
  FastLED.show();
  lcd.clear();
  lcd.setCursor(3, 0);
  lcd.print("Race Ended");
  digitalWrite(RELAY_LANE_1, HIGH);  //Cut Power to all Lanes
  digitalWrite(RELAY_LANE_2, HIGH);
  digitalWrite(RELAY_LANE_3, HIGH);
  digitalWrite(RELAY_LANE_4, HIGH);
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
//Monitors All Race Attributes
void Race_Metrics() {
  if (Toggle_Menu_Initialize == 1 && Race_Over == 0) {
    LEDS.setBrightness(NP_Brightness);  //Initialize Settings for race
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Race In Progress");
    Toggle_Menu_Initialize = 0;
    Time_Current = millis();
    cars[0].total_time = Time_Current;
    cars[1].total_time = Time_Current;
    cars[2].total_time = Time_Current;
    cars[3].total_time = Time_Current;
  }
  if (Monitor_Stop == 1) {  //Monitor the Stop Button to cancel Race
    Toggle_Race_Stop = 1;
    Toggle_Race_Metrics = 0;
    Stop_Race();
  }
  if (Monitor_Start == 1 && Toggle_Race_Metrics == 1) {  //Monitor the Start Button to Pause Race
    Toggle_Menu_Initialize = 1;
    Toggle_Race_Metrics = 0;
    Toggle_Race_Pause = 1;
  }
  if (Penality_Lane1 == 0 || Penality_Lane2 == 0 || Penality_Lane3 == 0 || Penality_Lane4 == 0) {  //Watches for penailty flags on any lane, restores power to the lane(s) after penality duration
    if (Penality_Lane1 == 0) {
      if (Time_Current > (Time_Reference_Debounce + Delay_Penality)) {
        digitalWrite(RELAY_LANE_1, LOW);
        Penality_Lane1 = 1;
        for (int i = 0; i < 4; i++) {
          leds[NP_Lane_1[i]] = CHSV(NP_Boot_Colors[0], 255, 255);
        }
      }
    }
    if (Penality_Lane2 == 0) {
      if (Time_Current > (Time_Reference_Debounce + Delay_Penality)) {
        digitalWrite(RELAY_LANE_2, LOW);
        Penality_Lane2 = 1;
        for (int i = 0; i < 4; i++) {
          leds[NP_Lane_2[i]] = CHSV(NP_Boot_Colors[0], 255, 255);
        }
      }
    }
    if (Penality_Lane3 == 0) {
      if (Time_Current > (Time_Reference_Debounce + Delay_Penality)) {
        digitalWrite(RELAY_LANE_3, LOW);
        Penality_Lane3 = 1;
        for (int i = 0; i < 4; i++) {
          leds[NP_Lane_3[i]] = CHSV(NP_Boot_Colors[0], 255, 255);
        }
      }
    }
    if (Penality_Lane4 == 0) {
      if (Time_Current > (Time_Reference_Debounce + Delay_Penality)) {
        digitalWrite(RELAY_LANE_4, LOW);
        Penality_Lane4 = 1;
        for (int i = 0; i < 4; i++) {
          leds[NP_Lane_4[i]] = CHSV(NP_Boot_Colors[0], 255, 255);
        }
      }
    }
    FastLED.show();
  }
  L1_State = digitalRead(MONITOR_LAP_LANE_1);  //Reads the lap counter pins for car crossings
  L2_State = digitalRead(MONITOR_LAP_LANE_2);
  L3_State = digitalRead(MONITOR_LAP_LANE_3);
  L4_State = digitalRead(MONITOR_LAP_LANE_4);

  if (L1_State == LOW && Time_Current > (cars[0].total_time + Debounce_Track)) {  //When a car crosses the pin, record the current lap time and normalize for the display, mark time for Last Lap and increment the Lap counter.
    cars[0].lap_time = (Time_Current - cars[0].total_time) / 10;
    cars[0].total_time = Time_Current;
    cars[0].cur_lap = cars[0].cur_lap + 1;

    Lap_Counter();
    DetermineRacePlace();
  }
  if (L2_State == LOW && Time_Current > (cars[1].total_time + Debounce_Track)) {
    cars[1].lap_time = (Time_Current - cars[1].total_time) / 10;
    cars[1].total_time = Time_Current;
    cars[1].cur_lap = cars[1].cur_lap + 1;

    Lap_Counter();
    DetermineRacePlace();
  }
  if (L3_State == LOW && Time_Current > (cars[2].total_time + Debounce_Track)) {
    cars[2].lap_time = (Time_Current - cars[2].total_time) / 10;
    cars[2].total_time = Time_Current;
    cars[2].cur_lap = cars[2].cur_lap + 1;

    Lap_Counter();
    DetermineRacePlace();
  }
  if (L4_State == LOW && Time_Current > (cars[3].total_time + Debounce_Track)) {
    cars[3].lap_time = (Time_Current - cars[3].total_time) / 10;
    cars[3].total_time = Time_Current;
    cars[3].cur_lap = cars[3].cur_lap + 1;

    Lap_Counter();
    DetermineRacePlace();
  }
  // Monitor for Lap Record
  for (int i = 0; i < Num_Racers; i++) {
    if (cars[i].lap_time < Record_Lap && (cars[i].lap_time * 10) > Debounce_Track) {
      Record_Lap = cars[i].lap_time;
      Record_Car_Num = i;
      Record_Car = cars[Record_Car_Num].car_number;
      LapRecordDisplay();
    }
  }
}
//Reads Lap Record from EEPROM and Displays on 7 Sgement Displays
void LapRecordDisplay() {
  char LapTimeRec_Buffer[4];
  char LapRecNum_Buffer[4];
  LapRecNum_Buffer[0] = Car_Numbers[Record_Car][0];
  LapRecNum_Buffer[1] = Car_Numbers[Record_Car][1];
  sprintf(LapTimeRec_Buffer, "%4d", Record_Lap);
  LapRecNum.writeDigitAscii(0, LapRecNum_Buffer[0]);
  LapRecNum.writeDigitAscii(1, LapRecNum_Buffer[1]);
  LapTimeRec.writeDigitAscii(0, LapTimeRec_Buffer[0]);
  LapTimeRec.writeDigitAscii(1, LapTimeRec_Buffer[1], true);
  LapTimeRec.writeDigitAscii(2, LapTimeRec_Buffer[2]);
  LapTimeRec.writeDigitAscii(3, LapTimeRec_Buffer[3]);
  LapTimeRec.writeDisplay();
  LapRecNum.writeDisplay();
}
//When new Lap Record is achieved it is written to EEPROM
void LapRecord() {
  EEPROM_writelong(0x02, Record_Lap);
  EEPROM.write(0x00, cars[Record_Car_Num].car_number);
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
//Monitor Lap Number and Display on 7 Segment Display
void Lap_Counter() {
  if (Num_Laps >= Current_Lap_Num) {
    if (Current_Lap_Num < max(max(max(cars[0].cur_lap, cars[1].cur_lap), cars[2].cur_lap), cars[3].cur_lap)) {  //If the current lap counter is less than the highest lap, clear the display
      Current_Lap_Num = max(max(max(cars[0].cur_lap, cars[1].cur_lap), cars[2].cur_lap), cars[3].cur_lap);      //update and the Lap counter
      char LapBuffer[2];
      dtostrf(Current_Lap_Num, 2, 0, LapBuffer);  //Convert the Lap number individual char in an array and update lap count 7 segment displays.
      if (Current_Lap_Num < 10) {
        LapRecNum.writeDigitAscii(3, LapBuffer[1]);
      } else {
        LapRecNum.writeDigitAscii(2, LapBuffer[0]);
        LapRecNum.writeDigitAscii(3, LapBuffer[1]);
      }
      LapRecNum.writeDisplay();
    }
  }
  if (Num_Laps <= Current_Lap_Num) {
    String Final_Lap = "FL";
    char FL[2];
    LapRecNum.clear();
    LapRecNum.writeDisplay();
    FL[0] = Final_Lap[0];
    FL[1] = Final_Lap[1];
    LapRecNum.writeDigitAscii(2, FL[0]);
    LapRecNum.writeDigitAscii(3, FL[1]);
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
//Display Sorted Car Numbers and Lap times on Pole Position 7 Segmet Displays
void Display_Leaderboard() {
  char P1Buffer_Time[4];
  char P2Buffer_Time[4];
  char P3Buffer_Time[4];
  char P4Buffer_Time[4];
  char P1Buffer_Car[2];
  char P2Buffer_Car[2];
  char P3Buffer_Car[2];
  char P4Buffer_Car[2];

  // Sorts the cars based on how many laps completed and lowest total race time
  qsort(cars, Num_Racers, sizeof(struct Car), cmp_lap_and_total_time);

  //Update Leaderboard Display Data
  P1Buffer_Car[0] = Car_Numbers[cars[0].car_number][0];
  P1Buffer_Car[1] = Car_Numbers[cars[0].car_number][1];
  sprintf(P1Buffer_Time, "%4d", cars[0].lap_time);

  P2Buffer_Car[0] = Car_Numbers[cars[1].car_number][0];
  P2Buffer_Car[1] = Car_Numbers[cars[1].car_number][1];
  sprintf(P2Buffer_Time, "%4d", cars[1].lap_time);

  P3Buffer_Car[0] = Car_Numbers[cars[2].car_number][0];
  P3Buffer_Car[1] = Car_Numbers[cars[2].car_number][1];
  sprintf(P3Buffer_Time, "%4d", cars[2].lap_time);

  P4Buffer_Car[0] = Car_Numbers[cars[3].car_number][0];
  P4Buffer_Car[1] = Car_Numbers[cars[3].car_number][1];
  sprintf(P4Buffer_Time, "%4d", cars[3].lap_time);

  // Now we need to get back into lane order
  qsort(cars, Num_Racers, sizeof(struct Car), lane_order);

  //Refresh Leaderboard Display
  if (Current_Lap_Num >= 2) {
    P1Time.clear();
    P2Time.clear();
    P3Time.clear();
    P4Time.clear();
    P1Time.writeDisplay();
    P2Time.writeDisplay();
    P3Time.writeDisplay();
    P4Time.writeDisplay();
    P1P2Num.clear();
    P3P4Num.clear();
    P1P2Num.writeDisplay();
    P3P4Num.writeDisplay();
    if (Num_Racers >= 1) {
      P1Time.writeDigitAscii(0, P1Buffer_Time[0]);
      P1Time.writeDigitAscii(1, P1Buffer_Time[1], true);
      P1Time.writeDigitAscii(2, P1Buffer_Time[2]);
      P1Time.writeDigitAscii(3, P1Buffer_Time[3]);
      P1P2Num.writeDigitAscii(2, P1Buffer_Car[0]);
      P1P2Num.writeDigitAscii(3, P1Buffer_Car[1]);
    }
    if (Num_Racers >= 2) {
      P2Time.writeDigitAscii(0, P2Buffer_Time[0]);
      P2Time.writeDigitAscii(1, P2Buffer_Time[1], true);
      P2Time.writeDigitAscii(2, P2Buffer_Time[2]);
      P2Time.writeDigitAscii(3, P2Buffer_Time[3]);
      P1P2Num.writeDigitAscii(0, P2Buffer_Car[0]);
      P1P2Num.writeDigitAscii(1, P2Buffer_Car[1]);
    }
    if (Num_Racers >= 3) {
      P3Time.writeDigitAscii(0, P3Buffer_Time[0]);
      P3Time.writeDigitAscii(1, P3Buffer_Time[1], true);
      P3Time.writeDigitAscii(2, P3Buffer_Time[2]);
      P3Time.writeDigitAscii(3, P3Buffer_Time[3]);
      P3P4Num.writeDigitAscii(2, P3Buffer_Car[0]);
      P3P4Num.writeDigitAscii(3, P3Buffer_Car[1]);
    }
    if (Num_Racers == 4) {
      P4Time.writeDigitAscii(0, P4Buffer_Time[0]);
      P4Time.writeDigitAscii(1, P4Buffer_Time[1], true);
      P4Time.writeDigitAscii(2, P4Buffer_Time[2]);
      P4Time.writeDigitAscii(3, P4Buffer_Time[3]);
      P3P4Num.writeDigitAscii(0, P4Buffer_Car[0]);
      P3P4Num.writeDigitAscii(1, P4Buffer_Car[1]);
    }
    P1Time.writeDisplay();
    P2Time.writeDisplay();
    P3Time.writeDisplay();
    P4Time.writeDisplay();
    P1P2Num.writeDisplay();
    P3P4Num.writeDisplay();
    LapCountdown();
  }
}
//Display The Correct Number of Laps LED Pattern
void LapCountdown() {
  if ((Num_Laps - cars[0].cur_lap) == 1) {
    leds[NP_Lane_1[0]] = CRGB(0, 0, 0);
    leds[NP_Lane_1[1]] = CHSV(NP_Boot_Colors[0], 255, 255);
    leds[NP_Lane_1[2]] = CRGB(0, 0, 0);
    leds[NP_Lane_1[3]] = CRGB(0, 0, 0);
  }
  if ((Num_Laps - cars[0].cur_lap) == 2) {
    leds[NP_Lane_1[0]] = CHSV(NP_Boot_Colors[0], 255, 255);
    leds[NP_Lane_1[1]] = CRGB(0, 0, 0);
    leds[NP_Lane_1[2]] = CHSV(NP_Boot_Colors[0], 255, 255);
    leds[NP_Lane_1[3]] = CRGB(0, 0, 0);
  }
  if ((Num_Laps - cars[0].cur_lap) == 3) {
    leds[NP_Lane_1[0]] = CHSV(NP_Boot_Colors[0], 255, 255);
    leds[NP_Lane_1[1]] = CHSV(NP_Boot_Colors[0], 255, 255);
    leds[NP_Lane_1[2]] = CHSV(NP_Boot_Colors[0], 255, 255);
    leds[NP_Lane_1[3]] = CRGB(0, 0, 0);
  }
  if ((Num_Laps - cars[1].cur_lap) == 1) {
    leds[NP_Lane_2[0]] = CRGB(0, 0, 0);
    leds[NP_Lane_2[1]] = CHSV(NP_Boot_Colors[0], 255, 255);
    leds[NP_Lane_2[2]] = CRGB(0, 0, 0);
    leds[NP_Lane_2[3]] = CRGB(0, 0, 0);
  }
  if ((Num_Laps - cars[1].cur_lap) == 2) {
    leds[NP_Lane_2[0]] = CHSV(NP_Boot_Colors[0], 255, 255);
    leds[NP_Lane_2[1]] = CRGB(0, 0, 0);
    leds[NP_Lane_2[2]] = CHSV(NP_Boot_Colors[0], 255, 255);
    leds[NP_Lane_2[3]] = CRGB(0, 0, 0);
  }
  if ((Num_Laps - cars[1].cur_lap) == 3) {
    leds[NP_Lane_2[0]] = CHSV(NP_Boot_Colors[0], 255, 255);
    leds[NP_Lane_2[1]] = CHSV(NP_Boot_Colors[0], 255, 255);
    leds[NP_Lane_2[2]] = CHSV(NP_Boot_Colors[0], 255, 255);
    leds[NP_Lane_2[3]] = CRGB(0, 0, 0);
  }
  if ((Num_Laps - cars[2].cur_lap) == 1) {
    leds[NP_Lane_3[0]] = CRGB(0, 0, 0);
    leds[NP_Lane_3[1]] = CHSV(NP_Boot_Colors[0], 255, 255);
    leds[NP_Lane_3[2]] = CRGB(0, 0, 0);
    leds[NP_Lane_3[3]] = CRGB(0, 0, 0);
  }
  if ((Num_Laps - cars[2].cur_lap) == 2) {
    leds[NP_Lane_3[0]] = CHSV(NP_Boot_Colors[0], 255, 255);
    leds[NP_Lane_3[1]] = CRGB(0, 0, 0);
    leds[NP_Lane_3[2]] = CHSV(NP_Boot_Colors[0], 255, 255);
    leds[NP_Lane_3[3]] = CRGB(0, 0, 0);
  }
  if ((Num_Laps - cars[2].cur_lap) == 3) {
    leds[NP_Lane_3[0]] = CHSV(NP_Boot_Colors[0], 255, 255);
    leds[NP_Lane_3[1]] = CHSV(NP_Boot_Colors[0], 255, 255);
    leds[NP_Lane_3[2]] = CHSV(NP_Boot_Colors[0], 255, 255);
    leds[NP_Lane_3[3]] = CRGB(0, 0, 0);
  }
  if ((Num_Laps - cars[3].cur_lap) == 1) {
    leds[NP_Lane_4[0]] = CRGB(0, 0, 0);
    leds[NP_Lane_4[1]] = CHSV(NP_Boot_Colors[0], 255, 255);
    leds[NP_Lane_4[2]] = CRGB(0, 0, 0);
    leds[NP_Lane_4[3]] = CRGB(0, 0, 0);
  }
  if ((Num_Laps - cars[3].cur_lap) == 2) {
    leds[NP_Lane_4[0]] = CHSV(NP_Boot_Colors[0], 255, 255);
    leds[NP_Lane_4[1]] = CRGB(0, 0, 0);
    leds[NP_Lane_4[2]] = CHSV(NP_Boot_Colors[0], 255, 255);
    leds[NP_Lane_4[3]] = CRGB(0, 0, 0);
  }
  if ((Num_Laps - cars[3].cur_lap) == 3) {
    leds[NP_Lane_4[0]] = CHSV(NP_Boot_Colors[0], 255, 255);
    leds[NP_Lane_4[1]] = CHSV(NP_Boot_Colors[0], 255, 255);
    leds[NP_Lane_4[2]] = CHSV(NP_Boot_Colors[0], 255, 255);
    leds[NP_Lane_4[3]] = CRGB(0, 0, 0);
  }
  FastLED.show();
}
//Final Lap and Finish Actions
void End_Race() {
  //At Last Lap Turn all LEDs white per lane and play the Last Lap song
  if (Num_Laps == Current_Lap_Num && Toggle_Menu_Initialize == 1) {
    Toggle_Menu_Initialize = 0;
    Last_Lap = 1;
  }
  if (Num_Laps <= cars[0].cur_lap && L1_Last_Lap == 0) {
    if (sound_buffer <= (Time_Current - 3500)) {
      sound_buffer = Time_Current;
      playSdWav1.play("LASTLAP.WAV");
    }
    L1_Last_Lap = 1;
    for (int i = 0; i < 4; i++) {
      leds[NP_Lane_1[i]] = CRGB(255, 255, 255);
    }
  }
  if (Num_Laps <= cars[1].cur_lap && L2_Last_Lap == 0) {
    if (sound_buffer <= (Time_Current - 3500)) {
      sound_buffer = Time_Current;
      playSdWav1.play("LASTLAP.WAV");
    }
    L2_Last_Lap = 1;
    for (int i = 0; i < 4; i++) {
      leds[NP_Lane_2[i]] = CRGB(255, 255, 255);
    }
  }
  if (Num_Laps <= cars[2].cur_lap && L3_Last_Lap == 0) {
    if (sound_buffer <= (Time_Current - 3500)) {
      sound_buffer = Time_Current;
      playSdWav1.play("LASTLAP.WAV");
    }
    L3_Last_Lap = 1;
    for (int i = 0; i < 4; i++) {
      leds[NP_Lane_3[i]] = CRGB(255, 255, 255);
    }
  }
  if (Num_Laps <= cars[3].cur_lap && L4_Last_Lap == 0) {
    if (sound_buffer <= (Time_Current - 3500)) {
      sound_buffer = Time_Current;
      playSdWav1.play("LASTLAP.WAV");
    }
    L4_Last_Lap = 1;
    for (int i = 0; i < 4; i++) {
      leds[NP_Lane_4[i]] = CRGB(255, 255, 255);
    }
  }
  FastLED.show();

  //Action When Car in Lane 1 Finishes the Race
  if (cars[0].cur_lap > Num_Laps && cars[0].finish == 0) {
    digitalWrite(RELAY_LANE_1, HIGH);  //Cut Power to the Lane
    cars[0].finish = 1;
    for (int i = 0; i < 4; i++) {
      leds[NP_Lane_1[i]] = CRGB(0, 0, 0);
    }
    FastLED.show();
    delay(10);
    //Display The Correct Position LED Pattern
    if (cars[0].place == 1) {
      sound_buffer = Time_Current;
      leds[NP_Lane_1[1]] = CRGB(0, 255, 255);
    }
    if (cars[0].place == 2) {
      if (sound_buffer <= (Time_Current - 8500)) {
        playSdWav1.play("RECORD.WAV");
      }
      leds[NP_Lane_1[0]] = CRGB(0, 255, 255);
      leds[NP_Lane_1[2]] = CRGB(0, 255, 255);
    }
    if (cars[0].place == 3) {
      if (sound_buffer <= (Time_Current - 8500)) {
        playSdWav1.play("RECORD.WAV");
      }
      for (int i = 0; i < 3; i++) {
        leds[NP_Lane_1[i]] = CRGB(0, 255, 255);
      }
    }
    if (cars[0].place == 4) {
      if (sound_buffer <= (Time_Current - 8500)) {
        playSdWav1.play("RECORD.WAV");
      }
      for (int i = 0; i < 4; i++) {
        leds[NP_Lane_1[i]] = CRGB(0, 255, 255);
      }
    }
    FastLED.show();
  }
  //Action When Car in Lane 2 Finishes the Race
  if (cars[1].cur_lap > Num_Laps && cars[1].finish == 0) {
    digitalWrite(RELAY_LANE_2, HIGH);  //Cut Power to the Lane
    cars[1].finish = 1;
    for (int i = 0; i < 4; i++) {
      leds[NP_Lane_2[i]] = CRGB(0, 0, 0);
    }
    FastLED.show();
    delay(10);
    //Display The Correct Position LED Pattern
    if (cars[1].place == 1) {
      sound_buffer = Time_Current;
      leds[NP_Lane_2[1]] = CRGB(0, 255, 255);
    }
    if (cars[1].place == 2) {
      if (sound_buffer <= (Time_Current - 8500)) {
        playSdWav1.play("RECORD.WAV");
      }
      leds[NP_Lane_2[0]] = CRGB(0, 255, 255);
      leds[NP_Lane_2[2]] = CRGB(0, 255, 255);
    }
    if (cars[1].place == 3) {
      if (sound_buffer <= (Time_Current - 8500)) {
        playSdWav1.play("RECORD.WAV");
      }
      for (int i = 0; i < 3; i++) {
        leds[NP_Lane_2[i]] = CRGB(0, 255, 255);
      }
    }
    if (cars[1].place == 4) {
      if (sound_buffer <= (Time_Current - 8500)) {
        playSdWav1.play("RECORD.WAV");
      }
      for (int i = 0; i < 4; i++) {
        leds[NP_Lane_2[i]] = CRGB(0, 255, 255);
      }
    }
    FastLED.show();
  }
  //Action When Car in Lane 3 Finishes the Race
  if (cars[2].cur_lap > Num_Laps && cars[2].finish == 0) {
    digitalWrite(RELAY_LANE_3, HIGH);  //Cut Power to the Lane
    cars[2].finish = 1;
    for (int i = 0; i < 4; i++) {
      leds[NP_Lane_3[i]] = CRGB(0, 0, 0);
    }
    FastLED.show();
    delay(10);
    //Display The Correct Position LED Pattern
    if (cars[2].place == 1) {
      sound_buffer = Time_Current;
      leds[NP_Lane_3[1]] = CRGB(0, 255, 255);
    }
    if (cars[2].place == 2) {
      if (sound_buffer <= (Time_Current - 8500)) {
        playSdWav1.play("RECORD.WAV");
      }
      leds[NP_Lane_3[0]] = CRGB(0, 255, 255);
      leds[NP_Lane_3[2]] = CRGB(0, 255, 255);
    }
    if (cars[2].place == 3) {
      if (sound_buffer <= (Time_Current - 8500)) {
        playSdWav1.play("RECORD.WAV");
      }
      for (int i = 0; i < 3; i++) {
        leds[NP_Lane_3[i]] = CRGB(0, 255, 255);
      }
    }
    if (cars[2].place == 4) {
      if (sound_buffer <= (Time_Current - 8500)) {
        playSdWav1.play("RECORD.WAV");
      }
      for (int i = 0; i < 4; i++) {
        leds[NP_Lane_3[i]] = CRGB(0, 255, 255);
      }
    }
    FastLED.show();
  }
  //Action When Car in Lane 4 Finishes the Race
  if (cars[3].cur_lap > Num_Laps && cars[3].finish == 0) {
    digitalWrite(RELAY_LANE_4, HIGH);  //Cut Power to the Lane
    cars[3].finish = 1;
    for (int i = 0; i < 4; i++) {
      leds[NP_Lane_4[i]] = CRGB(0, 0, 0);
    }
    FastLED.show();
    delay(10);
    //Display The Correct Position LED Pattern
    if (cars[3].place == 1) {
      sound_buffer = Time_Current;
      leds[NP_Lane_4[1]] = CRGB(0, 255, 255);
    }
    if (cars[3].place == 2) {
      if (sound_buffer <= (Time_Current - 8500)) {
        playSdWav1.play("RECORD.WAV");
      }
      leds[NP_Lane_4[0]] = CRGB(0, 255, 255);
      leds[NP_Lane_4[2]] = CRGB(0, 255, 255);
    }
    if (cars[3].place == 3) {
      if (sound_buffer <= (Time_Current - 8500)) {
        playSdWav1.play("RECORD.WAV");
      }
      for (int i = 0; i < 3; i++) {
        leds[NP_Lane_4[i]] = CRGB(0, 255, 255);
      }
    }
    if (cars[3].place == 4) {
      if (sound_buffer <= (Time_Current - 8500)) {
        playSdWav1.play("RECORD.WAV");
      }
      for (int i = 0; i < 4; i++) {
        leds[NP_Lane_4[i]] = CRGB(0, 255, 255);
      }
    }
    FastLED.show();
  }
  // Determine the number of cars that have finished the race
  int cars_finished = 0;
  for (int i = 0; i < Num_Racers; i++) {
    if (cars[i].finish == 1) {
      cars_finished++;
    }
  }
  // When the first car crosses the finish line play the finish Song
  if (First_Car_Finish == 0) {
    if (cars_finished >= 1) {
      playSdWav1.play("FINISH.WAV");
      First_Car_Finish = 1;
    }
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
  L1_State = 0;
  L2_State = 0;
  L3_State = 0;
  L4_State = 0;
  Num_Laps = 5;
  Num_Racers = 4;
  Num_Lane = 1;
  // Set all cars back to their default values
  cars[0] = (struct Car){ .lane = 1, .car_number = 10, .cur_lap = 0, .place = 1, .lap_time = 0, .total_time = 0, .finish = 0 };
  cars[1] = (struct Car){ .lane = 2, .car_number = 10, .cur_lap = 0, .place = 2, .lap_time = 0, .total_time = 0, .finish = 0 };
  cars[2] = (struct Car){ .lane = 3, .car_number = 10, .cur_lap = 0, .place = 3, .lap_time = 0, .total_time = 0, .finish = 0 };
  cars[3] = (struct Car){ .lane = 4, .car_number = 10, .cur_lap = 0, .place = 4, .lap_time = 0, .total_time = 0, .finish = 0 };
  // Clear Leaderboard Display
  P1Time.clear();
  P2Time.clear();
  P3Time.clear();
  P4Time.clear();
  P1Time.writeDisplay();
  P2Time.writeDisplay();
  P3Time.writeDisplay();
  P4Time.writeDisplay();
  P1P2Num.clear();
  P3P4Num.clear();
  P1P2Num.writeDisplay();
  P3P4Num.writeDisplay();
  LapTimeRec.clear();
  LapRecNum.clear();
  LapTimeRec.writeDisplay();
  LapRecNum.writeDisplay();
  FastLED.show();
  LapRecordDisplay();
}
