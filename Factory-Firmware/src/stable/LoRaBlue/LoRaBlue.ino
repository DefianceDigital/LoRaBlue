// debug appears to throw off the ability to hop channels correctly when echoing

#include "config.h"
#include "help.h"
#include <bluefruit.h>
#include <Adafruit_LittleFS.h>
#include <InternalFileSystem.h>

#include "nrf_nvic.h"
#include "nrf_soc.h"

//#include "base64.hpp"

#include "Adafruit_nRFCrypto.h"
nRFCrypto_Hash hash;
#include "nRF_AES.h"
nRFCrypto_AES aes;
nRFCrypto_Random rnd;

#include <RadioLib.h>

// SX1278 has the following connections:
// NSS pin:   10
// DIO0 pin:  2
// RESET pin: 9
// DIO1 pin:  3
#define NRST 13
SX1276 radio = new Module(12, A0, NRST, A1);

// BLE Service
BLEDfu  bledfu;  // OTA DFU service
BLEDis  bledis;  // device information
BLEUart bleuart; // uart over ble
BLEBas  blebas;  // battery

float version = 1.25;
#define MTU 247
uint8_t periph = 0; //used to respond on appropriate peripheral
volatile bool requestReceipt = false; // if we're echoing a message, we want or need a response

// These settings have been painstakingly tested for best results
// Essentially you can just NEVER EVER have a hopping period of less than 12. Even if it appears to work, it doesn't
// range(%)           {41%   58%   82%   100%  }
// range(mi)          {1     1.5   2     2.5   } (With one upstairs)
// seconds            {0.5   1.7   8.5   17    }
float bwIndex[5] =    {500,  250,  62.5, 31.25 }; 
uint16_t hpIndex[5] = {255,  125,  25,   12,   };    
uint8_t sfIndex[5] =  {9,    10,   10,   10,   };    
uint8_t crIndex[5] =  {8,    8,    8,    8,    };
// Link Budget        {143,  149,  155,  158.4 }
// Rec. Sensitivity   {-123, -129, -135, -138.4}
// LB-RS              {266,  278,  290,  296.8 }

volatile bool transmittedFlag = false;
volatile bool receivedFlag = false;
volatile bool fhssChangeFlag = false;
volatile bool timeoutFlag = false;
volatile bool detectedFlag = false;
static float channels[128] = {902.20,902.40,902.60,902.80,903.00,903.20,903.40,903.60,903.80,904.00,904.20,904.40,904.60,904.80,905.00,905.20,905.40,905.60,905.80,906.00,906.20,906.40,906.60,906.80,907.00,907.20,907.40,907.60,907.80,908.00,908.20,908.40,908.60,908.80,909.00,909.20,909.40,909.60,909.80,910.00,910.20,910.40,910.60,910.80,911.00,911.20,911.40,911.60,911.80,912.00,912.20,912.40,912.60,912.80,913.00,913.20,913.40,913.60,913.80,914.00,914.20,914.40,914.60,914.80,915.00,915.20,915.40,915.60,915.80,916.00,916.20,916.40,916.60,916.80,917.00,917.20,917.40,917.60,917.80,918.00,918.20,918.40,918.60,918.80,919.00,919.20,919.40,919.60,919.80,920.00,920.20,920.40,920.60,920.80,921.00,921.20,921.40,921.60,921.80,922.00,922.20,922.40,922.60,922.80,923.00,923.20,923.40,923.60,923.80,924.00,924.20,924.40,924.60,924.80,925.00,925.20,925.40,925.60,925.80,926.00,926.20,926.40,926.60,926.80,927.00,927.20,927.40,927.60};
int numberOfChannels = 128;
int hopsCompleted = 0;
int packetCounter = 0;
int transmissionState = RADIOLIB_ERR_NONE;

volatile bool queueWaiting = false;
uint8_t pendingRaw[MTU];
unsigned char pendingMsg[208];
unsigned char pendingEnc;
unsigned char pendingEch;
unsigned char pendingMid[4];
unsigned long pendingTs;

unsigned long queueDelay = 0xFFFFFFFF;
void queueMsg(const char msg[208], unsigned char enc, unsigned char ech, unsigned char mid[4], unsigned long ts){  
  queueDelay = millis(); // no delay required since it's our message
  memcpy(pendingMsg, msg, sizeof(pendingMsg));
  pendingEnc = enc;
  pendingEch = ech;
  memcpy(pendingMid, mid, sizeof(pendingMid));
  pendingTs = ts;
}
void queueRaw(uint8_t msg[MTU], unsigned long mid, unsigned long ts){
  int randomDelay = (radio.randomByte() * 20) + millis(); // 0-5.1 seconds to try and avoid everybody echoing at once
  queueDelay = randomDelay;
  memcpy(pendingRaw, msg, sizeof(pendingRaw));
}

void setTxFlag(void) {
  transmittedFlag = true;
}
void setRxFlag(void) {
  receivedFlag = true;
}
void setFHSSFlag(void) {
  fhssChangeFlag = true;
}
void setFlagTimeout(void) {
  timeoutFlag = true;
}
void setFlagDetected(void) {
  detectedFlag = true;
}
unsigned long toaStart = 0;
unsigned long toaStop = 0;
unsigned long toaMax = 0;

volatile int interruptCounter;

String errorCode[] = {
  "NONE",
  "UNKNOWN_ERROR",
  "CHIP_NOT_FOUND",
  "MEMORY_ALLOCATION_FAILED",
  "PACKET_TOO_LONG",
  "TX_TIMEOUT",
  "RX_TIMEOUT",
  "CRC_MISMATCH",
  "INVALID_BANDWIDTH",
  "INVALID_SPREADING_FACTOR",
  "INVALID_CODING_RATE",
  "INVALID_BIT_RANGE",
  "INVALID_FREQUENCY",
  "INVALID_OUTPUT_POWER",
  "PREAMBLE_DETECTED",
  "CHANNEL_FREE",
  "SPI_WRITE_FAILED",
  "INVALID_CURRENT_LIMIT",
  "INVALID_PREAMBLE_LENGTH ",
  "INVALID_GAIN",
  "WRONG_MODEM",
  "INVALID_NUM_SAMPLES",
  "INVALID_RSSI_OFFSET",
  "INVALID_ENCODING",
  "LORA_HEADER_DAMAGED",
  "UNSUPPORTED",
  "INVALID_DIO_PIN",
  "INVALID_RSSI_THRESHOLD",
  "NULL_POINTER"
};

unsigned long lastHop; // used to return to home channel if gets stuck in a random hop
bool connected = false;
#define SWITCH 4 // SW pin

volatile bool wakeUp = false;

void isrButton()
{
    wakeUp = true;
}

