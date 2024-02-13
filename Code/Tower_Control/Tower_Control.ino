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

//NeoPixel Assignments
#define NUM_LEDS 16  // How many leds in your strip?
#define DATA_PIN 39

//I2S Audio Assignments
AudioPlaySdWav playSdWav1;
AudioMixer4 mixer1;
AudioOutputI2S i2s1;
AudioConnection patchCord1(playSdWav1, 0, mixer1, 0);
AudioConnection patchCord2(playSdWav1, 1, mixer1, 1);
AudioConnection patchCord3(mixer1, 0, i2s1, 0);
AudioConnection patchCord4(mixer1, 0, i2s1, 1);

//Neopixel Arrays
CRGB leds[NUM_LEDS];                                                        // Define the array of leds
int BootNP[16] = { 13, 14, 12, 15, 0, 11, 1, 10, 2, 9, 3, 8, 4, 7, 5, 6 };  //LED Order for Boot Animation
int BootColors[4] = { 0, 64, 96, 160 };                                     //Colors Used in Boot Animation
int BootColorArray = 4;                                                     //Color Transitions in Boot Animation
int RaceStartRed[4] = { 12, 13, 14, 15 };                                   //Red LEDS for Race Start
int Lane1NP[4] = { 11, 10, 9, 12 };                                         //Lane One LEDs
int Lane2NP[4] = { 8, 7, 6, 13 };                                           //Lane Two LEDs
int Lane3NP[4] = { 5, 4, 3, 14 };                                           //Lane Three LEDs
int Lane4NP[4] = { 2, 1, 0, 15 };                                           //Lane Four LEDs

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
#define RE_BUTTON 26                  //Rotary Encoder Button
#define BACK_BUTTON 24                //Back Button
#define START_BUTTON 25               //Start Race Button
#define STOP_BUTTON 27                //Pause Button
#define LANE1RELAYPIN 17              //Controls the Relay for Lane 1 in the Control Box
#define LANE2RELAYPIN 16              //Controls the Relay for Lane 2 in the Control Box
#define LANE3RELAYPIN 15              //Controls the Relay for Lane 3 in the Control Box
#define LANE4RELAYPIN 14              //Controls the Relay for Lane 4 in the Control Box
#define LANE1LAP_TR_PIN 36            //Lap counter switch in Lane 1
#define LANE2LAP_TR_PIN 35            //Lap counter switch in Lane 2
#define LANE3LAP_TR_PIN 34            //Lap counter switch in Lane 3
#define LANE4LAP_TR_PIN 33            //Lap counter switch in Lane 4
#define LED_PIN 13                    //Notification LED
#define ENCODER1_PIN 29               //Rotary Encoder Pin +
#define ENCODER2_PIN 28               //Rotary Encoder Pin -

// LCD Backpack Setup
Adafruit_LiquidCrystal lcd(1);  //default address #0 (A0-A2 not jumpered)

//Encoder Setup
Encoder myEnc(ENCODER1_PIN, ENCODER2_PIN);

//Variables
//Menu Navigation - The variables trigger a menu change when flagged as 1
int Welcome_Message_Menu = 1;
int Number_of_Racers_Menu = 0;
int Number_of_Laps_Menu = 0;
int Car_Num_Lane_Assign_Menu = 0;
int Start_Race_Menu = 0;
int Erase_Record_Lap_Menu = 0;
int Enter_Menu = 1;
int Screen_Change = 0;
int Array_Increment = 0;
int Race_Metrics_Toggle = 0;
int Stop_Race_Toggle = 0;
int Pause_Race_Toggle = 0;
int Hazard_Toggle = 0;

//Rotary Encoder - Logs the position of the Rotary Encoder
long Old_Position = -999;
long New_Position;

//Debouncing
unsigned long Current_Time;             //Current Millis
unsigned long Debounce_Time_Reference;  //Millis reference when button is presses
unsigned long L1_Debounce_Time_Reference;
unsigned long L2_Debounce_Time_Reference;
unsigned long L3_Debounce_Time_Reference;
unsigned long L4_Debounce_Time_Reference;
unsigned long Pause_Offset;  //Time to ajdust in the event the race is paused
#define RE_DEBOUNCE 125      //Debounce time for the Rotary Encoder
#define BUTTON_DEBOUNCE 200  //Debounce time for a Button Press

//Race Identifiers
int Num_of_Laps = 5;    //Number of laps in the Race (Can be Modified in Menu 5-99)
int Num_of_Racers = 4;  //Number of Racers in the Race (Can be Modified in Menu 1-4)
int Num_Lane = 1;       //Used for Lane Assignment of Drivers/Car/Lap Times

//Button Status Monitors
int Start_Button_Press = 0;  //Triggers an event when the Start Button is pressed
int Back_Button_Press = 0;   //Triggers an event when the Back Button is pressed
int Stop_Button_Press = 0;   //Triggers an event when the Stop Button is pressed
int Last_Back_Press = 0;     //Flags an event when the Back Button is pressed
int Last_Start_Press = 0;    //Flags an event when the Start Button is pressed
int Last_Stop_Press = 0;     //Flags an event when the Stop Button is pressed

//Race Information
int Current_Lap_Num = 0;            //Lap Count in Current Race
int Race_Over = 0;                  //Race Complete Flag
int First_Car_Finish = 0;           //Flag to Play Finish WAV
int Last_Lap = 0;                   //Last Lap Flag
unsigned long Race_Time;            //Race Duration in millis
unsigned short Record_Lap = 10000;  //Lap Record Time
int Record_Car_Num;                 //Lap Record Car Number
int EEPROMCar = 0;
int RecordCar;
int L1_State = 0;  //In Track Lap Counter Monitors State, per lane
int L2_State = 0;
int L3_State = 0;
int L4_State = 0;
int Last_L1_State = 0;
int Last_L2_State = 0;
int Last_L3_State = 0;
int Last_L4_State = 0;

// Struct (or class/object) that defines everything that a car needs to have
struct Car {
  int lane;                  // Lane the car is in
  int car_number;            // The number that represents which car type is in use
  int cur_lap;               // Current lap the car is on
  int place;                 // Placee the car is currently in
  unsigned short lap_time;   // The time of the last time
  unsigned long total_time;  // Total race time
  int finish;                // Has the car finished the race
};

