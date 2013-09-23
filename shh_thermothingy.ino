/* ===========================================================
    SHH_Thermothingy. GPLv3 20130923 JT
    Moans to /dev/null if i screwed it up.
  ===========================================================*/

//setup our shit for the eeprom
#include <EEPROMEx.h>  //EEPROMEx Library. Located at http://playground.arduino.cc//Code/EEPROMex
#define CONFIG_VERSION 1001 // ID of the settings block. change to trigger rewrite of eeprom.
#define memoryBase 32 //location of data in EEPROM storage
#define MCHARS 8            // max characters to accept in interactive cli
#define ENTER 13            // byte value of the Enter key (LF)
char inStr[MCHARS];         // the input string for serial cli
int strCount = 0;           // counter for the string
bool ok  = true; //this is used to check if eeprom is read ok. changed to false later on in the code when eeprom fails to verify.
bool doWait = true; //used if we need to wait in a loop
bool unsavedChanges = false; //changed to true if user attempts to exit and does not save changes back to eeprom
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
  totalLEDs=(srCount*srLEDs); //multiply leds on register by number of register
  Serial.begin(9600); //tell ide that we want a serial session opened
  Serial.println(); //nice tidy new line for our crap
  Serial.print("Load... "); //let user know we're starting up
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
    Serial.println("EEPROM Initialized. Restarting!");
    delay(500); //Settle down.
    doReset();
  } else { //otherwise the config has loaded and verified ok
    Serial.print("Reading signature: '"); Serial.print(storage.version); Serial.println("' Config verified!"); // let user know that the config's good
    currFunds=storage.currFunds; //load current funds variable with the data stored in our EEPROM
    targetFunds=storage.targetFunds; //load target funds from the EEPROM too.
    Serial.println("Current settings are as follows");
    Serial.print("cF:"); Serial.print(currFunds); Serial.print(" tF:"); Serial.println(targetFunds);  //print out the data we've just loaded from eeprom.
  }
  computeSegments(); //run segment computation sub to give display initial data before we jump into main loop. (no need to write display in a loop!)
  Serial.print("> "); //print prompt for user
}

void loop() {
  if (Serial.available() > 0) { //if serial data is available
  inByte = Serial.read(); //read serial data to var inByte
  switch (inByte) {  //what is our inByte?
    case 99: // inbyte is ascii "c" 
      serialSettings(); //call serialSettings
      break;    
    default: // case not matched, fall back to default
      Serial.print("er ");
      Serial.print(inByte, DEC);
      Serial.println("?");
      Serial.println("Hint: Send ascii 'c' to enter configure mode.");
      Serial.print("> ");
    }
   }
}