void setup()
{ 
  USBDevice.setID(0x1915, 0x521F); // vid/pid
  USBDevice.setManufacturerDescriptor("Defiance Digital");
  USBDevice.setProductDescriptor("LoRaBlue");

  bool ledStatus = true;
  //pinMode(LED_BUILTIN, OUTPUT); // output by default
  digitalWrite(LED_BUILTIN, HIGH);

  if(!Serial){
    Serial.begin(115200);
  }

  delay(1000);
  if ( !Serial ) yield();

  // Initialize Internal File System
  InternalFS.begin();
  //InternalFS.format(); // erase all internal data

  //InternalFS.remove("USER");
  //InternalFS.remove("PASSPHR");

  // PAIRING PIN (onlsty way to make it work since it can be changed)
  file.open("PIN", FILE_O_READ);
  if ( !file )
  {
    if( file.open("PIN", FILE_O_WRITE) )
    {
      file.print("123456");
      file.close();
    }
  } 

  // NAME (only way to make it work since it can be changed)
  file.open("NAME", FILE_O_READ);
  if ( !file )
  {
    if( file.open("NAME", FILE_O_WRITE) )
    {
      file.print("LoRaBlue");
      file.close();
    }
  }

  // USER
  file.open("USER", FILE_O_READ);
  if ( file )
  {
    memset(USER, 0, sizeof(USER));
    for(uint8_t i = 0; i < sizeof(USER); i++){
      if(file.available()){
        USER[i] = file.read();
      }
    }
    file.close();
  } else
  {
    if( file.open("USER", FILE_O_WRITE) )
    {
      file.print("ANONYMOUS");
      file.close();
    }
  }

  file.open("PASSPHR", FILE_O_READ);
  if ( file )
  {
    for(uint8_t i = 0; i < sizeof(PASSPHR); i++){
      if(file.available()){
        PASSPHR[i] = file.read();
      }
    }
    file.close();
  } else
  {
    if( file.open("PASSPHR", FILE_O_WRITE) )
    {
      file.print("open sesame");
      file.close();
    }
  }

  // PASSKEY
  //delay(3000); // necessary only for PASSPHR info to print
  sha256((uint8_t*)PASSPHR);
  
  // CONFIG FILE
  file.open("CONFIG", FILE_O_READ);
  if ( file )
  {
    getConfig();
  } else {
    setConfig();
    getConfig();
    //Serial.println("File not found");
  }

  // flash delay
  for(uint8_t i = 0; i < 10; i++){
    ledStatus = ledStatus^1; // toggle status
    digitalWrite(LED_BUILTIN, ledStatus);
    delay(100);
  }

  Serial1.begin(available_baud[BAUD]);

  // flash delay
  for(uint8_t i = 0; i < 20; i++){
    ledStatus = ledStatus^1; // toggle status
    digitalWrite(LED_BUILTIN, ledStatus);
    delay(100);
  }
  
    int state;
    if(FHSS){
      state = radio.begin(/*freq*/ channels[HC],/*bw*/ BW,/*sf*/ SF, /*cr*/ CR, /*sw*/ SW, /*pwr*/ PWR, /*pre*/ PRE, /*gain*/ GAIN); 
    } else {
      state = radio.begin(/*freq*/ FREQ,/*bw*/ BW,/*sf*/ SF, /*cr*/ CR, /*sw*/ SW, /*pwr*/ PWR, /*pre*/ PRE, /*gain*/ GAIN); 
    }
    if (state == RADIOLIB_ERR_NONE) {
      state = radio.setCRC(CRC); // doesn't seem to actually work ?
      if (state == RADIOLIB_ERR_NONE) {
        if(FHSS){
          state = radio.setFHSSHoppingPeriod(HP);
          //radio.setDio1Action(setFHSSFlag, RISING);
        }
        if (state == RADIOLIB_ERR_NONE) {
          //radio.setDio0Action(setRxFlag, RISING);
          //state = radio.startReceive();
          if (state == RADIOLIB_ERR_NONE) {
            //Serial.println("AT+STARTUP=OK");
            //Serial1.println("AT+STARTUP=OK");
          } else {
            //Serial.println("AT+STARTUP=" + errorCode[abs(state)]);
            //Serial1.println("AT+STARTUP=" + errorCode[abs(state)]);
          }
        } else {
          //Serial.println("AT+STARTUP=" + errorCode[abs(state)]);
          //Serial1.println("AT+STARTUP=" + errorCode[abs(state)]);
          //while (true);
        }
      } else {
        //Serial.println("AT+STARTUP=" + errorCode[abs(state)]);
        //Serial1.println("AT+STARTUP=" + errorCode[abs(state)]);
        //while (true);
     
      }
    } else {
      //Serial.println("AT+STARTUP=" + errorCode[abs(state)]);
      //Serial1.println("AT+STARTUP=" + errorCode[abs(state)]);
      //while (true);
    }

    // reset the counter
  hopsCompleted = 0;
  if(FHSS){
    // return to home channel before the next transaction
    int state = radio.setFrequency(channels[HC]);
    if (state != RADIOLIB_ERR_NONE) {
      Serial.print(F("[LoRa] Failed to change frequency, code "));
      Serial.println(errorCode[abs(state)]);
    }

    // set the function to call when we need to change frequency
    radio.setDio1Action(setFHSSFlag, RISING);
  }
    
    radio.setDio0Action(setRxFlag, RISING);
    state = radio.startReceive();

    // Setup the BLE LED to be enabled on CONNECT
    // Note: This is actually the default behavior, but provided
    // here in case you want to control this LED manually via PIN 19
    Bluefruit.autoConnLed(false);

    // Config the peripheral connection with maximum bandwidth 
    // more SRAM required by SoftDevice
    // Note: All config***() function must be called before begin()
    Bluefruit.configPrphBandwidth(BANDWIDTH_MAX); // BANDWIDTH_MAX allows upt to 247 bytes

  Bluefruit.begin();  
  Bluefruit.setTxPower(8);    // Check bluefruit.h for supported values (default=4 max=8)

  file.open("PIN", FILE_O_READ);
  Bluefruit.Security.setPIN(file.readString().c_str());

  file.open("NAME", FILE_O_READ);
  Bluefruit.setName(file.readString().c_str());
  
  Bluefruit.Periph.setConnectCallback(connect_callback);
  Bluefruit.Periph.setDisconnectCallback(disconnect_callback);

  // trying to make bluetooth stay connected
  /*while ( !Serial ) yield(); // wait for serial (will not work without it)
  Bluefruit.Periph.setConnSupervisionTimeout(0);
  Bluefruit.Periph.setConnInterval(0,0);
  Bluefruit.Periph.printInfo();*/

  // To be consistent OTA DFU should be added first if it exists
  //bledfu.begin();

  // Configure and Start Device Information Service
  bledis.setManufacturer("Defiance Digital");
  bledis.setModel("LoRaBlue");
  bledis.begin();

  // Configure and Start BLE Uart Service
    // Set Permission to access BLE Uart is to require man-in-the-middle protection
    // This will cause central to perform pairing with static PIN we set above
    //Serial.println("Configure BLE Uart to require man-in-the-middle protection for PIN pairing");
    bleuart.setPermission(SECMODE_ENC_WITH_MITM, SECMODE_ENC_WITH_MITM);
    bleuart.begin();

  // Start BLE Battery Service
  //blebas.begin();
  //blebas.write(100);

  // Set up and start advertising
  startAdv();

  while(Serial.available()){
      Serial.read();
    }
    while(Serial1.available()){
      Serial1.read();
    }
    while(bleuart.available()){
      bleuart.read();
    }
    if((LED > 0) && (LED < 3)){
      if(connected == false){
        digitalWrite(LED_BUILTIN, LOW); // for some reason the led needs to be turned off here or it will be on even though it's not connected
      }
    } else {
      digitalWrite(LED_BUILTIN, LOW);
    }

    pinMode(2, OUTPUT); // led2
    if(LED > 1){
      digitalWrite(2, HIGH);
    } else {
      digitalWrite(2, LOW);
    }

    /*Serial.print("BNDUSER: ");
    Serial.println((char*)BNDUSER);
    Serial.print("BNDUSER: ");
  for(uint8_t i = 0; i < sizeof(BNDUSER); i++){
    if(BNDUSER[i] < 0x10){
      Serial.print("0");
    }
    Serial.print(BNDUSER[i], HEX);
  }
  Serial.println();*/

  SaSi_LibInit();
    aes.begin();
    rnd.begin();
    //transmit("This is a test message\n");
    
    Scheduler.startLoop(atLoop); // monitors serial and bluetooth data
    Scheduler.startLoop(queueLoop); // handles waiting echoes

    pinMode(SWITCH, INPUT_PULLUP);

    Serial.println("AT+STARTUP=OK");
            Serial1.println("AT+STARTUP=OK");
}

void startAdv(void)
{
  // Advertising packet
  Bluefruit.Advertising.addFlags(BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE);
  Bluefruit.Advertising.addTxPower();

  // Include bleuart 128-bit uuid
  Bluefruit.Advertising.addService(bleuart);

  // Secondary Scan Response packet (optional)
  // Since there is no room for 'Name' in Advertising packet
  Bluefruit.ScanResponse.addName();
  
  /* Start Advertising
   * - Enable auto advertising if disconnected
   * - Interval:  fast mode = 20 ms, slow mode = 152.5 ms
   * - Timeout for fast mode is 30 seconds
   * - Start(timeout) with timeout = 0 will advertise forever (until connected)
   * 
   * For recommended advertising interval
   * https://developer.apple.com/library/content/qa/qa1931/_index.html   
   */
  Bluefruit.Advertising.restartOnDisconnect(true);
  Bluefruit.Advertising.setInterval(32, 244);    // in unit of 0.625 ms
  Bluefruit.Advertising.setFastTimeout(30);      // number of seconds in fast mode
  Bluefruit.Advertising.start(0);                // 0 = Don't stop advertising after n seconds  
}

void queueLoop(){
  if(millis() > queueDelay){
    queueDelay = 0xFFFFFFFF;
    queueWaiting = true;

    // disable led so it doesn't show scans
    //digitalWrite(2, LOW);

    radio.setDio0Action(setFlagTimeout, RISING);
    radio.setDio1Action(setFlagDetected, RISING);
    radio.startChannelScan();
  }
  if(queueWaiting){
    //  handle queue
    if(detectedFlag || timeoutFlag) {
      if(detectedFlag) {
        // preamble detected
        timeoutFlag = false;
        detectedFlag = false;
        radio.startChannelScan();
      } else {
        // channel free
        queueWaiting = false;
        timeoutFlag = false;
        detectedFlag = false;

        // enable led to show hops (now that we're not scanning anymore)
        /*if(LED > 1){
          digitalWrite(2, HIGH);
        }*/

        if(requestReceipt){
          transmit((char*)pendingMsg, pendingEnc, pendingEch, (unsigned char*)pendingMid, pendingTs);
        } else { // if echo
          rawTransmit((uint8_t*)pendingRaw, sizeof(pendingRaw));
        }
      }
    }
  }
}

void atLoop(){
  if (Serial.available())
  {
    char msgOut[MTU];
    memset(msgOut, 0, sizeof(msgOut));
    periph = 0;
    delay(100);
    // Delay to wait for enough input, since we have a limited transmission buffer
    uint8_t len = Serial.available();
    if(len > 208){
      len = 208;
    }
    uint8_t buf[208];
    memset(buf, 0, sizeof(buf));
    int count = Serial.readBytes(buf, sizeof(buf));
    while(Serial.available()){
      Serial.read(); // trash anything over max length
    }
    if(strncmp((char*)buf, (char*)"AT+", 3) == 0){ // this should process AT commands
      callCMD(buf, len).toCharArray(msgOut, sizeof(msgOut));
      //Serial.print(callCMD(buf, len));
    } else { // this should send to LoRa
      //Serial.write(buf, sizeof(buf)); //Serial.println();
      if(DEBUG){
        strcat(msgOut, "[LoRa] Transmitting...\n");
      }
      if(DEFICHAT){
        requestReceipt = true; // we want a success/fail response on this transmission
        unsigned char newID[4];
        newID[0] = radio.randomByte(); // used to make echoID
        newID[1] = radio.randomByte(); // used to make echoID
        newID[2] = radio.randomByte(); // used to make echoID
        newID[3] = radio.randomByte(); // used to make echoID
        queueMsg((char*)buf, (unsigned char)ENCRYPT, 0, newID, millis());
      } else {
        rawTransmit(buf, len);
      }
    }
    exportData(msgOut, strlen(msgOut));
  }

  if (Serial1.available())
  {
    char msgOut[MTU];
    memset(msgOut, 0, sizeof(msgOut));
    //Serial.print("Serial1 Available: "); Serial.println(Serial1.available());
    periph = 1;
    delay(100);
    uint8_t len = Serial1.available();
    if(len > 208){
      len = 208;
    }
    uint8_t buf[208];
    memset(buf, 0, sizeof(buf));
    int count = Serial1.readBytes(buf, sizeof(buf));
    while(Serial1.available()){
      Serial1.read(); // trash anything over max length
    }
    if(strncmp((char*)buf, (char*)"AT+", 3) == 0){ // this should process AT commands
      callCMD(buf, len).toCharArray(msgOut, sizeof(msgOut));
    } else { // this should send to LoRa
      //Serial.write(buf, sizeof(buf)); //Serial.println();
      if(DEBUG){
        strcat(msgOut, "[LoRa] Transmitting...\n");
      }
      if(DEFICHAT){
        requestReceipt = true; // we want a success/fail response on this transmission
        unsigned char newID[4];
        newID[0] = radio.randomByte(); // used to make echoID
        newID[1] = radio.randomByte(); // used to make echoID
        newID[2] = radio.randomByte(); // used to make echoID
        newID[3] = radio.randomByte(); // used to make echoID
        queueMsg((char*)buf, (unsigned char)ENCRYPT, 0, newID, millis());
      } else {
        rawTransmit(buf, len);
      }
    }
    exportData(msgOut, strlen(msgOut));
  }
  
  // Forward from BLEUART to HW Serial
  if( bleuart.available() )
  {
    char msgOut[MTU];
    memset(msgOut, 0, sizeof(msgOut));
    periph = 2;
    delay(1250);
    uint8_t len = bleuart.available();
    if(len > 208){
      len = 208;
    }
    uint8_t buf[208];
    memset(buf, 0, sizeof(buf));
    for(uint8_t i = 0; i < sizeof(buf); i++){
      uint8_t singleByte = bleuart.read();
      if(singleByte != 0xFF){
        buf[i] = singleByte;
      }
    }
    while(bleuart.available()){
      bleuart.read(); // trash anything over max length
    }
    if(strncmp((char*)buf, (char*)"AT+", 3) == 0){ // this should process AT commands
      callCMD(buf, len).toCharArray(msgOut, sizeof(msgOut));
    } else { // this should send to LoRa
      //Serial.write(buf, sizeof(buf)); //Serial.println();
      if(DEBUG){
        strcat(msgOut, "[LoRa] Transmitting...\n");
      }
      if(DEFICHAT){
        requestReceipt = true; // we want a success/fail response on this transmission
        unsigned char newID[4];
        newID[0] = radio.randomByte(); // used to make echoID
        newID[1] = radio.randomByte(); // used to make echoID
        newID[2] = radio.randomByte(); // used to make echoID
        newID[3] = radio.randomByte(); // used to make echoID
        queueMsg((char*)buf, (unsigned char)ENCRYPT, 0, newID, millis());
      } else {
        rawTransmit(buf, len);
      }
    }
    exportData(msgOut, strlen(msgOut));
  }

  if(FHSS){ // if FHSS is enabled
    if((millis() - lastHop) >= 10000){ // if it's been more than 10 seconds since the last hop
      //Serial.println("[LoRa] Timeout Reached");
      radio.setFrequency(channels[HC]); // return to home channel (in case we get stuck on random channel
      lastHop = millis(); // log a new time
    }
  }
}

