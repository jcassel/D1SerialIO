//simple D1 mini Serial IO Driver
//Takes serial commands to control configured IO.
//Can create automations for interactions between inputs, outputs and actions. (Future)

//Eventually should support a closed loop temerature control. Need to have a sensor and a output to control a heating element. Could also do a fan / exaust system to cool enclosure down. 

//Target outputs 
//Digital/relay: PSU,Lighting,Enclosure heater, alarm, filter relay,exaust fan
//Digital (PWM):cooling fan(PWM*) (Future)

//Target inputs
//Digital(5v/3.3v): General switches(NO or NC Selectable)
//temperature inputs(Future + TBA on type)


#define VERSIONINFO "D1SerialIO 1.0.4"
#define COMPATIBILITY "SIOPlugin 0.1.1"
#include "TimeRelease.h"
#include <ArduinoJson.h> 
#include "FS.h"
#define FS_NO_GLOBALS
#include <ESP8266WiFi.h>
#include <Bounce2.h>


#define IO0 16// In1 (5v)
#define IO1 12// (MISO) In2 (5v)
#define IO2 13// (MOSI) In3 (3.3v)
#define IO3 4 // (SDA) In4 (5v)
#define IO4 5 // (SCL) In5 (5v)
#define IO5 14// (SCK) In6 (3.3v)
#define IO6 0 // (10k pull-up) (RELAY 1)
#define IO7 15// (10k pull-down,SS) (Relay 2) 
#define IO8 2 // (10k pull-up, BUILTIN_LED)
//#define IOA A0//Analog 0 


#define IOSize  9
bool _debug = false;
int IOType[IOSize]{INPUT_PULLUP,INPUT_PULLUP,INPUT_PULLUP,INPUT_PULLUP,INPUT_PULLUP,INPUT_PULLUP,OUTPUT,OUTPUT,OUTPUT};
int IOMap[IOSize] {IO0,IO1,IO2,IO3,IO4,IO5,IO6,IO7,IO8};
int IO[IOSize];
Bounce Bnc[IOSize];
bool EventTriggeringEnabled = 1;


bool isOutPut(int IOP){
  return IOType[IOP] == OUTPUT; 
}

void ConfigIO(){
  
  Serial.println("Setting IO");
  for (int i=0;i<IOSize;i++){
    if(IOType[i] == 0 ||IOType[i] == 2 || IOType[i] == 3){ //if it is an input
      pinMode(IOMap[i],IOType[i]);
      Bnc[i].attach(IOMap[i],IOType[i]);
      Bnc[i].interval(5);
    }else{
      pinMode(IOMap[i],IOType[i]);
      digitalWrite(IOMap[i],LOW);
    }
  }

}



TimeRelease IOReport;
TimeRelease IOTimer[9];
unsigned long reportInterval = 3000;



void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  delay(300);
  Serial.println(VERSIONINFO);
  Serial.println("Disabling Wifi");
  WiFi.mode(WIFI_OFF);    //The entire point of this project is to not use wifi but have directly connected IO for use with the D1SerialIO OctoPrint PlugIn and subPlugIn
  Serial.println("Start Types");
  InitStorageSetup();
  loadIOConfig();
  Serial.println("Initializing IO");
  ConfigIO();
  
  reportIOTypes();
  Serial.println("End Types");

  //need to get a baseline state for the inputs and outputs.
  for (int i=0;i<IOSize;i++){
    IO[i] = digitalRead(IOMap[i]);  
  }
  
  IOReport.set(100ul);
  Serial.println("RR"); //send ready for commands    
}


bool _pauseReporting = false;
bool ioChanged = false;

//*********************Start loop***************************//
void loop() {
  // put your main code here, to run repeatedly:
  checkSerial();
  if(!_pauseReporting){
    ioChanged = checkIO();
    reportIO(ioChanged);
    
  }
  
}
//*********************End loop***************************//

void reportIO(bool forceReport){
  if (IOReport.check()||forceReport){
    Serial.print("IO:");
    for (int i=0;i<IOSize;i++){
      if(IOType[i] == 1 ){ //if it is an output
        IO[i] = digitalRead(IOMap[i]);  
      }
      Serial.print(IO[i]);
    }
    Serial.println();
    IOReport.set(reportInterval);
    
  }
}

bool checkIO(){
  bool changed = false;
  
  for (int i=0;i<IOSize;i++){
    if(!isOutPut(i)){
      Bnc[i].update();
      if(Bnc[i].changed()){
       changed = true;
       IO[i]=Bnc[i].read();
       if(_debug){Serial.print("Output Changed: ");Serial.println(i);}
      }
    }else{
      //is the current state of this output not the same as it was on last report.
      //this really should not happen if the only way an output can be changed is through Serial commands.
      //the serial commands force a report after it takes action.
      if(IO[i] != digitalRead(IOMap[i])){
        if(_debug){Serial.print("Output Changed: ");Serial.println(i);}
        changed = true;
      }
    }
    
  }
    
  return changed;
}


