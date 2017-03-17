/*************************************************************************
   TriStar Observatory Shutter Controller
   2017MAR14
   v0.2.6

   This shutter slave will handle all shutter control functions, and report
   shutter information to the dome master to relay to ASCOM.

   Parts derived from :
      * maniacbug's RF24 library pingpair_ack example
      * Pololu's SMC Arduino examples

 *************************************************************************/

// Included libraries
#include <SPI.h>
#include <SoftwareSerial.h>
#include "RF24.h"
#include <HMC5983.h>
#include <Wire.h>

//***** Variables, constants, and defines *****//

// Pololu SMC config
const int rxPin = 3;          // pin 3 connects to SMC TX
const int txPin = 4;          // pin 4 connects to SMC RX
const int resetPin = 5;       // pin 5 connects to SMC nRST
const int errPin = 6;         // pin 6 connects to SMC ERR

// SMC Variable IDs
#define ERROR_STATUS 0
#define LIMIT_STATUS 3
#define SPEED 21
#define INPUT_VOLTAGE 23
#define TEMPERATURE 24

// SMC motor limit IDs
#define FORWARD_ACCELERATION 5
#define FORWARD_DECELERATION 6
#define REVERSE_ACCELERATION 9
#define REVERSE_DECELERATION 10
#define DECELERATION 2

// Variables for SMC values
uint16_t limitStatus = 0;
int motorSpeed = 0;
int errorStatus = 0;
float voltage=0.0;
int temp=0;
int shutterStatus=0;

// Variables for RF24 data
byte statusPayload[6];
byte cmd[5];

// Timer variables for stuffStatus() call
unsigned long lastMillis = 0;
unsigned long currentMillis = 0;

// Variables for compass data
float compassReading = -999;
int numReadings=20;
float azimuth = 0.0;

//***** Instances of various hardware & comms *****//

// nRF24L01 radio on SPI bus plus pins 7 & 8
RF24 radio(7, 8);

// SoftwareSerial for communication w/ SMC
SoftwareSerial smcSerial = SoftwareSerial(rxPin, txPin);

// Compass
HMC5983 compass;

//***** Main Setup() and Loop() functions *****//

void setup() {

  //Serial lines
  
  Serial.begin(9600);       // For serial monitor troubleshooting
  smcSerial.begin(19200);   // Begin serial to SMC

  // Start up compass
  compass.begin();       // Include true for troubleshooting/debugging info
  
// Debug
//  Serial.println("TriStar Observatory");
//  Serial.println("Shutter Control");


  // Reset SMC when Arduino starts up
  pinMode(resetPin, OUTPUT);
  digitalWrite(resetPin, LOW);  // reset SMC
  delay(1);  // wait 1 ms
  pinMode(resetPin, INPUT);  // let SMC run again

  // must wait at least 1 ms after reset before transmitting
  delay(10);

  // Set up motor controller.
  smcSerial.write(0xAA);  // send baud-indicator byte
  setMotorLimit(FORWARD_ACCELERATION, 100);
  setMotorLimit(REVERSE_ACCELERATION, 100);
  setMotorLimit(FORWARD_DECELERATION, 100);
  setMotorLimit(REVERSE_DECELERATION, 100);
  setMotorLimit(DECELERATION, 1);

  // clear the safe-start violation and let the motor run
  exitSafeStart();

  // Setup and configure rf radio

  radio.begin();
  radio.setAutoAck(true);
  radio.enableAckPayload();               // Allow optional ack payloads
  radio.setRetries(2, 15);                // Min time between retries, max # of retries
  radio.openReadingPipe(1, 0xABCDABCD71LL); // No need for a writing pipe for Ack payloads
  radio.startListening();                 // Start listening
  radio.setDataRate(RF24_1MBPS);          // Works best, for some reason
  radio.setPALevel(RF24_PA_MIN);          // Issues with LOW?
  radio.setChannel(77);                   // In US, channel should be betwene 70-80

  // Do our first stuffStatus
  stuffStatus();
  lastMillis = millis();              // We'll compare this in loop(), and poll status every 1s
  doCommand("snfo");                  // This will update the LCD for our initial readout
  
}   //end setup

