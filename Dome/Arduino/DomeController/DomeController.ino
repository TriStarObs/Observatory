/*************************************************************************
 * TriStar Observatory Dome Master
 * 2017MAR14
 * v0.2
 * 
 * The dome master will handle all dome rotation functions, and report dome and
 * shutter information to ASCOM.
 * 
 * It will also relay any shutter commands and shutter status info to/from the
 * shutter slave controller via RF24 link
 * 
 * Parts derived from :
 * maniacbug's RF24 library pingpair_ack example
 * Stanley Seow's nRF Serial Chat
 * Pololu's SMC Arduino examples
 *************************************************************************/

#include <SPI.h>
#include <SoftwareSerial.h>
#include "RF24.h"
#include <Wire.h>

//***** Variables, constants, and defines *****//

// Pololu SMC config
const int rxPin = 3;          // pin 10 connects to SMC TX
const int txPin = 4;          // pin 4 connects to SMC RX
const int resetPin = 5;       // pin 5 connects to SMC nRST
const int errPin = 6;         // pin 6 connects to SMC ERR

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

// Variables for Shutter Ack Payload Values
byte shLimitStatus = 0;
byte shErrorInfo = 0;
byte shControllerTemp = 0;
byte azOffset=0;
byte azIndicator=0;
byte shStatus=0;

// Local variables
int currentAzimuth = 0;
int toAzimuth = 0;
int toAzmithNormalized = 0;
int currentAzimuthNormalized = 0;
int travelDistance = 0;
int travelDistanceNormalized = 0;

bool Slewing = 0;
bool debugPrint = 0;
bool atPark = 0;

unsigned long lastMillis = 0;
unsigned long currentMillis = 0;

String strCmd;
String shArray[] = {"Open", "Closed", "Opening", "Closing", "Error"};

byte cmd[5];                              // Hold the command we're sending to the shutter
                                          // c - Close shutter
                                          // o - Open shutter
                                          // s - Shutter status
                                          // x - Halt all shutter motion immediately

byte statusBytes[6];                      // Array for shutter status
                                          // [0] - Binary status info (Table of bits in shutter sketch)
                                          // [1] - Error Info - Reference : Shutter sketch and https://www.pololu.com/docs/0J44/6.4
                                          // [2] - Shutter Controller Temp (°C, rounded to nearest int)
                                          // [3] - Azimuth offset from 180°
                                          // [4] - Indicator Azimuth offset, 1 or 0.  1 is 180 + offset, 0 is 180 - offset
                                          // [5] - Shutter status, mapped to 
                                          //       ASCOM ShutterState http://www.ascom-standards.org/Help/Developer/html/T_ASCOM_DeviceInterface_ShutterState.htm

//***** Instances of various hardware & comms *****//

// SoftwareSerial for communication w/ SMC
SoftwareSerial smcSerial = SoftwareSerial(rxPin, txPin);

// nRF24L01 radio on SPI bus plus pins 7 & 8 
RF24 radio(7,8);

//***** Functions containing references, since Arduino does not create prototypes for references *****//

boolean sendShutter(byte (&cmdArray)[5])
{
  if (!radio.write( &cmdArray, sizeof(cmdArray) ))
  {
    if (debugPrint)
    {
      Serial.println("Sending of shutter command failed!");
    }
    return false;
  }
  else
  {
    if(!radio.available())
    { 
      if (debugPrint)
      {
        Serial.println("Blank Payload.");  
      }
      return false;
    }
    else
    {
      while(radio.available() )
      {
        radio.read(statusBytes, 6 );
      }
    }
    return true;
  }
}

//***** Main Setup() and Loop() functions *****//

void setup(){

  // Serial lines
  
  Serial.begin(9600);       // For serial monitor troubleshooting
  smcSerial.begin(19200);   // Begin serial to SMC
  
  // Setup and configure rf radio

  radio.begin();
  radio.enableAckPayload();               // Allow ack payloads
  radio.setRetries(2,15);                 // Min time between retries, max # of retries
  radio.openWritingPipe(0xABCDABCD71LL);  // No need for a reading pipe for Ack payloads
  radio.setDataRate(RF24_1MBPS);          // Works best, for some reason
  radio.setPALevel(RF24_PA_MIN);          // Problems with LOW?
  radio.setChannel(77);                   // In US, channel should be betwene 70-80

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
  
  while (!radio.write( &cmd, 4 )) 
    {
      if (debugPrint)
      {
        Serial.println("No connection to shutter");  
      }
      
    }

  shLimitStatus = statusBytes[0];
  shErrorInfo = statusBytes[1];
  shControllerTemp = statusBytes[2];
  azOffset=statusBytes[3];
  azIndicator=statusBytes[4];
  shStatus=statusBytes[5];
  currentMillis=millis();
  lastMillis=currentMillis;
  if (azIndicator == 0)
  {
    currentAzimuth = 180 - azOffset;
  }
  else
  {
    currentAzimuth = 180 + azOffset;
  }

  if (currentAzimuth == 0 && shStatus == 1)
  {
    atPark = 1;
  }
} // end setup()

