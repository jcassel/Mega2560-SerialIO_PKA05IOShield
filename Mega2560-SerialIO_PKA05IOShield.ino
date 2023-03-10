//Serial IO Driver
//Takes serial commands to control configured IO.

//Target outputs 
//Digital/relay: PSU,Lighting,Enclosure heater, alarm, filter relay,exaust fan
//Digital (PWM):cooling fan(PWM*) (Future)

//Target inputs
//Digital(5v/3.3v): General switches(NO or NC Selectable)
//temperature inputs(Future + TBA on type)


#define VERSIONINFO "MegaSerialIO_PKA05IOShield 1.0.4"
#define COMPATIBILITY "SIOPlugin 0.1.1"
#include "TimeRelease.h"
#include <Bounce2.h>
#include <EEPROM.h>

//6 digital inputs
//6 digital outputs
//6 analog inputs (not used)

#define IO0 2   // In1 
#define IO1 3   // In2
#define IO2 4   // In3
#define IO3 5   // In4
#define IO4 6   // In5
#define IO5 7   // In6

#define IO6 8   // Out1
#define IO7 9   // Out2
#define IO8 10  // Out3
#define IO9 11  // Out4
#define IO10 12 // Out5
#define IO11 13 // Out6

#define IOSize  12 

bool _debug = false;
//note that the board has pullups setup so you dont need to set that here. Just inputs and outputs 
int IOType[IOSize]{INPUT,INPUT,INPUT,INPUT,INPUT,INPUT,OUTPUT,OUTPUT,OUTPUT,OUTPUT,OUTPUT,OUTPUT}; //0-11
int IOMap[IOSize] {IO0,IO1,IO2,IO3,IO4,IO5,IO6,IO7,IO8,IO9,IO10,IO11};
int IO[IOSize];
Bounce Bnc[IOSize];
bool EventTriggeringEnabled = 1;


void StoreIOConfig(){
  if(_debug){Serial.println("Storing IO Config.");}
  int cs = 0;
  for (int i=0;i<IOSize;i++){
    EEPROM.update(i+1,IOType[i]); //store IO type map in eeprom but only if they are different... Will make it last a little longer but unlikely to really matter.:) 
    cs += IOType[i];
    if(_debug){Serial.print("Writing to EE pos:");Serial.print(i);Serial.print(" type:");Serial.println(IOType[i]);Serial.print("Current CS:");Serial.println(cs);}
  }
  EEPROM.update(0,cs); 
  if(_debug){Serial.print("Final Simple Check Sum:");Serial.println(cs);}
}


void FetchIOConfig(){
  int cs = EEPROM.read(0); //specifics of number is completely random. 
  int tempCS = 0;
  int IOTTmp[IOSize];
  

  for (int i=0;i<IOSize;i++){ //starting at 2 to avoide reading in first 2 IO points.
    IOTTmp[i] = EEPROM.read(i+1);//retreve IO type map from eeprom. 
    tempCS += IOTTmp[i];
  }
  
  //if(_debug){
    Serial.print("tempCS: ");Serial.println(tempCS);
  //}
      
  if(cs == tempCS){
    //if(_debug){
      Serial.println("Using stored IO set");
      //}
    for (int i=0;i<IOSize;i++){
      IOType[i] = IOTTmp[i];
    }
    return;  
  }

  //if(_debug){
    Serial.print("ALERT: Can't trust stored IO config. Using defaults. ");Serial.println(tempCS);
   // }
}



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
  Serial.println("Start Types");
  FetchIOConfig();
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
