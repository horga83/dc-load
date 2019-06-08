/*
 * ================================================================================
 * 2.8" TOUCH SCREEN DC LOAD        
 * 
 * Copyright George Farris (ve7frg) and Lawrence Glaister (ve7it) June 01, 2019
 * This software is provied under the terms of the GNU Public License Version 3.
 * 
 * DC LOAD test software for testing power supplies and batteries.  Tests can
 * have a current value set and voltage and time limts.  Main test displays
 * voltage, current, running time, amp_hours and also limts values.
 * 
 * This software runs on an ESP32, specifically a DOIT ESP32 DEVKIT V1 board
 * and the display is a Geekcreit 2.8" TFT colur touch display.  We also use
 * an ADS1115 16 bit I2C 4 port analog input board as the analog I/O on the ESP32
 * is not great.  All parts can be found on Ebay and places like Banggood.  
 * 
 * The load board can handle 10 to 20 amps depending on how you build.  The 
 * schematic for the board can be found on github in the same repository as this 
 * software.
 * 
 * NOTE: 
 * 
 * Please modify the OWNER define to personalize your project.  As an
 * example mine is set to to my Ham Radio callsign.
 * 
/================================================================================
*/
#include <Wire.h>
#include <Adafruit_ADS1015.h>

#include <TFT_eSPI.h>
TFT_eSPI tft = TFT_eSPI();
#include "TouchScreen.h"

#define OWNER "VE7FRG"

// Default load current in amps if i_limit is 0
#define DEFAULT_CURRENT 1.00

// Define PWM and Analog inputs
#define MOSFET_PWM 19
#define FAN_PWM 23
#define TEMP_ADC 0      //channel 0 on ads1115
#define VOLTAGE_ADC 1   //channel 1 on ads1115
#define CURRENT_ADC 2   //channel 2 on ads1115

// ADS1115 anaolog input scaling factors
#define AMPS_SCALE 210.0
#define TEMP_SCALE 25900.0
#define VOLTS_SCALE 292.0

// Fan run level temperatures and alarm
#define FAN_START_TEMP 28
#define FAN_FULL_TEMP 50
#define FAN_ALARM_TEMP 75

// Temperature values for thermistor calculations
#define THERMISTORNOMINAL  10000.0 // resistance at 25 degrees C    
#define TEMPERATURENOMINAL 25.0    // temp. for nominal resistance (almost always 25 C)
#define BCOEFFICIENT       3892.0  // The beta coefficient of the thermistor (usually 3000-4000)
#define SERIESRESISTOR     10000.0  // the value of the pullup resistor

// Min and max pressure on touch screen
#define MINPRESSURE 40
#define MAXPRESSURE 1000

// Screen colours
#define BLACK   0x0000
#define WHITE   0xFFFF
#define GRAY    0x8410
#define BLUE    0x001F
#define RED     0xF800
#define GREEN   0x07E0
#define CYAN    0x07FF
#define MAGENTA 0xF81F
#define YELLOW  0xFFE0
#define ORANGE  0xFA60
#define LIME    0x07FF
#define AQUA    0x04FF
#define PINK    0xF8FF

// For button inverted or normal
#define NORMAL false
#define INVERTED true

/******************* UI details */
#define BUTTON_X 35
#define BUTTON_Y 80
#define BUTTON_W 60
#define BUTTON_H 30
#define BUTTON_SPACING_X 15
#define BUTTON_SPACING_Y 10
#define BUTTON_TEXTSIZE 2

// text box where numbers go
#define TEXT_X 40
#define TEXT_Y 10
#define TEXT_W 220
#define TEXT_H 50
#define TEXT_TSIZE 3
#define TEXT_TCOLOR MAGENTA

// the data (#) we store in the textfield
#define TEXT_LEN 12
char textfield[TEXT_LEN + 1] = "";
uint8_t textfield_i = 0;

// We have a status line for like, is FONA working
#define STATUS_X 10
#define STATUS_Y 65

/* create 12 buttons, in classic numpad style */
char buttonlabels[14][6] = {"1", "2", "3", "4", "5", "6", "7", "8", "9", ":", "0", ".", "Ok", "Clear"};
uint16_t buttoncolors[14] = {BLUE, BLUE, BLUE,
                             BLUE, BLUE, BLUE,
                             BLUE, BLUE, BLUE,
                             ORANGE, BLUE, ORANGE, GREEN, BLACK
                            };
