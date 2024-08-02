#include "Adafruit_LittleFS.h"
#include <InternalFileSystem.h>
#include <Adafruit_TinyUSB.h> // for Serial

using namespace Adafruit_LittleFS_Namespace;

bool cfgDebug = false; // delay of 3s is required after Serial.begin()

unsigned long echoID[100]; // message identifier
unsigned long echoTS[100]; // message time stamp
uint8_t MAXECHO = 5; // sets maximum bounces you will perform

File file(InternalFS);

// AT Command Variables
bool BTEN = 1; // Bluetooth Enabled
uint8_t BAUD = 3; // UART BAUD
bool DEBUG = 0;
bool FHSS = 1; // Frequency Hopping Spread Spectrum
uint8_t HC = 105; // LoRa Home Channel (FHSS=true)
float FREQ = 912.8; // LoRa Frequency (FHSS=false)
uint16_t HP = 25; // LoRa hopping period (FHSS=false)
float BW = 250.0; // LoRa bandwidth
uint8_t SF = 12; // LoRa Spreading Factor
uint8_t CR = 5; // LoRa coding rate
uint8_t SW = 0xDC; // LoRa Sync Word (0xDD)
uint8_t PRE = 12; // LoRa Preamble
uint8_t PWR = 20; // LoRa Power Level
uint8_t GAIN = 0; //LoRa GAIN
bool CRC = 1; // LoRa Cyclic Redundancy Check
uint8_t LED = 1; // 0=none, 1=con only, 2=con+fhss, 3=fhss only
bool ENAP = 0; // Enable all peripherals
// AT Command Variables

bool DEFICHAT = true;
bool ENCRYPT = true;
static uint8_t USER[16];
static uint8_t PASSPHR[64];

uint8_t IV[16];
uint8_t PASSKEY[16];

unsigned long available_baud[] = {
  1200, // (actual rate: 1205)     [0]
  2400, // (actual rate: 2396)     [1]
  4800, // (actual rate: 4808)     [2]
  9600, // (actual rate: 9598)     [3]
  14400, // (actual rate: 14414)   [4]
  19200, // (actual rate: 19208)   [5]
  28800, // (actual rate: 28829)   [6]
  31250, //                        [7]
  38400, // (actual rate: 38462)   [8]
  56000, // (actual rate: 55944)   [9]
  57600, // (actual rate: 57762)   [10]
  76800, // (actual rate: 76923)   [11]
  115200, // (actual rate: 115942) [12]
  230400, // (actual rate: 231884) [13]
  460800, // (actual rate: 470588) [14]
  921600, // (actual rate: 941176) [15]
  1000000 //                       [16]
};

