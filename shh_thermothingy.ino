/* ===========================================================
    SHH_Thermothingy. GPLv3 20130916 R01B JT
    Moans to /dev/null if i screwed it up.
  ============================================================
  Changelog:
  
  20130916 R01A - Initial revision
    Made some shit work- can calculate a few things but no output yet.
  
  20130917 R01B -
    Added EEPROM Read/Write, shiftOut and stuff to make sure it displays properly.
    Still to do: Serial interface, writing serial read values back to eeprom.  
  
  ==========================================================*/

//setup our shit for the eeprom
#include <EEPROMEx.h>  //EEPROMEx Library. Located at http://playground.arduino.cc//Code/EEPROMex
#define CONFIG_VERSION 1001 // ID of the settings block. change to trigger rewrite of eeprom.
#define memoryBase 32 //location of data in EEPROM storage
bool ok  = true; //this is used to check if eeprom is read ok. changed to false later on in the code when eeprom fails to verify.
int configAddress=0; //define where the config starts

//IO vars. Won't change.
const int sdPin = 8; //shift register DATA pin
const int scPin = 12; //shift register CLOCK pin
const int slPin = 11; //shift register LATCH pin
int srA = 0; //storing shift register valuses
int srB = 0; //storing shift reg values
int inByte=0; //byte used for serial read

//define how we've configured the shift registers outputs (the leds)
// the idea of this being we could hook up 3 registers with 6 outputs each and the code recalculates how to display.
// currently this functionality is only half-completed. FIXME
int srCount = 2; //how many shift registers we're using. may be used later.
int srLEDs = 8; //how many leds we have connected to each register.
int totalLEDs = 0; //don't touch. we set this at setup.

//we'll load these two with data from the EEPROM in a minute, for the moment null them.
// don't edit these. they'll get loaded from the eeprom anyways.
long currFunds; //we store current funds here
long targetFunds; //we store the target here
long fundsPerSeg; //will be worked out later
int litSegs = 0; //we store segment data here.

struct StoreStruct {long version, targetFunds, currFunds;} storage = { CONFIG_VERSION, 0, 0,}; // The variables of the settings to store in eeprom

//crumpets
void setup() {
  Serial.begin(9600); //tell ide that we want a serial session opened
  totalLEDs=(srCount*srLEDs); //multiply leds on register by number of register
  Serial.println("Load...");
  pinMode(sdPin, OUTPUT); //set shift register DATA pin as an output
  pinMode(scPin, OUTPUT); //same for CLOCK pin..
  pinMode(slPin, OUTPUT); //and the LATCH pin too.
  digitalWrite(slPin, 0); //tell shift registers that they are to expect data.
  shiftOut(sdPin, scPin, MSBFIRST, 255);  //shift out 255
  shiftOut(sdPin, scPin, MSBFIRST, 255);  //and again, to turn on all the leds
  digitalWrite(slPin, 1);                 //tell shift registers that they are to no longer expect any data.
  
  // *************** This bit performs checks on the EEPROM and resets if if needs be. *********************************
  EEPROM.setMemPool(memoryBase, EEPROMSizeATmega328); //Set memorypool base to whatever the 328 has. change if you're using a smaller chip.
  configAddress  = EEPROM.getAddress(sizeof(StoreStruct)); // Size of config object
  ok = loadConfig(); //ok will return either true or false depending on results of loadConfig.
  if ( ok == false ) { //if config fails to load
    Serial.println("EEPROM Config failed to verify. Resetting!");
    storage.version = CONFIG_VERSION; //write config version to storage
    storage.targetFunds = 16000;  //write example target funds value to storage
    storage.currFunds = 8000;  //write example current funds value to storage
    saveConfig();  //subroutine saveConfig writes values from storage to eeprom.
    Serial.println("EEPROM Initialized. Restarting.");
    delay(500); //Settle down.
    asm volatile("JMP 0"); //L0L H4X! Restart code... (FIXME?)
  } else { //otherwise the config has loaded and verified ok
    Serial.print("Reading signature: '"); Serial.print(storage.version); Serial.println("' Config verified!"); // let user know that the config's good
    currFunds=storage.currFunds; //load current funds variable with the data stored in our EEPROM
    targetFunds=storage.targetFunds; //load target funds from the EEPROM too.
    Serial.print("cF:"); Serial.print(currFunds); Serial.print(" tF:"); Serial.println(targetFunds);  //print out the data we've just loaded from eeprom.

  }
  computeSegments(); //run segment computation sub to give display initial data before we jump into main loop. (no need to write display in a loop!)
}

