# dc-load
2.8" TOUCH SCREEN DC LOAD        

June 01, 2019
This software is provied under the terms of the GNU Public License Version 3.

DC LOAD test software for testing power supplies and batteries.  Tests can
have a current value set and voltage and time limts.  Main test displays
voltage, current, running time, amp_hours and also limts values.

This software runs on an ESP32, specifically a DOIT ESP32 DEVKIT V1 board
and the display is a Geekcreit 2.8" TFT colur touch display.  We also use
an ADS1115 16 bit I2C 4 port analog input board as the analog I/O on the ESP32
is not great.  All parts can be found on Ebay and places like Banggood.  
 
The load board can handle 10 to 20 amps depending on how you build.  The 
schematic for the board can be found on github in the same repository as this 
software.

NOTE: 

Please modify the OWNER define to personalize your project.  As an
example mine is set to to my Ham Radio callsign.
This is all setup for the ESP32 Doit V1 board and Touch display

This project requires the following Ardunio libraries:

TFT_eSPI code from: https://github.com/Bodmer/TFT_eSPI  
Touchscreen code from: https://github.com/s60sc/Adafruit_TouchScreen  
ADS1115 code from: https://github.com/adafruit/Adafruit_ADS1X15  

I have included a copy of the touchscreen.[cpp,h] here with modified values.

You will have to calibrate your screen and setup the coords[] array.  This code
is setup for a 2.8" resistive touch display.  Hardware is:

ESP32 board:    Doit devkit V1, 36 pin board  
Touch Display:  2.8" Geekcreit TFT LCD Touch Display for Ardunio (ILI9341)  
ADS1115:        4 port 16 bit anaolog I2C board.  

Search Banggood for display, Doit ESP32 and ADS1115

CONNECTION
-------------------------------------------------------------------------
Pinouts are as follows:

Display  | ESP32
-------- | -----------
D0       | D12 GPIO12  
D1       | D13 GPIO13  
D2       | D26 GPIO26  
D3       | D25 GPIO25
D4       | TX2 GPIO17
D5       | RX2 GPIO16
D6       | D27 GPIO27
D7       | D14 GPIO14
LCD_CS   | D33 GPIO33
LCD_RS   | D15 GPIO15
LCD_WR   | D4  GPIO04
LCD_RD   | D2  GPIO02
+5V      | VIN
GND      | GND


SETUP
--------------------------------------------------------------------------
Touchscreen.h and Touchscreen.cpp should reside locally in the directory 
of the code you are writing so they don't collide with the official Arduino 
code.

TOUCHSCREEN
-----------
Modifications to touchscreen.h

Line 14    #define aXM 33  // analog input pin connected to LCD_CS 
Line 15    #define aYP 15  // analog input pin connected to LCD_RS

Modifications to touchscreen.cpp

Added layout parameter to the Touchscreen creation.
From this: 
     TouchScreen ts = TouchScreen(XP, YP, XM, YM, 300);
To This:
TouchScreen ts = TouchScreen(XP, YP, XM, YM, 300, rotation);

and it is used in getPoint()


TFT_eSPI
--------
This is a parallel connected display so we need to modify a few files in the
TFT_eSPI Arduino library which you can install with your Library Manager in 
the Arduino IDE.

First make sure the pinouts are defined as above in:
~/Arduino/libraries/TFT_eSPI/User_Setups/Setup14_ILI9341_Parallel.h

Make sure the driver is set in:
~/Arduino/libraries/TFT_eSPI/User_Setup.h
#define ILI9341_DRIVER is set in:

Make sure board is set in:
~/Arduino/libraries/TFT_eSPI/User_Setup_Select.h
#include <User_Setups/Setup14_ILI9341_Parallel.h>

