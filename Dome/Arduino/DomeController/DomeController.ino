/*************************************************************************
 * TriStar Observatory Dome Master
 * 2017FEB13
 * v0.1
 * 
 * The dome master will handle all dome rotation functions, and report dome and
 * shutter information to ASCOM.
 * 
 * It will also relay any shutter commands and shutter status info to/from the
 * shutter slave controller via RF24 link
 * 
 * Parts derived from maniacbug's RF24 library pingpair_ack example
 * and Stanley Seow's nRF Serial Chat
 *************************************************************************/

#include <SPI.h>
#include <LCD.h>
#include <LiquidCrystal_I2C.h>
#include "RF24.h"
#include <Wire.h>

// LCD Pin assignments
#define I2C_ADDR    0x27 
#define BACKLIGHT_PIN     3
#define BACKLIGHT_BRIGHTNESS    9
#define En_pin  2
#define Rw_pin  1
#define Rs_pin  0
#define D4_pin  4
#define D5_pin  5
#define D6_pin  6
#define D7_pin  7

// Create instance of LCD
LiquidCrystal_I2C  lcd(I2C_ADDR,En_pin,Rw_pin,Rs_pin,D4_pin,D5_pin,D6_pin,D7_pin);

// Hardware configuration: nRF24L01 radio on SPI bus plus pins 7 & 8 
RF24 radio(7,8);

String strCmd;
byte cmd[5];                              // Hold the command we're sending to the shutter
                                          // c - Close shutter
                                          // o - Open shutter
                                          // s - Shutter status
                                          // x - Halt all shutter motion immediately

byte statusBytes[3];                      // Array for shutter status
                                          // [0] - Binary status info (Table of bits in shutter sketch)
                                          // [1] - Specific Error Info
                                          // [2] - Temp (°C, rounded to nearest int)
                                          
// Variables

// Functions

boolean sendShutter(byte (&cmdArray)[5])
{
  if (!radio.write( &cmdArray, sizeof(cmdArray) ))
  {
    lcd.print("Sending of shutter");
    lcd.setCursor(0,1);
    lcd.print("command failed.");
    return false;
  }
  else
  {
    if(!radio.available())
    { 
      lcd.clear();
      lcd.print("Blank Payload.");
    }
    else
    {
      lcd.clear();
      while(radio.available() )
      {
        radio.read(statusBytes, 3 );
        lcd.print(statusBytes[0]);
        lcd.print(" - ");
        lcd.print(statusBytes[1]);
        lcd.print(" - ");
        lcd.print(statusBytes[2]);
      }
    }
    return true;
  }
}

void setup(){

  // Setup and configure rf radio

  radio.begin();
  radio.enableAckPayload();               // Allow ack payloads
  radio.setRetries(0,15);                 // Min time between retries, max # of retries
  radio.setPayloadSize(4);                // 4 byte commands
  radio.openWritingPipe(0xABCDABCD71LL);  // No need for a reading pipe for Ack payloads
  radio.setDataRate(RF24_1MBPS);          // Works best, for some reason
  radio.setPALevel(RF24_PA_MIN);          // Problems with LOW?
  radio.setChannel(77);                   // In US, channel should be betwene 70-80

  Serial.begin(115200);

  lcd.begin (20,4);
  lcd.setBacklightPin(BACKLIGHT_PIN,POSITIVE);
  lcd.setBacklight(HIGH);
  analogWrite(BACKLIGHT_BRIGHTNESS,128);
  lcd.clear();
  lcd.print("TriStar Observatory");
  lcd.setCursor (0,1); 
  lcd.print("Dome Control");
  lcd.setCursor (0,2); 
  
    if (!radio.write( &cmd, 4 ))
    {
      lcd.print("No connection to shutter.");
    }
    else
    {
      lcd.print("Shutter connected.");
      lcd.setCursor(0,3);
      radio.read(statusBytes, 3 );
      lcd.print(statusBytes[0]);
      lcd.print(" - ");
      lcd.print(statusBytes[1]);
      lcd.print(" - ");
      lcd.print(statusBytes[2]);
      delay(3000);
    }
  lcd.clear();
} // end setup()

void loop(void) 
{
  while (Serial.available() > 0)
  {
    Serial.readBytesUntil('#', cmd, 5);
    strCmd = (char*)cmd;
    if (strCmd == "info")
    {
      Serial.println("status#");
//      for (int i=0; i<=1; i++)
//      {
//        Serial.print(statusBytes[i]);
//        Serial.print(',');  
//      }
//      Serial.print(statusBytes[2]);
//      Serial.print('#');
      lcd.clear();
      lcd.print("Got tick");
    }
    else if (strCmd.startsWith("s"))
    {
      sendShutter(cmd);        //TODO : Write this function.
    }
    else
    {
        //stuff that involves getting info or rotating the dome.
    }
  }
} // End loop()


