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

//NeoPixel Assignments
#define NUM_LEDS 16 // How many leds in your strip?
#define DATA_PIN 39

//I2S Audio Assignments
AudioPlaySdWav           playSdWav1;
AudioMixer4              mixer1;
AudioOutputI2S           i2s1;
AudioConnection          patchCord1(playSdWav1, 0, mixer1, 0);
AudioConnection          patchCord2(playSdWav1, 1, mixer1, 1);
AudioConnection          patchCord3(mixer1, 0, i2s1, 0);
AudioConnection          patchCord4(mixer1, 0, i2s1, 1);

//Neopixel Arrays
CRGB leds[NUM_LEDS]; // Define the array of leds
int BootNP[16] = {13, 14, 12, 15, 0, 11, 1, 10, 2, 9, 3, 8, 4, 7, 5, 6};
int BootColors[4] = {0, 64, 96, 160};
int BootColorArray = 4;
int RaceStartRed[4] = {12, 13, 14, 15};
int Lane1NP[4] = {11, 10, 9, 12};
int Lane2NP[4] = {8, 7, 6, 13};
int Lane3NP[4] = {5, 4, 3, 14};
int Lane4NP[4] = {2, 1, 0, 15};

//7 Seg LED Assignments
Adafruit_AlphaNum4 P1P2Num = Adafruit_AlphaNum4();
Adafruit_AlphaNum4 P3P4Num = Adafruit_AlphaNum4();
Adafruit_AlphaNum4 LapRecNum = Adafruit_AlphaNum4();
Adafruit_AlphaNum4 P2Time = Adafruit_AlphaNum4();
Adafruit_AlphaNum4 P1Time = Adafruit_AlphaNum4();
Adafruit_AlphaNum4 P3Time = Adafruit_AlphaNum4();
Adafruit_AlphaNum4 P4Time = Adafruit_AlphaNum4();
Adafruit_AlphaNum4 LapTimeRec = Adafruit_AlphaNum4();

//Pin Assignments
#define SDCARD_CS_PIN    BUILTIN_SDCARD //used for Audio Playback
#define SDCARD_MOSI_PIN  61             //used for Audio Playback 11
#define SDCARD_SCK_PIN   60             //used for Audio Playback 13
#define RE_BUTTON        26             //Rotary Encoder Button
#define BACK_BUTTON      24             //Back Button
#define START_BUTTON     25             //Start Race Button
#define STOP_BUTTON      27             //Pause Button
#define LANE1RELAYPIN    17             //Controls the Relay for Lane 1 in the Control Box
#define LANE2RELAYPIN    16             //Controls the Relay for Lane 2 in the Control Box
#define LANE3RELAYPIN    15             //Controls the Relay for Lane 3 in the Control Box
#define LANE4RELAYPIN    14             //Controls the Relay for Lane 4 in the Control Box
#define LANE1LAP_TR_PIN  36             //Lap counter switch in Lane 1
#define LANE2LAP_TR_PIN  35             //Lap counter switch in Lane 2
#define LANE3LAP_TR_PIN  34             //Lap counter switch in Lane 3
#define LANE4LAP_TR_PIN  33             //Lap counter switch in Lane 4
#define LED_PIN          13             //Notification LED
#define ENCODER1_PIN     29             //Notification LED
#define ENCODER2_PIN     28             //Notification LED

// LCD Backpack Setup
Adafruit_LiquidCrystal lcd(1); //default address #0 (A0-A2 not jumpered)

//Encoder Setup
Encoder myEnc(ENCODER1_PIN, ENCODER2_PIN);

//Variables
//Menu Navigation - The variables trigger a menu change when flagged as 1
int Welcome_Message_Menu = 1;
int Number_of_Racers_Menu = 0;
int Track_Select_Menu = 0;
int Number_of_Laps_Menu = 0;
int Driver_Lane_Assign_Menu = 0;
int Car_Num_Lane_Assign_Menu = 0;
int Start_Race_Menu = 0;
int Enter_Menu = 1;
int Screen_Change = 0;
int Array_Increment = 0;
int Race_Metrics_Toggle = 0;
int Stop_Race_Toggle = 0;
int Pause_Race_Toggle = 0;
int Hazard_Toggle = 0;

//Rotary Encoder - Logs the position of the Rotary Encoder
long Old_Position  = -999;
long New_Position;

//Debouncing
unsigned long Current_Time;           //Current Millis
unsigned long Debounce_Time_Reference; //Millis reference when button is presses
unsigned long Pause_Offset;           //Time to ajdust in the event the race is paused
int  Last_Back_Press = 0;             //Flags an event when the Back Button is pressed
int  Last_Start_Press = 0;            //Flags an event when the Start Button is pressed
int  Last_Stop_Press = 0;             //Flags an event when the Stop Button is pressed
#define RE_DEBOUNCE 125               //Debounce time for the Rotary Encoder 
#define BUTTON_DEBOUNCE 200           //Debounce time for a Button Press 

//Race Identifiers
int Num_of_Laps = 5;   //Number of laps in the Race (Can be Modified in Menu 5-99)
int Num_of_Racers = 4; //Number of Racers in the Race (Can be Modified in Menu 1-4)
int Num_Track = 1;     //Current Track Configuration (Can be Modified in Menu 1-99)
int Num_Lane = 1;      //Used for Lane Assignment of Drivers/Car/Lap Times