static void disconnectBle() {
  uint16_t connections = Bluefruit.connected();
  for (uint16_t conn = 0; conn < connections; conn++) {
    Bluefruit.disconnect(conn);
  }
}

void ble_sleep(void) {
  Bluefruit.Advertising.restartOnDisconnect(false);
  disconnectBle();
  Bluefruit.Advertising.stop();
}

void loop()
{
  if (fhssChangeFlag == true) {
    // we do, change it now
    int state = radio.setFrequency(channels[radio.getFHSSChannel() % numberOfChannels]);
    if (state != RADIOLIB_ERR_NONE) {
      //Serial.print("[LoRa] Failed to change frequency, code ");
      //Serial.println(state);
    }

    // the first hop actually takes much longer than other hops, as evidenced by the amber LED on LoRaBlue boards.
    // If we remove this from the average (because legally we haven't started hopping yet), our average channel occupancy time goes way down.
    // This is likely how the protocol works, and the starting transmission carries some sort of protocol information or is used for syncing.
    // Using this beginning transmission for calculating average channel occupancy time would go against the spirit of the FCC regulation on FHSS.
    // We are confident that ignoring this first section of the transmission is both lawful and justified.
    // Uncomment the code below to see just how long this first section of the transmission is (using USB)
    if(hopsCompleted == 1){
      toaMax = millis() - toaStart;
      //Serial.print("[LoRa] Initial Channel Occupancy: "); Serial.print(toaMax); Serial.println("ms");
    }
    
    // increment the counter
    hopsCompleted++;

    // log when hop is done
    lastHop = millis();
    
    // clear the FHSS interrupt
    radio.clearFHSSInt();

    // we're ready to do another hop, clear the flag
    fhssChangeFlag = false;
  } 

  if (receivedFlag == true) {
    receivedFlag = false;
    
    ///////////////////////////////////////////////////////////////////////////
    // These appear to be necessary RIGHT HERE to keep everything in sync    //
    radio.clearFHSSInt(); // clear the FHSS interrupt                        //
    fhssChangeFlag = false; // we're ready to do another hop, clear the flag //
    ///////////////////////////////////////////////////////////////////////////

    uint8_t incoming[MTU];
    uint8_t message[MTU];
    bool relayMSG = true; // will change to false if not decrypted properly
    memset(incoming, 0, sizeof(incoming)); //typically not necessary but here it is
    memset(message, 0, sizeof(message));
    int state = radio.readData(incoming, 0); // 0 apparently reads all
    /*Serial.println();
    Serial.print("IN: ");
    for(uint8_t i = 0; i < sizeof(incoming); i++){
      if(incoming[i] < 0x10){
        Serial.print("0");
      }
      Serial.print(incoming[i], HEX);
    }
    Serial.println();*/
    if (state == RADIOLIB_ERR_NONE) {
      if(DEFICHAT){
        if(incoming[0] == 0x00){
          //Serial.println("Unencrypted DEFICHAT");

          // we get a weird block here and i'm not sure what's going on
          for(uint8_t i = 0; i < sizeof(incoming)-6; i++){
            message[i] = incoming[i+6];
          }
        } else if(incoming[0] == 0x01){
          //Serial.println("Encrypted DEFICHAT");
          uint8_t blocks = 0;
          uint8_t msgPos = 6;
          for(uint8_t i = 0; i < 15; i++){
            uint8_t emptyBlock[16];
            memset(emptyBlock, 0x00, 16);
            uint8_t tempBlock[16];
            for(uint8_t i = 0; i < 16; i++){
              tempBlock[i] = incoming[msgPos];
              msgPos++;
            }
            if(memcmp(tempBlock, emptyBlock, 16) != 0){
              blocks++;
            }
          }
          uint8_t msgBlocks = blocks - 2; 
          //Serial.print("Blocks: "); Serial.println(blocks);
          //Serial.print("Message Blocks: "); Serial.println(msgBlocks);
          if(msgBlocks > 0){
            // check nullBlock
            char encBlock[16];
            char decBlock[16];
            uint8_t emptyBlock[16];
            memset(emptyBlock, 0x00, 16);
            for(uint8_t i = 0; i < 16; i++){
              encBlock[i] = incoming[i+6];
            }
            aes.Process(encBlock, 16, IV, PASSKEY, sizeof(PASSKEY), decBlock, aes.decryptFlag, aes.cbcMode);
            if(memcmp(decBlock, emptyBlock, 16) == 0){
              //Serial.println("Pass Phrases Match!");
              // get sender
              for(uint8_t i = 0; i < 16; i++){
                encBlock[i] = incoming[i+22];
              }
              aes.Process(encBlock, 16, IV, PASSKEY, sizeof(PASSKEY), decBlock, aes.decryptFlag, aes.cbcMode);
              //Serial.print("Sender: "); Serial.println(decBlock);
              uint8_t senderLen = strlen(decBlock);
              //Serial.print("Sender Length: "); Serial.println(senderLen);
              memset(message, 0, sizeof(message));
              for(uint8_t i = 0; i < senderLen; i++){
                message[i] = decBlock[i];
              }
              message[senderLen] = ':';
              message[senderLen+1] = ' ';

              // decrypt message blocks
              uint8_t inPos = 38;
              uint8_t msgPos = senderLen + 2;
              for(uint8_t i = 0; i < msgBlocks; i++){
                for(uint8_t i = 0; i < 16; i++){
                  encBlock[i] = incoming[inPos];
                  inPos++;
                }
                aes.Process(encBlock, 16, IV, PASSKEY, sizeof(PASSKEY), decBlock, aes.decryptFlag, aes.cbcMode);
                for(uint8_t i = 0; i < 16; i++){
                  message[msgPos] = decBlock[i];
                  msgPos++;
                }
              }
            } else {
              // decryption failed
              relayMSG = false;
            }
          } else {
            // empty message
            relayMSG = false;
          }
        } else {
          //Serial.println("Unformatted Message");
          memcpy(message, incoming, sizeof(incoming));
        }
      } else {
        // DEFICHAT off
        memcpy(message, incoming, sizeof(incoming));
      }

      // check if the recieved message is in our echo buffer

      unsigned char incomingID[4];
      for(uint8_t i = 0; i < 4; i++){
        incomingID[i] = incoming[i+2];
      }
      unsigned long newID = blockToLong(incomingID);

      ///////////////// experimental //////////////////////

      // check if the recieved message is already in our echo buffer
      unsigned char compBuf[4];
      for(uint8_t i = 0; i < 4; i++){
        compBuf[i] = pendingRaw[i+2]; // get id bytes
      }
      // This will mean we're probably waiting to transmit this message already. 
      if(memcmp(incomingID, compBuf, 4) == 0){
        incoming[1] = incoming[1] + 1; // we have heard the message so increment the echo number before we send it
      }

      //Serial.print("\nmemcmp: "); Serial.println(memcmp(incomingID, compBuf, 4)); Serial.println();
      //////////////////////////////////////////////////////

      volatile bool newEcho = isNewEcho(newID);
      if(newEcho){
        // log it so we don't print it again
        logMsgID(newEcho, millis());
      }

      //Serial.print("\nnewID: "); Serial.print(newID); Serial.print("\t");
      //Serial.print("isNewEcho: "); Serial.print(newEcho); Serial.print("\t");
      if(DEFICHAT && (incoming[1] < MAXECHO) && isNewEcho(newID)){
        logMsgID(newID, millis());
        if(relayMSG){
          exportData(message, strlen((char*)message));
          //Serial.print("\nCondition: "); Serial.print("1");
        }
        // echo message
        unsigned char outgoing[MTU];
        memcpy(outgoing, incoming, sizeof(outgoing));
        outgoing[1] = incoming[1] + 1;
        requestReceipt = false; // we don't want a response for an echo
        queueRaw((uint8_t*)outgoing, newID, millis());
      } else if(DEFICHAT && (MAXECHO == 0) && isNewEcho(newID)){ // still receive new messages even if we don't want to echo them
        logMsgID(newID, millis());
        if(relayMSG){
          //Serial.print("Condition: "); Serial.print("2"); Serial.print("\tMessage: ");
          exportData(message, strlen((char*)message));
        }
      } else if(DEFICHAT == false){
        if(relayMSG){
          //Serial.print("\nCondition: "); Serial.print("3"); Serial.print("\tMessage: ");
          exportData(message, strlen((char*)message));
        }
      } 

      if((relayMSG == true) && (DEBUG == true) && (DEFICHAT == false)){  // debug will throw off our ability to echo. So if using DefiChat, we can't get debug data on the recieved message
        //bleuart.flush(); // probably required
        //delay(100); // also probably required

        // see note on toaMax to understand why we process the time in the way below (we essentially ignore anything prior to the first hop)
        unsigned long processed_time = (toaStop-toaStart)-toaMax;

        char debugMsg[MTU];
        memset(debugMsg, 0, sizeof(debugMsg));
        char num[32]; // 32 digit max 

        strcat(debugMsg, "\n");
        strcat(debugMsg, "[LoRa] Channels Used: ");
        memset(num, 0, sizeof(num));
        itoa(hopsCompleted+2, num, 10); // weird glitch. This is accurate
        strcat(debugMsg, num);
        strcat(debugMsg, "\n");
        strcat(debugMsg, "[LoRa] Signal Strength Indicator (RSSI): ");
        memset(num, 0, sizeof(num));
        itoa(radio.getRSSI(), num, 10);
        strcat(debugMsg, num);
        strcat(debugMsg, "dBm\n");
        strcat(debugMsg, "[LoRa] Signal-to-Noise Ratio (SNR): ");
        memset(num, 0, sizeof(num));
        itoa(radio.getSNR(), num, 10);
        strcat(debugMsg, num);
        strcat(debugMsg, "\n");
        strcat(debugMsg, "[LoRa] Frequency Error: ");
        memset(num, 0, sizeof(num));
        itoa(radio.getFrequencyError(), num, 10);
        strcat(debugMsg, num);
        strcat(debugMsg, "Hz\n");
          
        exportData(debugMsg, strlen(debugMsg));
      }   
    } else {
      char msgOut[MTU] = "AT+ERROR=";
      char tempCode[128];
      memset(tempCode, 0, sizeof(tempCode));
      String errorString = errorCode[abs(transmissionState)] + "\n";
      errorString.toCharArray(tempCode, errorString.length()+1);
      strcat(msgOut, tempCode);
      exportData(msgOut, strlen(msgOut));
    }

    // reset the counter
    hopsCompleted = 0;

    if(FHSS){
      state = radio.setFrequency(channels[HC]);
    }
  
    // put the module back to listen mode
    radio.startReceive();
  }

  // check if the transmission flag is set
  if (transmittedFlag == true) {
    // reset flag
    transmittedFlag = false;
    
    toaStop = millis();

    char msgOut[MTU] = "AT+ERROR=";

    if(requestReceipt){
      char tempCode[128];
      memset(tempCode, 0, sizeof(tempCode));
      String errorString = errorCode[abs(transmissionState)] + "\n";
      errorString.toCharArray(tempCode, errorString.length()+1);
      strcat(msgOut, tempCode);
      //strcat(msgOut, "\n");

      if (transmissionState == RADIOLIB_ERR_NONE) {
        if (DEBUG){
          // see note on toaMax to understand why we process the time in the way below (we essentially ignore anything prior to the first hop)
          unsigned long processed_time = (toaStop-toaStart)-toaMax;

          char num[32]; // 32 digit max 

          strcat(msgOut, "[LoRa] Channels Used: ");
          memset(num, 0, sizeof(num));
          itoa(hopsCompleted, num, 10);
          strcat(msgOut, num);
          strcat(msgOut, "\n");
          strcat(msgOut, "[LoRa] Time On Air: ");
          memset(num, 0, sizeof(num));
          itoa(toaStop-toaStart, num, 10);
          strcat(msgOut, num);
          strcat(msgOut, "ms\n");
          strcat(msgOut, "[LoRa] Dwell Time / Channel: ");
          memset(num, 0, sizeof(num));
          itoa(processed_time/hopsCompleted, num, 10);
          strcat(msgOut, num);
          strcat(msgOut, "ms\n");
        }
      }
      exportData(msgOut, strlen(msgOut));
    }
    
    // reset the counter
    hopsCompleted = 0;
    if(FHSS){
      radio.setFrequency(channels[HC]);
      radio.setDio1Action(setFHSSFlag, RISING);
    }

    radio.setDio0Action(setRxFlag, RISING);
    radio.startReceive();
  }

  if(digitalRead(SWITCH) == LOW){ //sleep
    
    bool deep_sleep = true;

    ble_sleep();
    radio.sleep();
    for(uint8_t i = 0; i < 6; i++){
      digitalToggle(LED_BUILTIN); // turn the LED on (HIGH is the voltage level)
      delay(100);
    }

    pinMode(LED_BUILTIN, INPUT);
    pinMode(2, INPUT);

    if(deep_sleep == true){
      pinMode(SWITCH, INPUT_PULLDOWN_SENSE);  // this pin (SWITCH PIN) is pulled down and wakes up the board when externally connected to 3.3v.
      pinMode(0, INPUT_PULLUP_SENSE);  // this pin (UART RX) is pulled up and wakes up the board when externally connected to GND or recieves UART communication
      sd_power_system_off(); // this function puts the whole nRF52 to deep sleep (no Bluetooth)
    } else {
      pinMode(SWITCH, INPUT_PULLDOWN);
      
      // use push button to wake up via interrupt
      attachInterrupt(digitalPinToInterrupt(SWITCH), isrButton, RISING);

      // disable RTC1 interrupts
      sd_nvic_DisableIRQ(RTC1_IRQn);

      // sleep until wake-up flag has been set by button interrupt-service routine
      while (!wakeUp)
        sd_app_evt_wait();

      // enable RTC1 interrupts
      sd_nvic_EnableIRQ(RTC1_IRQn);

      // reset interrupt flag
      detachInterrupt(digitalPinToInterrupt(SWITCH));
      wakeUp = false;

      // after wakep
      pinMode(LED_BUILTIN, OUTPUT);
      if((LED > 0) && (LED < 3)){
        if(connected == false){
          digitalWrite(LED_BUILTIN, LOW); // for some reason the led needs to be turned off here or it will be on even though it's not connected
        }
      } else {
        digitalWrite(LED_BUILTIN, LOW);
      }

      pinMode(2, OUTPUT); // led2
      if(LED > 1){
        digitalWrite(2, HIGH);
      } else {
        digitalWrite(2, LOW);
      }
      
      pinMode(SWITCH, INPUT_PULLUP);
      for(uint8_t i = 0; i < 6; i++){
        digitalToggle(LED_BUILTIN); // turn the LED on (HIGH is the voltage level)
        delay(100);
      }
      
      //radio.setDio0Action(setRxFlag, RISING);
      radio.startReceive();

      startAdv();
    }
  }
}

