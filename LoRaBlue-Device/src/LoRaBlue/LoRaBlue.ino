//LoRaBlue Power Bank Standby Time: 18 hours

// Recieving Current: <20mA
// Sending Current: <85mA
// Sleep Current: <0.4mA

/*********************************************************************
 This is an example for our nRF52 based Bluefruit LE modules

 Pick one up today in the adafruit shop!

 Adafruit invests time and resources providing this open source code,
 please support Adafruit and open-source hardware by purchasing
 products from Adafruit!

 MIT license, check LICENSE for more information
 All text above, and the splash screen below must be included in
 any redistribution
*********************************************************************/
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

float version = 1.18;
uint8_t periph = 0; //used to respond on appropriate peripheral

// These settings have been painstakingly tested for best results
// Essentially you can just NEVER EVER have a hopping period of less than 12. Even if it appears to work, it doesn't
// range(%)           {41%   58%   82%   100%  }
// range(mi)          {1     1.5   2     2.5   } (With one upstairs)
// seconds            {0.5   1.7   8.5   17    }
float bwIndex[6] =    {500,  250,  62.5, 31.25 }; 
uint16_t hpIndex[6] = {255,  125,  25,   12,   };    
uint8_t sfIndex[6] =  {9,    10,   10,   10,   };    
uint8_t crIndex[6] =  {8,    8,    8,    8,    };
// Link Budget        {143,  149,  155,  158.4 }
// Rec. Sensitivity   {-123, -129, -135, -138.4}
// LB-RS              {266,  278,  290,  296.8 }

volatile bool transmittedFlag = false;
volatile bool receivedFlag = false;
volatile bool fhssChangeFlag = false;
static float channels[128] = {902.20,902.40,902.60,902.80,903.00,903.20,903.40,903.60,903.80,904.00,904.20,904.40,904.60,904.80,905.00,905.20,905.40,905.60,905.80,906.00,906.20,906.40,906.60,906.80,907.00,907.20,907.40,907.60,907.80,908.00,908.20,908.40,908.60,908.80,909.00,909.20,909.40,909.60,909.80,910.00,910.20,910.40,910.60,910.80,911.00,911.20,911.40,911.60,911.80,912.00,912.20,912.40,912.60,912.80,913.00,913.20,913.40,913.60,913.80,914.00,914.20,914.40,914.60,914.80,915.00,915.20,915.40,915.60,915.80,916.00,916.20,916.40,916.60,916.80,917.00,917.20,917.40,917.60,917.80,918.00,918.20,918.40,918.60,918.80,919.00,919.20,919.40,919.60,919.80,920.00,920.20,920.40,920.60,920.80,921.00,921.20,921.40,921.60,921.80,922.00,922.20,922.40,922.60,922.80,923.00,923.20,923.40,923.60,923.80,924.00,924.20,924.40,924.60,924.80,925.00,925.20,925.40,925.60,925.80,926.00,926.20,926.40,926.60,926.80,927.00,927.20,927.40,927.60};
int numberOfChannels = 128;
int hopsCompleted = 0;
int packetCounter = 0;
int transmissionState = RADIOLIB_ERR_NONE;