void serialSettings(){
  printSettings(); //call printSettings to print out settings data
  Serial.println("Adjust? y/n?");
  //trap ourselves in a loop here and wait for serial input
  while (doWait == true) {
       if (Serial.available() > 0) {
  inByte = Serial.read();
  switch (inByte) {
    case 121:     // ascii "y" - user wants to adjust settings.
      Serial.print("Config> ");                      // print the config prompt
      while (doWait == true) {                      // use doWait to stay in loop
          if(Serial.available()) {                 // if bytes are available to be read
          inByte = Serial.read();                // read the byte
          Serial.print(char(inByte));            // echo byte entered to terminal as a char
          if(inByte != ENTER) {                  // if the byte is not a NEWLINE
            inStr[strCount] = char(inByte);      // add the byte to the input string array
            strCount++;                          // increase the string item count
          } 
        }
        if(inByte == ENTER || strCount >= MCHARS) {  // if enter was pressed or max chars reached
          Serial.flush();                         // flush the serial data (overkill?)
          Serial.println("");                     // print a newline
          if (inStr[0] == 'c') {  //if input stats with c, such as "c1234567"
            currFunds = atol(&inStr[1]); //convert char arrya inStr to long integer. YO ITS atol() FOR LONGS NOT atoi() YOU LAZY SHITBAG
            Serial.println(currFunds);  //print out new data
          } else if (inStr[0] == 't') { //if input starts with t, such as "t1234567"
            targetFunds = atol(&inStr[1]); //convert char arrya inStr to long integer.
            Serial.println(targetFunds);  //print out new data
          } else if (strcmp(inStr, "reset") == 0){ //if thingy has reset in it
            doReset();  //go to reset subroutine
          } else if (strcmp(inStr, "write") == 0){
            storage.targetFunds = targetFunds;  //write target funds value to storage
            storage.currFunds = currFunds;  //write current funds value to storage
            saveConfig();  //subroutine saveConfig writes values from storage to eeprom.  
            doWait = false; //get out of loop
          } else if (strcmp(inStr, "exit") == 0){ //if user says exit
            if (currFunds != storage.currFunds  || targetFunds != storage.targetFunds) {//Check if there's unsaved changes - does ram vars match eeprom vars?
              Serial.print("Unsaved changes detected. ");
              printSettings();
              unsavedChanges = true;
            } 
            if (unsavedChanges == true) { //if there are unsaved changes
              Serial.print("Save before exit? y/n");
               while (doWait == true) {
                 if (Serial.available() > 0) {
                  inByte = Serial.read();
                  switch (inByte) {
                    case 121:     //y
                    Serial.println(" y");
                    Serial.print("Writing changes to EEPROM.... ");
                    storage.targetFunds = targetFunds;  //write target funds value to storage
                    storage.currFunds = currFunds;  //write current funds value to storage
                    saveConfig();  //subroutine saveConfig writes values from storage to eeprom.    
                    doWait=false;
                    break;
                    case 110:     //n
                    Serial.println(" n");
                    doWait=false;
                    Serial.println("Changes remain unsaved to EEPROM.");
                    break;   
                    default:
                    Serial.print("er ");
                    Serial.print(inByte, DEC);
                    Serial.println("? A valid response is y or n.");
                  }
                 }
                }
                doWait=false;    
            }
            doWait = false;
          } else {                                // if the input text doesn't match any defined above
            Serial.print(inStr[0]);
            Serial.println(" is invalid.");           // echo back to the terminal
            Serial.println("Hint: Valid command 'c0000000' and 't0000000' to set current and target funds, exit to exit, write to write.");
          }
          strCount = 0;                           // get ready for new input... reset strCount 
          inByte = 0;                             // reset the inByte variable
          for(int i = 0; inStr[i] != '\0'; i++) { // while the string does not have null
            inStr[i] = '\0';                      // fill it with null to erase it
          }
          if (doWait == true) {  
            Serial.println("");                     // print a newline
            Serial.print("Config> ");                      // print the prompt
          } else {
            Serial.println("Done configuring.");
            printSettings(); //print out configuration data.
            computeSegments(); //redraw display
            Serial.print("> "); //print the prompt
          }
        }
      }
      doWait == false;
      break;
    case 110:     //n
      doWait=false;
      Serial.println("Adjustment cancelled.");
      Serial.print("> ");
      break;   
    default:
      Serial.print("er ");
      Serial.print(inByte, DEC);
      Serial.println("?");
    }
   }
  }
  doWait=true;
}

void computeSegments(){
  // *********** This bit works out how many segments to display. ***********
  Serial.print("computeSegments called. ");
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
  shiftOut(sdPin, scPin, MSBFIRST, srB); //shift out 2nd register FIRSTR (will be pushed into srB when data pushed into srA)
  shiftOut(sdPin, scPin, MSBFIRST, srA); //shift out data to first register.
  digitalWrite(slPin,1); //tell shift registers to expect no more data.
}



bool loadConfig() {
  EEPROM.readBlock(configAddress, storage);    //read storage block of EEPROM.
  return (storage.version == CONFIG_VERSION); //does version from EEPROM match that from our code?
}

void saveConfig() {
   EEPROM.writeBlock(configAddress, storage);  //write back the config block.
   delay(1000); //sleep so we can't burn out the eeprom if we're stuck in a loop
   Serial.println("Wrote to EEPROM!");  //let the user know what just happened.
}

void doReset() {
   asm volatile("NOP");
   asm volatile("JMP 0"); //L0L H4X! Restart code...
}

void printSettings () { //This is used to print out setting in chip ram and eeprom. (
    Serial.print("Settings in RAM: ");
    Serial.print("cF:"); Serial.print(currFunds); Serial.print(" tF:"); Serial.println(targetFunds);
    Serial.print("Settings in EEPROM: ");
    Serial.print("cF:"); Serial.print(storage.currFunds); Serial.print(" tF:"); Serial.println(storage.targetFunds);
}