//Driver Information
String L1Driver;               //Driver Names (Used for Lap Record) (References Driver_Names Array)
String L2Driver;
String L3Driver;
String L4Driver;

//Car Information
char L1CarNum;                //Car Numbers (References Driver_Numbers Array)
char L2CarNum;
char L3CarNum;
char L4CarNum;
char P1_CarNum;               //Car Number in Order of Place in Race
char P2_CarNum;
char P3_CarNum;
char P4_CarNum;

//Button Status Monitors
int Start_Button_Press = 0;   //Triggers an event when the Start Button is pressed
int Back_Button_Press = 0;    //Triggers an event when the Back Button is pressed
int Stop_Button_Press = 0;    //Triggers an event when the Stop Button is pressed

//Race Information
int lane_order[4];
int Current_Lap_Num = 0;         //Overall Lap Count
int Lane1State = 0;              //In Track Lap Counter Monitors State, per lane
int Lane2State = 0;
int Lane3State = 0;
int Lane4State = 0;
unsigned long Race_Time;         //Overall Race Duration in millis
unsigned long L1LastLap_Time;    //Last Lap time marker in millis, per lane
unsigned long L2LastLap_Time;
unsigned long L3LastLap_Time;
unsigned long L4LastLap_Time;
int L1LapCount = 0;              //Lap Count, Per Lane
int L2LapCount = 0;
int L3LapCount = 0;
int L4LapCount = 0;
unsigned long L1CurrentLap;                //Per Car Current Lap Time, per lane
unsigned long L2CurrentLap;
unsigned long L3CurrentLap;
unsigned long L4CurrentLap;
int P1CurrentLap;                //Car Lap Times in order of place in race, per lane
int P2CurrentLap;
int P3CurrentLap;
int P4CurrentLap;
int P1Finish = 0;                //Flag for when a car has finished the race, per lane
int P2Finish = 0;
int P3Finish = 0;
int P4Finish = 0;

//Screen Variables
int Center_Value;  //Used to center the text on the screen

//Neopixel Variables
#define NP_Brightness 84             //Set Neopixel Brightness
#define START_SEQUENCE_DELAY 100     //Start Animation Speed (Higher = Slower)
#define DIM_DELAY 50                 //Dimming Speed (Higher = Slower)
#define YELLOW_LIGHT_DELAY 750       //Delay between Yellow Lights
#define RED_LIGHT_DELAY 4250         //Time for Red Lights
#define STOP_RACE_DELAY 10000        //Time for red lights to be active before reset

//Relay/Penality Variables
int Penality_Lane1 = 0;        //Flag if car crosses start line before the green light, per lane
int Penality_Lane2 = 0;
int Penality_Lane3 = 0;
int Penality_Lane4 = 0;
#define PENALITY_DELAY 5000    //Penality Duration

//Menu Arrays
//Used to Display Driver Names on Screen and record lap records
String Driver_Names[6] = {"Gino", "Luca", "Patrick", "Mike", "Laura", "Guest"};
//Used to Diaplay Car Names and Numbers on LCD
String Car_Names[10] = {"01 Skyline", "03 Ford Capri", "05 BMW 3.5 CSL", "05 Lancia LC2", "33 BMW M1", "51 Porsche 935", "576 Lancia Beta", "80 Audi RS5", "DE BTTF Delorean", "MM GT Falcon V8"};
//Used to Display Numbers on 7 Segment and Lap Records
String Car_Numbers[10] = {"01", "03", "05", "05", "33", "51", "57", "80", "DE", "MM"}; //was *char


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

  //Setup the 7-Segment LED Panels
  P1P2Num.begin(0x70);      // pass in the address for the Place 1 and 2 Car Numbers
  P3P4Num.begin(0x71);      // pass in the address for the Place 3 and 4 Car Numbers
  LapRecNum.begin(0x72);    // pass in the address for the Lap Record Car Number
  LapTimeRec.begin(0x77);   // pass in the address for the Lap Record Time
  P2Time.begin(0x73);       // pass in the address for the Place 1 Lap Time
  P1Time.begin(0x74);       // pass in the address for the Place 2 Lap Time
  P3Time.begin(0x75);       // pass in the address for the Place 3 Lap Time
  P4Time.begin(0x76);       // pass in the address for the Place 4 Lap Time

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
}

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
  if (Track_Select_Menu == 1) {
    Track_Select();
  }
  if (Number_of_Laps_Menu == 1) {
    Number_of_Laps();
  }
  if (Race_Metrics_Toggle == 1) {
    Race_Metrics();
  }
  if (Start_Race_Menu == 1) {
    Lane1State = digitalRead(LANE1LAP_TR_PIN);
    Lane2State = digitalRead(LANE2LAP_TR_PIN);
    Lane3State = digitalRead(LANE3LAP_TR_PIN);
    Lane4State = digitalRead(LANE4LAP_TR_PIN);
    Driver_Lane_Assign_Menu = 0;
    Start_Race();
  }
  if (Driver_Lane_Assign_Menu == 1) {
    Driver_Lane_Assign();
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
  if (Back_Button_Press == 1 && Last_Back_Press == 0 && Current_Time > (Debounce_Time_Reference + RE_DEBOUNCE)) {
    Menu_Back();
  }
  //Record button presses to avoid rapid repeats
  Last_Back_Press = Back_Button_Press;
  Last_Start_Press = Start_Button_Press;
  Last_Stop_Press = Stop_Button_Press;
}

