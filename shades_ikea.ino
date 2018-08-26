
/////////////////////////////////////////////////////////////////////////////
// Combine IKEA zigbee lightbrains with 3 mechanical switches for controlling
// electrical shades/doors --> 3 two way opto-relays
//
// TODO: Maybe use smoothing (running average) on Zigbee input PWM to make
// everything more responsive ...

#define DEBUG
#define TASTER

// interrupt library for connecting more than 2 interrupts to every digital pin
// #include <EnableInterrupt.h>
// Button library for wall switches
#include <OneButton.h>

// constants: frequency IKEA, num of shades, shade positions, etc.
const int PWM_FREQ = 600;
const int DRV_TIME_WIN = 21; // complete shutdown/open time in s default 20
const int DRV_TIME_DOR = 30; // complete shutdown/open time in s default 29
const int SHADES = 3; // number of motors/shades
const int HE10 = 60; // position 10 percent open
const int HE25 = 105; // position 25 percent open
const int HE50 = 150; // position 50 percent open
const int HE75 = 310; // position 75 percent open
const int HE90 = 440; // position 90 percent open
const int HE100 = 520; // position 100 percent open
const int COMF_TIME = 2200; // press switch > COMF_TIME and shades drive on after switch release
const int HYST = 20; // minimum of difference for Zigbee positions before new movement

// PIN TABLE OUTPUT RELAYS
int motorUP[SHADES] = {13, 12, 11}; 
int motorDOWN[SHADES] = {10, 9, 8};

// PIN TABLE INPUT IKEA styrelse
int inA[SHADES] = {0, 1, 2};

// PIN TABLE INPUT SWITCHES 3 MOTORS, UP and DOWN pin number for interrupts
#define switch0UP   5
#define switch0DOWN 2
#define switch1UP   6
#define switch1DOWN 3
#define switch2UP   7
#define switch2DOWN 4

// Button objects with internal pullup
OneButton btn_sw0UP(switch0UP, true);
OneButton btn_sw0DOWN(switch0DOWN, true);
OneButton btn_sw1UP(switch1UP, true);
OneButton btn_sw1DOWN(switch1DOWN, true);
OneButton btn_sw2UP(switch2UP, true);
OneButton btn_sw2DOWN(switch2DOWN, true);

// MOTOR LOGIC STATE
bool motorUP_isDriving[SHADES] = {false, false, false};
bool motorDOWN_isDriving[SHADES] = {false, false, false};
bool comf_enabled[SHADES] = {false, false, false};
unsigned long comfcheck_time[SHADES] = {0, 0, 0};
unsigned long stopcheck_time[SHADES] = {0, 0, 0};
bool now_ZigbeeDrive[SHADES] = {false, false, false};
bool now_SwitchDrive[SHADES] = {false, false, false};
      
// LOW/HIGH reverse possibility
int AN = LOW;
int AUS = HIGH;

// Debounced button presses
const int DEBOUNCE_TIME = 50; // time in ms
bool UP_isPressed[SHADES] = {false, false, false};
bool DOWN_isPressed[SHADES] = {false, false, false};

// actual logical position of shades (0.0 -> closed, 1.0 -> open)
float posShade[SHADES] = {1.0, 1.0, 1.0};
// last position of shades before moving
float lastPosShade[SHADES] = {1.0, 1.0, 1.0};
// position of Zigbee system
float posShadeZig[SHADES] = {1.0, 1.0, 1.0};
float last_posShadeZig[SHADES] = {1.0, 1.0, 1.0};


/////////////////////////////////////////////////////////////////////////////////
// BUTTONS

void checkSwitch(int i) {
  switch(i) {
    case 0: {
      btn_sw0UP.tick();
      btn_sw0DOWN.tick();
      break;
    }
    case 1: {
      btn_sw1UP.tick();
      btn_sw1DOWN.tick();
      break;
    }
    case 2: {
      btn_sw2UP.tick();
      btn_sw2DOWN.tick();
      break;
    }
    default: return;
    break;
  }
}

/////////////////////////////////////////////////////////////////////////////////
// switch/Taster 0 UP
void sw0_UP_pressed() {
  if(DOWN_isPressed[0]) return; // should be mechanically impossible but still ...
  UP_isPressed[0] = true;
  if(motorUP_isDriving[0]) return; 
  driveUp(0); 
}