void loop(void) 
{
  if (Slewing)
  {
    checkSlew();
  }

  currentMillis=millis();
  if (currentMillis - lastMillis > 1000)
  {
    lastMillis=currentMillis;
    getInfo();    
  }
  
  while (Serial.available() > 0)
  {
    Serial.readBytesUntil('#', cmd, sizeof(cmd));
    strCmd = (char*)cmd;

/*******************************************************
 * Supported commands
 * sNNN     :       Slew to the azimuth NNN°
 * abrt     :       Abort any/all current movement (Dome rotation and shutter operations
 * clos     :       Close Shutter
 * open     :       Open Shutter
 * park     :       Park dome : Slew to 0° & close shutter
 * info     :       Request from ASCOM for all available property values
                          * AtPark
                          * Azimuth
                          * ShutterStatus
                          * Slewing
*******************************************************/
    if (strCmd.startsWith("s"))
    {
      atPark = 0;
      toAzimuth = strCmd.substring(1).toFloat();
      if (toAzimuth >= 0 && toAzimuth < 360 && toAzimuth != currentAzimuth)
      {
        slewToAzimuth(toAzimuth);  
      }
    }
    else if (strCmd == "abrt")
    {
      setMotorSpeed(0);
      String xxxx = "xxxx";
      xxxx.getBytes(cmd,5);
      sendShutter(cmd);
    }
    else if (strCmd == "info")
    {
      Serial.print(atPark);
      Serial.print("|");
      Serial.print(currentAzimuth);
      Serial.print("|");
      Serial.print(shStatus);
      Serial.print("|");
      Serial.print(Slewing);
      Serial.println("#");
    }
    else if (strCmd == "clos" || strCmd == "open")
    {
      atPark = 0;
      sendShutter(cmd);
    }
    else if (strCmd == "park")
    {
      if (!atPark)
      {
        slewToAzimuth(0);
        if (shStatus != 1)
        {
          String clos="clos";
          clos.getBytes(cmd,5);
          sendShutter(cmd);
        }
        atPark = 1;
      }
    }
  }
} // End loop()

//***** Program Functions *****//

void getInfo()
{
  String strEmpty = "info";
  strEmpty.getBytes(cmd,5);
  sendShutter(cmd);

  shLimitStatus = statusBytes[0];
  shErrorInfo = statusBytes[1];
  shControllerTemp = statusBytes[2];
  azOffset=statusBytes[3];
  azIndicator=statusBytes[4];
  shStatus=statusBytes[5];

  if (azIndicator == 0)
  {
    currentAzimuth = 180 - azOffset;
  }
  else
  {
    currentAzimuth = 180 + azOffset;
  }
  if (debugPrint)
  {
    Serial.print("Azimuth: ");
    Serial.println(currentAzimuth);
    Serial.print("Shutter: ");
    Serial.println(shArray[shStatus]);
    Serial.print("Dome: ");
    if (Slewing)
    {
      Serial.print("Slewing to ");
      Serial.println(toAzimuth);
    }
    else
    {
      Serial.println("Stationary");
    }
    Serial.println("-------------------------");
  }
} //end getInfo()

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

void slewToAzimuth(int destination)
{
  Slewing = 1;
  
  // Normalize the current azimuth and the destination to +/- 180
  // TODO : Write this as a function
  toAzmithNormalized = destination;
  while (toAzmithNormalized > 180) toAzmithNormalized -= 360;
  while (toAzmithNormalized < -180) toAzmithNormalized += 360;

  currentAzimuthNormalized = currentAzimuth;
  while (currentAzimuthNormalized > 180) currentAzimuthNormalized -= 360;
  while (currentAzimuthNormalized < -180) currentAzimuthNormalized += 360;

  travelDistanceNormalized = destination - currentAzimuthNormalized;
  while (travelDistanceNormalized > 180) travelDistanceNormalized -= 360;
  while (travelDistanceNormalized < -180) travelDistanceNormalized += 360;
   
  if (travelDistanceNormalized < 0)
  {
      setMotorSpeed(800);                 //  Counter-Clockwise
  }
  else
  {
    setMotorSpeed(-800);                //  Clockwise
  }
}

void checkSlew()
{
  if (travelDistanceNormalized < 0)
  {
    currentAzimuthNormalized = currentAzimuth;
    while (currentAzimuthNormalized > 180) currentAzimuthNormalized -= 360;
    while (currentAzimuthNormalized < -180) currentAzimuthNormalized += 360;

    travelDistanceNormalized = toAzmithNormalized - currentAzimuthNormalized;
    while (travelDistanceNormalized > 180) travelDistanceNormalized -= 360;
    while (travelDistanceNormalized < -180) travelDistanceNormalized += 360;

    if (travelDistanceNormalized >= 0)
    {
      setMotorSpeed(0);
      Slewing=0;
    }
    else if (travelDistanceNormalized > -10)
    {
      setMotorSpeed(400);
    }
  }
  else
  {
    currentAzimuthNormalized = currentAzimuth;
    while (currentAzimuthNormalized > 180) currentAzimuthNormalized -= 360;
    while (currentAzimuthNormalized < -180) currentAzimuthNormalized += 360;

    travelDistanceNormalized = toAzmithNormalized - currentAzimuthNormalized;
    while (travelDistanceNormalized > 180) travelDistanceNormalized -= 360;
    while (travelDistanceNormalized < -180) travelDistanceNormalized += 360;

    if (travelDistanceNormalized <= 0)
    {
      setMotorSpeed(0);
      Slewing=0;
    }
    else if (travelDistanceNormalized < 10)
    {
      setMotorSpeed(-400);
    }    
  }
}