void reportIOTypes(){
  for (int i=0;i<IOSize;i++){
    Serial.print(IOType[i]);
    //if(i<IOSize-1){Serial.print(";");}
  }
  Serial.println();
}

void checkSerial(){
  if (Serial.available()){
    
    String buf = Serial.readStringUntil('\n');
    buf.trim();
    if(_debug){Serial.print("buf:");Serial.println(buf);}
    int sepPos = buf.indexOf(" ");
    String command ="";
    String value = "";
    
    if(sepPos){
      command = buf.substring(0,sepPos);
      value = buf.substring(sepPos+1);;
      if(_debug){
        Serial.print("command:");Serial.print("[");Serial.print(command);Serial.println("]");
        Serial.print("value:");Serial.print("[");Serial.print(value);Serial.println("]");
      }
    }else{
      command = buf;
      if(_debug){
        Serial.print("command:");Serial.print("[");Serial.print(command);Serial.println("]");
      }
    }
    
    if(command == "BIO"){ 
      ack();
      _pauseReporting = false; //restarts IO reporting 
      return;
    }    
    else if(command == "EIO"){ //this is the command meant to test for good connection.
      ack();
      _pauseReporting = true; //stops all IO reporting so it does not get in the way of a good confimable response.
      //Serial.print("Version ");
      //Serial.println(VERSIONINFO);
      //Serial.print("COMPATIBILITY ");
      //Serial.println(COMPATIBILITY);
      return;
    }
    else if (command == "IC") { //io count.
      ack();
      Serial.print("IC:");
      Serial.println(IOSize);
      return;
    }
    
    else if(command =="debug"){
      ack();
      if(value == "1"){
        _debug = true;
        Serial.println("Serial debug On");
      }else{
        _debug=false;
        Serial.println("Serial debug Off");
      }
      return;
    }
    else if(command=="CIO"){ //set IO Configuration
      ack();
      if (validateNewIOConfig(value)){
        updateIOConfig(value);
      }
      return;
    }
    
    else if(command=="SIO"){
      ack();
      StoreIOConfig();
      return;
    }
    else if(command=="IOT"){
      ack();
      reportIOTypes();
      return;
    }
    
    //Set IO point high or low (only applies to IO set to output)
    else if(command =="IO" && value.length() > 0){
      ack();
      int IOPoint = value.substring(0,value.indexOf(" ")).toInt();
      int IOSet = value.substring(value.indexOf(" ")+1).toInt();
      if(_debug){
        Serial.print("IO #:");
        Serial.println(IOMap[IOPoint]);
        Serial.print("Set:");Serial.println(IOSet);
      }
      
      if(isOutPut(IOPoint)){
        if(IOSet == 1){
          digitalWrite(IOMap[IOPoint],HIGH);
        }else{
          digitalWrite(IOMap[IOPoint],LOW);
        }
      }else{
        Serial.println("ERROR: Attempt to set IO which is not an output");   
      }
      delay(200); // give it a moment to finish changing the 
      reportIO(true);
      return;
    }
    
    //Set AutoReporting Interval  
    else if(command =="SI" && value.length() > 0){
      ack();
      unsigned long newTime = 0;
      if(strToUnsignedLong(value,newTime)){ //will convert to a full long.
        if(newTime >=500){
          reportInterval = newTime;
          IOReport.clear();
          IOReport.set(reportInterval);
          if(_debug){
            Serial.print("Auto report timing changed to:");Serial.println(reportInterval);
          }
        }else{
          Serial.println("ERROR: minimum value 500");
        }
        return;
      }
      Serial.println("ERROR: bad format number out of range");
      return; 
    }
    //Enable event trigger reporting Mostly used for E-Stop
    else if(command == "SE" && value.length() > 0){
      ack();
      EventTriggeringEnabled = value.toInt();
      return;
    }
    else if (command == "restart" || command == "reset" || command == "reboot"){
      Serial.println("[WARNING]: Restarting device");
      ESP.restart();
    }

    //Get States 
    else if(command == "GS"){
      ack();
      reportIO(true);
      return; 
    }
    else{
      Serial.print("ERROR: Unrecognized command[");
      Serial.print(command);
      Serial.println("]");
    }
  }
}

void ack(){
  Serial.println("OK");
}

bool validateNewIOConfig(String ioConfig){
  
  if(ioConfig.length() != IOSize){
    if(_debug){Serial.println("IOConfig validation failed(Wrong len)");}
    return false;  
  }

  for (int i=0;i<IOSize;i++){
    int pointType = ioConfig.substring(i,i+1).toInt();
    if(pointType > 4){//cant be negative. we would have a bad parse on the number so no need to check negs
      if(_debug){
        Serial.println("IOConfig validation failed");Serial.print("Bad IO Point type: index[");Serial.print(i);Serial.print("] type[");Serial.print(pointType);Serial.println("]");
      }
      return false;
    }
  }
  if(_debug){Serial.println("IOConfig validation good");}
  return true; //seems its a good set of point Types.
}

