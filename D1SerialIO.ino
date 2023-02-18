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


#define VERSIONINFO "D1SerialIO 1.0.3"
#define COMPATIBILITY "SIOPlugin 0.1.1"
#include "TimeRelease.h"
#include <ArduinoJson.h> 
#include "FS.h"
#define FS_NO_GLOBALS
#include <ESP8266WiFi.h>
#include <Bounce2.h>


#define IO0 16// In1
#define IO1 5 // (SCL) In5
#define IO2 4 // (SDA) In4
#define IO3 0 // (10k pull-up) (RELAY 1)
#define IO4 2 // (10k pull-up, BUILTIN_LED)
#define IO5 14// (SCK) In6 
#define IO6 12// (MISO) In2
#define IO7 13// (MOSI) In3
#define IO8 15// (10k pull-down,SS) (Relay 2) 
//#define IOA A0//Analog 0 


#define IOSize  9
bool _debug = false;
int IOType[IOSize]{INPUT_PULLUP,INPUT_PULLUP,INPUT_PULLUP,OUTPUT,OUTPUT,INPUT_PULLUP,OUTPUT,INPUT_PULLUP,OUTPUT};
int IOMap[IOSize] {IO0,IO1,IO2,IO3,IO4,IO5,IO6,IO7,IO8};
int IO[IOSize];
Bounce Bnc[IOSize];
bool EventTriggeringEnabled = 1;



bool isOutPut(int IOP){
  return IOType[IOP] == OUTPUT; 
}

void ConfigIO(){
  if(loadIOConfig()){
    Serial.println("Setting IO");
    for (int i=0;i<IOSize;i++){
      if(IOType[i] == 0 ||IOType[i] == 2 || IOType[i] == 3){ //if it is an input
        Bnc[i].attach(IOMap[i],IOType[i]);
        Bnc[i].interval(5);
      }else{
        pinMode(IOMap[i],IOType[i]);
      }
    }
  }
}



TimeRelease IOReport;
TimeRelease IOTimer[9];
int reportInterval = 3000;



void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  delay(300);
  Serial.println(VERSIONINFO);
  Serial.println("Disabling Wifi");
  WiFi.mode(WIFI_OFF);    //The entire point of this project is to not use wifi but have directly connected IO for use with the D1SerialIO OctoPrint PlugIn and subPlugIn
  Serial.println("Start Types");
  reportIOTypes();
  Serial.println("End Types");
  InitStorageSetup();
  Serial.println("Initializing IO");
  ConfigIO();

  //need to get a baseline state for the inputs and outputs.
  for (int i=0;i<IOSize;i++){
    IO[i] = digitalRead(IOMap[i]);  
  }
  
  IOReport.set(100ul);
  
}
bool _pauseReporting = false;
void loop() {
  // put your main code here, to run repeatedly:
  checkSerial();
  if(!_pauseReporting){
    reportIO(checkInputs());
  }
}


void reportIO(bool forceReport){
  if (IOReport.check()||forceReport){
    Serial.print("IO:");
    for (int i=0;i<IOSize;i++){
      if(IOType[i] == 1 ){ //if it is an output
        IO[i] = digitalRead(IOMap[i]);  
      }
      Serial.print(IO[i]);
    }
    //Serial.print(analogRead(A0)); //future maybe
    Serial.println();
    IOReport.set(reportInterval);
    
  }
}

bool checkInputs(){
  bool changed = false;
  for (int i=0;i<IOSize;i++){
    Bnc[i].update();
    if(Bnc[i].changed()){
     changed = true;
     IO[i]=Bnc[i].read();
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
    
    String buf = Serial.readString();
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
      
    }    
    else if(command == "EIO"){ //this is the command meant to test for good connection.
      ack();
      _pauseReporting = true; //stops all IO reporting so it does not get in the way of a good confimable response.
      //Serial.print("Version ");
      //Serial.println(VERSIONINFO);
      //Serial.print("COMPATIBILITY ");
      //Serial.println(COMPATIBILITY);
      
    }

    
    else if (command == "IC") { //io count.
      ack();
      Serial.print("IC:");
      Serial.println(IOSize);
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
    }
    else if(command=="CIO"){ //set IO Configuration
      ack();
      if (validateNewIOConfig(value)){
        updateIOConfig(value);
      }
    }
    
    else if(command=="SIO"){
      ack();
      if(!saveIOConfig()){
        Serial.println("Failed to save config.");
      }
    }
    else if(command=="IOT"){
      ack();
      reportIOTypes();
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
        reportIO(true);
    }
    
    //Set AutoReporting Interval  
    else if(command =="SI" && value.length() > 0){
      ack();
      unsigned long newTime = value.toInt(); //will convert to a full long.
      if(newTime >=500){
        reportInterval = newTime;
      }else{
        Serial.println("ERROR: Value to small min 500ms");
      }
    }

    //Enable event trigger reporting Mostly used for E-Stop
    else if(command == "SE" && value.length() > 0){
      ack();
      EventTriggeringEnabled = value.toInt();
    }

    //Get States 
    else if(command == "GS"){
      ack();
      reportIO(true);
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
    return false;  
  }

  for (int i=0;i<IOSize;i++){
    int pointType = ioConfig.substring(i,i+1).toInt();
    if(pointType > 4){//cant be negative. we would have a bad parse on the number
      if(_debug){
        Serial.print("Bad IO Point type: index[");Serial.print(i);Serial.print("] type[");Serial.print(pointType);Serial.println("]");
      }
      return false;
    }
  }
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


bool loadIOConfig(){
  
if (!SPIFFS.exists("/IOConfig.json"))
  {
    Serial.println("[WARNING]: IOConfig file not found!");
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
    

  configfile.close();

  if (error)
  {
    Serial.println("[ERROR]: deserializeJson() error in loadIOConfig");
    Serial.println(error.c_str());
    return false;
  }

  return true;
}


bool saveIOConfig(){
  SPIFFS.remove("/IOConfig.json");
  File file = SPIFFS.open("/IOConfig.json", "w");

  DynamicJsonDocument doc(256);

  JsonObject configObj = doc.to<JsonObject>();
  
  configObj["IO0"]= getIOTypeString(IOType[0]);
  configObj["IO1"]= IOType[1];
  configObj["IO2"]= IOType[2];
  configObj["IO3"]= IOType[3];
  configObj["IO4"]= IOType[4];
  configObj["IO5"]= IOType[5];
  configObj["IO6"]= IOType[6];
  configObj["IO7"]= IOType[7];
  configObj["IO8"]= IOType[8];
    
  if (serializeJsonPretty(doc, file) == 0)
  {
    Serial.println("[WARNING]: Failed to write to file:/config/mqttconfig.json");
    return false;
  }
  file.close();
  
  return true;
}



String getIOTypeString(int ioType){
  if (ioType = 0){return "INPUT";}
  if (ioType = 1){return "OUTPUT";}
  if (ioType = 2){return "INPUT_PULLUP";}
  if (ioType = 3){return "INPUT_PULLDOWN";}
  if (ioType = 4){return "OUTPUT_OPEN_DRAIN";}
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


void InitStorageSetup(){
  SPIFFS.begin();
  //do anything that might be needed related to file and storage on startup.
}