void sw0_UP_released() {
  if((millis() - comfcheck_time[0] > COMF_TIME) && (motorUP_isDriving[0])) {
    comf_enabled[0] = true; // continue driving
    UP_isPressed[0] = false;
  }
  else {
    UP_isPressed[0] = false;
    stopMotor(0);
  }
}

void sw0_UP_clicked() {
  stopMotor(0);
}

// switch/Taster 0 DOWN
void sw0_DOWN_pressed() {
  if(UP_isPressed[0]) return;  // should be mechanically impossible but still ...
  DOWN_isPressed[0] = true;
  if(motorDOWN_isDriving[0]) return; 
  driveDown(0);  
}

void sw0_DOWN_released() {
  if((millis() - comfcheck_time[0] > COMF_TIME) && (motorDOWN_isDriving[0])) {
    comf_enabled[0] = true; // continue driving
    DOWN_isPressed[0] = false;
  }
  else {
    DOWN_isPressed[0] = false;
    stopMotor(0);
  }
}

void sw0_DOWN_clicked() {
  stopMotor(0);
}

/////////////////////////////////////////////////////////////////////////////////
// switch/Taster 1 UP
void sw1_UP_pressed() {
  if(DOWN_isPressed[1]) return; // should be mechanically impossible but still ...
  UP_isPressed[1] = true;
  if(motorUP_isDriving[1]) return; 
  driveUp(1); 
}

void sw1_UP_released() {
  if((millis() - comfcheck_time[1] > COMF_TIME) && (motorUP_isDriving[1])) {
    comf_enabled[1] = true; // continue driving
    UP_isPressed[1] = false;
  }
  else {
    UP_isPressed[1] = false;
    stopMotor(1);
  }
}

void sw1_UP_clicked() {
  stopMotor(1);
}

// switch/Taster 1 DOWN
void sw1_DOWN_pressed() {
  if(UP_isPressed[1]) return;  // should be mechanically impossible but still ...
  DOWN_isPressed[1] = true;
  if(motorDOWN_isDriving[1]) return; 
  driveDown(1);  
}

void sw1_DOWN_released() {
  if((millis() - comfcheck_time[1] > COMF_TIME) && (motorDOWN_isDriving[1])) {
    comf_enabled[1] = true; // continue driving
    DOWN_isPressed[1] = false;
  }
  else {
    DOWN_isPressed[1] = false;
    stopMotor(1);
  }
}

void sw1_DOWN_clicked() {
  stopMotor(1);
}

/////////////////////////////////////////////////////////////////////////////////
// switch/Taster 2 UP
void sw2_UP_pressed() {
  if(DOWN_isPressed[2]) return; // should be mechanically impossible but still ...
  UP_isPressed[2] = true;
  if(motorUP_isDriving[2]) return; 
  driveUp(2); 
}

void sw2_UP_released() {
  if((millis() - comfcheck_time[2] > COMF_TIME) && (motorUP_isDriving[2])) {
    comf_enabled[2] = true; // continue driving
    UP_isPressed[2] = false;
  }
  else {
    UP_isPressed[2] = false;
    stopMotor(2);
  }
}

void sw2_UP_clicked() {
  stopMotor(2);
}

// switch/Taster 2 DOWN
void sw2_DOWN_pressed() {
  if(UP_isPressed[2]) return;  // should be mechanically impossible but still ...
  DOWN_isPressed[2] = true;
  if(motorDOWN_isDriving[2]) return; 
  driveDown(2);  
}

void sw2_DOWN_released() {
  if((millis() - comfcheck_time[2] > COMF_TIME) && (motorDOWN_isDriving[2])) {
    comf_enabled[2] = true; // continue driving
    DOWN_isPressed[2] = false;
  }
  else {
    DOWN_isPressed[2] = false;
    stopMotor(2);
  }
}

void sw2_DOWN_clicked() {
  stopMotor(2);
}

// drive up motor mNum 
void driveUp(unsigned mNum) {
  // Drive motor UP
  stopMotor(mNum);
  now_SwitchDrive[mNum] = true;
  comfcheck_time[mNum] = millis();
  stopcheck_time[mNum] = millis();
  motorUP_isDriving[mNum] = true;
  digitalWrite(motorUP[mNum], AN);
  last_posShadeZig[mNum] = posShadeZig[mNum];
  now_ZigbeeDrive[mNum] = false;
}

