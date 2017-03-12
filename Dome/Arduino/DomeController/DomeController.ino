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
#include <SoftwareSerial.h>
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

// Interrupt for azimuth encoder
const byte interruptPin = 3;
static unsigned long last_interrupt_time = 0;
unsigned long interrupt_time;

// Create instance of LCD
LiquidCrystal_I2C  lcd(I2C_ADDR,En_pin,Rw_pin,Rs_pin,D4_pin,D5_pin,D6_pin,D7_pin);

// Pololu SMC config
const int rxPin = 10;          // pin 10 connects to SMC TX
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

int Azimuth = 0;

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

void sendDome(String command)
{
//  Serial.print("Got command ");
//  Serial.println(command);
  if (command=="dazm")
  {
    Serial.print(Azimuth);
    Serial.println("#");
  }
  if (command=="dneg")
  {
    setMotorSpeed(-800);
  }
  if (command=="dpos")
  {
    setMotorSpeed(800);
  }
  if (command=="dstp")
  {
    setMotorSpeed(0);
  }
  
}

void azimuthTick()
{


 // If interrupts come faster than 200ms, assume it's a bounce and ignore
interrupt_time=millis();
if (interrupt_time - last_interrupt_time > 800) 
 
//  if (getSMCVariable(SPEED) > 0)
  {
    Azimuth = Azimuth + 5;
  }
//  else
//  {
//    Azimuth = Azimuth - 5;
//  }
last_interrupt_time = interrupt_time;
}

void setup(){

  pinMode(interruptPin, INPUT);
  attachInterrupt(digitalPinToInterrupt(interruptPin), azimuthTick, CHANGE);

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
  smcSerial.begin(19200);
  
  lcd.begin (20,4);
  lcd.setBacklightPin(BACKLIGHT_PIN,POSITIVE);
  lcd.setBacklight(HIGH);
  analogWrite(BACKLIGHT_BRIGHTNESS,128);
  lcd.clear();
  lcd.print("TriStar Observatory");
  lcd.setCursor (0,1); 
  lcd.print("Dome Control");
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
    if (strCmd.startsWith("s"))
    {
      sendShutter(cmd);        //TODO : Write this function.
      if (strCmd == "snfo")
      {
        Serial.print(statusBytes[0]);
        Serial.println("#");
      }
    }
    else
    {
        sendDome(strCmd);
    }
  }
} // End loop()


