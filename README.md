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