uint16_t handle; // which device is connected
// callback invoked when central connects
void connect_callback(uint16_t conn_handle)
{
  handle = conn_handle;
  // Get the reference to current connection
  BLEConnection* connection = Bluefruit.Connection(conn_handle);

  connected = true;

  char central_name[32] = { 0 };
  connection->getPeerName(central_name, sizeof(central_name));

  periph = 2; // automatically switch to BLE on connection

  /*Serial.print("Connected to ");
  Serial.println(central_name);*/
  if((LED > 0) && (LED < 3)){
    digitalWrite(LED_BUILTIN, HIGH);
  }
}

/**
 * Callback invoked when a connection is dropped
 * @param conn_handle connection where this event happens
 * @param reason is a BLE_HCI_STATUS_CODE which can be found in ble_hci.h
 */
void disconnect_callback(uint16_t conn_handle, uint8_t reason)
{
  (void) conn_handle;
  (void) reason;

  connected = false;
  
  //Serial.print("Disconnected, reason = 0x"); Serial.println(reason, HEX);

  if((LED > 0) && (LED < 3)){
    digitalWrite(LED_BUILTIN, LOW);
  }
}

void rawTransmit(uint8_t* buf, uint8_t len){
  // reset the counter
  hopsCompleted = 0;
  if(FHSS){
    // return to home channel before the next transaction
    int state = radio.setFrequency(channels[HC]);
    if (state != RADIOLIB_ERR_NONE) {
      Serial.print(F("[LoRa] Failed to change frequency, code "));
      Serial.println(errorCode[abs(state)]);
    }

    // set the function to call when we need to change frequency
    radio.setDio1Action(setFHSSFlag, RISING);
  }
  
  // set the function to call when transmission is finished
  radio.setDio0Action(setTxFlag, RISING);
  
  /*Serial.println();
  Serial.print("OUT: ");
  for(uint8_t i = 0; i < len; i++){
    if(buf[i] < 0x10){
      Serial.print("0");
    }
    Serial.print(buf[i], HEX);
  }
  Serial.println();*/

  toaStart = millis();
  transmissionState = radio.startTransmit(buf, len);
}

void reset(){
  Scheduler.startLoop(resetLoop);
}

void resetLoop(){
  //reset 0.5 second in future
  //delay(50);
  char msg[14] = "AT+RESETTING\n";
  exportData(msg, strlen(msg));
  delay(250);
  

  // anything you can call 'end()' on needs to be called prior to 'NVIC_SystemReset'
  Bluefruit.disconnect(handle);
  Serial.flush();
  Serial1.flush();
  Serial.end();
  Serial1.end();
  InternalFS.end();
  NVIC_SystemReset();
}