void updateIOConfig(String newIOConfig){
  for (int i=2;i<IOSize;i++){//start at 2 to avoid D0 and D1
    int nIOC = newIOConfig.substring(i,i+1).toInt();
    if(IOType[i] != nIOC){
      IOType[i] = nIOC;
      if(nIOC == OUTPUT){
        digitalWrite(IOMap[i],LOW); //set outputs to low since they will be high coming from INPUT_PULLUP
      }
    }
  }
}

int getIOType(String typeName){
  if(typeName == "INPUT"){return 0;}
  if(typeName == "OUTPUT"){return 1;}
  if(typeName == "INPUT_PULLUP"){return 2;}
  if(typeName == "INPUT_PULLDOWN"){return 3;}
  if(typeName == "OUTPUT_OPEN_DRAIN"){return 4;} //not sure on this value have to double check
}

bool strToUnsignedLong(String& data, unsigned long& result) {
  data.trim();
  long tempResult = data.toInt();
  if (String(tempResult) != data) { // check toInt conversion
    // for very long numbers, will return garbage, non numbers returns 0
   // Serial.print(F("not a long: ")); Serial.println(data);
    return false;
  } else if (tempResult < 0) { //  OK check sign
   // Serial.print(F("not an unsigned long: ")); Serial.println(data);
    return false;
  } //else
  result = tempResult;
  return true;
}


bool loadIOConfig(){
  
if (!SPIFFS.exists("/IOConfig.json"))
  {
    Serial.println("[WARNING]: IOConfig file not found!");
    Serial.println("Using Default Config");
    return false;
  }
  File configfile = SPIFFS.open("/IOConfig.json","r");

  DynamicJsonDocument doc(256);

  DeserializationError error = deserializeJson(doc, configfile);
  
  
  IOType[0] = getIOType(doc["IO0"]) ;
  IOType[1] = getIOType(doc["IO1"]) ;
  IOType[2] = getIOType(doc["IO2"]) ;
  IOType[3] = getIOType(doc["IO3"]) ;
  IOType[4] = getIOType(doc["IO4"]) ;
  IOType[5] = getIOType(doc["IO5"]) ;
  IOType[6] = getIOType(doc["IO6"]) ;
  IOType[7] = getIOType(doc["IO7"]) ;
  IOType[8] = getIOType(doc["IO8"]) ;

  Serial.println("loaded IO From json: ");  
  serializeJson(doc, Serial);
  
  configfile.close();

  if (error)
  {
    Serial.println("[ERROR]: deserializeJson() error in loadIOConfig");
    Serial.println(error.c_str());
    return false;
  }else{
    Serial.println("Loaded IO Config from storage");
  }
  
  return true;
}


bool StoreIOConfig(){
  SPIFFS.remove("/IOConfig.json");
  File file = SPIFFS.open("/IOConfig.json", "w");
  if(_debug){Serial.println("saving IO Config");}
  DynamicJsonDocument doc(256);

  JsonObject configObj = doc.to<JsonObject>();
  
  configObj["IO0"]= getIOTypeString(IOType[0]);
  configObj["IO1"]= getIOTypeString(IOType[1]);
  configObj["IO2"]= getIOTypeString(IOType[2]);
  configObj["IO3"]= getIOTypeString(IOType[3]);
  configObj["IO4"]= getIOTypeString(IOType[4]);
  configObj["IO5"]= getIOTypeString(IOType[5]);
  configObj["IO6"]= getIOTypeString(IOType[6]);
  configObj["IO7"]= getIOTypeString(IOType[7]);
  configObj["IO8"]= getIOTypeString(IOType[8]);

  Serial.println("Saved IO as json: ");  
  serializeJson(configObj, Serial);  
  
  if (serializeJsonPretty(doc, file) == 0)
  {
    Serial.println("[WARNING]: Failed to write to file:/config/mqttconfig.json");
    return false;
  }
  file.close();
  if(_debug){Serial.println("Saved IO Config");}
  return true;
}


float SpaceLeft(){
  FSInfo fs_info;
  
  float freeMemory = fs_info.totalBytes - fs_info.usedBytes;
  return freeMemory;
  
}

bool IsSpaceLeft()
{
  float minmem = 100000.00; // Always leave 100 kB free pace on SPIFFS
  float freeMemory = SpaceLeft();
  Serial.printf("[INFO]: Free memory left: %f bytes\n", freeMemory);
  if (freeMemory < minmem)
  {
    return false;
  }

  return true;
}

String getIOTypeString(int ioType){
  if (ioType == 0){return "INPUT";}
  if (ioType == 1){return "OUTPUT";}
  if (ioType == 2){return "INPUT_PULLUP";}
  if (ioType == 3){return "INPUT_PULLDOWN";}
  if (ioType == 4){return "OUTPUT_OPEN_DRAIN";}
}

void InitStorageSetup(){
  SPIFFS.begin();
  //do anything that might be needed related to file and storage on startup.
}