TFT_eSPI_Button buttons[23];

// Either run TouchScreen_Calibr_native.ino and apply results to the arrays below
// or just use trial and error from drawing on screen
// ESP32 coordinates at default 12 bit resolution have range 0 - 4095
// however the ADC cannot read voltages below 150mv and tops out around 3.15V
// so the actual coordinates will not be at the extremes
// each library and driver may have different coordination and rotation sequence

// coords for rotation = 1
//const int coords[] = {650, 3700, 3880, 840}; // portrait - left, right, top, bottom
// coords for rotation = 3
const int coords[] = {3700, 650, 840, 3880}; // portrait - left, right, top, bottom


const int rotation = 3; //  in rotation order - portrait, landscape, etc

const int XP = 13, XM = 33, YP = 15, YM = 12; // default ESP32 Doit touchscreen pins
TouchScreen ts = TouchScreen(XP, YP, XM, YM, 300, rotation);

// Running states
boolean start_enabled = true;
boolean setup_enabled = false;
boolean run_enabled = false;
boolean stop_enabled = false;
boolean numpad_enabled = false;
boolean running = false;

// Buttons and values when set
boolean numpad_ok = false;
boolean volt_set = false;
boolean amp_set = false;
boolean time_set = false;
boolean setup_ok = false;
boolean run = false;
boolean stopped = false;
boolean cont = false;
boolean set_up = false;

int second = 0, minute = 0, hour = 0; // declare time variables
float v_limit = 00.00;
float i_limit = DEFAULT_CURRENT;
char t_limit[9] = "00:00:00";
long long t_limit_long = 0; // for time comparisons

float volts = 0.0;
float amps = 0.0;
int16_t temp_ain = 0;
int16_t temperature = 0;
float milliamp_hours = 0.0;

// PWM settings for MOSFET
int mosfet_pwm_freq = 10000;
int mosfet_pwm_channel = 0;
int mosfet_pwm_resolution = 8;

// PWM settings for FAN
int fan_pwm_freq = 5000;
int fan_pwm_channel = 1;
int fan_pwm_resolution = 8;

Adafruit_ADS1115 ads(0x48); /* I2C address of analog board */

void setup() {
    Serial.begin(9600);
    Serial.println("Starting...");

    ads.begin();  //Init analog board
    ads.setGain(GAIN_TWOTHIRDS);  // 2/3x gain +/- 6.144V  1 bit = 0.1875mV (default)
    //ads.setGain(GAIN_ONE);        // 1x gain   +/- 4.096V  1 bit = 0.125mV
    
    // Setup MOSFET PWM channel
    ledcSetup(mosfet_pwm_channel, mosfet_pwm_freq, mosfet_pwm_resolution);
    ledcAttachPin(MOSFET_PWM, mosfet_pwm_channel);
    
    // Setup FAN PWM channel
    ledcSetup(fan_pwm_channel, fan_pwm_freq, fan_pwm_resolution);
    ledcAttachPin(FAN_PWM, fan_pwm_channel);

    //tft.reset();
    tft.begin(0x9341);
    tft.setRotation(rotation);
    
    tft.fillScreen(BLACK);
    
    //Draw white frame
    tft.drawRect(0, 0, 319, 239, WHITE);
    
    // numberpad buttons
    for (uint8_t row = 0; row < 4; row++)
    {
        for (uint8_t col = 0; col < 3; col++)
        {
          buttons[col + row * 3].initButton(&tft, BUTTON_X + col * (BUTTON_W + BUTTON_SPACING_X),
                                            BUTTON_Y + row * (BUTTON_H + BUTTON_SPACING_Y), // x, y, w, h, outline, fill, text
                                            BUTTON_W, BUTTON_H, WHITE, buttoncolors[col + row * 3], WHITE,
                                            buttonlabels[col + row * 3], BUTTON_TEXTSIZE);
        }
    }
      
    buttons[12].initButton(&tft, 270, 80, 80, 40, WHITE, GREEN, WHITE, "Ok", 2);
    buttons[13].initButton(&tft, 270, 200, 80, 40, WHITE, BLACK, WHITE, "Clear", 2);
    
    //setup_ok
    buttons[14].initButton(&tft, 290, 25, 50, 25, WHITE, BLUE, WHITE, "Ok", 2);
    //voltage button_set (vbutton)
    buttons[15].initButton(&tft, 290, 65, 50, 25, WHITE, BLUE, WHITE, "Set", 2);
    //current button_set (ibutton)
    buttons[16].initButton(&tft, 290, 105, 50, 25, WHITE, BLUE, WHITE, "Set", 2);
    //time button_set (tbutton)
    buttons[17].initButton(&tft, 290, 145, 50, 25, WHITE, BLUE, WHITE, "Set", 2);
    //run button_set (runbutton)
    buttons[18].initButton(&tft, 35, 20, 55, 25, BLACK, GREEN, BLACK, "Run", 2);
    //stop button_set (stopbutton)
    buttons[19].initButton(&tft, 285, 20, 55, 25, BLACK, RED, BLACK, "Stop", 2);
    //continue button
    buttons[20].initButton(&tft, 35, 150, 55, 25, BLACK, CYAN, BLACK, "Cont", 2);
    //back to setup
    buttons[21].initButton(&tft, 285, 150, 55, 25, BLACK, MAGENTA, BLACK, "Set", 2);
    //ESP Restart
    buttons[22].initButton(&tft, 160, 205, 75, 25, WHITE, RED, WHITE, "Reset", 2);

    
    //Print "Intro screen"
    //tft.setFont(&FreeSans24pt7b);
    tft.setCursor(90, 20);
    tft.setTextColor(GREEN);
    tft.setTextSize(4);
    tft.print(OWNER);
    
//    tft.setFont();
    tft.setCursor(65, 80);
    tft.setTextColor(RED);
    tft.setTextSize(5);
    tft.print("DC LOAD");

    tft.setCursor(30, 188);
    tft.setTextColor(WHITE);
    tft.setTextSize(2);
    tft.print("Touch screen to start");
}

