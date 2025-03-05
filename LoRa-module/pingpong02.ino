/*
  RadioLib SX126x Ping-Pong Example

  This example is intended to run on two wio_sx1262_with_xiao_esp32s3_kit_class,
  and send packets between the two.

  For default module settings, see the wiki page
  https://github.com/jgromes/RadioLib/wiki/Default-configuration#sx126x---lora-modem

  For full API reference, see the GitHub Pages
  https://jgromes.github.io/RadioLib/

  error codes 
  https://jgromes.github.io/RadioLib/group__status__codes.html


  Seeed getting started

  https://wiki.seeedstudio.com/wio_sx1262_with_xiao_esp32s3_kit_class/

  
*/

#include <RadioLib.h>

#define INITIATING_NODE   // so it sends the first transmission

#define LORA_MISO 8   // confirmed
#define LORA_SCK 7    // confirmed
#define LORA_MOSI 9  // confirmed
#define LORA_CS 41    //NSS
#define LORA_DIO2 38
#define LORA_DIO1 39     // irq
#define LORA_RESET 42
#define LORA_BUSY 40

 
// this file contains binary patch for the SX1262
#include <modules/SX126x/patches/SX126x_patch_scan.h>

SX1262 radio = new Module(LORA_CS, LORA_DIO1, LORA_RESET, LORA_BUSY);
  // int state = radio.begin(frequency, bandwidth, spreadingFactor, codingRate, syncWord, outputPower, preambleLength, 1.6, false);
  // int state = radio.begin(915.0, 500.0, 6, 5, 0x34, 2, 20);  // LoRaWan
  // int state = radio.begin(915.0, 125.0, 7, 5, 0x12, 14, 8);  // LoRa like portenta



volatile bool operationDone = false;
bool transmitFlag = false;
int transmissionState = RADIOLIB_ERR_NONE;

#if defined(ESP8266) || defined(ESP32)
ICACHE_RAM_ATTR
#endif
void setFlag(void) {
  operationDone = true;
}

// Function to validate parameters
bool validateParameters(float frequency, float bandwidth, uint8_t spreadingFactor, uint8_t codingRate, uint8_t syncWord, float outputPower, uint16_t preambleLength) {
  if (frequency < 150.0 || frequency > 960.0) {
    Serial.println(F("Error: Frequency must be between 150.0 MHz and 960.0 MHz."));
    return false;
  }
  if (bandwidth != 7.8 && bandwidth != 10.4 && bandwidth != 15.6 && bandwidth != 20.8 && bandwidth != 31.25 &&
      bandwidth != 41.7 && bandwidth != 62.5 && bandwidth != 125.0 && bandwidth != 250.0 && bandwidth != 500.0) {
    Serial.println(F("Error: Invalid bandwidth value."));
    return false;
  }
  if (spreadingFactor < 6 || spreadingFactor > 12) {
    Serial.println(F("Error: Spreading factor must be between 6 and 12."));
    return false;
  }
  if (codingRate < 5 || codingRate > 8) {
    Serial.println(F("Error: Coding rate must be between 5 and 8."));
    return false;
  }
  if (outputPower < -17.0 || outputPower > 22.0) {
    Serial.println(F("Error: Output power must be between -17 dBm and 22 dBm."));
    return false;
  }
  if (preambleLength < 6 || preambleLength > 65535) {
    Serial.println(F("Error: Preamble length must be between 6 and 65535 symbols."));
    return false;
  }
  if (syncWord > 0xFF) {
    Serial.println(F("Error: Sync word must be a valid 1-byte value."));
    return false;
  }
  return true;
}

// Initialize LoRa module with parameter validation
void initRadio() {
  float frequency = 915.0;
  float bandwidth = 125;   
  uint8_t spreadingFactor = 7;
  uint8_t codingRate = 5;  
  uint8_t syncWord = 0x12;    // 0x12; for LoRa   // 0x34 for LoRaWan
  float outputPower = 14;   
  uint16_t preambleLength = 8; 

  // Validate parameters
  if (!validateParameters(frequency, bandwidth, spreadingFactor, codingRate, syncWord, outputPower, preambleLength)) {
    Serial.println(F("Error: Invalid parameters. Stopping."));
    while (true) { delay(10); }
  }

  Serial.print(F("[SX1262] Initializing ... "));
  int state = radio.begin(frequency, bandwidth, spreadingFactor, codingRate, syncWord, outputPower, preambleLength, 1.6, false);

  if (state == RADIOLIB_ERR_NONE) {
    Serial.println(F("success!"));
    radio.setDio1Action(setFlag);
  } else {
    Serial.print(F("failed, code "));
    Serial.println(state);
    while (true) { delay(10); }
  }
}

// Send packet
void sendPacket(const char* message) {
  Serial.print(F("[SX1262] Sending packet ... "));
  Serial.print(message);
  Serial.print(F(" "));
  transmissionState = radio.startTransmit(message);
  transmitFlag = true;
}

// Receive packet
void receivePacket() {
  String str;
  int state = radio.readData(str);
  if (state == RADIOLIB_ERR_NONE) {
    Serial.println(F("[SX1262] Received packet!"));
    Serial.print(F("[SX1262] Data: "));
    Serial.println(str);
    Serial.print(F("[SX1262] RSSI: "));
    Serial.print(radio.getRSSI());
    Serial.println(F(" dBm"));
    Serial.print(F("[SX1262] SNR: "));
    Serial.print(radio.getSNR());
    Serial.println(F(" dB"));
  } else {
    Serial.print(F("readData failed, code "));
    Serial.println(state);
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(LED_BUILTIN, OUTPUT); 
  digitalWrite(LED_BUILTIN, 1); 
  randomSeed(analogRead(0)); // Seed random with noise from an analog pin
  delay(5000);
  digitalWrite(LED_BUILTIN, 0); 
  initRadio();
  #if defined(INITIATING_NODE)
    sendPacket("234576");
  #else
    radio.startReceive();
  #endif
}

void loop() {
  if (operationDone) {
    operationDone = false;
    if (transmitFlag) {
      if (transmissionState == RADIOLIB_ERR_NONE) {
        Serial.println(F("transmission finished!"));
      } else {
        Serial.print(F("failed, code "));
        Serial.println(transmissionState);
      }
      radio.startReceive();
      transmitFlag = false;
    } else {
      receivePacket();
      digitalWrite(LED_BUILTIN, 1); 
      delay(5000);
      digitalWrite(LED_BUILTIN, 0);

      char myMessage[20]; // Buffer to store the formatted string
      int myRandomNumber = random(100, 1000);
      sprintf(myMessage, "Hello:%d", myRandomNumber); // Format string
      sendPacket(myMessage);
    }
  }
}