void getConfig(){
  file.open("CONFIG", FILE_O_READ);
  
  // BTEN
    while(file.read() != '=');
    BTEN = file.readStringUntil('\n').toInt();
    if(cfgDebug){
      Serial.print("BTEN="); Serial.println(BTEN);
    }
    
    // BAUD
    while(file.read() != '=');
    BAUD = file.readStringUntil('\n').toInt();
    if(cfgDebug){
      Serial.print("BAUD="); Serial.println(BAUD);
    }
    
    // DEBUG
    while(file.read() != '=');
    DEBUG = file.readStringUntil('\n').toInt();
    if(cfgDebug){
      Serial.print("DEBUG="); Serial.println(DEBUG);
    }
    
    // FHSS
    while(file.read() != '=');
    FHSS = file.readStringUntil('\n').toInt();
    if(cfgDebug){
      Serial.print("FHSS="); Serial.println(FHSS);
    }
    
    // HC
    while(file.read() != '=');
    HC = file.readStringUntil('\n').toInt();
    if(cfgDebug){
      Serial.print("HC="); Serial.println(HC);
    }
    
    // FREQ
    while(file.read() != '=');
    FREQ = file.readStringUntil('\n').toFloat();
    if(cfgDebug){
      Serial.print("FREQ="); Serial.println(FREQ);
    }
    
    // HP
    while(file.read() != '=');
    HP = file.readStringUntil('\n').toInt();
    if(cfgDebug){
      Serial.print("HP="); Serial.println(HP);
    }
    
    // BW
    while(file.read() != '=');
    BW = file.readStringUntil('\n').toFloat();
    if(cfgDebug){
      Serial.print("BW="); Serial.println(BW);
    }
    
    // SF
    while(file.read() != '=');
    SF = file.readStringUntil('\n').toInt();
    if(cfgDebug){
      Serial.print("SF="); Serial.println(SF);
    }
    
    // CR
    while(file.read() != '=');
    CR = file.readStringUntil('\n').toInt();
    if(cfgDebug){
      Serial.print("CR="); Serial.println(CR);
    }
    
    // SW
    while(file.read() != '=');
    SW = file.readStringUntil('\n').toInt();
    if(cfgDebug){
      Serial.print("SW="); Serial.println(SW);
    }
    
    // PRE
    while(file.read() != '=');
    PRE = file.readStringUntil('\n').toInt();
    if(cfgDebug){
      Serial.print("PRE="); Serial.println(PRE);
    }
    
    // PWR
    while(file.read() != '=');
    PWR = file.readStringUntil('\n').toInt();
    if(cfgDebug){
      Serial.print("PWR="); Serial.println(PWR);
    }
    
    // GAIN
    while(file.read() != '=');
    GAIN = file.readStringUntil('\n').toInt();
    if(cfgDebug){
      Serial.print("GAIN="); Serial.println(GAIN);
    }
    
    // CRC
    while(file.read() != '=');
    CRC = file.readStringUntil('\n').toInt();
    if(cfgDebug){
      Serial.print("CRC="); Serial.println(CRC);
    }

    // LED
    while(file.read() != '=');
    LED = file.readStringUntil('\n').toInt();
    if(cfgDebug){
      Serial.print("LED="); Serial.println(LED);
    }

    // Enable all peripherals
    while(file.read() != '=');
    ENAP = file.readStringUntil('\n').toInt();
    if(cfgDebug){
      Serial.print("ENAP="); Serial.println(ENAP);
    }

    // ENCRYPT
    while(file.read() != '=');
    ENCRYPT = file.readStringUntil('\n').toInt();
    if(cfgDebug){
      Serial.print("ENCRYPT="); Serial.println(ENCRYPT);
    }

    // DEFICHAT
    while(file.read() != '=');
    DEFICHAT = file.readStringUntil('\n').toInt();
    if(cfgDebug){
      Serial.print("DEFICHAT="); Serial.println(DEFICHAT);
    }

    // MAXECHO
    while(file.read() != '=');
    MAXECHO = file.readStringUntil('\n').toInt();
    if(cfgDebug){
      Serial.print("MAXECHO="); Serial.println(MAXECHO);
    }
    
    file.close();
}

void setConfig(){
  // delete file
  InternalFS.remove("CONFIG");
  
  /*if(cfgDebug){
    Serial.print("Open " "CONFIG" " file to write ... ");
  }*/

    if( file.open("CONFIG", FILE_O_WRITE) )
    {
      /*if(cfgDebug){
        Serial.println("OK");
      }*/
      
      file.print("BTEN="); file.println(BTEN);
      file.print("BAUD="); file.println(BAUD);
      file.print("DEBUG="); file.println(DEBUG);
      file.print("FHSS="); file.println(FHSS);
      file.print("HC="); file.println(HC);
      file.print("FREQ="); file.println(FREQ);
      file.print("HP="); file.println(HP);
      file.print("BW="); file.println(BW);
      file.print("SF="); file.println(SF);
      file.print("CR="); file.println(CR);
      file.print("SW="); file.println(SW);
      file.print("PRE="); file.println(PRE);
      file.print("PWR="); file.println(PWR);
      file.print("GAIN="); file.println(GAIN);
      file.print("CRC="); file.println(CRC);
      file.print("LED="); file.println(LED);
      file.print("ENAP="); file.println(ENAP);
      file.print("ENCRYPT="); file.println(ENCRYPT);
      file.print("DEFICHAT="); file.println(DEFICHAT);
      file.print("MAXECHO="); file.println(MAXECHO);
      file.close();
    }else
    {
      if(cfgDebug){
        Serial.println("Failed to write config file!");
      }
    }    
}
