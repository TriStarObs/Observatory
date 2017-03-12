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
#include <LCD.h>
#include <LiquidCrystal_I2C.h>
#include <SoftwareSerial.h>
#include "RF24.h"

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
byte cmd[4];

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

// Stuff the statusPayload bytes
/**********************************************************************
  0 : Binary status indicators according to the following table
    0   Shutter is Open
    1   Shutter is Closed
    2   Shutter is Opening
    3   Shutter is Closing
    4   Shutter Error (Any error.  Details in next byte)
    5   Shutter is stopped (Neither open nor closed)
    6   Reserved
    7   Reserved
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

**********************************************************************/

void stuffStatus()
{
  statusPayload[0]=0;
  statusPayload[1]=0;
  statusPayload[2]=0;
  uint16_t limitStatus = getSMCVariable(LIMIT_STATUS);
  int motorSpeed = getSMCVariable(SPEED);
  int errorStatus = getSMCVariable(ERROR_STATUS);

  // Stuff status byte 0
  bitWrite(statusPayload[0], 0, bitRead(limitStatus,8));                  // LIMIT_STATUS Bit 8 is AN2 (Open side) limit switch active
  bitWrite(statusPayload[0], 1, bitRead(limitStatus,7));                  // LIMIT_STATUS Bit 7 is AN1 (Closed side) limit switch active
  bitWrite(statusPayload[0], 2, motorSpeed < 0);                          // Negative motor speed is actively moving, opening the shutter
  bitWrite(statusPayload[0], 3, motorSpeed > 0);                          // Positive motor speed is actively moving, Closing the shutter  
  bitWrite(statusPayload[0], 4, errorStatus > 0);                         // If ERROR_STATUS > 0, some error has occurred.
  bitWrite(statusPayload[0], 5, (limitStatus == 0 && motorSpeed == 0));   // If no limit switch is active, and speed is 0, shutter is stopped between limits
  //bitWrite(statusPayload[0], 6, (limitStatus > 0 && motorSpeed != 0));    // If either limit switch is active, and motor is moving, we have an issue
  
  // Stuff status byte 1
  bitWrite(statusPayload[1], 0, bitRead(errorStatus, 0));                  // Reference : https://www.pololu.com/docs/0J44/6.4
  bitWrite(statusPayload[1], 1, bitRead(errorStatus, 2));
  bitWrite(statusPayload[1], 2, bitRead(errorStatus, 4));
  bitWrite(statusPayload[1], 3, bitRead(errorStatus, 5));
  bitWrite(statusPayload[1], 4, bitRead(errorStatus, 6));
  bitWrite(statusPayload[1], 5, bitRead(errorStatus, 7));
  bitWrite(statusPayload[1], 6, bitRead(errorStatus, 8));
  bitWrite(statusPayload[1], 7, bitRead(errorStatus, 9));

  // Byte 2 is rounded temp from SMC - Don't think we need to return this, so returning 0 for now
    statusPayload[2] = 0;
//  int temp = round(float(getSMCVariable(TEMPERATURE)) / 10);
//  statusPayload[2] = temp;

}

void updateLCD()
{
      lcd.clear();
      lcd.print("T: ");
      lcd.print(float(float(getSMCVariable(TEMPERATURE)) / 10),1);
      lcd.print("C  ");
      lcd.print("VIn: ");
      lcd.print(float(float(getSMCVariable(INPUT_VOLTAGE)) / 1000),1);
      lcd.print("v");        
      lcd.setCursor(0,1);      
//      lcd.print("E: 0x");
//      lcd.print(getSMCVariable(ERROR_STATUS), BIN);
//      lcd.setCursor(0,1);      
      lcd.print("Shutter: ");
      if (getSMCVariable(LIMIT_STATUS) == 0)
        {
          if (getSMCVariable(SPEED) > 0)
          {
            lcd.print("Closing");
          }
          else
          {
            lcd.print("Opening");
          }
        }
        else
        {
          if (bitRead(getSMCVariable(LIMIT_STATUS),7))
          {
            lcd.print("Closed");
          }
          else
          {
            lcd.print("Open");
          }
        }
//      lcd.print(getSMCVariable(LIMIT_STATUS),BIN);
//      Serial.print("Motor Speed = ");
//      Serial.println(getSMCVariable(SPEED),DEC);
//      Serial.println();
      lcd.setCursor(0,2);
      lcd.print("S: ");
//      Serial.print("statusPayload[0] = ");
      lcd.print(statusPayload[0],DEC);
      lcd.print("-");
//      Serial.println(statusPayload[0],BIN);
//      Serial.print("statusPayload[1] = ");
      lcd.print(statusPayload[1],DEC);
      lcd.print("-");
//      Serial.println(statusPayload[1],BIN);
//      Serial.print("statusPayload[2] = ");
      lcd.print(statusPayload[2],DEC);
//      Serial.print(" - ");
//      Serial.println(statusPayload[2],BIN);
//      Serial.println();

}
void doCommand(String command)
{
  if (command=="shcl")  
   {
     setMotorSpeed(800);
     updateLCD();
   }
  else if (command=="shop")  
   {
     setMotorSpeed(-800);
     updateLCD();
   }
  else if (command=="xxxx")  
   {
     setMotorSpeed(0);
     updateLCD();
   }
  if (command=="rset")
    {
        exitSafeStart();
        updateLCD();
    }
  else if (command=="snfo")
    {
      updateLCD();
    }
}
void setup(){

//  Serial.begin(115200);
//  Serial.println(F("TriStar Observatory : Shutter Secondary Controller"));
  smcSerial.begin(19200);

  lcd.begin (20,4);
  lcd.setBacklightPin(BACKLIGHT_PIN,POSITIVE);
  lcd.setBacklight(HIGH);
  analogWrite(BACKLIGHT_BRIGHTNESS,128);
  lcd.clear();
  lcd.print("TriStar Observatory");
  lcd.setCursor (0,1); 
  lcd.print("Shutter Control");
  lcd.setCursor (0,2); 

  // Reset SMC when Arduino starts up
  pinMode(resetPin, OUTPUT);
  digitalWrite(resetPin, LOW);  // reset SMC
  delay(1);  // wait 1 ms
  pinMode(resetPin, INPUT);  // let SMC run again

  // must wait at least 1 ms after reset before transmitting
  delay(1000);

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
  lastMillis = millis();             // We'll compare this in loop(), and poll status every 1s
  doCommand("snfo");
  
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
  radio.flush_tx();
  radio.writeAckPayload(1,statusPayload, 3 );
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
  if (currentMillis - lastMillis > 1000)
  {
    stuffStatus();
    //doCommand("snfo");
    lastMillis = currentMillis;
    radio.flush_tx();
    radio.writeAckPayload(1,statusPayload, 3 );
  }

  while( radio.available())
  {
    radio.read( &cmd, 4 );
    String theCommand = (char*)cmd;
    theCommand.remove(4);
//    lcd.clear();
//    lcd.print("Received command : ");
//    lcd.setCursor(0,1);
//    lcd.print(theCommand);
    doCommand(theCommand);
  }   //end while
}   //end loop
