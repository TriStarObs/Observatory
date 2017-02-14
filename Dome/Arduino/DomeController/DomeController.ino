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
#include "RF24.h"

// Hardware configuration: nRF24L01 radio on SPI bus plus pins 7 & 8 
RF24 radio(7,8);

byte cmd=0;                               // Hold the command we're sending to the shutter
                                          // c - Close shutter
                                          // o - Open shutter
                                          // s - Shutter status
                                          // x - Halt all shutter motion immediately

byte statusBytes[3];                      // Array for shutter status
                                          // [0] - Binary status info (TODO Table of bits)
                                          // [1] - Controller temp
                                          // [2] - Controller error info (TODO Build byte)
                                          
// Serial handling variables
boolean stringComplete = false;  // whether the string is complete
char serialBuffer[2] = "";
static int dataBufferIndex = 0;

void setup(){

  Serial.begin(115200);
  Serial.println(F("TriStar Observatory : Dome Master Controller"));

  // Setup and configure rf radio

  radio.begin();
  radio.enableAckPayload();               // Allow ack payloads
  radio.setRetries(0,15);                 // Min time between retries, max # of retries
  radio.setPayloadSize(3);                // Ack Payload is 3 bytes.
  radio.openWritingPipe(0xABCDABCD71LL);  // No need for a reading pipe for Ack payloads
  radio.setDataRate(RF24_1MBPS);          // Works best, for some reason
  radio.setPALevel(RF24_PA_MIN);          // Problems with LOW?
  radio.setChannel(77);                   // In US, channel should be betwene 70-80
//  radio.printDetails();                 // Dump the configuration of the rf unit 
                                          // for debugging

}

void loop(void) 
{
    serial_receive();
} // End loop

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
    Serial.print(F("Sending:"));  
    Serial.print(cmd);          
    Serial.println();
    if (!radio.write( &cmd, 1 ))
    {
      Serial.println(F("Send failed."));
    }
    else
    {
      if(!radio.available())
      { 
        Serial.println(F("Blank Payload Received.")); 
      }
      else
      {
        while(radio.available() )
        {
          radio.read( statusBytes, 3 );
          Serial.print(F("Got response StatusByte - "));
          Serial.print(statusBytes[0]);
          Serial.print(F(", TempByte - "));
          Serial.print(statusBytes[1]); 
          Serial.print(F(", ErrorByte - "));
          Serial.println(statusBytes[2]); 
          Serial.println();
        }
      }
    }
    stringComplete = false;
    dataBufferIndex = 0;
    serialBuffer[dataBufferIndex]=0;
  } // end is string complete
} // end serial_receive()   
  
  

