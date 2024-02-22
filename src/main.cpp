#include <HIDPowerDevice.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h> // подключаем библиотеку для QAPASS 1602
#include "INA226.h"


#define MINUPDATEINTERVAL   26
#define MINUPDATEINTERVAL_ACCIDENT   600 // 600 сек

int iIntTimer=0;
int iIntTimerAccident=0;


INA226 INA(0x40);
LiquidCrystal_I2C LCD(0x27,16,2); // присваиваем имя LCD для дисплея


// String constants 
const char STRING_DEVICECHEMISTRY[] PROGMEM = "LiFePo4";
const char STRING_OEMVENDOR[] PROGMEM = "MyCoolUPS";
const char STRING_SERIAL[] PROGMEM = "UPS10"; 

const byte bDeviceChemistry = IDEVICECHEMISTRY;
const byte bOEMVendor = IOEMVENDOR;

uint16_t iPresentStatus = 0, iPreviousStatus = 0;

byte bRechargable = 1;
byte bCapacityMode = 2;  // units are in %%

// Physical parameters
const uint16_t iConfigVoltage = 1382;
uint16_t iVoltage =1300, iPrevVoltage = 0;
uint16_t iRunTimeToEmpty = 0, iPrevRunTimeToEmpty = 0;
uint16_t iAvgTimeToFull = 7200;
uint16_t iAvgTimeToEmpty = 7200;
uint16_t iRemainTimeLimit = 600;
int16_t  iDelayBe4Reboot = -1;
int16_t  iDelayBe4ShutDown = -1;

byte iAudibleAlarmCtrl = 2; // 1 - Disabled, 2 - Enabled, 3 - Muted


// Parameters for ACPI compliancy
const byte iDesignCapacity = 100;
byte iWarnCapacityLimit = 10; // warning at 10% 
byte iRemnCapacityLimit = 5; // low at 5% 
const byte bCapacityGranularity1 = 1;
const byte bCapacityGranularity2 = 1;
byte iFullChargeCapacity = 100;

byte iRemaining =0, iPrevRemaining=0;

int iRes=0;


void setup() {

  // Serial.begin(9600);


  Wire.begin();
  INA.setMaxCurrentShunt(50, 0.0015);
    
  PowerDevice.begin();

  // PowerDevice.setSerial(STRING_SERIAL); 
  // PowerDevice.setOutput(Serial);
  
  
  pinMode(4, INPUT_PULLUP); // ground this pin to simulate power failure. 
  pinMode(5, OUTPUT);  // output flushing 1 sec indicating that the arduino cycle is running. 
  pinMode(9, OUTPUT); // output is on once commuication is lost with the host, otherwise off.
  pinMode(10, OUTPUT); // реле

  PowerDevice.setFeature(HID_PD_PRESENTSTATUS, &iPresentStatus, sizeof(iPresentStatus));
  
  PowerDevice.setFeature(HID_PD_RUNTIMETOEMPTY, &iRunTimeToEmpty, sizeof(iRunTimeToEmpty));
  PowerDevice.setFeature(HID_PD_AVERAGETIME2FULL, &iAvgTimeToFull, sizeof(iAvgTimeToFull));
  PowerDevice.setFeature(HID_PD_AVERAGETIME2EMPTY, &iAvgTimeToEmpty, sizeof(iAvgTimeToEmpty));
  PowerDevice.setFeature(HID_PD_REMAINTIMELIMIT, &iRemainTimeLimit, sizeof(iRemainTimeLimit));
  PowerDevice.setFeature(HID_PD_DELAYBE4REBOOT, &iDelayBe4Reboot, sizeof(iDelayBe4Reboot));
  PowerDevice.setFeature(HID_PD_DELAYBE4SHUTDOWN, &iDelayBe4ShutDown, sizeof(iDelayBe4ShutDown));
  
  PowerDevice.setFeature(HID_PD_RECHARGEABLE, &bRechargable, sizeof(bRechargable));
  PowerDevice.setFeature(HID_PD_CAPACITYMODE, &bCapacityMode, sizeof(bCapacityMode));
  PowerDevice.setFeature(HID_PD_CONFIGVOLTAGE, &iConfigVoltage, sizeof(iConfigVoltage));
  PowerDevice.setFeature(HID_PD_VOLTAGE, &iVoltage, sizeof(iVoltage));

  PowerDevice.setStringFeature(HID_PD_IDEVICECHEMISTRY, &bDeviceChemistry, STRING_DEVICECHEMISTRY);
  PowerDevice.setStringFeature(HID_PD_IOEMINFORMATION, &bOEMVendor, STRING_OEMVENDOR);

  PowerDevice.setFeature(HID_PD_AUDIBLEALARMCTRL, &iAudibleAlarmCtrl, sizeof(iAudibleAlarmCtrl));

  PowerDevice.setFeature(HID_PD_DESIGNCAPACITY, &iDesignCapacity, sizeof(iDesignCapacity));
  PowerDevice.setFeature(HID_PD_FULLCHRGECAPACITY, &iFullChargeCapacity, sizeof(iFullChargeCapacity));
  PowerDevice.setFeature(HID_PD_REMAININGCAPACITY, &iRemaining, sizeof(iRemaining));
  PowerDevice.setFeature(HID_PD_WARNCAPACITYLIMIT, &iWarnCapacityLimit, sizeof(iWarnCapacityLimit));
  PowerDevice.setFeature(HID_PD_REMNCAPACITYLIMIT, &iRemnCapacityLimit, sizeof(iRemnCapacityLimit));
  PowerDevice.setFeature(HID_PD_CPCTYGRANULARITY1, &bCapacityGranularity1, sizeof(bCapacityGranularity1));
  PowerDevice.setFeature(HID_PD_CPCTYGRANULARITY2, &bCapacityGranularity2, sizeof(bCapacityGranularity2));

  // дисплей
  LCD.init(); // инициализация LCD дисплея
  LCD.backlight(); // включение подсветки дисплея
   
}