void loop(void)
{

  // SMC Error Status == 1 if only error is Safe Start Violation.  Since
  // all other errors are 0, it is safe to exit Safe Start
  if (getSMCVariable(ERROR_STATUS) == 1)
  {
    exitSafeStart();
  }   // end if

  // Build status payload
  stuffStatus();
    
//  // Print debug info every 3s
//  currentMillis = millis();
//  if (currentMillis - lastMillis > 3000)
//  {
//    printDebug();
//    lastMillis = currentMillis;
//  }   // end if

  // Check for command and execute it
  if ( radio.available())
  {
    radio.read( &cmd, 4 );
    String theCommand = (char*)cmd;
//    theCommand.remove(4);
    doCommand(theCommand);
//    printDebug();
  }   //end while
}   //end loop

//***** Program Functions *****//

// readSMCByte : Read an SMC serial byte
int readSMCByte()
{
  char c;
  if (smcSerial.readBytes(&c, 1) == 0) {
    return -1;
  }
  return (byte)c;
} //end readSMCByte()

// exitSafeStart : Exits controller Safe Start mode
// required to allow motor to move
// must be called when controller restarts and after any error
void exitSafeStart()
{
  smcSerial.write(0x83);
} //end exitSafeStart()

// setMotorspeed : Move the motor
void setMotorSpeed(int speed)
{
  if (speed < 0)
  {
    smcSerial.write(0x86);  // motor reverse command
    speed = -speed;  // make speed positive
  }
  else
  {
    smcSerial.write(0x85);  // motor forward command
  }
  smcSerial.write(speed & 0x1F);
  smcSerial.write(speed >> 5);
} //end setMotorspeed()

// setMotorLimit : Function to set motor limits
unsigned char setMotorLimit(unsigned char  limitID, unsigned int limitValue)
{
  smcSerial.write(0xA2);
  smcSerial.write(limitID);
  smcSerial.write(limitValue & 0x7F);
  smcSerial.write(limitValue >> 7);
  return readSMCByte();
} //end setMotorLimit()

// getSMCVariable : returns the specified variable as an unsigned integer.
int getSMCVariable(unsigned char variableID)
{
  smcSerial.write(0xA1);
  smcSerial.write(variableID);
  return readSMCByte() + 256 * readSMCByte();
} // end getSMCVariable()

// Stuff status bytes into RF24 Ack Payload byte array named statusPayload
/**********************************************************************
  0 : Binary limit status indicators according to the table below
  1 : Error byte.  Pulled from SMC Error Status, with some bits dropped
    0   Safe Start Violation
    1   Serial Error (Requires Exit from Safe Start AND valid speed command)
    2   Limit switch active, and attempt has been made to move into it
    3   Low VIN (VIN value in next byte)
    4   High VIN (VIN value in next byte)
    5   Over Temperature  (TODO Set threshold in GUI)
    6   Motor Driver Error
    7   ERR Line High (With no other error)
  2 : Controller temperature
  3 : Azimuth offset from 180°
  4 : Indicator Azimuth offset, 1 or 0.  1 is 180 + offset, 0 is 180 - offset
  5 : Shutter status, mapped to ASCOM ShutterState http://www.ascom-standards.org/Help/Developer/html/T_ASCOM_DeviceInterface_ShutterState.htm
**********************************************************************/