// setup_screen()
// Draw the setup screen for entering limits for volts amps and time
void setup_screen()
{
    tft.fillScreen(BLACK);
    //Draw frame
    tft.drawRect(0, 0, 319, 239, WHITE);

    tft.setCursor(10, 15);
    tft.setTextColor(RED);
    tft.setTextSize(3);
    tft.print("SETUP");
    buttons[14].drawButton();
    
    // Voltage set
    tft.setCursor(10, 60);
    tft.setTextColor(CYAN);
    tft.setTextSize(2);
    tft.print("V Limit");
    tft.setCursor(120, 60);
    tft.setTextColor(WHITE);
    tft.print(v_limit);
    tft.print("V");
    buttons[15].drawButton();
    
    // Current set
    tft.setCursor(10, 100);
    tft.setTextColor(CYAN);
    tft.print("I Limit");
    tft.setCursor(120, 100);
    tft.setTextColor(WHITE);
    tft.print(i_limit);
    tft.print("A");
    buttons[16].drawButton();
    
    // Time set
    tft.setCursor(10, 140);
    tft.setTextColor(CYAN);
    tft.print("Time");
    tft.setCursor(120, 140);
    tft.setTextColor(WHITE);
    tft.print(t_limit);
    buttons[17].drawButton();

    // Reset button
    buttons[22].drawButton();
}

void draw_numpad()
{
    tft.fillScreen(BLACK);
    
    // create 'text field'
    tft.drawRect(0, 10, 220, 40, WHITE);

    // create buttons
    for (uint8_t row = 0; row < 4; row++)
    {
        for (uint8_t col = 0; col < 3; col++)
        {
            buttons[col + row * 3].drawButton();
        }
    }
    //draw ok and clear buttons
    buttons[12].drawButton();
    buttons[13].drawButton();
}