void show(int iRes, bool bDischarging){
  float voltage = INA.getBusVoltage();
  float current = INA.getCurrent();
  float power = current * 12.2;

  const String status = bDischarging ? "OB" : "OL" ;

  LCD.clear();

  LCD.setCursor(0, 0);     // ставим курсор на 1 символ первой строки
  LCD.print(String(round(fmin(iRemaining, 100))) + "% " + String(voltage) + "V" + " " + status + " " + String(iPresentStatus));    

  LCD.setCursor(0, 1);        // ставим курсор на 1 символ второй строки
  LCD.print(String(round(power * 10) / 10) + "W " + String(current) + "A r:" + String(iRes));     
}


int b = 0;


bool bAccident = false;

void loop() {

  //*********** Measurements Unit ****************************
  bool bCharging = digitalRead(4);
  bool bACPresent = bCharging;    // TODO - replace with sensor
  bool bDischarging = !bCharging; // TODO - replace with sensor

  float maxV = 13.8;
  float minV = 11.2;


  float current = INA.getBusVoltage();
  float currentV = current >= minV ? current : minV;

  iRemaining = (byte)(round((float)100*(currentV - minV)/(maxV - minV)));
  iRunTimeToEmpty = (uint16_t)round((float)iAvgTimeToEmpty*iRemaining/100);
  
    // Charging
  if(bCharging) 
    bitSet(iPresentStatus,PRESENTSTATUS_CHARGING);
  else
    bitClear(iPresentStatus,PRESENTSTATUS_CHARGING);
  if(bACPresent) 
    bitSet(iPresentStatus,PRESENTSTATUS_ACPRESENT);
  else
    bitClear(iPresentStatus,PRESENTSTATUS_ACPRESENT);
  if(iRemaining == iFullChargeCapacity) 
    bitSet(iPresentStatus,PRESENTSTATUS_FULLCHARGE);
  else 
    bitClear(iPresentStatus,PRESENTSTATUS_FULLCHARGE);
    
  // Discharging
  if(bDischarging) {
    iIntTimerAccident = 0;
    bAccident = true;
    bitSet(iPresentStatus,PRESENTSTATUS_DISCHARGING);
    // if(iRemaining < iRemnCapacityLimit) bitSet(iPresentStatus,PRESENTSTATUS_BELOWRCL);
    
    if(iRunTimeToEmpty < iRemainTimeLimit) 
      bitSet(iPresentStatus, PRESENTSTATUS_RTLEXPIRED);
    else
      bitClear(iPresentStatus, PRESENTSTATUS_RTLEXPIRED);

  }
  else {
    bitClear(iPresentStatus,PRESENTSTATUS_DISCHARGING);
    bitClear(iPresentStatus, PRESENTSTATUS_RTLEXPIRED);
  }

  // Shutdown requested
  if(iDelayBe4ShutDown > 0 ) {
      bitSet(iPresentStatus, PRESENTSTATUS_SHUTDOWNREQ);
      // Serial.println("shutdown requested");
  }
  else
    bitClear(iPresentStatus, PRESENTSTATUS_SHUTDOWNREQ);

  // Shutdown imminent
  if((iPresentStatus & (1 << PRESENTSTATUS_SHUTDOWNREQ)) || 
     (iPresentStatus & (1 << PRESENTSTATUS_RTLEXPIRED))) {
    bitSet(iPresentStatus, PRESENTSTATUS_SHUTDOWNIMNT);
    // Serial.println("shutdown imminent");
  }
  else
    bitClear(iPresentStatus, PRESENTSTATUS_SHUTDOWNIMNT);


  
  bitSet(iPresentStatus ,PRESENTSTATUS_BATTPRESENT);

  

  //************ Delay ****************************************  
  delay(500);
  iIntTimer++;
  delay(500);
  iIntTimer++;


  //************ Check if we are still online ******************

  

  //************ Bulk send or interrupt ***********************

  if((iPresentStatus != iPreviousStatus) || (iRemaining != iPrevRemaining) || (iRunTimeToEmpty != iPrevRunTimeToEmpty) || (iIntTimer>MINUPDATEINTERVAL) ) {

    PowerDevice.sendReport(HID_PD_REMAININGCAPACITY, &iRemaining, sizeof(iRemaining));
    if(bDischarging) PowerDevice.sendReport(HID_PD_RUNTIMETOEMPTY, &iRunTimeToEmpty, sizeof(iRunTimeToEmpty));
    iRes = PowerDevice.sendReport(HID_PD_PRESENTSTATUS, &iPresentStatus, sizeof(iPresentStatus));

    if(iRes <0 ) {
      digitalWrite(9, HIGH);
    }
    else {
      digitalWrite(9, LOW);
    }
        
    iIntTimer = 0;
    iPreviousStatus = iPresentStatus;
    iPrevRemaining = iRemaining;
    iPrevRunTimeToEmpty = iRunTimeToEmpty;
  }

  // show(iRes,bDischarging);

  float voltageV = INA.getBusVoltage();
  float currentA = INA.getCurrent();
  float power = currentA * 12.2;

  const String status = bDischarging ? "OB" : "OL" ;
  LCD.clear();
  LCD.setCursor(0, 0);     // ставим курсор на 1 символ первой строки
  LCD.print(String(round(fmin(iRemaining, 100))) + "% " + String(voltageV) + "V" + " " + status + " " + String(iPresentStatus));    
  LCD.setCursor(0, 1);        // ставим курсор на 1 символ второй строки
  // LCD.print(String(round(power * 10) / 10) + "W " + String(currentA) + "A r:" + String(iRes));    
  LCD.print(String(round(power * 10) / 10) + "W " + String(currentA) + "A " + String(MINUPDATEINTERVAL_ACCIDENT - iIntTimerAccident));     

  if(bAccident && bACPresent){ // если было отключение и эл снова подали
    iIntTimerAccident++;
    if ((iIntTimerAccident>MINUPDATEINTERVAL_ACCIDENT) || power < 5){ // если нагрузка не отключается в течении 10 минут или наоборот отключилась, то сбрасываем bAccident
      bAccident = false;
      iIntTimerAccident = 0;
      if(power < 5) { // если нагрузка упала - включаем реле на 10 сек, оно разорвет цепь питания и потом восстановит
        delay(10000);
        digitalWrite(10, HIGH);
        delay(10000);
        digitalWrite(10, LOW);
      }
    }
  }

  // if(bAccident && power < 5){
  //   delay(10000);
  //   digitalWrite(10, LOW);
  //   if(bACPresent) {
  //     delay(10000);
  //     digitalWrite(10, HIGH);
  //     bAccident = false;
  //   }
  // }


  // LCD.clear();
  // delay(100);

  // uint8_t raportID[10]={0};
  // const int len1 = PowerDevice.receive(&raportID,sizeof(raportID));
  // LCD.setCursor(0, 0);    
  // LCD.print(len1);   
  // for(int i=0; i<7; i++){
  //   LCD.setCursor(i+2, 0);    
  //   LCD.print(raportID[i]);    
  // }


  // uint8_t raw[10]={0}; 
  // const int len2 = PowerDevice.receive(&raw,sizeof(raw));
  
  // LCD.setCursor(0, 1);    
  // LCD.print(len2);   
  // for(int i=0; i<7; i++){
  //   LCD.setCursor(i+2, 1);    
  //   LCD.print(raw[i]);    
  // }
  // if (PowerDevice.available())         //anything out there???
	//  {
  //   LCD.setCursor(0, 0);    
  //   LCD.print("available");   
		 
	//  } else {

  //   LCD.setCursor(0, 0);    
  //   LCD.print("not available");   
  //  }
  
  // Serial.println(iRemaining);
  // Serial.println(iRunTimeToEmpty);
  // Serial.println(iRes);
  
}