// drive down motor mNum 
void driveDown(unsigned mNum) {
  // Drive motor DOWN
  stopMotor(mNum);
  now_SwitchDrive[mNum] = true;
  comfcheck_time[mNum] = millis();
  stopcheck_time[mNum] = millis();
  motorDOWN_isDriving[mNum]=true;
  digitalWrite(motorDOWN[mNum], AN);
  last_posShadeZig[mNum] = posShadeZig[mNum];
  now_ZigbeeDrive[mNum] = false;
}

// Stop motor mNum
void stopMotor(unsigned mNum) {
  comf_enabled[mNum] = false;
  lastPosShade[mNum] = posShade[mNum];
  digitalWrite(motorUP[mNum], AUS);
  digitalWrite(motorDOWN[mNum], AUS);
  motorUP_isDriving[mNum] = false;
  motorDOWN_isDriving[mNum] = false;
  now_SwitchDrive[mNum] = false;
}

// DEPRECATED comfort mode activation
void checkComfortMode(int mNum) {
  if(comf_enabled[mNum] && !(UP_isPressed[mNum] || DOWN_isPressed[mNum])) return; //comfort function already enabled -> do nothing
  if((UP_isPressed[mNum] || DOWN_isPressed[mNum]) && (motorUP_isDriving[mNum] || motorDOWN_isDriving[mNum]))
    if(millis() - comfcheck_time[mNum] > COMF_TIME)
      comf_enabled[mNum]=true;
}

// check if motor should be stopped 
void checkStop(int mNum) {
  if(now_ZigbeeDrive[mNum]) { // Zigbee is driving 
    if(motorUP_isDriving[mNum] && (posShade[mNum] >= posShadeZig[mNum])) {
      stopMotor(mNum);
    }
    if(motorDOWN_isDriving[mNum] && (posShade[mNum] <= posShadeZig[mNum])) {
      stopMotor(mNum);
    }
    posShade[mNum] = (posShade[mNum]<0.0) ? 0.0 : posShade[mNum];
    posShade[mNum] = (posShade[mNum]>1.0) ? 1.0 : posShade[mNum];
  }
  // calculate actual positions
  switch(mNum) {
    case 2 : {// DOOR takes longer TODO replace switch with DRV_TIME array
              if(motorUP_isDriving[mNum]) posShade[mNum] = lastPosShade[mNum] + (float) (millis() - stopcheck_time[mNum]) / (DRV_TIME_DOR*1000);
              if(motorDOWN_isDriving[mNum]) posShade[mNum] = (lastPosShade[mNum] - (float)(millis() - stopcheck_time[mNum]) / (DRV_TIME_DOR*920));
              if((posShade[mNum]>=1.0) || (posShade[mNum]<=0.0))
              {
                stopMotor(mNum);
              }
              break;
    }
    default: {
             if(motorUP_isDriving[mNum]) posShade[mNum] = lastPosShade[mNum] + (float) (millis() - stopcheck_time[mNum]) / (DRV_TIME_WIN*1000);
             if(motorDOWN_isDriving[mNum]) posShade[mNum] = (lastPosShade[mNum] - (float)(millis() - stopcheck_time[mNum]) / (DRV_TIME_WIN*920));
             if((posShade[mNum]>=1.0) || (posShade[mNum]<=0.0))
             {
               stopMotor(mNum);
             }
             break;
    }
  } // end switch
  posShade[mNum] = (posShade[mNum]<0.0) ? 0.0 : posShade[mNum];
  posShade[mNum] = (posShade[mNum]>1.0) ? 1.0 : posShade[mNum];
}

// check if Zigbee should start driving a motor
void checkZigbee(int mNum) {
  if(now_SwitchDrive[mNum]) return;  // switches are driving! don't interfere
  if(motorUP_isDriving[mNum] || motorDOWN_isDriving[mNum]) return;
  if(lastZigPosHasChanged(mNum)) {
    if(posShadeZig[mNum] > posShade[mNum])
      zigbeeDriveUp(mNum);
    if(posShadeZig[mNum] < posShade[mNum])
      zigbeeDriveDown(mNum);  
  }  
}

// has there been a change in Zigbee positions?
bool lastZigPosHasChanged(int mNum) {
  return(posShadeZig[mNum] != last_posShadeZig[mNum]);
}

// drive up motor mNum
void zigbeeDriveUp(int mNum) {
  stopMotor(mNum);  // just for safety
  stopcheck_time[mNum] = millis();
  motorUP_isDriving[mNum]=true;
  last_posShadeZig[mNum]=posShadeZig[mNum];
  digitalWrite(motorUP[mNum], AN);
  now_SwitchDrive[mNum]=false;
  now_ZigbeeDrive[mNum]=true;
}

