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

// Status bytes
byte binStatus=0;
byte tempStatus=0;
byte errorStatus=0;

// Pololu SMC config
const int rxPin = 3;          // pin 3 connects to SMC TX
const int txPin = 4;          // pin 4 connects to SMC RX
const int resetPin = 5;       // pin 5 connects to SMC nRST
const int errPin = 6;         // pin 6 connects to SMC ERR

// Setup SoftwareSerial for communication w/ SMC
SoftwareSerial smcSerial = SoftwareSerial(rxPin, txPin);

// SMC Variable IDs
#define ERROR_STATUS 0
#define INPUT_VOLTAGE 23   //TODO Add byte for holding Input Voltage
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
byte cmd;

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

// handle commands received from RF24
bool doCommand(byte command)
{
  if (char(command)=='o')
  {
    statusPayload[0] = 4;
    setMotorSpeed(800);
    Serial.println(F("Shutter Opening"));
  }

  if (char(command)=='c')
  {
    statusPayload[0] = 8;
    setMotorSpeed(-800);
    Serial.println(F("Shutter Closing"));
  }

  if (char(command)=='x')
  {
    statusPayload[0] = 32;
    setMotorSpeed(0);
    Serial.println(F("Shutter Halted"));
  }
  return true;
}
void setup(){

  Serial.begin(115200);
  Serial.println(F("TriStar Observatory : Shutter Slave Controller"));
  smcSerial.begin(19200);

  // Reset SMC when Arduino starts up
  pinMode(resetPin, OUTPUT);
  digitalWrite(resetPin, LOW);  // reset SMC
  delay(1);  // wait 1 ms
  pinMode(resetPin, INPUT);  // let SMC run again

  // must wait at least 1 ms after reset before transmitting
  delay(5);

  // this lets us read the state of the SMC ERR pin (optional)
  pinMode(errPin, INPUT);

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
}   //end loop