void stuffStatus()
{
  statusPayload[0] = 0;
  statusPayload[1] = 0;
  statusPayload[2] = 0;
  statusPayload[3] = 0;
  statusPayload[4] = 0;
  statusPayload[5] = 0;

  limitStatus = getSMCVariable(LIMIT_STATUS);
  motorSpeed = getSMCVariable(SPEED);
  errorStatus = getSMCVariable(ERROR_STATUS);
  voltage = float(float(getSMCVariable(INPUT_VOLTAGE)) / 1000), 1;

  // Stuff status byte 0 (Limit info)
  bitWrite(statusPayload[0], 0, bitRead(limitStatus, 8));                 // LIMIT_STATUS Bit 8 is AN2 (Open side) limit switch active
  bitWrite(statusPayload[0], 1, bitRead(limitStatus, 7));                 // LIMIT_STATUS Bit 7 is AN1 (Closed side) limit switch active
  bitWrite(statusPayload[0], 2, motorSpeed < 0);                          // Negative motor speed is actively moving, opening the shutter
  bitWrite(statusPayload[0], 3, motorSpeed > 0);                          // Positive motor speed is actively moving, Closing the shutter
  bitWrite(statusPayload[0], 4, errorStatus > 0);                         // If ERROR_STATUS > 0, some error has occurred.
  bitWrite(statusPayload[0], 5, (limitStatus == 0 && motorSpeed == 0));   // If no limit switch is active, and speed is 0, shutter is stopped between limits
  //bitWrite(statusPayload[0], 6, (limitStatus > 0 && motorSpeed != 0));    // If either limit switch is active, and motor is moving, we have an issue

  // Stuff status byte 1 (Error info)
  bitWrite(statusPayload[1], 0, bitRead(errorStatus, 0));                  // Reference : https://www.pololu.com/docs/0J44/6.4
  bitWrite(statusPayload[1], 1, bitRead(errorStatus, 2));
  bitWrite(statusPayload[1], 2, bitRead(errorStatus, 4));
  bitWrite(statusPayload[1], 3, bitRead(errorStatus, 5));
  bitWrite(statusPayload[1], 4, bitRead(errorStatus, 6));
  bitWrite(statusPayload[1], 5, bitRead(errorStatus, 7));
  bitWrite(statusPayload[1], 6, bitRead(errorStatus, 8));
  bitWrite(statusPayload[1], 7, bitRead(errorStatus, 9));

  // Stuff status byte 2 (Controller temp)
  statusPayload[2] = round(float(getSMCVariable(TEMPERATURE)) / 10);
  temp = statusPayload[2];
  
  // Stuff status bytes 3 and 4 (Azimuth)
   // Average numReadings current valid compass readings for the current azimuth
    azimuth = 0.0;
    for (int i = 1; i <= numReadings; i++)
    {
      compassReading = -999;             // Reset to default condition
      compassReading = compass.read();   // This should put a value into c if the compass is alive
      if (compassReading != -999)
      {
        // Valid reading, include in average
        azimuth = azimuth + (compassReading - azimuth);  
      }
      else
      {
        // No reading from compass, discard this value and try again
        --i;
      }
    }
    statusPayload[3] = abs(180.0 - round(azimuth));
    if (azimuth < 180.0)
    {
      statusPayload[4] = 0  ;
    }
    else
    {
      statusPayload[4] = 1;
    }

  // Stuff status byte 5 (Shutter Status)
  shutterStatus = 4;                 // Start with an error condition, force some valid condition to overwrite it
  if (limitStatus == 0)             // Neither limit switch is closed.  Note if motorSpeed = 0, this is an error.
  {
    if (motorSpeed > 0)
    {
      shutterStatus = 3;            // Closing
    }
    else if (motorSpeed < 0)
    {
      shutterStatus = 2;            // Opening
    }
  }
  else                              // Something is limiting the motor.  We'll check for closed limit switches.  Anything else is an error.
  {
    if (statusPayload[0] == 1)
    {
      shutterStatus = 0;            // Open
    }
    else if (statusPayload[0] == 2)
    {
      shutterStatus = 1;            // Closed
    }
  }
  statusPayload[5] = shutterStatus;
  radio.flush_tx();
  radio.writeAckPayload(1, statusPayload, sizeof(statusPayload) );
} // end stuffStatus()

// print status and other info to serial monitor for debugging
void printDebug()
{
  Serial.print("Limit byte : ");
  Serial.println(statusPayload[0]);
  Serial.print("Error byte : ");
  Serial.println(statusPayload[1]);
  Serial.print("Temp byte : ");
  Serial.println(statusPayload[2]);
  Serial.print("Azimuth : ");
  Serial.println(azimuth);
  Serial.print("Azimuth Offset byte : ");
  Serial.println(statusPayload[3]);
  Serial.print("Offset Multiplier byte : ");
  Serial.println(statusPayload[4]);
  Serial.print("VIn : ");
  Serial.print(voltage);
  Serial.println("V");
  Serial.print("Shutter : ");
  Serial.print(statusPayload[5]);
  switch (statusPayload[5])
  {
    case 0:
      Serial.println(" - Open");
      break;
    case 1:
      Serial.println(" - Closed");
      break;
    case 2:
      Serial.println(" - Opening");
      break;
    case 3:
      Serial.println(" - Closing");
      break;
    case 4:
      Serial.println(" - Error");
      break;
      
  }
  Serial.println("-------------------------");
} // end printDebug()

//  doCommand function for handling shutter commands
void doCommand(String command)
{
  
  if (command == "clos")            // Close shutter
  {
    setMotorSpeed(800);
//    printDebug();
  }
  else if (command == "open")       // Open shutter
  {
    setMotorSpeed(-800);
//    printDebug();
  }
  else if (command == "xxxx")       // halt shutter immediately
  {
    setMotorSpeed(0);
//    printDebug();
  }
  else if (command == "rset")            // Reset shutter
  {
    exitSafeStart();
//    printDebug();
  }
  else if (command == "snfo")       // Get shutter info
  {
//    printDebug();
  }
} //end doCommand()