// drive down motor mNum
void zigbeeDriveDown(int mNum) {
  stopMotor(mNum);  // just for safety
  stopcheck_time[mNum] = millis();
  motorDOWN_isDriving[mNum]=true;
  last_posShadeZig[mNum]=posShadeZig[mNum];
  digitalWrite(motorDOWN[mNum], AN);
  now_SwitchDrive[mNum]=false;
  now_ZigbeeDrive[mNum]=true;
}


// DEBUG FUNCTION prints position of shades via terminal
void printPos(float a, float b, float c) {
  float fRolPos[3];
  
  fRolPos[0]=a;
  fRolPos[1]=b;
  fRolPos[2]=c;
  
  String sRollo="";
  Serial.println("POSITION: =======================================");
  sRollo+=a;
  sRollo+=" ";
  sRollo+=b;
  sRollo+=" ";
  sRollo+=c;
  Serial.println(sRollo);
  sRollo="";
  for(int i=0; i<3; i++) {
    if(fRolPos[i]<1) sRollo += "#####  ";
    else sRollo+="       ";
  }
  Serial.println(sRollo);
  sRollo = "";
  for(int i=0; i<3; i++) {
    if(fRolPos[i]<0.9) sRollo+="#####  ";
    else sRollo+="       ";
  }
  Serial.println(sRollo);
  sRollo = "";
  for(int i=0; i<3; i++) {
    if(fRolPos[i]<0.75) sRollo+="#####  ";
    else sRollo+="       ";
  }
  Serial.println(sRollo);
  sRollo = "";
  for(int i=0; i<3; i++) {
    if(fRolPos[i]<0.5) sRollo+="#####  ";
    else sRollo+="       ";
  }
  Serial.println(sRollo);
  sRollo = "";
  for(int i=0; i<3; i++) {
    if(fRolPos[i]<0.25) sRollo+="#####  ";
    else sRollo+="       ";
  }
  Serial.println(sRollo);
  sRollo = "";
  for(int i=0; i<3; i++) {
    if(fRolPos[i]<0.1) sRollo+="#####  ";
    else sRollo+="       ";
  }
  Serial.println(sRollo);  
}


void setup() {
  // initialize serial communication at 115200 bits per second:
  Serial.begin(115200);
  // init switches
  for(int i=0; i<SHADES; i++) {
    pinMode(motorUP[i], OUTPUT);
    digitalWrite(motorUP[i], AN);
    pinMode(motorDOWN[i], OUTPUT);
    digitalWrite(motorDOWN[i], AUS);
    pinMode(inA[i], INPUT_PULLUP);
  }  
  delay(DRV_TIME_DOR*1000); // wait for largest shade to open
    
  for(int i=0; i<SHADES; i++) digitalWrite(motorUP[i], AUS);
  
  // register switches
#ifdef TASTER
  pinMode(switch0UP, INPUT_PULLUP);
  pinMode(switch0DOWN, INPUT_PULLUP);
  btn_sw0UP.setDebounceTicks(DEBOUNCE_TIME);
  btn_sw0DOWN.setDebounceTicks(DEBOUNCE_TIME);
  btn_sw0UP.setClickTicks(200);
  btn_sw0DOWN.setClickTicks(200);
  btn_sw0UP.setPressTicks(500);
  btn_sw0DOWN.setPressTicks(500);
  btn_sw0UP.attachClick(sw0_UP_clicked);
  btn_sw0DOWN.attachClick(sw0_DOWN_clicked);
  btn_sw0UP.attachLongPressStart(sw0_UP_pressed);
  btn_sw0DOWN.attachLongPressStart(sw0_DOWN_pressed);
  btn_sw0UP.attachLongPressStop(sw0_UP_released);
  btn_sw0DOWN.attachLongPressStop(sw0_DOWN_released);
  
  pinMode(switch1UP, INPUT_PULLUP);
  pinMode(switch1DOWN, INPUT_PULLUP);
  btn_sw1UP.setDebounceTicks(DEBOUNCE_TIME);
  btn_sw1DOWN.setDebounceTicks(DEBOUNCE_TIME);
  btn_sw1UP.setClickTicks(200);
  btn_sw1DOWN.setClickTicks(200);
  btn_sw1UP.setPressTicks(500);
  btn_sw1DOWN.setPressTicks(500);
  btn_sw1UP.attachClick(sw1_UP_clicked);
  btn_sw1DOWN.attachClick(sw1_DOWN_clicked);
  btn_sw1UP.attachLongPressStart(sw1_UP_pressed);
  btn_sw1DOWN.attachLongPressStart(sw1_DOWN_pressed);
  btn_sw1UP.attachLongPressStop(sw1_UP_released);
  btn_sw1DOWN.attachLongPressStop(sw1_DOWN_released);

  pinMode(switch2UP, INPUT_PULLUP);
  pinMode(switch2DOWN, INPUT_PULLUP);
  btn_sw2UP.setDebounceTicks(DEBOUNCE_TIME);
  btn_sw2DOWN.setDebounceTicks(DEBOUNCE_TIME);
  btn_sw2UP.setClickTicks(200);
  btn_sw2DOWN.setClickTicks(200);
  btn_sw2UP.setPressTicks(500);
  btn_sw2DOWN.setPressTicks(500);
  btn_sw2UP.attachClick(sw2_UP_clicked);
  btn_sw2DOWN.attachClick(sw2_DOWN_clicked);
  btn_sw2UP.attachLongPressStart(sw2_UP_pressed);
  btn_sw2DOWN.attachLongPressStart(sw2_DOWN_pressed);
  btn_sw2UP.attachLongPressStop(sw2_UP_released);
  btn_sw2DOWN.attachLongPressStop(sw2_DOWN_released);
#endif
  // now Zigbee should drive to its last known state
}