String callCMD(const uint8_t* buf, const uint8_t len){
  uint8_t pos = 0;
  uint8_t bufStart;
  char process[len];
  memset(process, 0, len);

  String ret = "";
  if(strncmp((char*)buf, (char*)"AT+VERSION", sizeof("AT+VERSION")-1) == 0){ // this should process AT commands
    ret = "AT+VERSION=" + String(version);
  }
  else if(strncmp((char*)buf, (char*)"AT+RESET", sizeof("AT+RESET")-1) == 0){ // this should process AT commands
    ret = "AT+OK";
    reset();
  }
  else if(strncmp((char*)buf, (char*)"AT+UF2", sizeof("AT+UF2")-1) == 0){ // this should process AT commands
    //ret = "AT+OK";// won't return
    if(periph == 0){
      Serial.println("AT+OK");
      Serial.println("AT+RESETTING");
    } 
    else if(periph == 1){
      Serial1.println("AT+OK");
      Serial1.println("AT+RESETTING");
    }
    else if(periph == 2){
      bleuart.println("AT+OK");
      bleuart.println("AT+RESETTING");
    }
    NRF_POWER->GPREGRET = 0x57;
    reset();
  }
  else if(strncmp((char*)buf, (char*)"AT+DEFAULT=", sizeof("AT+DEFAULT=")-1) == 0){ // set LoRa to recommended settings (0-3)
    bufStart = sizeof("AT+DEFAULT=")-1;
    for(uint8_t i = pos; i < len; i++){
      if((char)buf[pos+bufStart] != '\r'){
        if((char)buf[pos+bufStart] != '\n'){
          if(buf[pos+bufStart] != 0x00){
            process[pos] = buf[pos+bufStart]; //buf[pos];
            pos++;
          }
        }
      }
    }
    bool def = String(process).toInt();
    if(def){
      InternalFS.format(); // erase all internal data
      ret = "AT+OK";
      reset();
    } else {
      ret = "AT+ERROR=DEFAULT_MUST_BE_1";
    }
  }
  else if(strncmp((char*)buf, (char*)"AT+DEFAULT", sizeof("AT+DEFAULT")-1) == 0){ // get name
    ret = "AT+ERROR=NO_VALUE_SPECIFIED";
  }
  else if(strncmp((char*)buf, (char*)"AT+SLEEP=", sizeof("AT+SLEEP=")-1) == 0){ // set LoRa to recommended settings (0-3)
    bufStart = sizeof("AT+SLEEP=")-1;
    for(uint8_t i = pos; i < len; i++){
      if((char)buf[pos+bufStart] != '\r'){
        if((char)buf[pos+bufStart] != '\n'){
          if(buf[pos+bufStart] != 0x00){
            process[pos] = buf[pos+bufStart]; //buf[pos];
            pos++;
          }
        }
      }
    }
    bool def = String(process).toInt();
    if(def){
      ble_sleep();
      radio.sleep();
      for(uint8_t i = 0; i < 6; i++){
        digitalToggle(LED_BUILTIN); // turn the LED on (HIGH is the voltage level)
        delay(100);
      }

      if(periph == 0){
        ret = "AT+ERROR=USB_CAN_NOT_CALL_SLEEP";
      } 
      else if(periph == 1){
        digitalWrite(LED_BUILTIN, LOW);
      digitalWrite(2, LOW);
        Serial1.println("AT+OK");
        delay(250);
        pinMode(SWITCH, INPUT_PULLDOWN_SENSE);  // this pin (SWITCH PIN) is pulled down and wakes up the board when externally connected to 3.3v.
        pinMode(0, INPUT_PULLUP_SENSE);  // this pin (UART RX) is pulled up and wakes up the board when externally connected to GND or recieves UART communication
        sd_power_system_off(); // this function puts the whole nRF52 to deep sleep (no Bluetooth)
      }
      else if(periph == 2){
        digitalWrite(LED_BUILTIN, LOW);
      digitalWrite(2, LOW);
        bleuart.println("AT+OK");
        delay(250);
        pinMode(SWITCH, INPUT_PULLDOWN_SENSE);  // this pin (SWITCH PIN) is pulled down and wakes up the board when externally connected to 3.3v.
        pinMode(0, INPUT_PULLUP_SENSE);  // this pin (UART RX) is pulled up and wakes up the board when externally connected to GND or recieves UART communication
        sd_power_system_off(); // this function puts the whole nRF52 to deep sleep (no Bluetooth)
      }

    } else {
      ret = "AT+ERROR=SLEEP_MUST_BE_1";
    }
  }
  else if(strncmp((char*)buf, (char*)"AT+SLEEP", sizeof("AT+SLEEP")-1) == 0){ // get name
    ret = "AT+ERROR=NO_VALUE_SPECIFIED";
  }
  else if(strncmp((char*)buf, (char*)"AT+SETRANGE=", sizeof("AT+SETRANGE=")-1) == 0){ // set LoRa to recommended settings (0-3)
    bufStart = sizeof("AT+SETRANGE=")-1;
    for(uint8_t i = pos; i < len; i++){
      if((char)buf[pos+bufStart] != '\r'){
        if((char)buf[pos+bufStart] != '\n'){
          if(buf[pos+bufStart] != 0x00){
            process[pos] = buf[pos+bufStart]; //buf[pos];
            pos++;
          }
        }
      }
    }
    uint8_t def = String(process).toInt();
    if(def < 6){
      FHSS = 1;
      HC = 64;
      FREQ = channels[HC];
      BW = bwIndex[def];
      HP = hpIndex[def];
      SF = sfIndex[def];
      CR = crIndex[def];
      SW = 0xDD; // LoRa Sync Word
      PRE = 6; // LoRa Preamble
      PWR = 20; // LoRa Power Level
      GAIN = 0; //LoRa GAIN
      setConfig();
      ret = "AT+OK";
      reset();
    }
    else {
      ret = "AT+ERROR=INVALID_SELECTION";
    }
  }
  else if(strncmp((char*)buf, (char*)"AT+SETRANGE", sizeof("AT+SETRANGE")-1) == 0){ // get name
    ret = "AT+ERROR=NO_VALUE_SPECIFIED";
  }
  else if(strncmp((char*)buf, (char*)"AT+BTEN=", sizeof("AT+BTEN=")-1) == 0){ // set new name
    bufStart = sizeof("AT+BTEN=")-1;
    for(uint8_t i = pos; i < len; i++){
      if((char)buf[pos+bufStart] != '\r'){
        if((char)buf[pos+bufStart] != '\n'){
          if(buf[pos+bufStart] != 0x00){
            process[pos] = buf[pos+bufStart]; //buf[pos];
            pos++;
          }
        }
      }
    }
    BTEN = String(process).toInt();
    setConfig();
    ret = "AT+OK";
    reset();
    // in this case, chamges won't take effect until reset
  }
  else if(strncmp((char*)buf, (char*)"AT+BTEN", sizeof("AT+BTEN")-1) == 0){ // get name
    ret = "AT+BTEN=" + String(BTEN);
  } 
  else if(strncmp((char*)buf, (char*)"AT+NAME=", sizeof("AT+NAME=")-1) == 0){ // set new name
    bufStart = sizeof("AT+NAME=")-1;
    for(uint8_t i = pos; i < len; i++){
      if((char)buf[pos+bufStart] != '\r'){
        if((char)buf[pos+bufStart] != '\n'){
          if(buf[pos+bufStart] != 0x00){
            process[pos] = buf[pos+bufStart]; //buf[pos];
            pos++;
          }
        }
      }
    }
    if(pos < 33){ // 32 CHARACTERS OR LESS 
      InternalFS.remove("NAME");
      if( file.open("NAME", FILE_O_WRITE) )
      {
        file.print(process);
        file.close();
        ret = "AT+OK";
        reset();
      } else {
        ret = "AT+ERROR=FAILED_TO_WRITE";
      }
    } else{
      ret = "AT+ERROR=NAME_MUST_BE_32_CHARACTERS_OR_LESS";
    }
  }
  else if(strncmp((char*)buf, (char*)"AT+NAME", sizeof("AT+NAME")-1) == 0){ // get name
    file.open("NAME", FILE_O_READ);
    ret = "AT+NAME=" + file.readString();
  }
  else if(strncmp((char*)buf, (char*)"AT+SETUP=", sizeof("AT+SETUP=")-1) == 0){ // set new name
    bufStart = sizeof("AT+SETUP=")-1;
    for(uint8_t i = pos; i < len; i++){
      if((char)buf[pos+bufStart] != '\r'){
        if((char)buf[pos+bufStart] != '\n'){
          if(buf[pos+bufStart] != 0x00){
            process[pos] = buf[pos+bufStart]; //buf[pos];
            pos++;
          }
        }
      }
    }
    if(pos < 17){ //must be 16 characters or less
      InternalFS.remove("NAME");
      if( file.open("NAME", FILE_O_WRITE) )
      {
        file.print("LoRaBlue-"); file.print(process);
        file.close();
        InternalFS.remove("USER");
        if( file.open("USER", FILE_O_WRITE) )
        {
          file.print(process);
          file.close();
          ret = "AT+OK";
          reset();
        } else {
          ret = "AT+ERROR=FAILED_TO_WRITE";
        }
      } else {
        ret = "AT+ERROR=FAILED_TO_WRITE";
      }
    } else {
      "AT+ERROR=MUST_BE_16_CHARACTERS_OR_LESS";
    }
  }
  else if(strncmp((char*)buf, (char*)"AT+SETUP", sizeof("AT+SETUP")-1) == 0){ // get name
    // do nothing
  }
  else if(strncmp((char*)buf, (char*)"AT+PIN=", sizeof("AT+PIN=")-1) == 0){ // set new name
    bufStart = sizeof("AT+PIN=")-1;
    for(uint8_t i = pos; i < len; i++){
      if((char)buf[pos+bufStart] != '\r'){
        if((char)buf[pos+bufStart] != '\n'){
          if(buf[pos+bufStart] != 0x00){
            process[pos] = buf[pos+bufStart]; //buf[pos];
            pos++;
          }
        }
      }
    }
    if(pos == 6){ // 6 characters
      InternalFS.remove("PIN");
      if( file.open("PIN", FILE_O_WRITE) )
      {
        file.print(process);
        file.close();
        ret = "AT+OK";
        reset();
      } else {
        ret = "AT+ERROR=FAILED_TO_WRITE";
      }
    } else {
      ret = "AT+ERROR=PIN_MUST_BE_6_DIGITS";
    }
    // in this case, chamges won't take effect until reset
  }
  else if(strncmp((char*)buf, (char*)"AT+PIN", sizeof("AT+PIN")-1) == 0){ // get name
    file.open("PIN", FILE_O_READ);
    ret = "AT+PIN=" + file.readString();
  }
  else if(strncmp((char*)buf, (char*)"AT+BAUD=", sizeof("AT+BAUD=")-1) == 0){ // set new name
    bufStart = sizeof("AT+BAUD=")-1;
    for(uint8_t i = pos; i < len; i++){
      if((char)buf[pos+bufStart] != '\r'){
        if((char)buf[pos+bufStart] != '\n'){
          if(buf[pos+bufStart] != 0x00){
            process[pos] = buf[pos+bufStart]; //buf[pos];
            pos++;
          }
        }
      }
    }
    if(String(process).toInt() < 17){
      BAUD = String(process).toInt();
      setConfig();
      Serial1.begin(available_baud[BAUD]);
      ret = "AT+OK";
    } else {
      ret = "AT+ERROR=INVALID_BAUD";
    }
    // in this case, chamges won't take effect until reset
  }
  else if(strncmp((char*)buf, (char*)"AT+BAUD", sizeof("AT+BAUD")-1) == 0){ // get name
    ret = "AT+BAUD=" + String(BAUD);
  }
  else if(strncmp((char*)buf, (char*)"AT+DEBUG=", sizeof("AT+DEBUG=")-1) == 0){ // set new name
    bufStart = sizeof("AT+DEBUG=")-1;
    for(uint8_t i = pos; i < len; i++){
      if((char)buf[pos+bufStart] != '\r'){
        if((char)buf[pos+bufStart] != '\n'){
          if(buf[pos+bufStart] != 0x00){
            process[pos] = buf[pos+bufStart]; //buf[pos];
            pos++;
          }
        }
      }
    }
    DEBUG = String(process).toInt();
    setConfig();
    ret = "AT+OK";
    // in this case, chamges won't take effect until reset
  }
  else if(strncmp((char*)buf, (char*)"AT+DEBUG", sizeof("AT+DEBUG")-1) == 0){ // get name
    ret = "AT+DEBUG=" + String(DEBUG);
  }
  else if(strncmp((char*)buf, (char*)"AT+FHSS=", sizeof("AT+FHSS=")-1) == 0){ // set new name
    bufStart = sizeof("AT+FHSS=")-1;
    for(uint8_t i = pos; i < len; i++){
      if((char)buf[pos+bufStart] != '\r'){
        if((char)buf[pos+bufStart] != '\n'){
          if(buf[pos+bufStart] != 0x00){
            process[pos] = buf[pos+bufStart]; //buf[pos];
            pos++;
          }
        }
      }
    }
    if(String(process).toInt()){ // if FHSS = 1
      int state = radio.setFHSSHoppingPeriod(HP);
      if (state == RADIOLIB_ERR_NONE) {
        FHSS = 1;
        FREQ = channels[HC];
        setConfig();
        ret = "AT+OK";
      } else {
        ret = "AT+ERROR=" + errorCode[abs(state)];
      }
    } else { // if FHSS = 0
      FHSS = 0;
      setConfig();
      ret = "AT+OK";
      reset();
    }
  }
  else if(strncmp((char*)buf, (char*)"AT+FHSS", sizeof("AT+FHSS")-1) == 0){ // get name
    ret = "AT+FHSS=" + String(FHSS);
  }
  else if(strncmp((char*)buf, (char*)"AT+HC=", sizeof("AT+HC=")-1) == 0){ // set new name
    bufStart = sizeof("AT+HC=")-1;
    for(uint8_t i = pos; i < len; i++){
      if((char)buf[pos+bufStart] != '\r'){
        if((char)buf[pos+bufStart] != '\n'){
          if(buf[pos+bufStart] != 0x00){
            process[pos] = buf[pos+bufStart]; //buf[pos];
            pos++;
          }
        }
      }
    }
    HC = String(process).toInt();
    setConfig();
    ret = "AT+OK";
    // in this case, chamges won't take effect until reset
  }
  else if(strncmp((char*)buf, (char*)"AT+HC", sizeof("AT+HC")-1) == 0){ // get name
    ret = "AT+HC=" + String(HC);
  }
  else if(strncmp((char*)buf, (char*)"AT+FREQ=", sizeof("AT+FREQ=")-1) == 0){ // set new name
    bufStart = sizeof("AT+FREQ=")-1;
    for(uint8_t i = pos; i < len; i++){
      if((char)buf[pos+bufStart] != '\r'){
        if((char)buf[pos+bufStart] != '\n'){
          if(buf[pos+bufStart] != 0x00){
            process[pos] = buf[pos+bufStart]; //buf[pos];
            pos++;
          }
        }
      }
    }
    if(FHSS){
      ret = "AT+ERROR=NOT_AVAILABLE_WITH_FHSS";
    } else {
      float def = String(process).toFloat();
      int state = radio.setFrequency(def);
      if (state == RADIOLIB_ERR_NONE) {
        FREQ = def;
        ret = "AT+OK";
      } else {
        ret = "AT+ERROR=" + errorCode[abs(state)] + "_[" + String(process).toInt() + "]";
      }
    }
  }
  else if(strncmp((char*)buf, (char*)"AT+FREQ", sizeof("AT+FREQ")-1) == 0){ // get name
    if(FHSS){
      ret = "AT+ERROR=NOT_AVAILABLE_WITH_FHSS";
    } else {
      ret = "AT+FREQ=" + String(FREQ);
    }
  }
  else if(strncmp((char*)buf, (char*)"AT+HP=", sizeof("AT+HP=")-1) == 0){ // set new name
    bufStart = sizeof("AT+HP=")-1;
    for(uint8_t i = pos; i < len; i++){
      if((char)buf[pos+bufStart] != '\r'){
        if((char)buf[pos+bufStart] != '\n'){
          if(buf[pos+bufStart] != 0x00){
            process[pos] = buf[pos+bufStart]; //buf[pos];
            pos++;
          }
        }
      }
    }
    int hp = String(process).toInt();
    int state = radio.setFHSSHoppingPeriod(hp);
    if (state == RADIOLIB_ERR_NONE) {
      FHSS = 1;
      HP = hp;
      setConfig();
      ret = "AT+OK";
    } else {
      ret = "AT+ERROR=" + errorCode[abs(state)];
    }
    // in this case, chamges won't take effect until reset
  }
  else if(strncmp((char*)buf, (char*)"AT+HP", sizeof("AT+HP")-1) == 0){ // get name
    ret = "AT+HP=" + String(HP);
  }
  else if(strncmp((char*)buf, (char*)"AT+BW=", sizeof("AT+BW=")-1) == 0){ // set new name
    bufStart = sizeof("AT+BW=")-1;
    for(uint8_t i = pos; i < len; i++){
      if((char)buf[pos+bufStart] != '\r'){
        if((char)buf[pos+bufStart] != '\n'){
          if(buf[pos+bufStart] != 0x00){
            process[pos] = buf[pos+bufStart]; //buf[pos];
            pos++;
          }
        }
      }
    }
    float def = String(process).toFloat();
    int state = radio.setBandwidth(def);
    if (state == RADIOLIB_ERR_NONE) {
      BW = def;
      setConfig();
      ret = "AT+OK";
    } else {
      ret = "AT+ERROR=" + errorCode[abs(state)] + "_[" + String(process).toInt() + "]";
    }
  }
  else if(strncmp((char*)buf, (char*)"AT+BW", sizeof("AT+BW")-1) == 0){ // get name
    ret = "AT+BW=" + String(BW);
  }
  else if(strncmp((char*)buf, (char*)"AT+SF=", sizeof("AT+SF=")-1) == 0){ // set new name
    bufStart = sizeof("AT+SF=")-1;
    for(uint8_t i = pos; i < len; i++){
      if((char)buf[pos+bufStart] != '\r'){
        if((char)buf[pos+bufStart] != '\n'){
          if(buf[pos+bufStart] != 0x00){
            process[pos] = buf[pos+bufStart]; //buf[pos];
            pos++;
          }
        }
      }
    }
    int def = String(process).toInt();
    int state = radio.setSpreadingFactor(def);
    if (state == RADIOLIB_ERR_NONE) {
      SF = def;
      setConfig();
      ret = "AT+OK";
    } else {
      ret = "AT+ERROR=" + errorCode[abs(state)];
    }
  }
  else if(strncmp((char*)buf, (char*)"AT+SF", sizeof("AT+SF")-1) == 0){ // get name
    ret = "AT+SF=" + String(SF);
  }
  else if(strncmp((char*)buf, (char*)"AT+CRC=", sizeof("AT+CRC=")-1) == 0){ // set new name
    bufStart = sizeof("AT+CRC=")-1;
    for(uint8_t i = pos; i < len; i++){
      if((char)buf[pos+bufStart] != '\r'){
        if((char)buf[pos+bufStart] != '\n'){
          if(buf[pos+bufStart] != 0x00){
            process[pos] = buf[pos+bufStart]; //buf[pos];
            pos++;
          }
        }
      }
    }
    bool def = String(process).toInt();
    int state = radio.setCRC(def);
    if (state == RADIOLIB_ERR_NONE) {
      CRC = def;
      setConfig();
      ret = "AT+OK";
    } else {
      ret = "AT+ERROR=" + errorCode[abs(state)] + "_[" + String(process).toInt() + "]";
    }
    // in this case, chamges won't take effect until reset
  }
  else if(strncmp((char*)buf, (char*)"AT+CRC", sizeof("AT+CRC")-1) == 0){ // get name
    ret = "AT+CRC=" + String(CRC);
  }
  else if(strncmp((char*)buf, (char*)"AT+CR=", sizeof("AT+CR=")-1) == 0){ // set new name
    bufStart = sizeof("AT+CR=")-1;
    for(uint8_t i = pos; i < len; i++){
      if((char)buf[pos+bufStart] != '\r'){
        if((char)buf[pos+bufStart] != '\n'){
          if(buf[pos+bufStart] != 0x00){
            process[pos] = buf[pos+bufStart]; //buf[pos];
            pos++;
          }
        }
      }
    }
    uint8_t def = String(process).toInt();
    int state = radio.setCodingRate(def);
    if (state == RADIOLIB_ERR_NONE) {
      CR = def;
      setConfig();
      ret = "AT+OK";
    } else {
      ret = "AT+ERROR=" + errorCode[abs(state)] + "_[" + String(process).toInt() + "]";
    }
  }
  else if(strncmp((char*)buf, (char*)"AT+CR", sizeof("AT+CR")-1) == 0){ // get name
    ret = "AT+CR=" + String(CR);
  }
  else if(strncmp((char*)buf, (char*)"AT+SW=", sizeof("AT+SW=")-1) == 0){ // set new name
    bufStart = sizeof("AT+SW=")-1;
    for(uint8_t i = pos; i < len; i++){
      if((char)buf[pos+bufStart] != '\r'){
        if((char)buf[pos+bufStart] != '\n'){
          if(buf[pos+bufStart] != 0x00){
            process[pos] = buf[pos+bufStart]; //buf[pos];
            pos++;
          }
        }
      }
    }
    uint8_t def = String(process).toInt();
    int state = radio.setSyncWord(def);
    if (state == RADIOLIB_ERR_NONE) {
      SW = def;
      setConfig();
      ret = "AT+OK";
    } else {
      ret = "AT+ERROR=" + errorCode[abs(state)];
    }
    // in this case, chamges won't take effect until reset
  }
  else if(strncmp((char*)buf, (char*)"AT+SW", sizeof("AT+SW")-1) == 0){ // get name
    ret = "AT+SW=" + String(SW);
  }  
  else if(strncmp((char*)buf, (char*)"AT+PRE=", sizeof("AT+PRE=")-1) == 0){ // set new name
    bufStart = sizeof("AT+PRE=")-1;
    for(uint8_t i = pos; i < len; i++){
      if((char)buf[pos+bufStart] != '\r'){
        if((char)buf[pos+bufStart] != '\n'){
          if(buf[pos+bufStart] != 0x00){
            process[pos] = buf[pos+bufStart]; //buf[pos];
            pos++;
          }
        }
      }
    }
    int def = String(process).toInt();
    int state = radio.setPreambleLength(def);
    if (state == RADIOLIB_ERR_NONE) {
      PRE = def;
      setConfig();
      ret = "AT+OK";
    } else {
      ret = "AT+ERROR=" + errorCode[abs(state)] + "_[" + String(process).toInt() + "]";
    }
  }
  else if(strncmp((char*)buf, (char*)"AT+PRE", sizeof("AT+PRE")-1) == 0){ // get name
    ret = "AT+PRE=" + String(PRE);
  }
  else if(strncmp((char*)buf, (char*)"AT+PWR=", sizeof("AT+PWR=")-1) == 0){ // set new name
    bufStart = sizeof("AT+PWR=")-1;
    for(uint8_t i = pos; i < len; i++){
      if((char)buf[pos+bufStart] != '\r'){
        if((char)buf[pos+bufStart] != '\n'){
          if(buf[pos+bufStart] != 0x00){
            process[pos] = buf[pos+bufStart]; //buf[pos];
            pos++;
          }
        }
      }
    }
    uint8_t def = String(process).toInt();
    int state = radio.setOutputPower(def);
    if (state == RADIOLIB_ERR_NONE) {
      PWR = def;
      setConfig();
      ret = "AT+OK";
    } else {
      ret = "AT+ERROR=" + errorCode[abs(state)] + "_[" + String(process).toInt() + "]";
    }
    // in this case, chamges won't take effect until reset
  }
  else if(strncmp((char*)buf, (char*)"AT+PWR", sizeof("AT+PWR")-1) == 0){ // get name
    ret = "AT+PWR=" + String(PWR);
  }
  else if(strncmp((char*)buf, (char*)"AT+GAIN=", sizeof("AT+GAIN=")-1) == 0){ // set new name
    bufStart = sizeof("AT+GAIN=")-1;
    for(uint8_t i = pos; i < len; i++){
      if((char)buf[pos+bufStart] != '\r'){
        if((char)buf[pos+bufStart] != '\n'){
          if(buf[pos+bufStart] != 0x00){
            process[pos] = buf[pos+bufStart]; //buf[pos];
            pos++;
          }
        }
      }
    }
    uint8_t def = String(process).toInt();
    int state = radio.setGain(def);
    if (state == RADIOLIB_ERR_NONE) {
      GAIN = def;
      setConfig();
      ret = "AT+OK";
    } else {
      ret = "AT+ERROR=" + errorCode[abs(state)] + "_[" + String(process).toInt() + "]";
    }
    // in this case, chamges won't take effect until reset
  }
  else if(strncmp((char*)buf, (char*)"AT+GAIN", sizeof("AT+GAIN")-1) == 0){ // get name
    ret = "AT+GAIN=" + String(GAIN);
  }
  else if(strncmp((char*)buf, (char*)"AT+ENAP=", sizeof("AT+ENAP=")-1) == 0){ // set new name
    bufStart = sizeof("AT+ENAP=")-1;
    for(uint8_t i = pos; i < len; i++){
      if((char)buf[pos+bufStart] != '\r'){
        if((char)buf[pos+bufStart] != '\n'){
          if(buf[pos+bufStart] != 0x00){
            process[pos] = buf[pos+bufStart]; //buf[pos];
            pos++;
          }
        }
      }
    }
    ENAP = String(process).toInt();
    setConfig();
    ret = "AT+OK";
    // in this case, chamges won't take effect until reset
  }
  else if(strncmp((char*)buf, (char*)"AT+ENAP", sizeof("AT+ENAP")-1) == 0){ // get name
    ret = "AT+ENAP=" + String(ENAP);
  }
  else if(strncmp((char*)buf, (char*)"AT+LED=", sizeof("AT+LED=")-1) == 0){ // set new name
    bufStart = sizeof("AT+LED=")-1;
    for(uint8_t i = pos; i < len; i++){
      if((char)buf[pos+bufStart] != '\r'){
        if((char)buf[pos+bufStart] != '\n'){
          if(buf[pos+bufStart] != 0x00){
            process[pos] = buf[pos+bufStart]; //buf[pos];
            pos++;
          }
        }
      }
    }
    uint8_t def = String(process).toInt();
    if(def < 4){
      LED = def;
      setConfig();
      ret = "AT+OK";
      reset();
    } else {
      ret = "AT+ERROR=INVALID_LED_SELECTION";
    }
    // in this case, chamges won't take effect until reset
  }
  else if(strncmp((char*)buf, (char*)"AT+LED", sizeof("AT+LED")-1) == 0){ // get name
    ret = "AT+LED=" + String(LED);
  }
  else if(strncmp((char*)buf, (char*)"AT+DEFICHAT=", sizeof("AT+DEFICHAT=")-1) == 0){ // set new name
    bufStart = sizeof("AT+DEFICHAT=")-1;
    for(uint8_t i = pos; i < len; i++){
      if((char)buf[pos+bufStart] != '\r'){
        if((char)buf[pos+bufStart] != '\n'){
          if(buf[pos+bufStart] != 0x00){
            process[pos] = buf[pos+bufStart]; //buf[pos];
            pos++;
          }
        }
      }
    }
    DEFICHAT = String(process).toInt();
    setConfig();
    ret = "AT+OK";
    // in this case, chamges won't take effect until reset
  }
  else if(strncmp((char*)buf, (char*)"AT+DEFICHAT", sizeof("AT+DEFICHAT")-1) == 0){ // get name
    ret = "AT+DEFICHAT=" + String(DEFICHAT);
  }
  else if(strncmp((char*)buf, (char*)"AT+ENCRYPT=", sizeof("AT+ENCRYPT=")-1) == 0){ // set new name
    bufStart = sizeof("AT+ENCRYPT=")-1;
    for(uint8_t i = pos; i < len; i++){
      if((char)buf[pos+bufStart] != '\r'){
        if((char)buf[pos+bufStart] != '\n'){
          if(buf[pos+bufStart] != 0x00){
            process[pos] = buf[pos+bufStart]; //buf[pos];
            pos++;
          }
        }
      }
    }
    ENCRYPT = String(process).toInt();
    setConfig();
    ret = "AT+OK";
    // in this case, chamges won't take effect until reset
  }
  else if(strncmp((char*)buf, (char*)"AT+ENCRYPT", sizeof("AT+ENCRYPT")-1) == 0){ // get name
    ret = "AT+ENCRYPT=" + String(ENCRYPT);
  }
  else if(strncmp((char*)buf, (char*)"AT+USER=", sizeof("AT+USER=")-1) == 0){ // set new name
    bufStart = sizeof("AT+USER=")-1;
    for(uint8_t i = pos; i < len; i++){
      if((char)buf[pos+bufStart] != '\r'){
        if((char)buf[pos+bufStart] != '\n'){
          if(buf[pos+bufStart] != 0x00){
            process[pos] = buf[pos+bufStart]; //buf[pos];
            pos++;
          }
        }
      }
    }
    if(pos < 17){ // must be 16 characters or less
      InternalFS.remove("USER");
      if( file.open("USER", FILE_O_WRITE) )
      {
        file.print(process);
        file.close();
        ret = "AT+OK";
        reset();
      } else {
        ret = "AT+ERROR=FAILED_TO_WRITE";
      }
    } else {
      "AT+ERROR=USER_MUST_BE_16_CHARACTERS_OR_LESS";
    }
  }
  else if(strncmp((char*)buf, (char*)"AT+USER", sizeof("AT+USER")-1) == 0){ // get name
    file.open("USER", FILE_O_READ);
    ret = "AT+USER=" + file.readString();
    file.close();
  }
  else if(strncmp((char*)buf, (char*)"AT+PASSPHR=", sizeof("AT+PASSPHR=")-1) == 0){ // set new name
    bufStart = sizeof("AT+PASSPHR=")-1;
    for(uint8_t i = pos; i < len; i++){
      if((char)buf[pos+bufStart] != '\r'){
        if((char)buf[pos+bufStart] != '\n'){
          if(buf[pos+bufStart] != 0x00){
            process[pos] = buf[pos+bufStart]; //buf[pos];
            pos++;
          }
        }
      }
    }
    if(pos < 65){ // must be 64 characters or less
      InternalFS.remove("PASSPHR");
      memset(PASSPHR, 0, sizeof(PASSPHR));
      memcpy(PASSPHR, process, sizeof(process));
      if( file.open("PASSPHR", FILE_O_WRITE) )
      {
        file.print(process);
        file.close();
        sha256((uint8_t*)process);
        ret = "AT+OK";
        reset();
      } else {
        ret = "AT+ERROR=FILE_WRITE_FAILED";
      }
    } else {
      "AT+ERROR=PASSPHR_MUST_BE_64_CHARACTERS_OR_LESS";
    }
  }
  else if(strncmp((char*)buf, (char*)"AT+PASSPHR", sizeof("AT+PASSPHR")-1) == 0){ // get name
    file.open("PASSPHR", FILE_O_READ);
    ret = "AT+PASSPHR=" + file.readString();
    file.close();
  }
  else if(strncmp((char*)buf, (char*)"AT+CONFIG", sizeof("AT+CONFIG")-1) == 0){ // get name
    file.open("NAME", FILE_O_READ);
    String devicename = file.readString();
    file.close();
    file.open("USER", FILE_O_READ);
    String user = file.readString();
    file.close();
    file.open("PASSPHR", FILE_O_READ);
    String passphrase = file.readString();
    file.close();
    file.open("PIN", FILE_O_READ);
    String pin = file.readString();
    file.close();
    ret = "AT+CONFIG="+devicename+','+String(version)+','+String(FHSS)+','+String(HC)+','+String(FREQ)+','+String(HP)+','+String(BW)+','+String(SF)+','+String(CR)+','+String(SW)+','+String(PRE)+','+String(PWR)+','+String(GAIN)+','+String(CRC)+','+String(LED)+','+user+','+passphrase+','+pin+','+String(MAXECHO);
  }
  else if(strncmp((char*)buf, (char*)"AT+ANT", sizeof("AT+ANT")-1) == 0){ // get name
    if(periph == 0){
      Serial.print("AT+OK");
    } 
    else if(periph == 1){
      Serial1.print("AT+OK");
    }
    radio.sleep();
    bool ledState = digitalRead(LED_BUILTIN);
    pinMode(LED_BUILTIN, OUTPUT);
    for(uint8_t i = 0; i < 60; i++){ // 30 seconds to get antenna replaced
      digitalToggle(LED_BUILTIN);
      delay(500);
    }
    digitalWrite(LED_BUILTIN, ledState);
    hopsCompleted = 0;
    if(FHSS){
      radio.setFrequency(channels[HC]);
      radio.setDio1Action(setFHSSFlag, RISING);
    }
    radio.setDio0Action(setRxFlag, RISING);
    radio.startReceive();
  }
  else if(strncmp((char*)buf, (char*)"AT+MAXECHO=", sizeof("AT+MAXECHO=")-1) == 0){ // set new name
    bufStart = sizeof("AT+MAXECHO=")-1;
    for(uint8_t i = pos; i < len; i++){
      if((char)buf[pos+bufStart] != '\r'){
        if((char)buf[pos+bufStart] != '\n'){
          if(buf[pos+bufStart] != 0x00){
            process[pos] = buf[pos+bufStart]; //buf[pos];
            pos++;
          }
        }
      }
    }
    MAXECHO = String(process).toInt();
    setConfig();
    ret = "AT+OK";
    // in this case, chamges won't take effect until reset
  }
  else if(strncmp((char*)buf, (char*)"AT+MAXECHO", sizeof("AT+MAXECHO")-1) == 0){ // get name
    ret = "AT+MAXECHO=" + String(MAXECHO);
  }
  /////////// End of variables ///////////
  else {
    ret = "AT+ERROR=UNKNOWN_COMMAND";
  }
  
  // the need for this appears to be a glitch we just can't find the cause
  if((LED > 0) && (LED < 3)){
    digitalWrite(LED_BUILTIN, HIGH);
  }

  return ret + "\n";
}