// draw_loadscreen()
// Draw the main operating screen when running or monitoring voltage.
// Run button resets all values to zero while Cont button pauses and
// resumes the test.  When stopped Set can be pressed to enter new values
// or reset back to splash screen.
void draw_loadscreen()
{
    tft.fillScreen(BLACK);
    //Draw frame
    tft.drawRect(0, 0, 319, 239, WHITE);

    buttons[18].drawButton();       //run
    buttons[19].drawButton(true);   //stop
    buttons[20].drawButton(true);   //cont
    buttons[21].drawButton();   //set
    
    tft.setCursor(110, 15);
    tft.setTextColor(RED);
    tft.setTextSize(4);
    tft.print("LOAD");
    
    tft.setCursor(20, 70);
    tft.setTextColor(WHITE);
    tft.setTextSize(3);
    tft.print("00.00V");

    tft.setCursor(170, 70);
    tft.print("00.00A");

    tft.setCursor(30, 110);
    tft.setTextSize(2);
    tft.print("0.00mAh");

    tft.setCursor(190, 110);
    tft.print("00:00:00");
    
    tft.setCursor(10, 180);
    tft.setTextColor(CYAN);
    tft.print("V Limit");
    tft.setCursor(120, 180);
    tft.setTextColor(WHITE);
    tft.setCursor(10, 210);
    tft.print(v_limit);
    
    tft.setCursor(120, 180);
    tft.setTextColor(CYAN);
    tft.print("I Limit");
    tft.setTextColor(WHITE);
    tft.setCursor(120, 210);
    tft.print(i_limit);
        
    tft.setCursor(225, 180);
    tft.setTextColor(CYAN);
    tft.print("Time");
    tft.setTextColor(WHITE);
    tft.setCursor(220, 210);
    tft.print(t_limit);

    // Draw limit table
    tft.drawFastHLine(2, 170, 316, RED);
    tft.drawFastHLine(2, 171, 316, RED);
    tft.drawFastHLine(2, 200, 316, RED);
    tft.drawFastVLine(105, 170, 68, RED);
    tft.drawFastVLine(215, 170, 68, RED);
}

// draw_clock
// We break it up into pieces so the entire time line doesn't get updated for
// each second, this avoids flicker as much as possible.
void draw_clock()
{
  char h[4] = {0};
  char m[4] = {0};
  char s[4] = {0};

  // if this isn't set always it won't write to display
  pinMode(XM, OUTPUT);
  pinMode(YP, OUTPUT);

  //tft.fillRect(190,110, 100, 40, BLACK);
  tft.setTextColor(WHITE);
  tft.setTextSize(2);

  if (minute == 0)
  {
    sprintf(h, "%02d:", hour);
    tft.fillRect(189, 110, 30, 30, BLACK);
    tft.setCursor(189, 110);
    tft.print(h);
  }

  if (second == 0)
  {
    sprintf(m, "%02d:", minute);
    tft.fillRect(225, 110, 30, 30, BLACK);
    tft.setCursor(225, 110);
    tft.print(m);
  }

  sprintf(s, "%02d", second);
  tft.fillRect(262, 110, 30, 30, BLACK);
  tft.setCursor(262, 110);
  tft.print(s);
}

void update_clock()
{
  static unsigned long lastTick = 0;

  // move forward one second every 1000 milliseconds
  if (millis() - lastTick >= 1000) {
    lastTick = millis();
    second++;
  }

  // move forward one minute every 60 seconds
  if (second > 59) {
    minute++;
    second = 0; // reset seconds to zero
  }

  // move forward one hour every 60 minutes
  if (minute > 59) {
    hour++;
    minute = 0; // reset minutes to zero
  }
}

// update_display()
// Update the display on the "Load" screen with running values.
void update_display()
{
    // Volts
    tft.fillRect(20, 70, 120, 30, BLACK);
    tft.setCursor(20, 70);
    tft.setTextColor(WHITE);
    tft.setTextSize(3);
    tft.print(volts);
    tft.print("V");

    // Amps
    tft.fillRect(170, 70, 120, 30, BLACK);
    tft.setCursor(170, 70);
    tft.setTextColor(WHITE);
    tft.setTextSize(3);
    tft.print(amps);
    tft.print("A");

    // Temperature
    tft.fillRect(125, 142, 80, 20, BLACK);
    tft.setCursor(140, 142);
    tft.setTextColor(WHITE);
    tft.setTextSize(2);
    tft.print(temperature);
    tft.print("C");

    // mah
    tft.fillRect(30, 110, 100, 20, BLACK);
    tft.setCursor(30, 110);
    tft.setTextSize(2);
    tft.print((long)milliamp_hours);
    tft.print("mAh");

//    tft.print(watt_hours);
//    tft.print("Wh");

}

