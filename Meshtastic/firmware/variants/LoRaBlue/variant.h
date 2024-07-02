#ifndef _VARIANT_LORABLUE_
#define _VARIANT_LORABLUE_

#define LORABLUE

/** Master clock frequency */
#define VARIANT_MCK (64000000ul)

#define USE_LFXO // Board uses 32khz crystal for LF
//#define USE_LFRC    // Board uses RC for LF

/*----------------------------------------------------------------------------
 *        Headers
 *----------------------------------------------------------------------------*/

#include "WVariant.h"

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

// Number of pins defined in PinDescription array
#define PINS_COUNT (48)
#define NUM_DIGITAL_PINS (48)
#define NUM_ANALOG_INPUTS (6)
#define NUM_ANALOG_OUTPUTS (0)

/*
 * Analog pins
 */
#define PIN_A0 (-1)

// LEDs ( we have them all turned off ) (if you turn them on and go back to factory default firmware, unpair device and uninstall meshtastic app before uploading -1 values)
#define PIN_LED1 (-1) // LED PIN = P0.06
#define PIN_LED2 (-1)    

#define LED_BUILTIN (-1)
#define LED_CONN (-1)

#define LED_GREEN (-1)
#define LED_BLUE (-1)

#define LED_STATE_ON 0 // State when LED is litted

/*
 * Buttons
 */

//#define PIN_BUTTON1 (32 + 15) // P1.15 Built in button

/*
 * Analog pins
 */

//static const uint8_t A0 = PIN_A0;
#define ADC_RESOLUTION 14

// Other pins
#define PIN_AREF (-1) // AREF            Not yet used

//static const uint8_t AREF = PIN_AREF;

/*
 * Serial interfaces
 */
#define PIN_SERIAL1_RX (0 + 25)
#define PIN_SERIAL1_TX (0 + 24)

// Connected to Jlink CDC
#define PIN_SERIAL2_RX PIN_SERIAL1_RX
#define PIN_SERIAL2_TX PIN_SERIAL1_TX

/*
 * SPI Interfaces
 */
#define SPI_INTERFACES_COUNT 1
#define PIN_SPI_MISO (0 + 20) // MISO      P0.20 D23
#define PIN_SPI_MOSI (0 + 15) // MOSI      P0.15 D24
#define PIN_SPI_SCK (0 + 13)  // SCK       P0.13 D25

/*static const uint8_t SS =  (0 + 11); // P0.11 D12
static const uint8_t MOSI = PIN_SPI_MOSI;
static const uint8_t MISO = PIN_SPI_MISO;
static const uint8_t SCK = PIN_SPI_SCK;*/

/*
 * Wire Interfaces
 */ 
#define WIRE_INTERFACES_COUNT 1

#define PIN_WIRE_SDA (0 + 21) // SDA
#define PIN_WIRE_SCL (0 + 22) // SCL

/*
 * LoRa Interfaces
 */ 
#define USE_SX1276
#define SX127X_RESET (0 + 12) // P0.12 D13
#define SX127X_CS (0 + 11) // P0.11 D12
#define SX127X_DIO0 (0 + 4) // P0.04 D14
#define SX127X_DIO1 (0 + 30) // P0.30 D15
#define SX127X_BUSY (-1)
// #define SX128X_TXEN  (32 + 9)
// #define SX128X_RXEN  (0 + 12)

#define LORA_RESET SX127X_RESET
#define LORA_CS SX127X_CS
#define LORA_DIO1 SX127X_DIO1
#define LORA_DIO0 SX127X_DIO0
#define LORA_MISO PIN_SPI_MISO
#define LORA_MOSI PIN_SPI_MOSI
#define LORA_SCK PIN_SPI_SCK

/*
 * GPS Interfaces
 */ 
#define PIN_GPS_EN (-1)
#define PIN_GPS_PPS (-1) // Pulse per second input from the GPS

#define GPS_RX_PIN (-1)
#define GPS_TX_PIN (-1)

/*
// Battery
// The battery sense is hooked to pin A0 (5)
#define BATTERY_PIN PIN_A0
// and has 12 bit resolution
#define BATTERY_SENSE_RESOLUTION_BITS 12
#define BATTERY_SENSE_RESOLUTION 4096.0
#undef AREF_VOLTAGE
#define AREF_VOLTAGE 3.0
#define VBAT_AR_INTERNAL AR_INTERNAL_3_0
#define ADC_MULTIPLIER (1.73F)
*/

#ifdef __cplusplus
}
#endif

/*----------------------------------------------------------------------------
 *        Arduino objects - C++ only
 *----------------------------------------------------------------------------*/

#endif