void sha256(uint8_t* input_data){
  nRFCrypto.begin();
  uint32_t result[16];
  uint8_t  result_len; // depending on Hash mode

  uint8_t HASH[32];
  memset(HASH, 0, sizeof(HASH));

  hash.begin(CRYS_HASH_SHA256_mode);
  hash.update(input_data, strlen((char*)input_data));
  result_len = hash.end(result);
  memcpy(HASH, &result, sizeof(HASH));

  // Since apparently only aes128 is supported by the core, split the has into IV and PASSKEY
  memset(IV, 0, sizeof(IV));
  memset(PASSKEY, 0, sizeof(PASSKEY));
  for(uint8_t i = 0; i < 16; i++){
    IV[i] = HASH[i];
  }
  for(uint8_t i = 0; i < 16; i++){
    PASSKEY[i] = HASH[i+16];
  }

  /*Serial.print("PASSPHR: "); Serial.write(PASSPHR, sizeof(PASSPHR)); Serial.println();

  Serial.print("HASH: ");
  for(uint8_t i = 0; i < sizeof(HASH); i++){
    if(HASH[i] < 0x10){
      Serial.print("0");
    }
    Serial.print(HASH[i], HEX);
  }
  Serial.println();

  Serial.print("IV: ");
  for(uint8_t i = 0; i < sizeof(IV); i++){
    if(IV[i] < 0x10){
      Serial.print("0");
    }
    Serial.print(IV[i], HEX);
  }
  Serial.println();

  Serial.print("PASSKEY: ");
  for(uint8_t i = 0; i < sizeof(PASSKEY); i++){
    if(PASSKEY[i] < 0x10){
      Serial.print("0");
    }
    Serial.print(PASSKEY[i], HEX);
  }
  Serial.println();*/

  nRFCrypto.end();
}