// check_temp()
// Calulate the temperature in degrees C from a 10K thermistor.
// This routine was copied from the Internet, I have no citation for it.
void check_temp()
{
    static boolean fan_kicked = false;
    uint8_t pwm = 0;
    int16_t temp = 0;
    float t = 0.0;
        
    // convert the value to resistance
    t = TEMP_SCALE / temp_ain - 1.0;
    t = SERIESRESISTOR / t;
 
    float steinhart;
    steinhart = t / THERMISTORNOMINAL;     // (R/Ro)
    steinhart = log(steinhart);                  // ln(R/Ro)
    steinhart /= BCOEFFICIENT;                   // 1/B * ln(R/Ro)
    steinhart += 1.0 / (TEMPERATURENOMINAL + 273.15); // + (1/To)
    steinhart = 1.0 / steinhart;                 // Invert
    steinhart -= 273.15;                         // convert to C
 
    temperature = (int16_t)steinhart;
    
    // Emergency stop
    if (temperature > FAN_ALARM_TEMP)
    {
        set_current(0);  //eventually do other stuff here
    }
    
    //ramp fan every FAN_RAMP_STEP degrees
    if (temperature > FAN_START_TEMP)
    {
        // Kick fan to 255, my fan won't start until about 80
        if (!fan_kicked)
        {
            ledcWrite(fan_pwm_channel, 255);
            fan_kicked = true;
            delay(1000);
        }
        
        // map temp to PWM, my fan won't run under 40
        if (temperature > FAN_FULL_TEMP)    // map falls apart if value is too high
            pwm = 255;
        else
            pwm = map(temperature, FAN_START_TEMP, FAN_FULL_TEMP, 40, 255); 
        ledcWrite(fan_pwm_channel, pwm);
    } else {
        ledcWrite(fan_pwm_channel, 0);  //fan off
        fan_kicked = false;
    }
}

// read_ads1115()
// read all the anaolog I/O on the ADS1115, 16bit analog I2C board and
// do the conversions to volts and amps, temperature conversion is done in 
// check_temp()
void read_ads1115()
{
    int16_t v;
    int16_t a;
    
    v = 0;
    a = 0;
    
    temp_ain = (int16_t)ads.readADC_SingleEnded(TEMP_ADC);
    //Serial.print(temp_ain); Serial.print(" v- ");
    
    v = (int16_t)ads.readADC_SingleEnded(VOLTAGE_ADC);
    volts = v / VOLTS_SCALE;
    //Serial.print(v); Serial.print(" a- ");

    a = (int16_t)ads.readADC_SingleEnded(CURRENT_ADC);
    amps = a / AMPS_SCALE;
    //Serial.println(a);
}

// check_time()
// If a time limit is set, check to see if it has counted down to zero and if
// so set the current to zero and stop the test.
// Global vars - seconds, t_limit, t_limit_long, buttons[], running
void check_time()
{
    static int last_second = 0;

    if (last_second != second)
    {
        last_second = second; 
        t_limit_long--;
        if (t_limit_long == 0)  //turn off and stop the run 
        {
            set_current(0.0);
            buttons[18].drawButton(NORMAL);    //run
            buttons[19].drawButton(INVERTED);  //stop
            buttons[20].drawButton(NORMAL);  //cont
            buttons[21].drawButton(NORMAL);  //set
            running = false;
        }
    }
}

// parse_time()
// Parse the i_limit text field from the set screen and error check it
// then if greter than zero also turn time into seconds in t_limit_long
// t_limit can be in the following forms, ss, :ss, mm:ss, hh:mm:ss
void parse_time()
{
    uint8_t count = 0;
    uint8_t time[3];
  
    char *p;
    p = strtok(t_limit,":");
    while (p != NULL)
    {
        count++;
        Serial.println(p);
        time[count-1] = atoi(p);
        p = strtok (NULL, ":");
    }

    switch (count) 
    {
        case 1:
            if (time[0] < 0 || time[0] >59)
            {
                time[0] = 0;
            }
            sprintf(t_limit, "00:00:%d", time[0]);
            t_limit_long = (long)time[0];
            break;
        case 2:
            if (time[0] < 0 || time[0] >59)
            {
                time[0] = 0; time[1] = 0;
            }
            if (time[1] < 0 || time[1] >59)
            {
                time[0] = 0; time[1] = 0;
            }
            sprintf(t_limit, "00:%d:%d", time[0],time[1]);
            t_limit_long = (long)(time[0] * 60 + time[1]);
            break;
        case 3:
            if (time[0] < 0 || time[0] >59)
            {
                time[0] = 0; time[1] = 0; time[2] = 0;
            }
            if (time[1] < 0 || time[1] >59)
            {
                time[0] = 0; time[1] = 0; time[2] = 0;
            }
            if (time[2] < 0 || time[2] >99)
            {
                time[0] = 0; time[1] = 0; time[2] = 0;
            }
            sprintf(t_limit, "%d:%d:%d", time[0],time[1],time[2]);
            t_limit_long = (long)(time[0] * 3600 + time[1] * 60 + time[0]);
            break;
    }
}