void loop() {  
  //read the serial for a mode command or some shit like that
  
  // kludge to manipulate cF var at runtime. somebody kill this before it kills you.
   if (Serial.available() > 0) {
  inByte = Serial.read();
  switch (inByte) {
    case 100:     //d
      Serial.println("100");
      currFunds=currFunds-1000;
      computeSegments();
      break;
    case 101:     //e
      Serial.println("101");
      currFunds=currFunds+1000;
      computeSegments();
      break;
    default:
      Serial.println("er");
    }
   }
}

void serialSettings(){
  Serial.println("Current settings are as follows");
  Serial.print("cF:"); Serial.print(currFunds); Serial.print(" tF:"); Serial.println(targetFunds);  //print out settings data
  Serial.println("Adjust? y/n?");
  //trap ourselves in a loop here and wait for serial input
}

void computeSegments(){
  // *********** This bit works out how many segments to display. ***********
  fundsPerSeg=(targetFunds/totalLEDs); //divide target funds by number of leds to work out how much we need for each segment to be lit
  litSegs=(currFunds/fundsPerSeg); //divide current funds by the calculated amount we need to light each segment.
  if ( litSegs < 0 ) { Serial.println("CalcERR - litSegs less than 0!"); litSegs=0; } //catch erroneous conditions
  if (litSegs > totalLEDs ) { Serial.println("CalcERR - litSegs more than no of leds!"); litSegs=0; } //catch another erroneous condition. generally won't happen unless you screw with the debug tools.
  Serial.print("We should be seeing "); Serial.print(litSegs); Serial.println(" lit segments."); //debug

  // ************ This bit does the displaying. *****************************
  // FIXME - I only calculate for a fixed 2 registers and 8 LEDs!
  int seq[9] = {0,1,3,7,15,31,63,127,255}; //bitmap for displaying each stage on LEDs
  if (litSegs <= 8) {
    srA=seq[litSegs]; //push bitmap sequence into shift register
    srB=0; //Lit segments is less than 8, so 2nd shift register is kept at 0
  } else if (litSegs > 8) {
    litSegs=(litSegs-8);
    srA=255; // more than 8 segments are lit. first register is always ON/255
    srB=seq[litSegs]; //push bitmap sequence into 2nd shift register
  }
  Serial.print("srA:"); Serial.print(srA); Serial.print(" srB:"); Serial.println(srB); //debug- print shift register values over serial.
  digitalWrite(slPin,0); //tell shift registers to expect data.
  shiftOut(sdPin, scPin, MSBFIRST, srB); //shift out 2nd register FIRSTR (will be pushed inso srB when data pushed into srA)
  shiftOut(sdPin, scPin, MSBFIRST, srA); //shift out data to first register.
  digitalWrite(slPin,1); //tell shift registers to expect no more data.
  
}

bool loadConfig() {
  EEPROM.readBlock(configAddress, storage);    //read storage block of EEPROM.
  return (storage.version == CONFIG_VERSION); //does version from EEPROM match that from our code?
}

void saveConfig() {
   EEPROM.writeBlock(configAddress, storage);  //write back the config block.
   Serial.println("Wrote to EEPROM. Sleeping for 1Secs.");  //let the user know what just happened.
   delay(1000); //sleep so we can't burn out the eeprom if we're stuck in a loop
}
