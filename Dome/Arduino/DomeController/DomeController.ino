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

byte cmd=0;                               // Hold the command we're sending to the shutter
                                          // c - Close shutter
                                          // o - Open shutter
                                          // s - Shutter status
                                          // x - Halt all shutter motion immediately

byte statusBytes[3];                      // Array for shutter status
                                          // [0] - Binary status info (Table of bits in shutter sketch)
                                          // [1] - Specific Error Info
                                          // [2] - Temp (°C, rounded to nearest int)
                                          
// Variables
boolean stringComplete = false;  
char serialBuffer[2] = "";
static int dataBufferIndex = 0;
unsigned long prevMillis=0;
unsigned long currentMillis=0;
int i=0;                          // Every arduino program ever winds up needing a counter for something.
bool displayLCD=0;                // TODO : Add driver code to toggle this.  We presume LCD desired == backlight desired

void setup(){

  // Setup and configure rf radio

  radio.begin();
  radio.enableAckPayload();               // Allow ack payloads
  radio.setRetries(0,15);                 // Min time between retries, max # of retries
  radio.setPayloadSize(3);                // Ack Payload is 3 bytes.
  radio.openWritingPipe(0xABCDABCD71LL);  // No need for a reading pipe for Ack payloads
  radio.setDataRate(RF24_1MBPS);          // Works best, for some reason
  radio.setPALevel(RF24_PA_MIN);          // Problems with LOW?
  radio.setChannel(77);                   // In US, channel should be betwene 70-80

  Serial.begin(115200);

  if (displayLCD)                         //TODO Turn this into a function called whenever displayLCD is set on
  {
  lcd.begin (20,4);
  lcd.setBacklightPin(BACKLIGHT_PIN,POSITIVE);
  lcd.setBacklight(HIGH);
  analogWrite(BACKLIGHT_BRIGHTNESS,128);
  lcd.clear();
  lcd.print("TriStar Observatory");
  lcd.setCursor (0,1); 
  lcd.print("Dome Control");
  lcd.setCursor (0,2); 
  
    if (!radio.write( &cmd, 1 ))
    {
      lcd.print("No connection to shutter.");
    }
    else
    {
      lcd.print("Shutter connected.");
      lcd.setCursor(0,3);

      // Yes, we're intentionally faking a loading screen.
      for (i=0; i++, i<=5;)                //See?  Told you.
      {
        lcd.print(".");
        delay(750);
      }
      
    }
    lcd.clear();
  }
} // end setup()

void loop(void) 
{
    currentMillis=millis();
    if (currentMillis - prevMillis > 5000 && displayLCD)
    {
      prevMillis=currentMillis;
    }
    serial_receive();

} // End loop()

void serialEvent()
// TODO : Handle buffer overflow from > 1 char before #
{
  while (Serial.available() > 0)
  {
    char incomingByte = Serial.read();
    
    if(incomingByte=='#')                         // All TSO drivers term commands
                                                  // with '#'
    {
      serialBuffer[dataBufferIndex] = 0; 
      stringComplete = true;

      // Clear any remaining characters in serial
      delay(100);       // No idea why this is necessary, but it is.
      while (Serial.available() > 0)
      {
        Serial.read();
      }   //end while()
    } 
    else 
    {
      serialBuffer[dataBufferIndex++] = incomingByte;
      serialBuffer[dataBufferIndex] = 0; 
    }          
  }   // end while()
}   //end serialEvent()

void serial_receive(void)
{
  if (stringComplete) 
  { 
    cmd = serialBuffer[0];

    if (displayLCD)
    {
      lcd.clear();
      lcd.print("Sending command : ");
      lcd.print(char(cmd));
      lcd.setCursor(0,1);
    }
    else
    {
      Serial.print(F("Sending command:"));  
      Serial.print(char(cmd));          
      Serial.println();
    }
    if (!radio.write( &cmd, 1 ))
    {
      if (displayLCD)
      {
        lcd.print("Sending of command failed.");
        lcd.setCursor(0,2);
      }
      else
      {
        Serial.println(F("Sending of command failed."));  
      }
      
    }
    else
    {
      if(!radio.available())
      { 
        if (displayLCD)
        {
          lcd.print("Blank Payload.");
          lcd.setCursor(0,3);
        }
        else
        {
          Serial.println(F("Blank Payload Received."));   
        }
        
      }
      else
      {
        while(radio.available() )
        {
          radio.read(statusBytes, 3 );
          if (displayLCD)
          {
            lcd.print(statusBytes[0]);
            lcd.print(" - ");
            lcd.print(statusBytes[1]);
            lcd.print(" - ");
            lcd.print(statusBytes[2]);
          }
          else
          {
          Serial.print(statusBytes[0]);
          Serial.print(F(" - "));
          Serial.print(statusBytes[1]); 
          Serial.print(F(" - "));
          Serial.println(statusBytes[2]); 
          Serial.println();
          }
        }
      }
    }
    stringComplete = false;
    dataBufferIndex = 0;
    serialBuffer[dataBufferIndex]=0;
  } // end is string complete
} // end serial_receive()   
  
  