// check_voltage()
// If a voltage limit is set, check the measured voltage against the set voltage 
// and end the test if measured voltage is below set voltage.
void check_voltage()
{
    if (volts < v_limit)  //turn off and stop the run 
    {
        set_current(0.0);
        buttons[18].drawButton(NORMAL);    //run
        buttons[19].drawButton(INVERTED);  //stop
        buttons[20].drawButton(NORMAL);  //cont
        buttons[21].drawButton(NORMAL);  //set
        running = false;
    }    
}

// calculate_mamp_hours()
// Calculate milliamp hours
void calculate_mamp_hours()
{
    static int last_second = 0;

    if (last_second != second)
    {
        last_second = second;
        milliamp_hours = milliamp_hours + (amps * 0.278);
    }
}

// Set the load current
void set_current(float current)
{
    uint16_t pwm = 0;

    pwm = (uint16_t)lround((412.018715 * current) + 3.594873);
    if (current < 0.02)
        pwm = 0;
    ledcWrite(mosfet_pwm_channel, pwm);
}

// check_buttons()
// Buttons are setup as an array and checked for state changes. Then
// we set vars for a state machine in loop()
void check_buttons(TSPoint p)
{
    pinMode(XM, OUTPUT);
    pinMode(YP, OUTPUT);

    
    // go thru all the buttons, checking if they were pressed
    for (uint8_t b = 0; b < 23; b++)
    {
        if (buttons[b].contains(p.x, p.y))
        {
            //Serial.print("Pressing: "); Serial.println(b);
            buttons[b].press(true);  // tell the button it is pressed
        } else {
            buttons[b].press(false);  // tell the button it is NOT pressed
        }
    }
    
    // now check for state changes
    for (uint8_t b = 0; b < 23; b++)
    {
        if (buttons[b].justReleased())
        {
            // Serial.print("Released: "); Serial.println(b);
            //buttons[b].drawButton();  // draw normal
        }
    
        if (buttons[b].justPressed())
        {
    
            if (numpad_enabled)
            {
                // if a numberpad button, append the relevant # to the textfield
                if (b >= 0 && b <= 11)
                {
                    if (textfield_i < TEXT_LEN)
                    {
                        textfield[textfield_i] = buttonlabels[b][0];
                        textfield_i++;
                        textfield[textfield_i] = 0; // zero terminate
                    }
                }
        
                // clr button! delete char
                if (b == 13)
                {
                    //Serial.print("Clear button pressed\n");
                    textfield[textfield_i] = 0;
                    if (textfield > 0)
                    {
                        textfield_i--;
                        textfield[textfield_i] = ' ';
                    }
                }
        
                // update the current text field
                pinMode(XM, OUTPUT);
                pinMode(YP, OUTPUT);
                tft.setCursor(TEXT_X, TEXT_Y + 10);
                tft.setTextColor(TEXT_TCOLOR, BLACK);
                tft.setTextSize(TEXT_TSIZE);
                tft.print(textfield);
        
                // we dont really check that the text field makes sense
                // just try to call
                if (b == 12) 
                {
                    //Serial.print("OK button pressed\n");
                    numpad_ok = true;
                }
            }
            
            if (setup_enabled)
            {
                if (b >= 14 and b <= 17) 
                {
                    //Serial.print("Set button pressed\n"); Serial.println(b);
                    if (b==14){ setup_ok = true; }                    
                    if (b==15){ volt_set = true; }
                    if (b==16){ amp_set = true; }
                    if (b==17){ time_set = true; }
                }
                
                // Reset button
                if (b == 22)
                {
                    ESP.restart();
                }
            }
            
            if (run_enabled)
            {
                if (b >= 18 and b <= 21) 
                {
                    if (b==18){ run = true; }
                    if (b==19){ stopped = true; }
                    if (b==20){ cont = true; }
                    if (b==21){ set_up = true; }
                }
            }
            
            delay(150); // UI debouncing
        }
    }
}