// Declare our cars array and fill them in with default values in the loop
struct Car *cars = (Car *)malloc(Num_of_Racers * sizeof *cars);

//Race Arrays
// int lap_times[4];        //Current Lap Times in Lane Order
int car_num_in_race[4];  //Car Numbers in the Race

//Screen Variables
int Center_Value;  //Used to center the text on the 16x2 LCD screen

//Neopixel Variables
#define NP_Brightness 84          //Set Neopixel Brightness
#define START_SEQUENCE_DELAY 100  //Start Animation Speed (Higher = Slower)
#define DIM_DELAY 50              //Dimming Speed (Higher = Slower)
#define YELLOW_LIGHT_DELAY 750    //Delay between Yellow Lights
#define RED_LIGHT_DELAY 4250      //Time for Red Lights
#define STOP_RACE_DELAY 10000     //Time for red lights to be active before reset

//Relay Penality Variables
int Penality_Lane1 = 0;  //Flag if car crosses start line before the green light, per lane
int Penality_Lane2 = 0;
int Penality_Lane3 = 0;
int Penality_Lane4 = 0;
#define PENALITY_DELAY 5000  //Penality Duration

//Menu Arrays
//Car Names and Numbers Displayed on LCD
String Car_Names[10] = { "01 Skyline", "03 Ford Capri", "05 BMW 3.5 CSL", "05 Lancia LC2", "33 BMW M1", "51 Porsche 935", "576 Lancia Beta", "80 Audi RS5", "DE BTTF Delorean", "MM GT Falcon V8" };
//Car Numbers on Displayed on Pole Position and Lap Record 7 Segment
String Car_Numbers[11] = { "01", "03", "05", "05", "33", "51", "57", "80", "DE", "MM", "--" };
//Menu selection for Erasing EEPROM
String Rec_Reset[20] = { "NO", "X", "XXX", "X", "XXX", "X", "XXX", "X", "XXX", "X", "YES", "X", "XXX", "X", "XXX", "X", "XXX", "X", "XXX", "X" };

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

  //Read EEPROM for Lap Record Info
  Record_Lap = EEPROMReadlong(0x02);
  RecordCar = EEPROM.read(0x00);

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

  //Pin Assignments
  pinMode(RE_BUTTON, INPUT);
  pinMode(BACK_BUTTON, INPUT);
  pinMode(START_BUTTON, INPUT);
  pinMode(STOP_BUTTON, INPUT);
  pinMode(LED_PIN, OUTPUT);
  pinMode(LANE1LAP_TR_PIN, INPUT);
  pinMode(LANE2LAP_TR_PIN, INPUT);
  pinMode(LANE3LAP_TR_PIN, INPUT);
  pinMode(LANE4LAP_TR_PIN, INPUT);
  pinMode(LANE1RELAYPIN, OUTPUT);
  pinMode(LANE2RELAYPIN, OUTPUT);
  pinMode(LANE3RELAYPIN, OUTPUT);
  pinMode(LANE4RELAYPIN, OUTPUT);

  //Neopixel Setup
  LEDS.addLeds<WS2812, DATA_PIN, RGB>(leds, NUM_LEDS);
  LEDS.setBrightness(NP_Brightness);

  //Disable Lanes before Race
  digitalWrite(LANE1RELAYPIN, HIGH);
  digitalWrite(LANE2RELAYPIN, HIGH);
  digitalWrite(LANE3RELAYPIN, HIGH);
  digitalWrite(LANE4RELAYPIN, HIGH);

  // Set up each of our cars with default values
  cars[0] = (struct Car){ .lane = 1, .car_number = 10, .cur_lap = 0, .place = 1, .lap_time = 0, .total_time = 0, .finish = 0 };
  cars[1] = (struct Car){ .lane = 2, .car_number = 10, .cur_lap = 0, .place = 2, .lap_time = 0, .total_time = 0, .finish = 0 };
  cars[2] = (struct Car){ .lane = 3, .car_number = 10, .cur_lap = 0, .place = 3, .lap_time = 0, .total_time = 0, .finish = 0 };
  cars[3] = (struct Car){ .lane = 4, .car_number = 10, .cur_lap = 0, .place = 4, .lap_time = 0, .total_time = 0, .finish = 0 };
}
//Main Loop
void loop() {
  //Record the Current time
  Current_Time = millis();

  //Monitor Buttons
  Back_Button_Press = digitalRead(BACK_BUTTON);
  Stop_Button_Press = digitalRead(STOP_BUTTON);
  Start_Button_Press = digitalRead(START_BUTTON);

  //Menu Navigation
  if (Welcome_Message_Menu == 1) {
    Welcome_Message();
  }
  if (Number_of_Racers_Menu == 1) {
    Number_of_Racers();
  }
  if (Number_of_Laps_Menu == 1) {
    Number_of_Laps();
  }
  if (Race_Metrics_Toggle == 1) {
    Race_Metrics();
  }
  if (Start_Race_Menu == 1) {
    // Read all the lane pins
    L1_State = digitalRead(LANE1LAP_TR_PIN);
    L2_State = digitalRead(LANE2LAP_TR_PIN);
    L3_State = digitalRead(LANE3LAP_TR_PIN);
    L4_State = digitalRead(LANE4LAP_TR_PIN);
    Start_Race();
  }
  if (Car_Num_Lane_Assign_Menu == 1) {
    Car_Num_Lane_Assign();
  }
  if (Pause_Race_Toggle == 1) {
    Pause_Race();
  }
  if (Stop_Race_Toggle == 1) {
    Stop_Race();
  }
  if (Erase_Record_Lap_Menu == 1) {
    Clear_Record_Lap();
  }
  if (Back_Button_Press == 1 && Last_Back_Press == 0 && Current_Time > (Debounce_Time_Reference + RE_DEBOUNCE)) {
    Menu_Back();
  }
  //Record button presses to avoid rapid repeats
  Last_Back_Press = Back_Button_Press;
  Last_Start_Press = Start_Button_Press;
  Last_Stop_Press = Stop_Button_Press;
  Last_L1_State = L1_State;
  Last_L2_State = L2_State;
  Last_L3_State = L3_State;
  Last_L4_State = L4_State;
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
  Debounce_Time_Reference = Current_Time;
  Enter_Menu = 1;  //Enables menu intros
  if (Number_of_Laps_Menu == 1) {
    Number_of_Racers_Menu = 1;
    Number_of_Laps_Menu = 0;
  }
  if (Car_Num_Lane_Assign_Menu == 1) {
    Num_Lane = 1;
    Number_of_Laps_Menu = 1;
    Car_Num_Lane_Assign_Menu = 0;
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
  for (int j = 0; j < BootColorArray; j++) {
    for (int i = 0; i < NUM_LEDS; i++) {
      leds[BootNP[i]] = CHSV(BootColors[j], 255, 255);
      i++;
      leds[BootNP[i]] = CHSV(BootColors[j], 255, 255);
      FastLED.show();
      delay(START_SEQUENCE_DELAY);
    }
    j++;
    for (int i = 15; i > 0; i--) {
      leds[BootNP[i]] = CHSV(BootColors[j], 255, 255);
      i--;
      leds[BootNP[i]] = CHSV(BootColors[j], 255, 255);
      FastLED.show();
      delay(START_SEQUENCE_DELAY);
    }
  }
  for (int i = 84; i >= 0; i--) {
    if (i > 1) {
      i--;
    }
    LEDS.setBrightness(i);
    FastLED.show();
    delay(DIM_DELAY);
  }

  //Setup for next menu
  Welcome_Message_Menu = 0;
  Number_of_Racers_Menu = 1;
  Enter_Menu = 1;
  lcd.clear();
  New_Position = myEnc.read();
  Old_Position = New_Position;
  Debounce_Time_Reference = Current_Time;
}
//Rotary Encoder monitoring
void Rotary_Encoder() {
  if (Current_Time > (Debounce_Time_Reference + RE_DEBOUNCE)) {  //Buffer is 250
    New_Position = myEnc.read();
  } else {
    New_Position = myEnc.read();
    Old_Position = New_Position;
  }
}
//Menu Section for selecting # of racers
void Number_of_Racers() {
  if (Enter_Menu == 1) {  //Initialization of the Racers Menu
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Number of Racers");
    lcd.setCursor(7, 1);
    lcd.print(Num_of_Racers);
    Num_of_Racers = 4;
    Enter_Menu = 0;
  }
  Rotary_Encoder();
  if (New_Position > Old_Position) {  //Watch the Rotary Encoder and add to the number of racers
    Debounce_Time_Reference = Current_Time;
    Num_of_Racers++;
    if (Num_of_Racers > 4) {
      Num_of_Racers = 4;
    }
    Screen_Change = 1;
  }
  if (New_Position < Old_Position) {  //Watch the Rotary Encoder and subtract from the number of racers
    Debounce_Time_Reference = Current_Time;
    Num_of_Racers--;
    if (Num_of_Racers < 1) {
      Num_of_Racers = 1;
    }
    Screen_Change = 1;
  }
  if (Screen_Change == 1) {  //Update the number of racers and display on LCD
    playSdWav1.play("TICK.WAV");
    lcd.setCursor(7, 1);
    lcd.print(Num_of_Racers);
    Debounce_Time_Reference = Current_Time;
    Old_Position = New_Position;
    Screen_Change = 0;
  }
  if (Start_Button_Press == 1 && Last_Start_Press == 0 && Current_Time > (Debounce_Time_Reference + BUTTON_DEBOUNCE)) {  //Move on to Number of Laps Menu
    Debounce_Time_Reference = Current_Time;
    Number_of_Racers_Menu = 0;
    Number_of_Laps_Menu = 1;
    Enter_Menu = 1;
  }
}
//Menu Section to Specify Number of Laps
void Number_of_Laps() {
  if (Enter_Menu == 1) {  //Initialization of the Laps Menu
    lcd.clear();
    lcd.setCursor(1, 0);
    lcd.print("Number of Laps");
    lcd.setCursor(7, 1);
    lcd.print(Num_of_Laps);
    Num_of_Laps = 5;
    Start_Button_Press = 0;
    Enter_Menu = 0;
  }
  Rotary_Encoder();
  if (New_Position > Old_Position) {  //Watch the Rotary Encoder and add to the number of Laps
    Num_of_Laps++;
    Debounce_Time_Reference = Current_Time;
    if (Num_of_Laps > 99) {
      Num_of_Laps = 5;
    }
    Screen_Change = 1;
  }
  if (New_Position < Old_Position) {  //Watch the Rotary Encoder and subtract from the number of Laps
    Debounce_Time_Reference = Current_Time;
    Num_of_Laps--;
    if (Num_of_Laps < 5) {
      Num_of_Laps = 99;
    }
    Screen_Change = 1;
  }
  if (Screen_Change == 1) {  //Update the number of laps and display on LCD
    playSdWav1.play("TICK.WAV");
    lcd.setCursor(7, 1);
    lcd.print(Num_of_Laps);
    if (Num_of_Laps < 10) {
      lcd.setCursor(8, 1);
      lcd.print(" ");
    }
    Debounce_Time_Reference = Current_Time;
    Old_Position = New_Position;
    Screen_Change = 0;
  }
  if (Start_Button_Press == 1 && Last_Start_Press == 0 && Current_Time > (Debounce_Time_Reference + BUTTON_DEBOUNCE)) {  //Move on to Car Number Select Menu
    Debounce_Time_Reference = Current_Time;
    Number_of_Laps_Menu = 0;
    Car_Num_Lane_Assign_Menu = 1;
    Enter_Menu = 1;
  }
}
//Centers the car's number on the LCD
void Center_Text_Car() {
  String Car_Name = Car_Names[Array_Increment];
  Center_Value = (16 - Car_Name.length()) / 2;
  lcd.setCursor(Center_Value, 1);
}
//Menu Section to Select Car Numbers per Lane
void Car_Num_Lane_Assign() {
  if (Enter_Menu == 1) {  //Initialization of the Cars Menu
    Array_Increment = 0;
    Debounce_Time_Reference = Current_Time;
    Start_Button_Press = 0;
    Enter_Menu = 0;
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
  if (New_Position > Old_Position) {  //Watch the Rotary Encoder and display the next car number
    Debounce_Time_Reference = Current_Time;
    Array_Increment++;
    if (Array_Increment > 9) {
      Array_Increment = 1;
    }
    Screen_Change = 1;
  }
  if (New_Position < Old_Position) {  //Watch the Rotary Encoder and display the previous car number
    Debounce_Time_Reference = Current_Time;
    Array_Increment--;
    if (Array_Increment < 1) {
      Array_Increment = 9;
    }
    Screen_Change = 1;
  }
  if (Screen_Change == 1) {  //Update the car number and display on LCD
    playSdWav1.play("TICK.WAV");
    lcd.setCursor(0, 1);
    lcd.print("                ");
    Center_Text_Car();
    lcd.print(Car_Names[Array_Increment]);
    Pole_Pos_Display();
    Old_Position = New_Position;
    Debounce_Time_Reference = Current_Time;
    Screen_Change = 0;
  }
  if (Start_Button_Press == 1 && Last_Start_Press == 0 && Num_Lane == 4 && Current_Time > (Debounce_Time_Reference + BUTTON_DEBOUNCE)) {  //Move on to Clear Record Lap Menu
    cars[3].car_number = Array_Increment;
    Enter_Menu = 1;
    Erase_Record_Lap_Menu = 1;
    Car_Num_Lane_Assign_Menu = 0;
  }
  if (Start_Button_Press == 1 && Last_Start_Press == 0 && Num_Lane == 3 && Current_Time > (Debounce_Time_Reference + BUTTON_DEBOUNCE)) {  //Goto Driver Select Menu for Lane 4
    cars[2].car_number = Array_Increment;
    Num_Lane++;
    Enter_Menu = 1;
    Car_Num_Lane_Assign_Menu = 1;
    if (Num_of_Racers == 3) {
      Erase_Record_Lap_Menu = 1;
      Car_Num_Lane_Assign_Menu = 0;
    }
  }
  if (Start_Button_Press == 1 && Last_Start_Press == 0 && Num_Lane == 2 && Current_Time > (Debounce_Time_Reference + BUTTON_DEBOUNCE)) {  //Goto Driver Select Menu for Lane 3
    cars[1].car_number = Array_Increment;
    Num_Lane++;
    Enter_Menu = 1;
    Car_Num_Lane_Assign_Menu = 1;
    if (Num_of_Racers == 2) {
      Erase_Record_Lap_Menu = 1;
      Car_Num_Lane_Assign_Menu = 0;
    }
  }
  if (Start_Button_Press == 1 && Last_Start_Press == 0 && Num_Lane == 1 && Current_Time > (Debounce_Time_Reference + BUTTON_DEBOUNCE)) {  //Goto Driver Select Menu for Lane 2
    cars[0].car_number = Array_Increment;
    Num_Lane++;
    Enter_Menu = 1;
    Car_Num_Lane_Assign_Menu = 1;
    if (Num_of_Racers == 1) {
      Erase_Record_Lap_Menu = 1;
      Car_Num_Lane_Assign_Menu = 0;
    }
  }
}
//Menu Section to Clear Lap Record from EEPROM
void Clear_Record_Lap() {
  if (Enter_Menu == 1) {  //Initialization of the EEPROM Menu
    Array_Increment = 0;
    Debounce_Time_Reference = Current_Time;
    Start_Button_Press = 0;
    Enter_Menu = 0;
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("ERASE LAP RECORD");
    lcd.setCursor(7, 1);
    lcd.print(Rec_Reset[Array_Increment]);
  }
  Rotary_Encoder();
  if (New_Position > Old_Position) {  //Watch the Rotary Encoder and display the next Next Option
    Debounce_Time_Reference = Current_Time;
    Array_Increment++;
    if (Array_Increment > 19) {
      Array_Increment = 0;
    }
    Screen_Change = 1;
  }
  if (New_Position < Old_Position) {  //Watch the Rotary Encoder and display the previous Option
    Debounce_Time_Reference = Current_Time;
    Array_Increment--;
    if (Array_Increment < 0) {
      Array_Increment = 19;
    }
    Screen_Change = 1;
  }
  if (Screen_Change == 1) {  //Update the Next Option and display on LCD
    playSdWav1.play("TICK.WAV");
    lcd.setCursor(1, 1);
    lcd.print("                ");
    Center_Text_EEPROM();
    lcd.print(Rec_Reset[Array_Increment]);
    Old_Position = New_Position;
    Debounce_Time_Reference = Current_Time;
    Screen_Change = 0;
  }
  if (Start_Button_Press == 1 && Last_Start_Press == 0 && Array_Increment == 10 && Current_Time > (Debounce_Time_Reference + BUTTON_DEBOUNCE)) {  //Reset Lap Record Variables and move on to Race Start
    Record_Lap = 10000;
    car_num_in_race[0] = 10;
    Record_Car_Num = 10;
    LapRecordDisplay();
  }
  if (Start_Button_Press == 1 && Last_Start_Press == 0 && Array_Increment != 10 && Current_Time > (Debounce_Time_Reference + BUTTON_DEBOUNCE)) {  //Do Not erase EEPROM and move on to Race Start
    Enter_Menu = 1;
    Start_Race_Menu = 1;
    Erase_Record_Lap_Menu = 0;
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
  if (Enter_Menu == 1) {  //Initialization of the Race Start Sequence
    FastLED.clear();
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Gentlemen, Start");
    lcd.setCursor(2, 1);
    lcd.print("Your Engines");
    LEDS.setBrightness(84);
    Enter_Menu = 0;
    for (int i = 0; i < BootColorArray; i++) {  //Turn on the Red Lights
      leds[RaceStartRed[i]] = CHSV(BootColors[2], 255, 255);
      delay(10);
    }
    FastLED.show();
    playSdWav1.play("RED.WAV");
    delay(RED_LIGHT_DELAY);
    digitalWrite(LANE1RELAYPIN, LOW);  //Enable Power to all Lanes
    digitalWrite(LANE2RELAYPIN, LOW);
    digitalWrite(LANE3RELAYPIN, LOW);
    digitalWrite(LANE4RELAYPIN, LOW);
    Debounce_Time_Reference = Current_Time;
    Array_Increment = 0;
  }
  if (Current_Time > (Debounce_Time_Reference + YELLOW_LIGHT_DELAY) && Array_Increment < 3) {  //Yellow light flashing sequence, unless a penality has occurred in a lane, see below
    if (Penality_Lane1 != 1) {
      leds[Lane1NP[Array_Increment]] = CHSV(BootColors[1], 255, 255);
    }
    if (Penality_Lane2 != 1) {
      leds[Lane2NP[Array_Increment]] = CHSV(BootColors[1], 255, 255);
    }
    if (Penality_Lane3 != 1) {
      leds[Lane3NP[Array_Increment]] = CHSV(BootColors[1], 255, 255);
    }
    if (Penality_Lane4 != 1) {
      leds[Lane4NP[Array_Increment]] = CHSV(BootColors[1], 255, 255);
    }
    Debounce_Time_Reference = Current_Time;
    playSdWav1.play("YELLOW.WAV");
    FastLED.show();
    Array_Increment++;
  }
  if (L1_State == 1 || L2_State == 1 || L3_State == 1 || L4_State == 1) {  //Whatches the Lanes for a premature start line cross and flags with a Penality
    if (L1_State == 1) {
      Penality_Lane1 = L1_State;
    }
    if (L2_State == 1) {
      Penality_Lane2 = L2_State;
    }
    if (L3_State == 1) {
      Penality_Lane3 = L3_State;
    }
    if (L4_State == 1) {
      Penality_Lane4 = L4_State;
    }
    Penality_Start();  //If a car crosses the start line before the green light, they are flagged with a penality and will be part of this function below
  }
  if (Current_Time > (Debounce_Time_Reference + YELLOW_LIGHT_DELAY) && Array_Increment == 3) {  //Green Light for the Start of the Race
    for (int i = 0; i < 4; i++) {
      if (Penality_Lane1 != 1) {
        leds[Lane1NP[i]] = CHSV(BootColors[0], 255, 255);
      }
      if (Penality_Lane2 != 1) {
        leds[Lane2NP[i]] = CHSV(BootColors[0], 255, 255);
      }
      if (Penality_Lane3 != 1) {
        leds[Lane3NP[i]] = CHSV(BootColors[0], 255, 255);
      }
      if (Penality_Lane4 != 1) {
        leds[Lane4NP[i]] = CHSV(BootColors[0], 255, 255);
      }
      delay(10);
    }
    playSdWav1.play("GREEN.WAV");
    FastLED.show();
    Start_Race_Menu = 0;
    Race_Metrics_Toggle = 1;
    Enter_Menu = 1;
    Debounce_Time_Reference = Current_Time;
  }
}
//If a car crosses the start line before the green light, they are flagged with a penality and the red lights turn on over the lane and power is cut for the penality duration
void Penality_Start() {
  if (Penality_Lane1 == 1) {
    digitalWrite(LANE1RELAYPIN, HIGH);
    for (int i = 0; i < 3; i++) {
      leds[Lane1NP[i]] = CHSV(BootColors[2], 255, 255);
    }
  }
  if (Penality_Lane2 == 1) {
    digitalWrite(LANE2RELAYPIN, HIGH);
    for (int i = 0; i < 3; i++) {
      leds[Lane2NP[i]] = CHSV(BootColors[2], 255, 255);
    }
  }
  if (Penality_Lane3 == 1) {
    digitalWrite(LANE3RELAYPIN, HIGH);
    for (int i = 0; i < 3; i++) {
      leds[Lane3NP[i]] = CHSV(BootColors[2], 255, 255);
    }
  }
  if (Penality_Lane4 == 1) {
    digitalWrite(LANE4RELAYPIN, HIGH);
    for (int i = 0; i < 3; i++) {
      leds[Lane4NP[i]] = CHSV(BootColors[2], 255, 255);
    }
  }
  FastLED.show();
}
//Temorarpy Pause of the Race
void Pause_Race() {
  if (Enter_Menu == 1) {  //Cut power to all lanes
    digitalWrite(LANE1RELAYPIN, HIGH);
    digitalWrite(LANE2RELAYPIN, HIGH);
    digitalWrite(LANE3RELAYPIN, HIGH);
    digitalWrite(LANE4RELAYPIN, HIGH);
    Pause_Offset = Current_Time;  //This is the time the race was paused so it can be adjusted for the current lap
    Enter_Menu = 0;
  }
  if (Current_Time > (Debounce_Time_Reference + YELLOW_LIGHT_DELAY) && Hazard_Toggle == 0) {  //Flash Yellow Lights and display message on LCD
    for (int i = 0; i < NUM_LEDS; i++) {
      leds[i] = CHSV(BootColors[1], 255, 255);
      delay(10);
    }
    lcd.clear();
    lcd.setCursor(2, 0);
    lcd.print("Race Paused!");
    Hazard_Toggle = 1;
    Debounce_Time_Reference = Current_Time;
  }
  if (Current_Time > (Debounce_Time_Reference + YELLOW_LIGHT_DELAY) && Hazard_Toggle == 1) {  //Flash Yellow Lights and display message on LCD
    FastLED.clear();
    lcd.clear();
    lcd.setCursor(2, 0);
    lcd.print("Press Start");
    lcd.setCursor(2, 1);
    lcd.print("to Continue");
    Hazard_Toggle = 0;
    Debounce_Time_Reference = Current_Time;
  }
  FastLED.show();

  if (Start_Button_Press == 1 && Last_Start_Press == 0 && Current_Time > (Debounce_Time_Reference + BUTTON_DEBOUNCE)) {  //Resumes Race
    lcd.clear();
    lcd.setCursor(2, 0);
    lcd.print("Resuming Race");
    FastLED.clear();
    for (int i = 0; i < BootColorArray; i++) {
      leds[RaceStartRed[i]] = CHSV(BootColors[2], 255, 255);
      delay(10);
    }
    playSdWav1.play("PAUSE.WAV");  //Neopixels go through Start sequence again and power is restores to all Lanes
    FastLED.show();
    delay(RED_LIGHT_DELAY);
    for (int i = 0; i < 3; i++) {
      playSdWav1.play("YELLOW.WAV");
      leds[Lane1NP[i]] = CHSV(BootColors[1], 255, 255);
      leds[Lane2NP[i]] = CHSV(BootColors[1], 255, 255);
      leds[Lane3NP[i]] = CHSV(BootColors[1], 255, 255);
      leds[Lane4NP[i]] = CHSV(BootColors[1], 255, 255);
      FastLED.show();
      delay(YELLOW_LIGHT_DELAY);
    }
    digitalWrite(LANE1RELAYPIN, LOW);
    digitalWrite(LANE2RELAYPIN, LOW);
    digitalWrite(LANE3RELAYPIN, LOW);
    digitalWrite(LANE4RELAYPIN, LOW);
    for (int i = 0; i < 4; i++) {
      playSdWav1.play("GREEN.WAV");
      leds[Lane1NP[i]] = CHSV(BootColors[0], 255, 255);
      leds[Lane2NP[i]] = CHSV(BootColors[0], 255, 255);
      leds[Lane3NP[i]] = CHSV(BootColors[0], 255, 255);
      leds[Lane4NP[i]] = CHSV(BootColors[0], 255, 255);
    }
    FastLED.show();
    Pause_Race_Toggle = 0;
    Race_Metrics_Toggle = 1;
    Enter_Menu = 1;
  }
}
//Stops race completely, Kills power to all lanes and resets unit for new race
void Stop_Race() {
  for (int i = 0; i < NUM_LEDS; i++) {  //Set all Lights to Red
    leds[i] = CHSV(BootColors[2], 255, 255);
    delay(10);
  }
  FastLED.show();
  lcd.clear();
  lcd.setCursor(3, 0);
  lcd.print("Race Ended");
  digitalWrite(LANE1RELAYPIN, HIGH);  //Cut Power to all Lanes
  digitalWrite(LANE2RELAYPIN, HIGH);
  digitalWrite(LANE3RELAYPIN, HIGH);
  digitalWrite(LANE4RELAYPIN, HIGH);
  delay(STOP_RACE_DELAY);
  Welcome_Message_Menu = 1;
  Stop_Race_Toggle = 0;
  Race_Over = 1;
}
//Monitors All Race Attributes
void Race_Metrics() {
  LEDS.setBrightness(NP_Brightness);  //Initialize Settings for race
  if (Enter_Menu == 1 && Race_Over == 0) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Race In Progress");
    Enter_Menu = 0;
    Current_Time = millis();
  }
  if (Stop_Button_Press == 1) {  //Monitor the Stop Button to cancel Race
    Stop_Race_Toggle = 1;
    Race_Metrics_Toggle = 0;
    Stop_Race();
  }
  if (Start_Button_Press == 1 && Race_Metrics_Toggle == 1) {  //Monitor the Start Button to Pause Race
    Enter_Menu = 1;
    Race_Metrics_Toggle = 0;
    Pause_Race_Toggle = 1;
  }
  if (Penality_Lane1 == 1 || Penality_Lane2 == 1 || Penality_Lane3 == 1 || Penality_Lane4 == 1) {  //Watches for penailty flags on any lane, restores power to the lane(s) after penality duration
    if (Penality_Lane1 == 1) {
      if (Current_Time > (Debounce_Time_Reference + PENALITY_DELAY)) {
        digitalWrite(LANE1RELAYPIN, LOW);
        Penality_Lane1 = 0;
        for (int i = 0; i < 4; i++) {
          leds[Lane1NP[i]] = CHSV(BootColors[0], 255, 255);
        }
      }
    }
    if (Penality_Lane2 == 1) {
      if (Current_Time > (Debounce_Time_Reference + PENALITY_DELAY)) {
        digitalWrite(LANE2RELAYPIN, LOW);
        Penality_Lane2 = 0;
        for (int i = 0; i < 4; i++) {
          leds[Lane2NP[i]] = CHSV(BootColors[0], 255, 255);
        }
      }
    }
    if (Penality_Lane3 == 1) {
      if (Current_Time > (Debounce_Time_Reference + PENALITY_DELAY)) {
        digitalWrite(LANE3RELAYPIN, LOW);
        Penality_Lane3 = 0;
        for (int i = 0; i < 4; i++) {
          leds[Lane3NP[i]] = CHSV(BootColors[0], 255, 255);
        }
      }
    }
    if (Penality_Lane4 == 1) {
      if (Current_Time > (Debounce_Time_Reference + PENALITY_DELAY)) {
        digitalWrite(LANE4RELAYPIN, LOW);
        Penality_Lane4 = 0;
        for (int i = 0; i < 4; i++) {
          leds[Lane4NP[i]] = CHSV(BootColors[0], 255, 255);
        }
      }
    }
    FastLED.show();
  }
  L1_State = digitalRead(LANE1LAP_TR_PIN);  //Reads the lap counter pins for car crossings
  L2_State = digitalRead(LANE2LAP_TR_PIN);
  L3_State = digitalRead(LANE3LAP_TR_PIN);
  L4_State = digitalRead(LANE4LAP_TR_PIN);
  if (L1_State == HIGH && Current_Time > (cars[0].total_time + 1000)) {  //When a car crosses the pin, record the current lap time and normalize for the display, mark time for Last Lap and increment the Lap counter.
    cars[0].lap_time = (Current_Time - cars[0].total_time) / 10;
    cars[0].total_time = Current_Time;
    cars[0].cur_lap = cars[0].cur_lap + 1;

    Lap_Counter();
    DetermineRacePlace();
  }
  if (L2_State == HIGH && Current_Time > (cars[1].total_time + 1000)) {
    cars[1].lap_time = (Current_Time - cars[1].total_time) / 10;
    cars[1].total_time = Current_Time;
    cars[1].cur_lap = cars[1].cur_lap + 1;

    Lap_Counter();
    DetermineRacePlace();
  }
  if (L3_State == HIGH && Current_Time > (cars[2].total_time + 1000)) {
    cars[2].lap_time = (Current_Time - cars[2].total_time) / 10;
    cars[2].total_time = Current_Time;
    cars[2].cur_lap = cars[2].cur_lap + 1;

    Lap_Counter();
    DetermineRacePlace();
  }
  if (L4_State == HIGH && Current_Time > (cars[3].total_time + 1000)) {
    cars[3].lap_time = (Current_Time - cars[3].total_time) / 10;
    cars[3].total_time = Current_Time;
    cars[3].cur_lap = cars[3].cur_lap + 1;

    Lap_Counter();
    DetermineRacePlace();
  }
  // Monitor for Lap Record
  for (int i = 0; i < Num_of_Racers; i++) {
    if (cars[i].lap_time < Record_Lap && cars[i].lap_time > 100) {
      Record_Lap = cars[i].lap_time;
      Record_Car_Num = i;
      RecordCar = cars[Record_Car_Num].car_number;
      LapRecordDisplay();
    }
  }
}
//Reads Lap Record from EEPROM and Displays on 7 Sgement Displays
void LapRecordDisplay() {
  char LapTimeRec_Buffer[4];
  char LapRecNum_Buffer[4];
  LapRecNum_Buffer[0] = Car_Numbers[RecordCar][0];
  LapRecNum_Buffer[1] = Car_Numbers[RecordCar][1];
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
//Monitor Lap Number and Display on 7 Segment Display
void Lap_Counter() {
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
// Determine what place each car is in
void DetermineRacePlace() {
  // Sorts the cars based on how many laps completed and lowest total race time
  qsort(cars, Num_of_Racers, sizeof(struct Car), cmp_lap_and_total_time);

  // By those metrics we can now determine what place each car is in
  for (int i = 0; i < Num_of_Racers; i++) {
    cars[i].place = i + 1;
  }

  // Now we need to get back into lane order
  qsort(cars, Num_of_Racers, sizeof(struct Car), lane_order);

  // Sorts the cars based on how many laps completed and lowest total race time
  qsort(cars, Num_of_Racers, sizeof(struct Car), cmp_lap_and_total_time);
  Display_Leaderboard();
  if (Num_of_Laps <= Current_Lap_Num) {
    if (Last_Lap == 0) {
      Enter_Menu = 1;
    }
    End_Race();
  }
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
  qsort(cars, Num_of_Racers, sizeof(struct Car), lane_order);

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
    if (Num_of_Racers >= 1) {
      P2Time.writeDigitAscii(0, P1Buffer_Time[0]);
      P2Time.writeDigitAscii(1, P1Buffer_Time[1], true);
      P2Time.writeDigitAscii(2, P1Buffer_Time[2]);
      P2Time.writeDigitAscii(3, P1Buffer_Time[3]);
      P1P2Num.writeDigitAscii(2, P1Buffer_Car[0]);
      P1P2Num.writeDigitAscii(3, P1Buffer_Car[1]);
    }
    if (Num_of_Racers >= 2) {
      P1Time.writeDigitAscii(0, P2Buffer_Time[0]);
      P1Time.writeDigitAscii(1, P2Buffer_Time[1], true);
      P1Time.writeDigitAscii(2, P2Buffer_Time[2]);
      P1Time.writeDigitAscii(3, P2Buffer_Time[3]);
      P1P2Num.writeDigitAscii(0, P2Buffer_Car[0]);
      P1P2Num.writeDigitAscii(1, P2Buffer_Car[1]);
    }
    if (Num_of_Racers >= 3) {
      P3Time.writeDigitAscii(0, P3Buffer_Time[0]);
      P3Time.writeDigitAscii(1, P3Buffer_Time[1], true);
      P3Time.writeDigitAscii(2, P3Buffer_Time[2]);
      P3Time.writeDigitAscii(3, P3Buffer_Time[3]);
      P3P4Num.writeDigitAscii(2, P3Buffer_Car[0]);
      P3P4Num.writeDigitAscii(3, P3Buffer_Car[1]);
    }
    if (Num_of_Racers == 4) {
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
  }
}
//Final Lap and Finish Actions
void End_Race() {
  //At Last Lap Turn all LEDs white and play the Last Lap song
  // if (Num_of_Laps == Current_Lap_Num && Enter_Menu == 1) {
  //   Enter_Menu = 0;
  //   Last_Lap = 1;
  //   playSdWav1.play("LASTLAP.WAV");
  //   for (int i = 0; i < 4; i++) {
  //     if (Num_of_Racers == 1) {
  //       leds[Lane1NP[i]] = CRGB(255, 255, 255);
  //       leds[Lane2NP[i]] = CRGB(0, 0, 0);
  //       leds[Lane3NP[i]] = CRGB(0, 0, 0);
  //       leds[Lane4NP[i]] = CRGB(0, 0, 0);
  //     }
  //     if (Num_of_Racers == 2) {
  //       leds[Lane1NP[i]] = CRGB(255, 255, 255);
  //       leds[Lane2NP[i]] = CRGB(255, 255, 255);
  //       leds[Lane3NP[i]] = CRGB(0, 0, 0);
  //       leds[Lane4NP[i]] = CRGB(0, 0, 0);
  //     }
  //     if (Num_of_Racers == 3) {
  //       leds[Lane1NP[i]] = CRGB(255, 255, 255);
  //       leds[Lane2NP[i]] = CRGB(255, 255, 255);
  //       leds[Lane3NP[i]] = CRGB(255, 255, 255);
  //       leds[Lane4NP[i]] = CRGB(0, 0, 0);
  //     }
  //     if (Num_of_Racers == 4) {
  //       leds[Lane1NP[i]] = CRGB(255, 255, 255);
  //       leds[Lane2NP[i]] = CRGB(255, 255, 255);
  //       leds[Lane3NP[i]] = CRGB(255, 255, 255);
  //       leds[Lane4NP[i]] = CRGB(255, 255, 255);
  //     }
  //   }
  //   FastLED.show();
  // }
  if (Num_of_Laps == Current_Lap_Num && Enter_Menu == 1) {
    Enter_Menu = 0;
    Last_Lap = 1;
    for (int i = 0; i < 4; i++) {
      if (Num_of_Laps == cars[0].cur_lap) {
        playSdWav1.play("LASTLAP.WAV");
        leds[Lane1NP[i]] = CRGB(255, 255, 255);
      }
      if (Num_of_Laps == cars[1].cur_lap) {
        playSdWav1.play("LASTLAP.WAV");
        leds[Lane2NP[i]] = CRGB(255, 255, 255);
      }
      if (Num_of_Laps == cars[2].cur_lap) {
        playSdWav1.play("LASTLAP.WAV");
        leds[Lane3NP[i]] = CRGB(255, 255, 255);
      }
      if (Num_of_Laps == cars[3].cur_lap) {
        playSdWav1.play("LASTLAP.WAV");
        leds[Lane4NP[i]] = CRGB(255, 255, 255);
      }
    }
    FastLED.show();
  }
  if (Num_of_Laps <= Current_Lap_Num) {
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
  //Action When Car in Lane 1 Finishes the Race
  if (cars[0].cur_lap > Num_of_Laps && cars[0].finish == 0) {
    digitalWrite(LANE1RELAYPIN, HIGH);  //Cut Power to the Lane
    cars[0].finish = 1;
    for (int i = 0; i < 4; i++) {
      leds[Lane1NP[i]] = CRGB(0, 0, 0);
    }
    FastLED.show();
    delay(10);
    //Display The Correct Position LED Pattern
    if (cars[0].place == 1) {
      leds[Lane1NP[1]] = CRGB(0, 255, 255);
    }
    if (cars[0].place == 2) {
      playSdWav1.play("RECORD.WAV");
      leds[Lane1NP[0]] = CRGB(0, 255, 255);
      leds[Lane1NP[2]] = CRGB(0, 255, 255);
    }
    if (cars[0].place == 3) {
      playSdWav1.play("RECORD.WAV");
      for (int i = 0; i < 3; i++) {
        leds[Lane1NP[i]] = CRGB(0, 255, 255);
      }
    }
    if (cars[0].place == 4) {
      playSdWav1.play("RECORD.WAV");
      for (int i = 0; i < 4; i++) {
        leds[Lane1NP[i]] = CRGB(0, 255, 255);
      }
    }
    FastLED.show();
  }
  //Action When Car in Lane 2 Finishes the Race
  if (cars[1].cur_lap > Num_of_Laps && cars[1].finish == 0) {
    digitalWrite(LANE2RELAYPIN, HIGH);  //Cut Power to the Lane
    cars[1].finish = 1;
    for (int i = 0; i < 4; i++) {
      leds[Lane2NP[i]] = CRGB(0, 0, 0);
    }
    FastLED.show();
    delay(10);
    //Display The Correct Position LED Pattern
    if (cars[1].place == 1) {
      leds[Lane2NP[1]] = CRGB(0, 255, 255);
    }
    if (cars[1].place == 2) {
      playSdWav1.play("RECORD.WAV");
      leds[Lane2NP[0]] = CRGB(0, 255, 255);
      leds[Lane2NP[2]] = CRGB(0, 255, 255);
    }
    if (cars[1].place == 3) {
      playSdWav1.play("RECORD.WAV");
      for (int i = 0; i < 3; i++) {
        leds[Lane2NP[i]] = CRGB(0, 255, 255);
      }
    }
    if (cars[1].place == 4) {
      playSdWav1.play("RECORD.WAV");
      for (int i = 0; i < 4; i++) {
        leds[Lane2NP[i]] = CRGB(0, 255, 255);
      }
    }
    FastLED.show();
  }
  //Action When Car in Lane 3 Finishes the Race
  if (cars[2].cur_lap > Num_of_Laps && cars[2].finish == 0) {
    digitalWrite(LANE3RELAYPIN, HIGH);  //Cut Power to the Lane
    cars[2].finish = 1;
    for (int i = 0; i < 4; i++) {
      leds[Lane3NP[i]] = CRGB(0, 0, 0);
    }
    FastLED.show();
    delay(10);
    //Display The Correct Position LED Pattern
    if (cars[2].place == 1) {
      leds[Lane3NP[1]] = CRGB(0, 255, 255);
    }
    if (cars[2].place == 2) {
      playSdWav1.play("RECORD.WAV");
      leds[Lane3NP[0]] = CRGB(0, 255, 255);
      leds[Lane3NP[2]] = CRGB(0, 255, 255);
    }
    if (cars[2].place == 3) {
      playSdWav1.play("RECORD.WAV");
      for (int i = 0; i < 3; i++) {
        leds[Lane3NP[i]] = CRGB(0, 255, 255);
      }
    }
    if (cars[2].place == 4) {
      playSdWav1.play("RECORD.WAV");
      for (int i = 0; i < 4; i++) {
        leds[Lane3NP[i]] = CRGB(0, 255, 255);
      }
    }
    FastLED.show();
  }
  //Action When Car in Lane 4 Finishes the Race
  if (cars[3].cur_lap > Num_of_Laps && cars[3].finish == 0) {
    digitalWrite(LANE4RELAYPIN, HIGH);  //Cut Power to the Lane
    cars[3].finish = 1;
    for (int i = 0; i < 4; i++) {
      leds[Lane4NP[i]] = CRGB(0, 0, 0);
    }
    FastLED.show();
    delay(10);
    //Display The Correct Position LED Pattern
    if (cars[3].place == 1) {
      leds[Lane4NP[1]] = CRGB(0, 255, 255);
    }
    if (cars[3].place == 2) {
      playSdWav1.play("RECORD.WAV");
      leds[Lane4NP[0]] = CRGB(0, 255, 255);
      leds[Lane4NP[2]] = CRGB(0, 255, 255);
    }
    if (cars[3].place == 3) {
      playSdWav1.play("RECORD.WAV");
      for (int i = 0; i < 3; i++) {
        leds[Lane4NP[i]] = CRGB(0, 255, 255);
      }
    }
    if (cars[3].place == 4) {
      playSdWav1.play("RECORD.WAV");
      for (int i = 0; i < 4; i++) {
        leds[Lane4NP[i]] = CRGB(0, 255, 255);
      }
    }
    FastLED.show();
  }
  // Determine the number of cars that have finished the race
  int cars_finished = 0;
  for (int i = 0; i < Num_of_Racers; i++) {
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
  if (Num_of_Racers == cars_finished) {
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
  Race_Time = 0;
  Num_of_Laps = 5;
  Num_of_Racers = 4;
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
