/*************************************************************************
 * TriStar Observatory Shutter Slave
 * 2017FEB13
 * v0.1
 * 
 * The shutter slave will handle all shutter control functions, and report  
 * shutter information to the dome master to relay to ASCOM.
 * 
 * Parts derived from maniacbug's RF24 library pingpair_ack example
 * 
 * 
 * 
 *************************************************************************/

#include <SPI.h>
#include <SoftwareSerial.h>
#include "RF24.h"

// Hardware configuration: nRF24L01 radio on SPI bus plus pins 7 & 8 
RF24 radio(7,8);


// Pololu SMC config
const int rxPin = 3;          // pin 3 connects to SMC TX
const int txPin = 4;          // pin 4 connects to SMC RX
const int resetPin = 5;       // pin 5 connects to SMC nRST
const int errPin = 6;         // pin 6 connects to SMC ERR

// Setup SoftwareSerial for communication w/ SMC
SoftwareSerial smcSerial = SoftwareSerial(rxPin, txPin);

// SMC Variable IDs
#define ERROR_STATUS 0
#define LIMIT_STATUS 3
#define TARGET_SPEED 20
#define SPEED 21
#define INPUT_VOLTAGE 23
#define TEMPERATURE 24

// SMC motor limit IDs
#define FORWARD_ACCELERATION 5
#define FORWARD_DECELERATION 6
#define REVERSE_ACCELERATION 9
#define REVERSE_DECELERATION 10
#define DECELERATION 2

// Variables for RF24 data
byte statusPayload[3];
byte pipeNum;
String cmd;

// Timer variables for stuffStatus() call
unsigned long lastMillis=0;
unsigned long currentMillis=0;

// read an SMC serial byte
int readSMCByte()
{
  char c;
  if(smcSerial.readBytes(&c, 1) == 0){ return -1; }
  return (byte)c;
}

// required to allow motor to move
// must be called when controller restarts and after any error
void exitSafeStart()
{
  smcSerial.write(0x83);
}

// speed should be a number from -3200 to 3200
// TODO : I'll be using a set speed, so this can probbaly go away to save mem.
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
}

unsigned char setMotorLimit(unsigned char  limitID, unsigned int limitValue)
{
  smcSerial.write(0xA2);
  smcSerial.write(limitID);
  smcSerial.write(limitValue & 0x7F);
  smcSerial.write(limitValue >> 7);
  return readSMCByte();
}

// returns the specified variable as an unsigned integer.
// if the requested variable is signed, the value returned by this function
// should be typecast as an int.
int getSMCVariable(unsigned char variableID)
{
  smcSerial.write(0xA1);
  smcSerial.write(variableID);
  return readSMCByte() + 256 * readSMCByte();
}

// Stuff the first statusPayload byte, which is binary status 
// indicators according to the following table
//
//  1   Shutter is Open
//  2   Shutter is Closed
//  4   Shutter is Opening
//  8   Shutter is Closing
//  16  Shutter Error
//  32  Shutter is stopped (Neither open nor closed)
//  64  Reserved
//  128 Reserved

void stuffStatus()
{
  statusPayload[0]=0;
  uint16_t limitStatus = getSMCVariable(LIMIT_STATUS);
  int motorSpeed = getSMCVariable(SPEED);
  int errorStatus = getSMCVariable(ERROR_STATUS);

  bitWrite(statusPayload[0], 0, bitRead(limitStatus,8));                 // LIMIT_STATUS Bit 8 is AN2 (Open side) limit switch active
  bitWrite(statusPayload[0], 1, bitRead(limitStatus,7));                 // LIMIT_STATUS Bit 7 is AN1 (Closed side) limit switch active
  bitWrite(statusPayload[0], 2, motorSpeed < 0);                         // Negative motor speed is actively moving, opening the shutter
  bitWrite(statusPayload[0], 3, motorSpeed > 0);                         // Negative motor speed is actively moving, opening the shutter  
  bitWrite(statusPayload[0], 4, getSMCVariable(ERROR_STATUS) > 0);       // If ERROR_STATUS > 0, some error has occurred.
  bitWrite(statusPayload[0], 5, (limitStatus == 0 && motorSpeed == 0));  // If no limit switch is active, and speed is 0, shutter is stopped between limits
}