// Main loop
void loop()
{
    static int last_second = 0;
    
    update_clock();
    
    TSPoint touch_point = ts.getPoint();  //Get touch point
    
    if (touch_point.z > ts.pressureThreshhold)
    {
        touch_point.x = map(touch_point.x, coords[2], coords[3], 0, 320); 
        touch_point.y = map(touch_point.y, coords[1], coords[0], 0, 240);
    
        // Start screen
        // The user has pressed start screen
        if (touch_point.x > 0 && touch_point.x < 320 && touch_point.y > 0 && touch_point.y < 240 && start_enabled)
        {
            // Program states
            start_enabled = false;
            setup_enabled = true;
            run_enabled = false;
            stop_enabled = false;
          
            //This is important, because the libraries are sharing pins
            pinMode(XM, OUTPUT);
            pinMode(YP, OUTPUT);
        
            //Erase the screen
            tft.fillScreen(BLACK);
            setup_screen();
        }
    }

    if (!start_enabled)
    {
        check_buttons(touch_point);

        if (setup_enabled)
        {
            //Serial.println(textfield);
            if (volt_set) 
            { 
                strcpy(textfield, "");
                textfield_i = 0;
                numpad_enabled = true;
                setup_enabled = false;
                draw_numpad();
            }
               
            if (amp_set) 
            { 
                strcpy(textfield, "");
                textfield_i = 0;
                numpad_enabled = true;
                setup_enabled = false;
                draw_numpad();
            }

            if (time_set) 
            { 
                strcpy(textfield, "");
                textfield_i = 0;
                numpad_enabled = true;
                setup_enabled = false;
                draw_numpad();
            }

            if (setup_ok)
            {
                setup_enabled = false;
                run_enabled = true;
                draw_loadscreen();
                setup_ok = false;
            }
        }

        if (numpad_enabled)
        {
            check_buttons(touch_point);
        
            if (numpad_ok)  // ok button pressed  
            {
                //Serial.print(textfield); Serial.print(" -np\n");
                if (volt_set) 
                {
                    v_limit = atof(textfield);
                    volt_set = false;
                }
                if (amp_set) 
                {
                    i_limit = atof(textfield);
                    amp_set = false;
                    //Serial.print("i_limit-> ");Serial.println(i_limit);
                }
                if (time_set) 
                {
                    strcpy(t_limit, textfield);
                    parse_time();  //error check it
                                        
                    tft.setCursor(120, 140);
                    tft.setTextColor(WHITE);
                    tft.print(t_limit);
                    
                    time_set = false;
                }

                numpad_enabled = false;
                numpad_ok = false;
                setup_enabled = true;
                setup_screen();
            }
        }

        if (run_enabled)
        {
            // give the ads1115 time to setup for the next read
            delay(10);
            // always read analog I/O
            read_ads1115();
            
            if (run) 
            { 
                running = true; 
                second = 0, minute = 0, hour = 0;
                milliamp_hours = 0;
                
                if (i_limit == 0.00)
                {
                    i_limit = DEFAULT_CURRENT;
                }
                //Serial.println(i_limit);
                set_current(i_limit);

                buttons[18].drawButton(INVERTED);  //run
                buttons[19].drawButton(NORMAL);    //stop
                buttons[20].drawButton(INVERTED);  //cont
                buttons[21].drawButton(INVERTED);  //set
                run = false;
            }
            if (stopped)
            { 
                running = false; 
                set_current(0);
                buttons[18].drawButton(NORMAL);    //run
                buttons[19].drawButton(INVERTED);  //stop
                buttons[20].drawButton(NORMAL);  //cont
                buttons[21].drawButton(NORMAL);  //set
                stopped = false;
            }
            if (cont)
            { 
                buttons[18].drawButton(INVERTED);    //run
                buttons[19].drawButton(NORMAL);  //stop
                buttons[20].drawButton(INVERTED);  //cont
                buttons[21].drawButton(INVERTED);  //set

                running = true;
                set_current(i_limit);
                cont = false;
            }
            if (set_up)
            { 
                set_current(0);
                setup_enabled = true;
                run_enabled = false;
                setup_screen();
                set_up = false;
            }

            // keep running temp and fan calc when in all states of load screen.
            if (!running)
            {
                if (last_second != second) // only update display once a second
                {
                    last_second = second;
                    check_temp();
                    update_display();
                }
            }
        }
  }

  // Running loop, we end up here when the test is actually running
  if (running)
  {
        float v = 0.0;
        float a = 0.0;

        if (last_second != second)
        {
            last_second = second;
            draw_clock();
            check_temp();
            check_voltage();
            // only do time check if time limit is set
            if (t_limit_long > 0)
            {
                check_time();
            }
            calculate_mamp_hours();
            update_display();
        }
  }

}