// message, encrypt on/off, echo number, message id
void transmit(const char msg[208], unsigned char enc, unsigned char ech, unsigned char mid[4], unsigned long ts){
  //6 header
  //16 nullblock
  //16 user
  
  // 160 is max message size because after mode and user, encrypted messages are base64 encoded and PKCS7 padded
  uint8_t msgLen = strlen(msg); 
  unsigned char outgoingMSG[246];
  memset(outgoingMSG, 0, sizeof(outgoingMSG));

  // header
  outgoingMSG[0] = enc;
  outgoingMSG[1] = ech;
  for(uint8_t i = 0; i < 4; i++){
    outgoingMSG[i+2] = mid[i];
  }
  
  logMsgID(blockToLong(mid), ts);

  if(msgLen <= 208){
     if(enc == 0x01){ // if encrypt
      char encBlock[16]; // encrypted block
      char ptBlock[16]; // plaintext block
      uint8_t packets = msgLen / 16 + !!(msgLen%16);
      //Serial.print("msg Packets: "); Serial.println(packets);
      uint8_t hexOut[(packets * 16) + 32]; // null block + username + message
      //Serial.print("hexOut Size: "); Serial.println(sizeof(hexOut));

      // this block is used to verify proper decryption
      char nullBlock[16];
      memset(encBlock, 0, 16);
      memset(nullBlock, 0, 16);
      aes.Process(nullBlock, 16, IV, PASSKEY, sizeof(PASSKEY), encBlock, aes.encryptFlag, aes.cbcMode);
      for(uint8_t i = 0; i < 16; i++){
        hexOut[i] = encBlock[i];
      }

      uint8_t hexPos = 16;
      memset(encBlock, 0, 16);
      aes.Process((char*)USER, 16, IV, PASSKEY, sizeof(PASSKEY), encBlock, aes.encryptFlag, aes.cbcMode);
      for(uint8_t i = 0; i < 16; i++){
        hexOut[hexPos] = encBlock[i];
        hexPos++;
      }

      uint8_t ptPos = 0; // plaintext position
      hexPos = 32; // hexOut position
      for(uint8_t i = 0; i < packets; i++){
        memset(ptBlock, 0, 16);
        for(uint8_t i= 0; i < 16; i++){
          if(ptPos < msgLen){
            ptBlock[i] = msg[ptPos];
          }
          ptPos++;            
        }
        
        memset(encBlock, 0, 16);
        aes.Process(ptBlock, 16, IV, PASSKEY, sizeof(PASSKEY), encBlock, aes.encryptFlag, aes.cbcMode);
        for(uint8_t i = 0; i < 16; i++){
          hexOut[hexPos] = encBlock[i];
          hexPos++;
        }
      }

      /*Serial.print("PlainText Message: "); Serial.println(msg);
      Serial.print("PlainText Length: "); Serial.println(msgLen);

      Serial.print("Encrypted Message (HEX): ");
      for(uint8_t i = 0; i < sizeof(hexOut); i++){
        if(hexOut[i] < 0x10){
          Serial.print("0");
        }
        Serial.print(hexOut[i], HEX);
      }
      Serial.println();*/

      // copy hexOut over to outgoing message, leaving the 6 header bytes at the beginning
      for(uint8_t i = 0; i < sizeof(hexOut); i++){
        outgoingMSG[i+6] = hexOut[i];
      }
      
      /*Serial.print("outgoingMSG: ");
      for(uint8_t i = 0; i < sizeof(outgoingMSG); i++){
        if(outgoingMSG[i] < 0x10){
          Serial.print("0");
        }
        Serial.print(outgoingMSG[i], HEX);
      }
      Serial.println();*/

      // reset the counter
  hopsCompleted = 0;
  if(FHSS){
    // return to home channel before the next transaction
    int state = radio.setFrequency(channels[HC]);
    if (state != RADIOLIB_ERR_NONE) {
      Serial.print(F("[LoRa] Failed to change frequency, code "));
      Serial.println(errorCode[abs(state)]);
    }

    // set the function to call when we need to change frequency
    radio.setDio1Action(setFHSSFlag, RISING);
  }

      // set the function to call when transmission is finished
      radio.setDio0Action(setTxFlag, RISING);
      toaStart = millis();
      transmissionState = radio.startTransmit(outgoingMSG, sizeof(hexOut)+6);

      /*unsigned char finalOut[242];
      int base64_length = encode_base64(hexOut, sizeof(hexOut), finalOut);
      Serial.print("Encrypted Message (BASE64): "); Serial.write(finalOut, base64_length); Serial.println();*/
    }
    else { // encrypt = false
      uint8_t outPos = (strlen((char*)USER))+6; // leave room for 6 header bytes
      for(uint8_t i = 0; i < strlen((char*)USER); i++){
        outgoingMSG[i+6] = USER[i];
        //Serial.print((char)outgoingMSG[i+6]);
      }
      outgoingMSG[outPos] = ':';
      outPos++;
      outgoingMSG[outPos] = ' ';
      outPos++;
      for(uint8_t i = 0; i < msgLen; i++){
        outgoingMSG[outPos] = msg[i];
        outPos++;
      }

      /*Serial.print("Message: ");
      for(uint8_t i = 0; i < outPos; i++){
        if(outgoingMSG[i] < 0x10){
          Serial.print("0");
        }
        Serial.print(outgoingMSG[i], HEX);
      }
      Serial.println();*/

      // reset the counter
  hopsCompleted = 0;
  if(FHSS){
    // return to home channel before the next transaction
    int state = radio.setFrequency(channels[HC]);
    if (state != RADIOLIB_ERR_NONE) {
      Serial.print(F("[LoRa] Failed to change frequency, code "));
      Serial.println(errorCode[abs(state)]);
    }

    // set the function to call when we need to change frequency
    radio.setDio1Action(setFHSSFlag, RISING);
  }
      
      // set the function to call when transmission is finished
      radio.setDio0Action(setTxFlag, RISING);
      toaStart = millis();
      transmissionState = radio.startTransmit(outgoingMSG, outPos);
    }
  }
}