void setTxFlag(void) {
  transmittedFlag = true;
}
void setRxFlag(void) {
  receivedFlag = true;
}
void setFHSSFlag(void) {
  fhssChangeFlag = true;
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
  bool ledStatus = true;
  //pinMode(LED_BUILTIN, OUTPUT); // output by default
  digitalWrite(LED_BUILTIN, HIGH);

  if(!Serial){
    Serial.begin(115200);
  }

  delay(1000);
  if ( !Serial ) yield();
  
  // flash delay
  /*for(uint8_t i = 0; i < 30; i++){
    ledStatus = ledStatus^1; // toggle status
    digitalWrite(LED_BUILTIN, ledStatus);
    delay(100);
  }*/

  // Initialize Internal File System
  InternalFS.begin();
  //InternalFS.format(); // erase all internal data

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
    
    radio.setDio0Action(setRxFlag, RISING);
    radio.setDio1Action(setFHSSFlag, RISING);
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
    //transmit(ENCRYPT, "This is a test message\n");
    
    Scheduler.startLoop(atLoop);

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

void atLoop(){
  if (Serial.available())
  {
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
      Serial.print(callCMD(buf, len));
    } else { // this should send to LoRa
      //Serial.write(buf, sizeof(buf)); //Serial.println();
      if(DEBUG){
        Serial.println("[LoRa] Transmitting...");
      }
      if(LORACHAT){
        transmit(ENCRYPT, (char*)buf);
      } else {
        rawTransmit(buf, len);
      }
    }
  }

  if (Serial1.available())
  {
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
      Serial1.print(callCMD(buf, len));
    } else { // this should send to LoRa
      //Serial.write(buf, sizeof(buf)); //Serial.println();
      if(DEBUG){
        Serial1.println("[LoRa] Transmitting...");
      }
      if(LORACHAT){
        transmit(ENCRYPT, (char*)buf);
      } else {
        rawTransmit(buf, len);
      }
    }
    //bleuart.write( buf, count );
  }
  
  // Forward from BLEUART to HW Serial
  if( bleuart.available() )
  {
    periph = 2;
    delay(1000);
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
      char response[247]; // app always gets 3 less than we send so it will only read 244 instead of 247
      memset(response,0, sizeof(response));
      callCMD(buf, sizeof(buf)).toCharArray(response, sizeof(response));
      bleuart.write(response, sizeof(response));
    } else { // this should send to LoRa
      //Serial.write(buf, sizeof(buf)); //Serial.println();
      if(DEBUG){
        bleuart.println("[LoRa] Transmitting...");
      }
      if(LORACHAT){
        transmit(ENCRYPT, (char*)buf);
      } else {
        rawTransmit(buf, len);
      }
    }
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
    
    ///////////////////////////////////////////////////////////////////////////
    // These appear to be necessary RIGHT HERE to keep everything in sync    //
    radio.clearFHSSInt(); // clear the FHSS interrupt                        //
    fhssChangeFlag = false; // we're ready to do another hop, clear the flag //
    ///////////////////////////////////////////////////////////////////////////

    uint8_t incoming[241];
    uint8_t message[247];
    bool relayMSG = true; // will change to false if not decrypted properly
    memset(incoming, 0, sizeof(incoming)); //typically not necessary but here it is
    memset(message, 0, sizeof(message));
    int state = radio.readData(incoming, 0); // 0 apparently reads all
    if (state == RADIOLIB_ERR_NONE) {
      if(LORACHAT){
        if(incoming[0] == 0x00){
          //Serial.println("Unencrypted LoRaChat");
          for(uint8_t i = 0; i < sizeof(incoming)-1; i++){
            message[i] = incoming[i+1];
          }
        } else if(incoming[0] == 0x01){
          //Serial.println("Encrypted LoRaChat");
          uint8_t blocks = 0;
          uint8_t msgPos = 1;
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
              encBlock[i] = incoming[i+1];
            }
            aes.Process(encBlock, 16, IV, PASSKEY, sizeof(PASSKEY), decBlock, aes.decryptFlag, aes.cbcMode);
            if(memcmp(decBlock, emptyBlock, 16) == 0){
              //Serial.println("Pass Phrases Match!");
              // get sender
              for(uint8_t i = 0; i < 16; i++){
                encBlock[i] = incoming[i+17];
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
              uint8_t inPos = 33;
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
        // LORACHAT off
        memcpy(message, incoming, sizeof(incoming));
      }

      if(relayMSG){
        if(periph == 0){
          Serial.write(message, sizeof(message));
        } 
        else if(periph == 1){
          Serial1.write(message, sizeof(message));
        }
        else if(periph == 2){
          bleuart.write(message, sizeof(message));
        }

        if (DEBUG){
          // see note on toaMax to understand why we process the time in the way below (we essentially ignore anything prior to the first hop)
          unsigned long processed_time = (toaStop-toaStart)-toaMax;
          String chu = "[LoRa] Channels Hopped: " + String(hopsCompleted);
          String rssi = "[LoRa] Signal Strength Indicator (RSSI): " + String(radio.getRSSI()) + "dBm";
          String snr = "[LoRa] Signal-to-Noise Ratio (SNR): " + String(radio.getSNR());
          String fer = "[LoRa] Frequency Error: " + String(radio.getFrequencyError()) + "Hz";
        
          if(periph == 0){
            Serial.println(chu); // channnels used
            Serial.println(rssi); // RSSI
            Serial.println(snr); // SNR
            Serial.println(fer); // Freq Error
          } 
          else if(periph == 1){
            Serial1.println(chu); // channnels used
            Serial1.println(rssi); // RSSI
            Serial1.println(snr); // SNR
            Serial1.println(fer); // Freq Error
          }
          else if(periph == 2){
            bleuart.println(chu); // channnels used
            bleuart.println(rssi); // RSSI
            bleuart.println(snr); // SNR
            bleuart.println(fer); // Freq Error
          }
        }
      }
    } else {
      if(periph == 0){
        Serial.println("AT+ERROR=" + errorCode[abs(state)]);
      } 
      else if(periph == 1){
        Serial1.println("AT+ERROR=" + errorCode[abs(state)]);
      }
      else if(periph == 2){
        char bleout[247] = "AT+ERROR=";
        char tempCode[128];
        memset(tempCode, 0, sizeof(tempCode));
        String errorString = errorCode[abs(transmissionState)] + "\n";
        errorString.toCharArray(tempCode, errorString.length()+1);
        strcat(bleout, tempCode);
        bleuart.write(bleout, sizeof(bleout));
      }
    }

    // we're ready to receive more packets, clear the flag
    receivedFlag = false;

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

    char bleout[247] = "AT+ERROR=";

    if(periph == 0){
      Serial.println("AT+ERROR=" + errorCode[abs(transmissionState)]);
    } 
    else if(periph == 1){
      Serial1.println("AT+ERROR=" + errorCode[abs(transmissionState)]);
    }
    else if(periph == 2){
      char tempCode[128];
      memset(tempCode, 0, sizeof(tempCode));
      String errorString = errorCode[abs(transmissionState)] + "\n";
      errorString.toCharArray(tempCode, errorString.length()+1);
      strcat(bleout, tempCode);
    }
    if (transmissionState == RADIOLIB_ERR_NONE) {
      if (DEBUG){
        // see note on toaMax to understand why we process the time in the way below (we essentially ignore anything prior to the first hop)
        unsigned long processed_time = (toaStop-toaStart)-toaMax;
        String chu = "[LoRa] Channels Hopped: " + String(hopsCompleted+1);
        String toa = "[LoRa] Time On Air: " + String(toaStop-toaStart) + "ms";
        String dtc = "[LoRa] Dwell Time / Channel: " + String(processed_time/hopsCompleted) + "ms";
        
        if(periph == 0){
          Serial.println(chu); // channnels used
          Serial.println(toa); // time on air
          Serial.println(dtc); // dwell time per channel
        } 
        else if(periph == 1){
          Serial1.println(chu); // channnels used
          Serial1.println(toa); // time on air
          Serial1.println(dtc); // dwell time per channel
        }
        else if(periph == 2){
          char chuChar[128];
          memset(chuChar, 0, sizeof(chuChar));
          chu = chu + "\n";
          chu.toCharArray(chuChar, chu.length()+1);
          strcat(bleout, chuChar);

          char toaChar[128];
          memset(toaChar, 0, sizeof(toaChar));
          toa = toa + "\n";
          toa.toCharArray(toaChar, toa.length()+1);
          strcat(bleout, toaChar);

          char dtcChar[128];
          memset(dtcChar, 0, sizeof(dtcChar));
          dtc = dtc + "\n";
          dtc.toCharArray(dtcChar, dtc.length()+1);
          strcat(bleout, dtcChar);
        }
        
      }
    }

    bleuart.write(bleout, sizeof(bleout));
    
    // reset the counter
    hopsCompleted = 0;
    if(FHSS){
      // return to home channel before the next transaction
      int state = radio.setFrequency(channels[HC]);
      if (state != RADIOLIB_ERR_NONE) {
        Serial.print(F("[LoRa] Failed to change frequency, code "));
        Serial.println(errorCode[abs(state)]);
      }
    }

    // we're ready to receive more packets, clear the flag
    receivedFlag = false;

    /*// clear all buffers and set transmitting to false
    while(Serial.available()){
      Serial.read();
    }
    while(Serial1.available()){
      Serial1.read();
    }
    while(bleuart.available()){
      bleuart.read();
    }*/

    // switch to receive
    // set the function to call when reception is finished
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

// callback invoked when central connects
void connect_callback(uint16_t conn_handle)
{
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
  //transmitting = true;
  
  //String message = "Test";
  //String message = "This is a test message. We want it to be 247 characters so we are kind of just writing some filler information until we get there. If we have the maximum amount of characters, our time on air should be fairly accurate. This should be enough......";
  int state = 0;
  
  // set the function to call when transmission is finished
  radio.setDio0Action(setTxFlag, RISING);

  // set the function to call when we need to change frequency
  radio.setDio1Action(setFHSSFlag, RISING);
  toaStart = millis();
  transmissionState = radio.startTransmit(buf, len);
}

void reset(){
  Scheduler.startLoop(resetLoop);
}

void resetLoop(){
  //reset 0.5 second in future
  delay(250);
  if(periph == 0){
    Serial.println("AT+RESETTING");
  } 
  else if(periph == 1){
    Serial1.println("AT+RESETTING");
  }
  else if(periph == 2){
    bleuart.println("AT+RESETTING");
  }
  delay(250);
  

  // anything you can call 'end()' on needs to be called prior to 'NVIC_SystemReset'
  Serial.flush();
  Serial1.flush();
  Serial.end();
  Serial1.end();
  InternalFS.end();
  NVIC_SystemReset();
}

String callCMD(uint8_t* buf, uint8_t len){
  String ret = "";
  if(strncmp((char*)buf, (char*)"AT+VERSION", sizeof("AT+VERSION")-1) == 0){ // this should process AT commands
    ret = "AT+VERSION=" + String(version);
  }
  else if(strncmp((char*)buf, (char*)"AT+RESET", sizeof("AT+RESET")-1) == 0){ // this should process AT commands
    ret = "AT+OK";
    reset();
  }
  else if(strncmp((char*)buf, (char*)"AT+OTA", sizeof("AT+OTA")-1) == 0){ // this should process AT commands
    ret = "AT+OK";
    NRF_POWER->GPREGRET = 0xA8;
    reset();
  }
  else if(strncmp((char*)buf, (char*)"AT+UF2", sizeof("AT+UF2")-1) == 0){ // this should process AT commands
    ret = "AT+OK";
    NRF_POWER->GPREGRET = 0x57;
    reset();
  }
  else if(strncmp((char*)buf, (char*)"AT+DEFAULT=", sizeof("AT+DEFAULT=")-1) == 0){ // set LoRa to recommended settings (0-3)
    uint8_t pos = 0;
    uint8_t bufStart = sizeof("AT+DEFAULT=")-1;
    char process[len-bufStart];
    for(uint8_t i = pos; i < sizeof(process); i++){
      if((char)buf[pos+bufStart] != '\r'){
        if((char)buf[pos+bufStart] != '\n'){
          if(buf[pos+bufStart] != 0){
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
    uint8_t pos = 0;
    uint8_t bufStart = sizeof("AT+SLEEP=")-1;
    char process[len-bufStart];
    for(uint8_t i = pos; i < sizeof(process); i++){
      if((char)buf[pos+bufStart] != '\r'){
        if((char)buf[pos+bufStart] != '\n'){
          if(buf[pos+bufStart] != 0){
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
        Serial1.println("AT+OK");
        delay(250);
        pinMode(SWITCH, INPUT_PULLDOWN_SENSE);  // this pin (SWITCH PIN) is pulled down and wakes up the board when externally connected to 3.3v.
        pinMode(0, INPUT_PULLUP_SENSE);  // this pin (UART RX) is pulled up and wakes up the board when externally connected to GND or recieves UART communication
        sd_power_system_off(); // this function puts the whole nRF52 to deep sleep (no Bluetooth)
      }
      else if(periph == 2){
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
    uint8_t pos = 0;
    uint8_t bufStart = sizeof("AT+SETRANGE=")-1;
    char process[len-bufStart];
    for(uint8_t i = pos; i < sizeof(process); i++){
      if((char)buf[pos+bufStart] != '\r'){
        if((char)buf[pos+bufStart] != '\n'){
          if(buf[pos+bufStart] != 0){
            process[pos] = buf[pos+bufStart]; //buf[pos];
            pos++;
          }
        }
      }
    }
    uint8_t SETRANGE = String(process).toInt();
    if(SETRANGE < 5){
      FHSS = 1;
      HC = 64;
      FREQ = channels[HC];
      BW = bwIndex[SETRANGE];
      HP = hpIndex[SETRANGE];
      SF = sfIndex[SETRANGE];
      CR = crIndex[SETRANGE];
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
    uint8_t pos = 0;
    uint8_t bufStart = sizeof("AT+BTEN=")-1;
    char process[len-bufStart];
    for(uint8_t i = pos; i < sizeof(process); i++){
      if((char)buf[pos+bufStart] != '\r'){
        if((char)buf[pos+bufStart] != '\n'){
          if(buf[pos+bufStart] != 0){
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
    uint8_t pos = 0;
    uint8_t bufStart = sizeof("AT+NAME=")-1;
    char process[len-bufStart];
    memset(process, 0, sizeof(process));
    for(uint8_t i = pos; i < sizeof(process); i++){
      if((char)buf[pos+bufStart] != '\r'){
        if((char)buf[pos+bufStart] != '\n'){
          if(buf[pos+bufStart] != 0){
            process[pos] = buf[pos+bufStart]; //buf[pos];
            pos++;
          }
        }
      }
    }
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
  }
  else if(strncmp((char*)buf, (char*)"AT+NAME", sizeof("AT+NAME")-1) == 0){ // get name
    file.open("NAME", FILE_O_READ);
    ret = "AT+NAME=" + file.readString();
  }
  else if(strncmp((char*)buf, (char*)"AT+PIN=", sizeof("AT+PIN=")-1) == 0){ // set new name
    uint8_t pos = 0;
    uint8_t bufStart = sizeof("AT+PIN=")-1;
    char process[len-bufStart];
    for(uint8_t i = pos; i < sizeof(process); i++){
      if((char)buf[pos+bufStart] != '\r'){
        if((char)buf[pos+bufStart] != '\n'){
          if(buf[pos+bufStart] != 0){
            process[pos] = buf[pos+bufStart]; //buf[pos];
            pos++;
          }
        }
      }
    }
    if((pos > 3) && (pos < 7)){ // 4-6 characters
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
      ret = "AT+ERROR=INVALID_SIZE";
    }
    // in this case, chamges won't take effect until reset
  }
  else if(strncmp((char*)buf, (char*)"AT+PIN", sizeof("AT+PIN")-1) == 0){ // get name
    file.open("PIN", FILE_O_READ);
    ret = "AT+PIN=" + file.readString();
  }
  else if(strncmp((char*)buf, (char*)"AT+BAUD=", sizeof("AT+BAUD=")-1) == 0){ // set new name
    uint8_t pos = 0;
    uint8_t bufStart = sizeof("AT+BAUD=")-1;
    char process[len-bufStart];
    for(uint8_t i = pos; i < sizeof(process); i++){
      if((char)buf[pos+bufStart] != '\r'){
        if((char)buf[pos+bufStart] != '\n'){
          if(buf[pos+bufStart] != 0){
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
    uint8_t pos = 0;
    uint8_t bufStart = sizeof("AT+DEBUG=")-1;
    char process[len-bufStart];
    for(uint8_t i = pos; i < sizeof(process); i++){
      if((char)buf[pos+bufStart] != '\r'){
        if((char)buf[pos+bufStart] != '\n'){
          if(buf[pos+bufStart] != 0){
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
    uint8_t pos = 0;
    uint8_t bufStart = sizeof("AT+FHSS=")-1;
    char process[len-bufStart];
    for(uint8_t i = pos; i < sizeof(process); i++){
      if((char)buf[pos+bufStart] != '\r'){
        if((char)buf[pos+bufStart] != '\n'){
          if(buf[pos+bufStart] != 0){
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
    uint8_t pos = 0;
    uint8_t bufStart = sizeof("AT+HC=")-1;
    char process[len-bufStart];
    for(uint8_t i = pos; i < sizeof(process); i++){
      if((char)buf[pos+bufStart] != '\r'){
        if((char)buf[pos+bufStart] != '\n'){
          if(buf[pos+bufStart] != 0){
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
    uint8_t pos = 0;
    uint8_t bufStart = sizeof("AT+FREQ=")-1;
    char process[len-bufStart];
    for(uint8_t i = pos; i < sizeof(process); i++){
      if((char)buf[pos+bufStart] != '\r'){
        if((char)buf[pos+bufStart] != '\n'){
          if(buf[pos+bufStart] != 0){
            process[pos] = buf[pos+bufStart]; //buf[pos];
            pos++;
          }
        }
      }
    }
    if(FHSS){
      ret = "AT+ERROR=NOT_AVAILABLE_WITH_FHSS";
    } else {
      int state = radio.setFrequency(String(process).toFloat());
      if (state == RADIOLIB_ERR_NONE) {
        FREQ = String(process).toFloat();
        ret = "AT+OK";
      } else {
        ret = "AT+ERROR=" + errorCode[abs(state)];
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
    uint8_t pos = 0;
    uint8_t bufStart = sizeof("AT+HP=")-1;
    char process[len-bufStart];
    for(uint8_t i = pos; i < sizeof(process); i++){
      if((char)buf[pos+bufStart] != '\r'){
        if((char)buf[pos+bufStart] != '\n'){
          if(buf[pos+bufStart] != 0){
            process[pos] = buf[pos+bufStart]; //buf[pos];
            pos++;
          }
        }
      }
    }
    int state = radio.setFHSSHoppingPeriod(String(process).toInt());
    if (state == RADIOLIB_ERR_NONE) {
      FHSS = 1;
      HP = String(process).toInt();
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
    uint8_t pos = 0;
    uint8_t bufStart = sizeof("AT+BW=")-1;
    char process[len-bufStart];
    for(uint8_t i = pos; i < sizeof(process); i++){
      if((char)buf[pos+bufStart] != '\r'){
        if((char)buf[pos+bufStart] != '\n'){
          if(buf[pos+bufStart] != 0){
            process[pos] = buf[pos+bufStart]; //buf[pos];
            pos++;
          }
        }
      }
    }
    int state = radio.setBandwidth(String(process).toFloat());
    if (state == RADIOLIB_ERR_NONE) {
      BW = String(process).toFloat();
      setConfig();
      ret = "AT+OK";
    } else {
      ret = "AT+ERROR=" + errorCode[abs(state)];
    }
  }
  else if(strncmp((char*)buf, (char*)"AT+BW", sizeof("AT+BW")-1) == 0){ // get name
    ret = "AT+BW=" + String(BW);
  }
  else if(strncmp((char*)buf, (char*)"AT+SF=", sizeof("AT+SF=")-1) == 0){ // set new name
    uint8_t pos = 0;
    uint8_t bufStart = sizeof("AT+SF=")-1;
    char process[len-bufStart];
    for(uint8_t i = pos; i < sizeof(process); i++){
      if((char)buf[pos+bufStart] != '\r'){
        if((char)buf[pos+bufStart] != '\n'){
          if(buf[pos+bufStart] != 0){
            process[pos] = buf[pos+bufStart]; //buf[pos];
            pos++;
          }
        }
      }
    }
    int state = radio.setSpreadingFactor(String(process).toInt());
    if (state == RADIOLIB_ERR_NONE) {
      SF = String(process).toInt();
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
    uint8_t pos = 0;
    uint8_t bufStart = sizeof("AT+CRC=")-1;
    char process[len-bufStart];
    for(uint8_t i = pos; i < sizeof(process); i++){
      if((char)buf[pos+bufStart] != '\r'){
        if((char)buf[pos+bufStart] != '\n'){
          if(buf[pos+bufStart] != 0){
            process[pos] = buf[pos+bufStart]; //buf[pos];
            pos++;
          }
        }
      }
    }
    int state = radio.setCRC(String(process).toInt());
    if (state == RADIOLIB_ERR_NONE) {
      CRC = String(process).toInt();
      setConfig();
      ret = "AT+OK";
    } else {
      ret = "AT+ERROR=" + errorCode[abs(state)];
    }
    // in this case, chamges won't take effect until reset
  }
  else if(strncmp((char*)buf, (char*)"AT+CRC", sizeof("AT+CRC")-1) == 0){ // get name
    ret = "AT+CRC=" + String(CRC);
  }
  else if(strncmp((char*)buf, (char*)"AT+CR=", sizeof("AT+CR=")-1) == 0){ // set new name
    uint8_t pos = 0;
    uint8_t bufStart = sizeof("AT+CR=")-1;
    char process[len-bufStart];
    for(uint8_t i = pos; i < sizeof(process); i++){
      if((char)buf[pos+bufStart] != '\r'){
        if((char)buf[pos+bufStart] != '\n'){
          if(buf[pos+bufStart] != 0){
            process[pos] = buf[pos+bufStart]; //buf[pos];
            pos++;
          }
        }
      }
    }
    int state = radio.setCodingRate(String(process).toFloat());
    if (state == RADIOLIB_ERR_NONE) {
      CR = String(process).toInt();
      setConfig();
      ret = "AT+OK";
    } else {
      ret = "AT+ERROR=" + errorCode[abs(state)];
    }
  }
  else if(strncmp((char*)buf, (char*)"AT+CR", sizeof("AT+CR")-1) == 0){ // get name
    ret = "AT+CR=" + String(CR);
  }
  else if(strncmp((char*)buf, (char*)"AT+SW=", sizeof("AT+SW=")-1) == 0){ // set new name
    uint8_t pos = 0;
    uint8_t bufStart = sizeof("AT+SW=")-1;
    char process[len-bufStart];
    for(uint8_t i = pos; i < sizeof(process); i++){
      if((char)buf[pos+bufStart] != '\r'){
        if((char)buf[pos+bufStart] != '\n'){
          if(buf[pos+bufStart] != 0){
            process[pos] = buf[pos+bufStart]; //buf[pos];
            pos++;
          }
        }
      }
    }
    int state = radio.setSyncWord(String(process).toInt());
    if (state == RADIOLIB_ERR_NONE) {
      SW = String(process).toInt();
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
    uint8_t pos = 0;
    uint8_t bufStart = sizeof("AT+PRE=")-1;
    char process[len-bufStart];
    for(uint8_t i = pos; i < sizeof(process); i++){
      if((char)buf[pos+bufStart] != '\r'){
        if((char)buf[pos+bufStart] != '\n'){
          if(buf[pos+bufStart] != 0){
            process[pos] = buf[pos+bufStart]; //buf[pos];
            pos++;
          }
        }
      }
    }
    int state = radio.setPreambleLength(String(process).toInt());
    if (state == RADIOLIB_ERR_NONE) {
      PRE = String(process).toInt();
      setConfig();
      ret = "AT+OK";
    } else {
      ret = "AT+ERROR=" + errorCode[abs(state)];
    }
  }
  else if(strncmp((char*)buf, (char*)"AT+PRE", sizeof("AT+PRE")-1) == 0){ // get name
    ret = "AT+PRE=" + String(PRE);
  }
  else if(strncmp((char*)buf, (char*)"AT+PWR=", sizeof("AT+PWR=")-1) == 0){ // set new name
    uint8_t pos = 0;
    uint8_t bufStart = sizeof("AT+PWR=")-1;
    char process[len-bufStart];
    for(uint8_t i = pos; i < sizeof(process); i++){
      if((char)buf[pos+bufStart] != '\r'){
        if((char)buf[pos+bufStart] != '\n'){
          if(buf[pos+bufStart] != 0){
            process[pos] = buf[pos+bufStart]; //buf[pos];
            pos++;
          }
        }
      }
    }
    int state = radio.setOutputPower(String(process).toInt());
    if (state == RADIOLIB_ERR_NONE) {
      PWR = String(process).toInt();
      setConfig();
      ret = "AT+OK";
    } else {
      ret = "AT+ERROR=" + errorCode[abs(state)];
    }
    // in this case, chamges won't take effect until reset
  }
  else if(strncmp((char*)buf, (char*)"AT+PWR", sizeof("AT+PWR")-1) == 0){ // get name
    ret = "AT+PWR=" + String(PWR);
  }
  else if(strncmp((char*)buf, (char*)"AT+GAIN=", sizeof("AT+GAIN=")-1) == 0){ // set new name
    uint8_t pos = 0;
    uint8_t bufStart = sizeof("AT+GAIN=")-1;
    char process[len-bufStart];
    for(uint8_t i = pos; i < sizeof(process); i++){
      if((char)buf[pos+bufStart] != '\r'){
        if((char)buf[pos+bufStart] != '\n'){
          if(buf[pos+bufStart] != 0){
            process[pos] = buf[pos+bufStart]; //buf[pos];
            pos++;
          }
        }
      }
    }
    int state = radio.setGain(String(process).toInt());
    if (state == RADIOLIB_ERR_NONE) {
      GAIN = String(process).toInt();
      setConfig();
      ret = "AT+OK";
    } else {
      ret = "AT+ERROR=" + errorCode[abs(state)];
    }
    // in this case, chamges won't take effect until reset
  }
  else if(strncmp((char*)buf, (char*)"AT+GAIN", sizeof("AT+GAIN")-1) == 0){ // get name
    ret = "AT+GAIN=" + String(GAIN);
  }
  else if(strncmp((char*)buf, (char*)"AT+LED=", sizeof("AT+LED=")-1) == 0){ // set new name
    uint8_t pos = 0;
    uint8_t bufStart = sizeof("AT+LED=")-1;
    char process[len-bufStart];
    for(uint8_t i = pos; i < sizeof(process); i++){
      if((char)buf[pos+bufStart] != '\r'){
        if((char)buf[pos+bufStart] != '\n'){
          if(buf[pos+bufStart] != 0){
            process[pos] = buf[pos+bufStart]; //buf[pos];
            pos++;
          }
        }
      }
    }
    if(String(process).toInt() < 4){
      LED = String(process).toInt();
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
  else if(strncmp((char*)buf, (char*)"AT+LORACHAT=", sizeof("AT+LORACHAT=")-1) == 0){ // set new name
    uint8_t pos = 0;
    uint8_t bufStart = sizeof("AT+LORACHAT=")-1;
    char process[len-bufStart];
    for(uint8_t i = pos; i < sizeof(process); i++){
      if((char)buf[pos+bufStart] != '\r'){
        if((char)buf[pos+bufStart] != '\n'){
          if(buf[pos+bufStart] != 0){
            process[pos] = buf[pos+bufStart]; //buf[pos];
            pos++;
          }
        }
      }
    }
    LORACHAT = String(process).toInt();
    setConfig();
    ret = "AT+OK";
    // in this case, chamges won't take effect until reset
  }
  else if(strncmp((char*)buf, (char*)"AT+LORACHAT", sizeof("AT+LORACHAT")-1) == 0){ // get name
    ret = "AT+LORACHAT=" + String(LORACHAT);
  }
  else if(strncmp((char*)buf, (char*)"AT+ENCRYPT=", sizeof("AT+ENCRYPT=")-1) == 0){ // set new name
    uint8_t pos = 0;
    uint8_t bufStart = sizeof("AT+ENCRYPT=")-1;
    char process[len-bufStart];
    for(uint8_t i = pos; i < sizeof(process); i++){
      if((char)buf[pos+bufStart] != '\r'){
        if((char)buf[pos+bufStart] != '\n'){
          if(buf[pos+bufStart] != 0){
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
    uint8_t pos = 0;
    uint8_t bufStart = sizeof("AT+USER=")-1;
    char process[len-bufStart];
    memset(process, 0, sizeof(process));
    for(uint8_t i = pos; i < sizeof(process); i++){
      if((char)buf[pos+bufStart] != '\r'){
        if((char)buf[pos+bufStart] != '\n'){
          if(buf[pos+bufStart] != 0){
            process[pos] = buf[pos+bufStart]; //buf[pos];
            pos++;
          }
        }
      }
    }
    memset(USER, 0, sizeof(USER));
    InternalFS.remove("USER");
    if( file.open("USER", FILE_O_WRITE) )
    {
      file.print(process);
      file.close();
      file.open("USER", FILE_O_READ);
      if ( file )
      {
        for(uint8_t i = 0; i < sizeof(USER); i++){
          if(file.available()){
            USER[i] = file.read();
          }
        }
        file.close();
        ret = "AT+OK";
      } else {
        ret = "AT+ERROR=FILE_FAILURE";
      }
      //reset();
    } else {
      ret = "AT+ERROR=FAILED_TO_WRITE";
    }
  }
  else if(strncmp((char*)buf, (char*)"AT+USER", sizeof("AT+USER")-1) == 0){ // get name
    file.open("USER", FILE_O_READ);
    ret = "AT+USER=" + file.readString();
    file.close();
  }
  else if(strncmp((char*)buf, (char*)"AT+PASSPHR=", sizeof("AT+PASSPHR=")-1) == 0){ // set new name
    uint8_t pos = 0;
    uint8_t bufStart = sizeof("AT+PASSPHR=")-1;
    char process[len-bufStart];
    memset(process, 0, sizeof(process));
    for(uint8_t i = pos; i < sizeof(process); i++){
      if((char)buf[pos+bufStart] != '\r'){
        if((char)buf[pos+bufStart] != '\n'){
          if(buf[pos+bufStart] != 0){
            process[pos] = buf[pos+bufStart]; //buf[pos];
            pos++;
          }
        }
      }
    }
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
    file.open("PASSPHR", FILE_O_READ);
    String passphrase = file.readString();
    file.close();
    ret = "AT+CONFIG="+devicename+','+String(version)+','+String(FHSS)+','+String(HC)+','+String(FREQ)+','+String(HP)+','+String(BW)+','+String(SF)+','+String(CR)+','+String(SW)+','+String(PRE)+','+String(PWR)+','+String(GAIN)+','+String(CRC)+','+String(LED)+','+String(ENCRYPT)+','+String((char*)USER)+','+passphrase;
  }
  /////////// End of variables ///////////
  else {
    ret = "AT+ERROR=UNKNOWN_COMMAND";
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

void transmit(uint8_t cmdByte, const char msg[208]){
  // 160 is max message size because after mode and user, encrypted messages are base64 encoded and PKCS7 padded
  uint8_t msgLen = strlen(msg);
  unsigned char outgoingMSG[241] = {cmdByte}; // first byte always command byte
  if(msgLen <= 208){
    if(cmdByte == 0x00){
      uint8_t outPos = (strlen((char*)USER))+1;
      for(uint8_t i = 1; i < outPos; i++){
        outgoingMSG[i] = USER[i-1];
      }
      outgoingMSG[outPos] = ':';
      outPos++;
      outgoingMSG[outPos] = ' ';
      outPos++;
      for(uint8_t i = 0; i < msgLen; i++){
        outgoingMSG[outPos] = msg[i];
        outPos++;
      }
      // set the function to call when transmission is finished
      radio.setDio0Action(setTxFlag, RISING);
      // set the function to call when we need to change frequency
      radio.setDio1Action(setFHSSFlag, RISING);
      toaStart = millis();
      transmissionState = radio.startTransmit(outgoingMSG, strlen(msg) + outPos);
    } else if(cmdByte == 0x01){ // if encrypt
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

      // copy hexOut over to outgoing message, leaving the 2 command bytes at the beginning
      for(uint8_t i = 0; i < sizeof(hexOut); i++){
        outgoingMSG[i+1] = hexOut[i];
      }
      
      /*Serial.print("outgoingMSG: ");
      for(uint8_t i = 0; i < sizeof(outgoingMSG); i++){
        if(outgoingMSG[i] < 0x10){
          Serial.print("0");
        }
        Serial.print(outgoingMSG[i], HEX);
      }
      Serial.println();*/

      // set the function to call when transmission is finished
      radio.setDio0Action(setTxFlag, RISING);
      // set the function to call when we need to change frequency
      radio.setDio1Action(setFHSSFlag, RISING);
      toaStart = millis();
      transmissionState = radio.startTransmit(outgoingMSG, sizeof(hexOut)+1);

      /*unsigned char finalOut[242];
      int base64_length = encode_base64(hexOut, sizeof(hexOut), finalOut);
      Serial.print("Encrypted Message (BASE64): "); Serial.write(finalOut, base64_length); Serial.println();*/
    }
  }
}