void loop() {
  // sum of spikes avg (integral) for PWM_FREQ
  long sumA[SHADES] = {0};
  #ifdef DEBUG
  #ifdef TASTER
  Serial.print("Taster_UP:  ");
  for(int i=0; i<3; i++) Serial.print(UP_isPressed[i]);
  Serial.print("  Taster_DOWN:  ");
  for(int i=0; i<3; i++) Serial.print(DOWN_isPressed[i]);
  Serial.println("");
  #endif
  Serial.print("MOTOR_UP drive signal:    ");
  for(int i=0; i<3; i++) Serial.print(motorUP_isDriving[i]);
  Serial.println("");
  Serial.print("MOTOR_DOWN drive signal:  ");
  for(int i=0; i<3; i++) Serial.print(motorDOWN_isDriving[i]);
  Serial.println("");
  #endif
  //calculate input signals from IKEA zigbee
  for(int i=0; i<PWM_FREQ; i++){
    for(int j=0; j<SHADES; j++){
       sumA[j] = sumA[j] + analogRead(inA[j]);
    }
  }
  for(int j=0; j<SHADES; j++){
       sumA[j] = sumA[j] / PWM_FREQ;
    }
  #ifdef DEBUG  
  Serial.println("Check: =======================================");
  Serial.print("A0  ");
  Serial.print(sumA[0]);  
  Serial.print("     A1  ");
  Serial.print(sumA[1]); 
  Serial.print("     A2  ");
  Serial.println(sumA[2]); 
  #endif
  
  // Define certain positions as fixed points for Zigbee wobbly positions
  for(int i=0; i<SHADES; i++) {
    
    posShadeZig[i]=1;
    // percent window coverage has to be fine tuned 
    if(sumA[i] < (HE100-(HE100-HE75)/2)) posShadeZig[i]=0.82;
    if(sumA[i] < (HE75-(HE75-HE50)/2)) posShadeZig[i]=0.62;
    if(sumA[i] < (HE50-(HE50-HE25)/2)) posShadeZig[i]=0.41;
    if(sumA[i] < (HE25/2)) posShadeZig[i]=0;
    
    #ifdef DEBUG
    Serial.print(posShadeZig[i]);
    Serial.print(" ");
    #endif
  }
  #ifdef DEBUG
  
  printPos(posShade[0],posShade[1],posShade[2]);  
  #endif
  for(int i=0; i<SHADES; i++) {
    checkZigbee(i);
    #ifdef TASTER
    checkSwitch(i);
    #endif
    #ifdef DEBUG
    Serial.print(" ComfMode:  ");
    Serial.print(comf_enabled[i]);
    #endif
    if(motorUP_isDriving[i] || motorDOWN_isDriving[i]) 
      checkStop(i);    
  }
  #ifdef DEBUG
  Serial.println("");
  #endif
}