void playFile(const char *filename) {
  playSdWav1.play(filename);        // Start playing the file.  The sketch continues to run while the file plays.
  delay(5);                         // A brief delay for the library read WAV info
  while (playSdWav1.isPlaying()) {  // Simply wait for the file to finish playing.
  }
}

void Menu_Back() {
  Debounce_Time_Reference = Current_Time;
  Enter_Menu = 1;                         //Enables menu intros

  //Navigate backwards through the race setup menu
  if (Number_of_Laps_Menu == 1) {
    Number_of_Racers_Menu = 1;
    Number_of_Laps_Menu = 0;
  }
  if (Track_Select_Menu == 1) {
    Number_of_Laps_Menu = 1;
    Track_Select_Menu = 0;
  }
  if (Driver_Lane_Assign_Menu == 1) {
    Num_Lane = 1;
    Track_Select_Menu = 1;
    Driver_Lane_Assign_Menu = 0;
  }
  if (Car_Num_Lane_Assign_Menu == 1) {
    Num_Lane = 1;
    Driver_Lane_Assign_Menu = 1;
    Car_Num_Lane_Assign_Menu = 0;
  }
}

void Welcome_Message() {
  lcd.setCursor(3, 0);
  lcd.print("Welcome to");
  lcd.setCursor(3, 1);
  lcd.print("the Race!");
  playSdWav1.play("WELCOME.WAV");

  //Intro Green/Yellow/Red Blue Neopixel Animation
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

void Rotary_Encoder() { //Rotary Encoder monitoring
  if (Current_Time > (Debounce_Time_Reference + RE_DEBOUNCE)) {  //Buffer is 250
    New_Position = myEnc.read();
  }
  else {
    New_Position = myEnc.read();
    Old_Position = New_Position;
  }
}

//Menu Section fro selecting # of racers
void Number_of_Racers() {
  if (Enter_Menu == 1) {           //Initialization of the Racers Menu
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Number of Racers");
    lcd.setCursor(7, 1);
    lcd.print(Num_of_Racers);
    Num_of_Racers = 4;
    Enter_Menu = 0;
  }
  Rotary_Encoder();
  if (New_Position > Old_Position) { //Watch the Rotary Encoder and add to the number of racers
    Debounce_Time_Reference = Current_Time;
    Num_of_Racers++;
    if (Num_of_Racers > 4) {
      Num_of_Racers = 4;
    }
    Screen_Change = 1;
  }
  if (New_Position < Old_Position) { //Watch the Rotary Encoder and subtract from the number of racers
    Debounce_Time_Reference = Current_Time;
    Num_of_Racers--;
    if (Num_of_Racers < 1) {
      Num_of_Racers = 1;
    }
    Screen_Change = 1;
  }
  if (Screen_Change == 1) {          //Update the number of racers and display on LCD
    playSdWav1.play("TICK.WAV");
    lcd.setCursor(7, 1);
    lcd.print(Num_of_Racers);
    Debounce_Time_Reference = Current_Time;
    Old_Position = New_Position;
    Screen_Change = 0;
  }
  if (Start_Button_Press == 1 && Last_Start_Press == 0 && Current_Time > (Debounce_Time_Reference + BUTTON_DEBOUNCE)) {    //Move on to Number of Laps Menu
    Debounce_Time_Reference = Current_Time;
    Number_of_Racers_Menu = 0;
    Number_of_Laps_Menu = 1;
    Enter_Menu = 1;
  }
}

void Number_of_Laps() {
  if (Enter_Menu == 1) {              //Initialization of the Laps Menu
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
  if (New_Position > Old_Position) {   //Watch the Rotary Encoder and add to the number of Laps
    Num_of_Laps++;
    Debounce_Time_Reference = Current_Time;
    if (Num_of_Laps > 99) {
      Num_of_Laps = 5;
    }
    Screen_Change = 1;
  }
  if (New_Position < Old_Position) {   //Watch the Rotary Encoder and subtract from the number of Laps
    Debounce_Time_Reference = Current_Time;
    Num_of_Laps--;
    if (Num_of_Laps < 5) {
      Num_of_Laps = 99;
    }
    Screen_Change = 1;
  }
  if (Screen_Change == 1) {          //Update the number of laps and display on LCD
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
  if (Start_Button_Press == 1 && Last_Start_Press == 0 && Current_Time > (Debounce_Time_Reference + BUTTON_DEBOUNCE)) { //Move on to Track Select Menu
    Debounce_Time_Reference = Current_Time;
    Number_of_Laps_Menu = 0;
    Track_Select_Menu = 1;
    Enter_Menu = 1;
  }
}

void Track_Select() {
  if (Enter_Menu == 1) {         //Initialization of the Tracks Menu
    lcd.clear();
    lcd.setCursor(2, 0);
    lcd.print("Track Number");
    lcd.setCursor(7, 1);
    lcd.print(Num_Track);
    Start_Button_Press = 0;
    Num_Track = 1;
    Enter_Menu = 0;
  }
  Rotary_Encoder();
  if (New_Position > Old_Position) {  //Watch the Rotary Encoder and add to the Track Number
    Debounce_Time_Reference = Current_Time;
    Num_Track++;
    if (Num_Track > 99) {
      Num_Track = 1;
    }
    Screen_Change = 1;
  }
  if (New_Position < Old_Position) {    //Watch the Rotary Encoder and subtract from the Track Number
    Debounce_Time_Reference = Current_Time;
    Num_Track--;
    if (Num_Track < 1) {
      Num_Track = 99;
    }
    Screen_Change = 1;
  }
  if (Screen_Change == 1) {          //Update the Track Number and display on LCD
    playSdWav1.play("TICK.WAV");
    lcd.setCursor(7, 1);
    lcd.print(Num_Track);
    if (Num_Track < 10) {
      lcd.setCursor(8, 1);
      lcd.print(" ");
    }
    Old_Position = New_Position;
    Debounce_Time_Reference = Current_Time;
    Screen_Change = 0;
  }
  if (Start_Button_Press == 1 && Last_Start_Press == 0 && Current_Time > (Debounce_Time_Reference + BUTTON_DEBOUNCE)) { //Move on to Driver Select Menu
    Debounce_Time_Reference = Current_Time;
    Track_Select_Menu = 0;
    Driver_Lane_Assign_Menu = 1;
    Enter_Menu = 1;
  }
}

void Center_Text_Driver() { //Centers the driver's name on the LCD
  String Driver_Name = Driver_Names[Array_Increment];
  Center_Value = (16 - Driver_Name.length()) / 2;
  lcd.setCursor(Center_Value, 1);
}

void Center_Text_Car() { //Centers the car's number on the LCD
  String Car_Name = Car_Names[Array_Increment];
  Center_Value = (16 - Car_Name.length()) / 2;
  lcd.setCursor(Center_Value, 1);
}

void Driver_Lane_Assign() {         //Initialization of the Driver Menu
  if (Enter_Menu == 1) {
    Debounce_Time_Reference = Current_Time;
    Start_Button_Press = 0;
    Array_Increment = 0;
    Enter_Menu = 0;
    lcd.clear();
    lcd.setCursor(2, 0);
    lcd.print("Lane");
    lcd.setCursor(7, 0);
    lcd.print(Num_Lane);
    lcd.setCursor(9, 0);
    lcd.print("Driver");
    Center_Text_Driver();
    lcd.print(Driver_Names[Array_Increment]);
  }
  Rotary_Encoder();
  if (New_Position > Old_Position) {   //Watch the Rotary Encoder and display the next driver name
    Debounce_Time_Reference = Current_Time;
    Array_Increment++;
    if (Array_Increment > 5) {
      Array_Increment = 1;
    }
    Screen_Change = 1;
  }
  if (New_Position < Old_Position) {  //Watch the Rotary Encoder and display the previous driver name
    Debounce_Time_Reference = Current_Time;
    Array_Increment--;
    if (Array_Increment < 1) {
      Array_Increment = 5;
    }
    Screen_Change = 1;
  }
  if (Screen_Change == 1) {             //Update the Driver Name and display on LCD
    playSdWav1.play("TICK.WAV");
    lcd.setCursor(0, 1);
    lcd.print("                ");
    Center_Text_Driver();
    lcd.print(Driver_Names[Array_Increment]);
    Old_Position = New_Position;
    Debounce_Time_Reference = Current_Time;
    Screen_Change = 0;
  }
  if (Start_Button_Press == 1 && Last_Start_Press == 0 && Num_Lane > Num_of_Racers) {  //Move on to Race Start (Used if less than 4 Drivers)
    Enter_Menu = 1;
    Start_Race_Menu = 1;
    Driver_Lane_Assign_Menu = 0;
    Car_Num_Lane_Assign_Menu = 0;
  }
  if (Start_Button_Press == 1 && Last_Start_Press == 0 && Num_Lane == 4  && Num_of_Racers == 4 && Current_Time > (Debounce_Time_Reference + BUTTON_DEBOUNCE)) {  //Goto Car Select Menu for Lane 4
    L4Driver = Array_Increment;
    Enter_Menu = 1;
    Car_Num_Lane_Assign_Menu = 1;
    Driver_Lane_Assign_Menu  = 0;
  }
  if (Start_Button_Press == 1 && Last_Start_Press == 0 && Num_Lane == 3  && Num_of_Racers >= 3 && Current_Time > (Debounce_Time_Reference + BUTTON_DEBOUNCE)) {  //Goto Car Select Menu for Lane 3
    L3Driver = Array_Increment;
    Enter_Menu = 1;
    Car_Num_Lane_Assign_Menu = 1;
    Driver_Lane_Assign_Menu  = 0;
  }
  if (Start_Button_Press == 1 && Last_Start_Press == 0 && Num_Lane == 2  && Num_of_Racers >= 2 && Current_Time > (Debounce_Time_Reference + BUTTON_DEBOUNCE)) {  //Goto Car Select Menu for Lane 2
    L2Driver = Array_Increment;
    Enter_Menu = 1;
    Car_Num_Lane_Assign_Menu = 1;
    Driver_Lane_Assign_Menu  = 0;
  }
  if (Start_Button_Press == 1 && Last_Start_Press == 0 && Num_Lane == 1 && Current_Time > (Debounce_Time_Reference + BUTTON_DEBOUNCE)) {  //Goto Car Select Menu for Lane 1
    L1Driver = Array_Increment;
    Enter_Menu = 1;
    Car_Num_Lane_Assign_Menu = 1;
    Driver_Lane_Assign_Menu  = 0;
  }
}

void Car_Num_Lane_Assign() {
  if (Enter_Menu == 1) {         //Initialization of the Cars Menu
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
  if (New_Position > Old_Position) {     //Watch the Rotary Encoder and display the next car number
    Debounce_Time_Reference = Current_Time;
    Array_Increment++;
    if (Array_Increment > 9) {
      Array_Increment = 1;
    }
    Screen_Change = 1;
  }
  if (New_Position < Old_Position) {    //Watch the Rotary Encoder and display the previous car number
    Debounce_Time_Reference = Current_Time;
    Array_Increment--;
    if (Array_Increment < 1) {
      Array_Increment = 9;
    }
    Screen_Change = 1;
  }
  if (Screen_Change == 1) {             //Update the car number and display on LCD
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
  if (Start_Button_Press == 1 && Last_Start_Press == 0 && Num_Lane == 4  && Num_of_Racers == 4 && Current_Time > (Debounce_Time_Reference + BUTTON_DEBOUNCE)) {      //Move on to Race Start
    L4CarNum = Array_Increment;
    Enter_Menu = 1;
    Start_Race_Menu = 1;
    Driver_Lane_Assign_Menu = 0;
    Car_Num_Lane_Assign_Menu = 0;
  }
  if (Start_Button_Press == 1 && Last_Start_Press == 0 && Num_Lane == 3  && Num_of_Racers >= 3 && Current_Time > (Debounce_Time_Reference + BUTTON_DEBOUNCE)) {    //Goto Driver Select Menu for Lane 4
    L3CarNum = Array_Increment;
    Num_Lane++;
    Enter_Menu = 1;
    Driver_Lane_Assign_Menu = 1;
    Car_Num_Lane_Assign_Menu = 0;
  }
  if (Start_Button_Press == 1 && Last_Start_Press == 0 && Num_Lane == 2  && Num_of_Racers >= 2 && Current_Time > (Debounce_Time_Reference + BUTTON_DEBOUNCE)) {    //Goto Driver Select Menu for Lane 3
    L2CarNum = Array_Increment;
    Num_Lane++;
    Enter_Menu = 1;
    Driver_Lane_Assign_Menu = 1;
    Car_Num_Lane_Assign_Menu = 0;
  }
  if (Start_Button_Press == 1 && Last_Start_Press == 0 && Num_Lane == 1 && Current_Time > (Debounce_Time_Reference + BUTTON_DEBOUNCE)) {    //Goto Driver Select Menu for Lane 2
    L1CarNum = Array_Increment;
    Num_Lane++;
    Enter_Menu = 1;
    Driver_Lane_Assign_Menu = 1;
    Car_Num_Lane_Assign_Menu = 0;

  }
}

void Pole_Pos_Display() {  //Inital Display of Car numnbers on 8 Segment Displays
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

void Start_Race() {
  if (Enter_Menu == 1) {         //Initialization of the Race Start Sequence
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
  if (Current_Time > (Debounce_Time_Reference + YELLOW_LIGHT_DELAY) && Array_Increment < 3) { //Yellow light flashing sequence, unless a penality has occurred in a lane, see below
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
  if (Lane1State == 1 || Lane2State == 1 || Lane3State == 1 || Lane4State == 1) {  //Whatches the Lanes for a premature start line cross and flags with a Penality
    if (Lane1State == 1) {
      Penality_Lane1 = Lane1State;
    }
    if (Lane2State == 1) {
      Penality_Lane2 = Lane2State;
    }
    if (Lane3State == 1) {
      Penality_Lane3 = Lane3State;
    }
    if (Lane4State == 1) {
      Penality_Lane4 = Lane4State;
    }
    Penality_Start(); //If a car crosses the start line before the green light, they are flagged with a penality and will be part of this function below
  }
  if (Current_Time > (Debounce_Time_Reference + YELLOW_LIGHT_DELAY) && Array_Increment == 3) { //Green Light for the Start of the Race
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

void Penality_Start() { //If a car crosses the start line before the green light, they are flagged with a penality and the yellow lights turn on over the lane and power is cut for the penality duration
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

void Pause_Race() {
  if (Enter_Menu == 1) { //Cut power to all lanes
    digitalWrite(LANE1RELAYPIN, HIGH);
    digitalWrite(LANE2RELAYPIN, HIGH);
    digitalWrite(LANE3RELAYPIN, HIGH);
    digitalWrite(LANE4RELAYPIN, HIGH);
    Pause_Offset = Current_Time;  //This is the time the race was paused so it can be adjusted for the current lap
    Enter_Menu = 0;
  }
  if (Current_Time > (Debounce_Time_Reference + YELLOW_LIGHT_DELAY) && Hazard_Toggle == 0) { //Flash Yellow Lights and display message on LCD
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
  if (Current_Time > (Debounce_Time_Reference + YELLOW_LIGHT_DELAY) && Hazard_Toggle == 1) { //Flash Yellow Lights and display message on LCD
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

void Stop_Race() { //Stops race completely, Kills power to all lanes and resets unit for new race
  for (int i = 0; i < NUM_LEDS; i++) {
    leds[i] = CHSV(BootColors[2], 255, 255);
    delay(10);
  }
  FastLED.show();
  lcd.clear();
  lcd.setCursor(3, 0);
  lcd.print("Race Ended");
  digitalWrite(LANE1RELAYPIN, HIGH);
  digitalWrite(LANE2RELAYPIN, HIGH);
  digitalWrite(LANE3RELAYPIN, HIGH);
  digitalWrite(LANE4RELAYPIN, HIGH);
  delay(STOP_RACE_DELAY);
  Welcome_Message_Menu = 1;
  Stop_Race_Toggle = 0;
  P1P2Num.clear();
  P3P4Num.clear();
  P1Time.clear();
  P2Time.clear();
  P3Time.clear();
  P4Time.clear();
  LapRecNum.clear();
  P1P2Num.writeDisplay();
  P3P4Num.writeDisplay();
  P1Time.writeDisplay();
  P2Time.writeDisplay();
  P3Time.writeDisplay();
  P4Time.writeDisplay();
  LapRecNum.writeDisplay();
  Num_Lane = 1;
  Num_of_Laps = 1;
  Num_of_Racers = 1;
  Num_Track = 1;
  Current_Lap_Num = 0;
  L1LapCount = 0;
  L2LapCount = 0;
  L3LapCount = 0;
  L4LapCount = 0;
}

void Race_Metrics() {
  LEDS.setBrightness(NP_Brightness);  //Initialize Settings for race
  if (Enter_Menu == 1) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Race In Progress");
    Enter_Menu = 0;
  }
  if (Stop_Button_Press == 1) { //Monitor the Stop Button to cancel Race
    Stop_Race_Toggle = 1;
    Race_Metrics_Toggle = 0;
    Stop_Race();
  }
  if (Start_Button_Press == 1 && Race_Metrics_Toggle == 1) { //Monitor the Start Button to Pause Race
    Enter_Menu = 1;
    Race_Metrics_Toggle = 0;
    Pause_Race_Toggle = 1;
  }
  if (Penality_Lane1 == 1 || Penality_Lane2 == 1 || Penality_Lane3 == 1 || Penality_Lane4 == 1) {  //Watches for penailty flags on any lane, cuts power to the lane(s) for penality duration
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
  Lane1State = digitalRead(LANE1LAP_TR_PIN);  //Reads the lap counter pins for car crossings
  Lane2State = digitalRead(LANE2LAP_TR_PIN);
  Lane3State = digitalRead(LANE3LAP_TR_PIN);
  Lane4State = digitalRead(LANE4LAP_TR_PIN);
  if (Lane1State == HIGH && Current_Time > (L1LastLap_Time + 1000)) {  //When a car crosses the pin, record the current lap time and normalize for the display, mark time for Last Lap and increment the Lap counter.
    L1CurrentLap = (Current_Time - L1LastLap_Time) / 10;
    L1LastLap_Time = Current_Time;
    L1LapCount = L1LapCount + 1;
    Sort();
  }
  if (Lane2State == HIGH && Current_Time > (L2LastLap_Time + 1000)) {
    L2CurrentLap = (Current_Time - L2LastLap_Time) / 10;
    L2LastLap_Time = Current_Time;
    L2LapCount = L2LapCount + 1;
    Sort();
  }
  if (Lane3State == HIGH && Current_Time > (L3LastLap_Time + 1000)) {
    L3CurrentLap = (Current_Time - L3LastLap_Time) / 10;
    L3LastLap_Time = Current_Time;
    L3LapCount = L3LapCount + 1;
    Sort();
  }
  if (Lane4State == HIGH && Current_Time > (L4LastLap_Time + 1000)) {
    L4CurrentLap = (Current_Time - L4LastLap_Time) / 10;
    L4LastLap_Time = Current_Time;
    L4LapCount = L4LapCount + 1;
    Sort();
  }
}

void Lap_Counter() {
  if (Current_Lap_Num < max(max(max(L1LapCount, L2LapCount), L3LapCount), L4LapCount)) {  //If the current lap counter is less than the highest lap, clear the display
    LapRecNum.clear();
    LapRecNum.writeDisplay();
    Current_Lap_Num = max(max(max(L1LapCount, L2LapCount), L3LapCount), L4LapCount); //update and the Lap counter
    char LapBuffer[2];
    dtostrf(Current_Lap_Num, 2, 0, LapBuffer);  //Convert the Lap number indivudal char in an array and update lap count 7 segment displays.
    if (Current_Lap_Num < 10) {
      LapRecNum.writeDigitAscii(3, LapBuffer[0]);
    }
    else {
      LapRecNum.writeDigitAscii(2, LapBuffer[0]);
      LapRecNum.writeDigitAscii(3, LapBuffer[1]);
    }
    LapRecNum.writeDisplay();
    if (Num_of_Laps == Current_Lap_Num) {
      Enter_Menu = 1;
      End_Race();
    }
  }
}

void Sort() {
  Lap_Counter();
  unsigned long LaneCurrentLap[4] = {L1CurrentLap, L2CurrentLap, L3CurrentLap, L4CurrentLap};
  unsigned long RacePosition[4] = { L1LastLap_Time, L2LastLap_Time, L3LastLap_Time, L4LastLap_Time}; //immediate past crossing clock
  int LapCounter[4] = {L1LapCount, L2LapCount, L3LapCount, L4LapCount}; //count of laps

  // set the lane order array to all -1  (meaning, unsorted)
  for (int j = 0; j < 4; j++) {
    lane_order[j] = -1;
  }


  // each lane will get marked with a place, starting with 1st place
  int place = 1;

  // start at the current lap number going down to zero.
  // this may get slow at high lap numbers but should be fine as long as it doesnt get into the thousands of laps
  Serial.print("Current Lap: "); Serial.println(Current_Lap_Num);
  Serial.print("Lane Lap Times (1-4): "); Serial.print(L1CurrentLap); Serial.print(" "); Serial.print(L2CurrentLap); Serial.print(" "); Serial.print(L3CurrentLap); Serial.print(" ");  Serial.println(L4CurrentLap);
  Serial.print("Lane Lap Numbers (1-4): "); Serial.print(L1LapCount); Serial.print(" "); Serial.print(L2LapCount); Serial.print(" "); Serial.print(L3LapCount); Serial.print(" "); Serial.println(L4LapCount);
  for (int i = Current_Lap_Num; i >= 0; i--) {

    // look at each lane, if they are on the current lap...
    // set the corresponding lane_order to 0 (meaning, to do)
    for (int j = 0; j < 4; j++) {
      if ( LapCounter[j] == i) {
        lane_order[j] = 0;
      }
    }


    // we want this to be the biggest time possible.
    unsigned long fastest = 0xFFFFFFFF;

    // of the lanes marked with a zero, what is the fastest time?
    for (int j = 0; j < 4; j++) {
      if ( lane_order[i] == 0) {
        fastest = min(fastest, LaneCurrentLap[j]);
	//fastest = min(fastest, RacePosition[j]);
      }
    }

    // which lane matches the fastest time?
    for (int j = 0; j < 4; j++) {
      if ( lane_order[j] == 0 && LaneCurrentLap[j] == fastest ) {
      //if ( lane_order[j] == 0 && RacePosition[j] == fastest ) {
        lane_order[j] = place;
        place++;
        Serial.print(lane_order[j]);
        break;
      }
      //Serial.println (" ");
    }

    // reset unsorted items
    for (int j = 0; j < 4; j++) {
      if ( lane_order[j] == 0 ) {
        lane_order[j] = -1;
      }
    }

    //place++;
    // only assign 5 places, then quit
    if (place == 5) {
      break;
    }
  }

//  for (int j = 0; j < 4; j++) {
//    Serial.print ( lane_order[j]);
//    Serial.print ( " ");
//  }
  Serial.println ( " ");

//   Display_Leaderboard();

}

// void Display_Leaderboard() {

//  int Laptimes[4] = {L1CurrentLap, L2CurrentLap, L3CurrentLap, L4CurrentLap};   // just the last time long to go aroun
//  int car_num[4] = {L1CarNum , L2CarNum, L3CarNum, L4CarNum};

//  char P1Buffer_Time[4];
//  char P2Buffer_Time[4];
//  char P3Buffer_Time[4];
//  char P4Buffer_Time[4];
//  char P1Buffer_Car[2];
//  char P2Buffer_Car[2];
//  char P3Buffer_Car[2];
//  char P4Buffer_Car[2];

//  for (int j = 0; j < 4; j++) {
//    if (lane_order[j] == 1) {
//      P1Buffer_Car[0] = Car_Numbers[car_num[j]][0];
//      P1Buffer_Car[1] = Car_Numbers[car_num[j]][1];
//      sprintf(P1Buffer_Time, "%4d", Laptimes[j]);

//    }

//    if ( lane_order[j] == 2) {
//      P2Buffer_Car[0] = Car_Numbers[car_num[j]][0];
//      P2Buffer_Car[1] = Car_Numbers[car_num[j]][1];
//      sprintf(P2Buffer_Time, "%4d", Laptimes[j]);
//    }

//    if ( lane_order[j] == 3) {
//      P3Buffer_Car[0] = Car_Numbers[car_num[j]][0];
//      P3Buffer_Car[1] = Car_Numbers[car_num[j]][1];
//      sprintf(P3Buffer_Time, "%4d", Laptimes[j]);

//    }

//    if (lane_order[j] == 4) {
//      P4Buffer_Car[0] = Car_Numbers[car_num[j]][0];
//      P4Buffer_Car[1] = Car_Numbers[car_num[j]][1];
//      sprintf(P4Buffer_Time, "%4d", Laptimes[j]);
//    }

//  }


//  if (Current_Lap_Num >= 2) {
//    P1Time.clear();
//    P2Time.clear();
//    P3Time.clear();
//    P4Time.clear();
//    P1Time.writeDisplay();
//    P2Time.writeDisplay();
//    P3Time.writeDisplay();
//    P4Time.writeDisplay();
//    P1Time.writeDigitAscii(0, P1Buffer_Time[0]);
//    P1Time.writeDigitAscii(1, P1Buffer_Time[1], true);
//    P1Time.writeDigitAscii(2, P1Buffer_Time[2]);
//    P1Time.writeDigitAscii(3, P1Buffer_Time[3]);
//    P2Time.writeDigitAscii(0, P2Buffer_Time[0]);
//    P2Time.writeDigitAscii(1, P2Buffer_Time[1], true);
//    P2Time.writeDigitAscii(2, P2Buffer_Time[2]);
//    P2Time.writeDigitAscii(3, P2Buffer_Time[3]);
//    P3Time.writeDigitAscii(0, P3Buffer_Time[0]);
//    P3Time.writeDigitAscii(1, P3Buffer_Time[1], true);
//    P3Time.writeDigitAscii(2, P3Buffer_Time[2]);
//    P3Time.writeDigitAscii(3, P3Buffer_Time[3]);
//    P4Time.writeDigitAscii(0, P4Buffer_Time[0]);
//    P4Time.writeDigitAscii(1, P4Buffer_Time[1], true);
//    P4Time.writeDigitAscii(2, P4Buffer_Time[2]);
//    P4Time.writeDigitAscii(3, P4Buffer_Time[3]);
//    P1P2Num.writeDigitAscii(2, P1Buffer_Car[0]);
//    P1P2Num.writeDigitAscii(3, P1Buffer_Car[1]);
//    P1P2Num.writeDigitAscii(0, P2Buffer_Car[0]);
//    P1P2Num.writeDigitAscii(1, P2Buffer_Car[1]);
//    P3P4Num.writeDigitAscii(2, P3Buffer_Car[0]);
//    P3P4Num.writeDigitAscii(3, P3Buffer_Car[1]);
//    P3P4Num.writeDigitAscii(0, P4Buffer_Car[0]);
//    P3P4Num.writeDigitAscii(1, P4Buffer_Car[1]);
//    P1Time.writeDisplay();
//    P2Time.writeDisplay();
//    P3Time.writeDisplay();
//    P4Time.writeDisplay();
//    P1P2Num.writeDisplay();
//    P3P4Num.writeDisplay();
//    Serial.print("1: ");
//    Serial.println(Car_Numbers[P1_CarNum]);
//    Serial.print("2: ");
//    Serial.println(Car_Numbers[P2_CarNum]);
//    Serial.print("3: ");
//    Serial.println(Car_Numbers[P3_CarNum]);
//    Serial.print("4: ");
//    Serial.println(Car_Numbers[P4_CarNum]);
//  }
// }

void End_Race() {
  if (Num_of_Laps == Current_Lap_Num && Enter_Menu == 1) {
    Enter_Menu = 0;
    playSdWav1.play("LASTLAP.WAV");
    for (int i = 0; i < 4; i++) {
      leds[Lane1NP[i]] = CRGB(255, 255, 255);
      leds[Lane2NP[i]] = CRGB(255, 255, 255);
      leds[Lane3NP[i]] = CRGB(255, 255, 255);
      leds[Lane4NP[i]] = CRGB(255, 255, 255);
    }
    FastLED.show();
  }
  if (P1CurrentLap >= Num_of_Laps + 1 && P1Finish == 0) {
    playSdWav1.play("FINISH.WAV");
    if (P1_CarNum == L1CarNum) {
      digitalWrite(LANE1RELAYPIN, HIGH);
    }
    if (P1_CarNum == L2CarNum) {
      digitalWrite(LANE2RELAYPIN, HIGH);
    }
    if (P1_CarNum == L3CarNum) {
      digitalWrite(LANE3RELAYPIN, HIGH);
    }
    if (P1_CarNum == L4CarNum) {
      digitalWrite(LANE4RELAYPIN, HIGH);
    }
    P1Finish = 1;
  }
  if (P2CurrentLap >= Num_of_Laps + 1 && P2Finish == 0) {
    if (P2_CarNum == L1CarNum) {
      digitalWrite(LANE1RELAYPIN, HIGH);
    }
    if (P2_CarNum == L2CarNum) {
      digitalWrite(LANE2RELAYPIN, HIGH);
    }
    if (P2_CarNum == L3CarNum) {
      digitalWrite(LANE3RELAYPIN, HIGH);
    }
    if (P2_CarNum == L4CarNum) {
      digitalWrite(LANE4RELAYPIN, HIGH);
    }
    P2Finish = 1;
  }
  if (P3CurrentLap >= Num_of_Laps + 1 && P3Finish == 0) {
    if (P3_CarNum == L1CarNum) {
      digitalWrite(LANE1RELAYPIN, HIGH);
    }
    if (P3_CarNum == L2CarNum) {
      digitalWrite(LANE2RELAYPIN, HIGH);
    }
    if (P3_CarNum == L3CarNum) {
      digitalWrite(LANE3RELAYPIN, HIGH);
    }
    if (P3_CarNum == L4CarNum) {
      digitalWrite(LANE4RELAYPIN, HIGH);
    }
    P3Finish = 1;
  }
  if (P4CurrentLap >= Num_of_Laps + 1 && P4Finish == 0) {
    if (P4_CarNum == L1CarNum) {
      digitalWrite(LANE1RELAYPIN, HIGH);
    }
    if (P4_CarNum == L2CarNum) {
      digitalWrite(LANE2RELAYPIN, HIGH);
    }
    if (P4_CarNum == L3CarNum) {
      digitalWrite(LANE3RELAYPIN, HIGH);
    }
    if (P4_CarNum == L4CarNum) {
      digitalWrite(LANE4RELAYPIN, HIGH);
    }
    P4Finish = 1;
  }
  if (P1Finish == 1 && P2Finish == 1 && P3Finish == 1 && P4Finish == 1) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Race Finished!!");

  }
}