void setup(){

  Serial.begin(115200);
  Serial.println(F("TriStar Observatory : Shutter Secondary Controller"));
  smcSerial.begin(19200);

  // Reset SMC when Arduino starts up
  pinMode(resetPin, OUTPUT);
  digitalWrite(resetPin, LOW);  // reset SMC
  delay(1);  // wait 1 ms
  pinMode(resetPin, INPUT);  // let SMC run again

  // must wait at least 1 ms after reset before transmitting
  delay(5);

  // this lets us read the state of the SMC ERR pin (optional)
  // pinMode(errPin, INPUT);

  smcSerial.write(0xAA);  // send baud-indicator byte
  setMotorLimit(FORWARD_ACCELERATION, 100);
  setMotorLimit(REVERSE_ACCELERATION, 100);
  setMotorLimit(FORWARD_DECELERATION, 100);
  setMotorLimit(REVERSE_DECELERATION, 100);
  setMotorLimit(DECELERATION, 1);
  
  // clear the safe-start violation and let the motor run
  exitSafeStart();

  // Do our first stuffStatus
  stuffStatus();
  lastMillis = millis();             // We'll compare this in loop(), and poll status every 5s
  
  // Setup and configure rf radio

  radio.begin();
  radio.setAutoAck(true);
  radio.enableAckPayload();               // Allow optional ack payloads
  radio.setRetries(0,15);                 // Min time between retries, max # of retries
  radio.openReadingPipe(1,0xABCDABCD71LL);// No need for a writing pipe for Ack payloads
  radio.startListening();                 // Start listening
  radio.setDataRate(RF24_1MBPS);          // Works best, for some reason
  radio.setPALevel(RF24_PA_MIN);          // Issues with LOW?
  radio.setChannel(77);                   // In US, channel should be betwene 70-80
//  radio.printDetails();                 // Dump the configuration of the rf unit 
                                          // for debugging
}   //end setup

void loop(void) 
{
// SMC Error Status == 1 if only error is Safe Start Violation.  Since
// all other errors are 0, it is safe to exit Safe Start

  if (getSMCVariable(ERROR_STATUS) == 1) 
  {
    exitSafeStart();
  }

  currentMillis = millis();
  if (currentMillis - lastMillis > 5000)
  {
    stuffStatus();
    lastMillis = currentMillis;
  }

  if (Serial.available() > 0)
  {
    cmd=Serial.readStringUntil('#');  // Read the serial buffer until we encounter #
                                            // no error checking here, just for manual testing
                                            // of other components

    if (cmd=="o")  
     {
       setMotorSpeed(-800);
     }

    if (cmd=="c")  
     {
       setMotorSpeed(800);
     }

    if (cmd=="x")  
     {
       setMotorSpeed(0);
     }
    if (cmd=="r")
      {
          exitSafeStart();
      }
    if (cmd=="i")
      {
        Serial.print("Controller Temp : ");
        Serial.print(float(float(getSMCVariable(TEMPERATURE)) / 10),1);
        Serial.print(char(186));
        Serial.println("C");
//        Serial.print("Error Status: 0x");
//        Serial.println(getSMCVariable(ERROR_STATUS), BIN);
        Serial.print("VIN = ");
        Serial.print(float(float(getSMCVariable(INPUT_VOLTAGE)) / 1000));
        Serial.println(" V");        
        Serial.print("Limit Status = ");
        Serial.print(getSMCVariable(LIMIT_STATUS),DEC);
        Serial.print(" - ");
        Serial.println(getSMCVariable(LIMIT_STATUS),BIN);
        Serial.print("Motor Speed = ");
        Serial.println(getSMCVariable(SPEED),DEC);
        Serial.print("statusPayload[0] = ");
        Serial.print(statusPayload[0],DEC);
        Serial.print(" - ");
        Serial.println(statusPayload[0],BIN);
        Serial.println();

      }
  }











/*************** RF24 code not used for Serial branch
  while( radio.available(&pipeNum))
  {
    radio.read( &cmd, 1 );
//    doCommand(cmd);
    statusPayload[0]=cmd; //binStatus;
//    statusPayload[1]=tempStatus;
//    statusPayload[2]=errorStatus;
    radio.writeAckPayload(pipeNum,statusPayload, 3 );
  }   //end while

  // if an error is stopping the motor, write the error status variable
  // and try to re-enable the motor
  if (digitalRead(errPin) == HIGH)
  {
    Serial.print("Error Status: 0x");
    Serial.println(getSMCVariable(ERROR_STATUS), DEC);
    // once all other errors have been fixed,
    // this lets the motors run again
    exitSafeStart();
  }   //end if
*******************************/
 
}   //end loop