unsigned long blockToLong(uint8_t inBlock[4]){
  unsigned long longOut = 0;
  longOut = (uint32_t) inBlock[3] << 0;
  longOut |= (uint32_t) inBlock[2] << 8;
  longOut |= (uint32_t) inBlock[1] << 16;
  longOut |= (uint32_t) inBlock[0] << 24;
  return longOut;
}

void logMsgID(unsigned long id, unsigned long ts){
  unsigned long lowestTS = ts;
  uint8_t index;
  for(uint8_t i = 0; i < 100; i++){
    if(echoTS[i] < lowestTS){
      lowestTS = echoTS[i];
      index = i;
    }
  }
  echoTS[index] = ts;
  echoID[index] = id;

  // testing
  /*for(uint8_t i = 0; i < 100; i++){
    if(echoTS[i] != 0){
      Serial.print("echoID: "); Serial.print(echoID[i]); Serial.print('\t');
      Serial.print("echoTS: "); Serial.print(echoTS[i]); Serial.println();
    }
  }*/
}

volatile bool isNewEcho(unsigned long id){
  volatile bool isNew = true;
  for(uint8_t i = 0; i < 100; i++){
    //Serial.println(echoID[i]);
    if(echoID[i] == id){
      isNew = false;
      break;
    }
  }
  return isNew;
}

void exportData(char exd[255], uint8_t len){
  if(ENAP){
    Serial.write(exd, len);
    Serial1.write(exd, len);
    bleuart.write(exd, MTU);
  } else if (periph == 0){
    Serial.write(exd, len);
  } else if (periph == 1){
    Serial1.write(exd, len);
  } else if (periph == 2){
    bleuart.write(exd, MTU);
  }
}

void exportData(uint8_t exd[255], uint8_t len){
  if(ENAP){
    Serial.write(exd, len);
    Serial1.write(exd, len);
    bleuart.write(exd, MTU);
  } else if (periph == 0){
    Serial.write(exd, len);
  } else if (periph == 1){
    Serial1.write(exd, len);
  } else if (periph == 2){
    bleuart.write(exd, MTU);
  }
}