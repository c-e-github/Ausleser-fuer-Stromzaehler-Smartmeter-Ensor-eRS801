// copyright c-e 

#include <Adafruit_SSD1306.h>
#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>
#include <LoRa.h>
#include <string>
#include "esp_task_wdt.h"

Adafruit_SSD1306 oled(128, 64, &Wire, -1);
std::string displayzeile[6]; // zeile 0 unbenutzt--> displayzeile[6] ist 5. zeile!
// 1. zeile: Power
// 2. zeile: In / Out
// 3. zeile: per Lora gesendeter Wert
// 4. zeile: Data Age
// 5. zeile: Datum und Zeit
unsigned long long int Data_Age = 0;
#define P1_MAXLENGTH 20000
char telegram[P1_MAXLENGTH];
byte message[P1_MAXLENGTH];
int telegramIndex = 0;
bool inTelegram = false;
bool stopadding = false;
uint16_t claimedCRC;
std::string ACTUAL_Date_Time;
long ACTUAL_CONSUMPTION;
long ACTUAL_RETURNDELIVERY;
int wattprozent = 0; // watt in % von 800 watt
unsigned long _lastmillis = 0;
unsigned long _lastmillisblink = 0;
unsigned long _lastmillisdisplay = 0;
bool display_on; // für display blinkfunktion
const char identifier[6] = { 'a', 'b', 'c', 'd', 'e', 'f' }; // identifier für lora
bool statusTaster = HIGH;         // aktueller Status des Taster an Pin 2
bool statusTasterLetzter = HIGH;  // vorheriger Status des Tasters an Pin 2
bool displayPowerSave; // true = display aus
int32_t powerSaldo = 0; 
int32_t psumme = 0; 
int zaehler = 0;

#define RADIO_SCLK_PIN              5
#define RADIO_MISO_PIN              19
#define RADIO_MOSI_PIN              27
#define RADIO_CS_PIN                18
#define RADIO_DIO0_PIN              26
#define RADIO_RST_PIN               23
#define I2C_SDA                     21
#define I2C_SCL                     22

void displayText() {
   oled.clearDisplay();
   oled.setTextColor(SSD1306_WHITE);
   oled.setCursor(0, 0);
   oled.setTextSize(2);
   oled.println(displayzeile[1].c_str());
   oled.setTextSize(1);
   oled.println(displayzeile[2].c_str());
   oled.println(displayzeile[3].c_str());
   oled.println(displayzeile[4].c_str());
   oled.println(displayzeile[5].c_str());
   oled.display();
}

void displayblinken(){
   if ((millis() - _lastmillisblink) > (500)) {
      _lastmillisblink = millis();
      if (display_on == true){
         oled.ssd1306_command(SSD1306_DISPLAYOFF);
         display_on = false;
      } else {
         oled.ssd1306_command(SSD1306_DISPLAYON);
         display_on = true;
      }
   }
}

void LoraSenden(){
   char payload[9];
   memcpy(payload, identifier, 6);  // Fill identifier (6 bytes)
   // Encode powerSaldo as signed 3-byte integer, powerSaldo kann ca. -1000 bis +20000 Watt sein
   payload[6] = (powerSaldo >> 16) & 0xFF;
   payload[7] = (powerSaldo >> 8) & 0xFF;
   payload[8] = powerSaldo & 0xFF;
   // --------------------------------- lora send packet -----------------------------
   LoRa.beginPacket(true); // `true` enables implicit header mode 
   LoRa.write((uint8_t*)payload, 9);
   LoRa.endPacket();
   // --------------------------------------------------------------------------------
   displayzeile[3] = "Lora: " + std::to_string(powerSaldo);
}

std::string extractValue(const std::string& telegram, const std::string& marker) {
    size_t start = telegram.find(marker);
    if (start == std::string::npos) return "";
    start += marker.length();
    size_t end = telegram.find(')', start); // find next ")"
    if (end == std::string::npos) return "";
    return telegram.substr(start, end - start);
}

void ParseReceivedData(int len) {
   //Serial.println(telegram);
   // 0-0:1.0.0 = Date and Time
   // bei alten Ensor-Geräten im Hex-Format, bei neuen YYMMDDhhmmssX / X=S Daylight Saving Time active / X=W DST not active
   // Erfassung von YYMMDDhhmmssX hier nicht implementiert!
   std::string temp = "";
   ACTUAL_Date_Time = extractValue(std::string(telegram), "0-0:1.0.0(");
   if (ACTUAL_Date_Time.empty() || ACTUAL_Date_Time.length() > 24) {
      Serial.println("Timestamp not found");
   } else {
      temp = ACTUAL_Date_Time.substr(0, 4);
      unsigned long tmp2 = std::stoul(temp, nullptr, 16);
      int year = static_cast<int>(tmp2);
      temp = ACTUAL_Date_Time.substr(4, 2);
      tmp2 = std::stoul(temp, nullptr, 16);
      int month = static_cast<int>(tmp2);
      temp = ACTUAL_Date_Time.substr(6, 2);
      tmp2 = std::stoul(temp, nullptr, 16);
      int day = static_cast<int>(tmp2);
      temp = ACTUAL_Date_Time.substr(10, 2);
      tmp2 = std::stoul(temp, nullptr, 16);
      int hour = static_cast<int>(tmp2);
      temp = ACTUAL_Date_Time.substr(12, 2);
      tmp2 = std::stoul(temp, nullptr, 16);
      int minute = static_cast<int>(tmp2);
      temp = ACTUAL_Date_Time.substr(14, 2);
      tmp2 = std::stoul(temp, nullptr, 16);
      int second = static_cast<int>(tmp2);
      char timestamp[20];
      sprintf(timestamp, "%02d.%02d.%04d %02d:%02d:%02d", day, month, year, hour, minute, second);
      displayzeile[5] = timestamp;
      Serial.print("Zeit: ");
      Serial.println(timestamp);
   }    
   // 1-0:1.7.0(000.424*kW) Aktueller Verbrauch (+P = Strombezug) in 1 Watt resolution
   temp = extractValue(std::string(telegram), "1-0:1.7.0(");
   auto pos = temp.find_first_not_of("0123456789."); // Remove suffix like '*kW' or 'kW', anything which is not a number
   if (pos != std::string::npos) {
      temp = temp.substr(0, pos);
   }
   ACTUAL_CONSUMPTION = static_cast<long>(std::stod(temp) * 1000);
    // 1-0:2.7.0(000.000*kW) Aktuelle Einspeisung (-P) in 1 Watt resolution
   temp = extractValue(std::string(telegram), "1-0:2.7.0(");
   pos = temp.find_first_not_of("0123456789."); // Remove suffix like '*kW' or 'kW', anything which is not a number
   if (pos != std::string::npos) {
      temp = temp.substr(0, pos);
   }
   ACTUAL_RETURNDELIVERY = static_cast<long>(std::stod(temp) * 1000);
   powerSaldo = ACTUAL_CONSUMPTION - ACTUAL_RETURNDELIVERY;
   displayzeile[1] = "P: " + std::to_string(powerSaldo) + " W";
   displayzeile[2] = "In: " + std::to_string(ACTUAL_CONSUMPTION) + " W / Out: " + std::to_string(ACTUAL_RETURNDELIVERY) + " W";


   // laut LoRa Air-Time Calculator https://iftnt.github.io/lora-air-time/index.html
   // payload 9, preamble 8, spreading 6, 125kHz, coding 4/5, implicit header mode: Air-Time = 18.048 ms
   // ==> alle 2 Sekunden senden hält 1 % Regel ein
   zaehler++;
   psumme = psumme + powerSaldo * zaehler;
   if (zaehler == 1){ 
      psumme = powerSaldo;
   }
   if (zaehler == 2){ // nur alle 2 messungen = alle 2 sekunden lora senden
      powerSaldo = (psumme + (powerSaldo * 4)) / 5; // gewichteter mittelwert über 2 messungen
      Serial.print("PowerSaldo, gewichtet: ");
      Serial.println(powerSaldo);
      LoraSenden();
      zaehler = 0;
      psumme = 0;
      _lastmillis = millis(); // data-age-sekunden-zähler auf 0 setzen
   }
}

// --------------- CRC VALIDATION ---------------
uint16_t crc16_arc(const uint8_t *data, size_t length) {
    uint16_t crc = 0x0000;
    for (size_t i = 0; i < length; i++) {
        crc ^= data[i];
        for (uint8_t j = 0; j < 8; j++) {
            if (crc & 1){
                crc = (crc >> 1) ^ 0xA001;  // 0x8005 reflected = 0xA001
            } else {
                crc >>= 1;
            }
        }
    }
    return crc;
}

void ValidateCRC(int len) {
   //Serial.print("CRC: ");
   //for (size_t i = 0; i < len; i++){
   //   if (message[i] < 0x10) Serial.print("0");
   //   Serial.print(message[i], HEX);
   //   //Serial.print(' ');
   //}
   //Serial.println(' ');
   uint16_t crc = crc16_arc(message, len);
   Serial.print("claimedCRC: ");
   Serial.println(claimedCRC);
   Serial.print("Calculated CRC: ");
   Serial.println(crc);
   if (claimedCRC == crc) {
         Serial.println(F("✅ CRC Valid"));
         ParseReceivedData(telegramIndex); 
   } else {
         Serial.println(telegram);
         Serial.println(F("❌ CRC Invalid"));
   }
   return;
}

void resetTelegram() {
    telegramIndex = 0;
    inTelegram = false;
    stopadding = false;
    memset(telegram, 0, P1_MAXLENGTH);
    memset(message, 0, P1_MAXLENGTH);
}

void ReadSerialData() {
    while (Serial1.available() > 0) {
        esp_task_wdt_reset();
        byte current_byte = Serial1.read(); // byte einlesen
        char c = static_cast<char>(current_byte); 

        if (!inTelegram) {
            if (c == '/') {
                resetTelegram();
                inTelegram = true;
            } else {
                continue;
            }
        }

        if (inTelegram && c == '!') {
            telegram[telegramIndex] = c; // adding "!"
            message[telegramIndex] = current_byte;
            telegramIndex++;
            stopadding = true;

            // Wait for 4-char CRC
            while (Serial1.available() < 4) {
                delay(1);
            }
            char crcStr[5];
            for (int i = 0; i < 4; ++i) {
                crcStr[i] = Serial1.read(); // byte einlesen
            }
            crcStr[4] = '\0';
            claimedCRC = (uint16_t)strtol(crcStr, NULL, 16);
            telegram[telegramIndex] = '\0';  // Null-terminate string
            ValidateCRC(telegramIndex);
            resetTelegram(); // reset for next telegram
            return;
        } else if (inTelegram && !stopadding) {
            if (telegramIndex < P1_MAXLENGTH - 1) {
                telegram[telegramIndex] = c;
                message[telegramIndex] = current_byte;
                telegramIndex++;
            } else { // Buffer overflow
                resetTelegram();
                return;
            }
        }
    }
}

// --------------- SETUP ---------------
void setup() {
   delay(1500);
   Serial.begin(115200);
   delay(1500);
   Serial.println("");
   Serial.println("Serial Monitor started.");

   pinMode(2, INPUT_PULLUP); // pin 2 für display-ein-aus-taster

   SPI.begin(RADIO_SCLK_PIN, RADIO_MISO_PIN, RADIO_MOSI_PIN);
   Wire.begin(I2C_SDA, I2C_SCL);
   delay(1500);
   LoRa.setPins(RADIO_CS_PIN, RADIO_RST_PIN, RADIO_DIO0_PIN);

   if (!oled.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
      Serial.println("SSD1306 OLED-Display allocation failed.");
      return;
   } else {
      oled.setRotation(2); // bildschirm upside-down
      Serial.println("SSD1306 OLED-Display okay.");
      displayzeile[1] = "OLED-Display okay.";
      displayText();
   }

   if (!LoRa.begin(868E6)) {
      Serial.println("Starting LoRa failed!");
      displayzeile[2] = "Starting LoRa failed!";
      displayText();
      delay(2000);
      return;
   } else {
      LoRa.setSpreadingFactor(6);  // Supported values are between `6` and `12`. defaults to `7`
      // If a spreading factor of `6` is set, implicit header mode must be used to transmit and receive packets.
      LoRa.setCodingRate4(5); // default `5`, Supported values `5` - `8`, correspond to coding rates of `4/5` and `4/8
      LoRa.disableCrc();  // by default a CRC is not used
      LoRa.setPreambleLength(8); // default 8, Supported values are between `6` and `65535`
      LoRa.setSyncWord(0x12); // byte value to use as the sync word, defaults to `0x12`
      LoRa.setSignalBandwidth(125E3);  // signal bandwidth in Hz, defaults to `125E3`
      // Supported values are `7.8E3`, `10.4E3`, `15.6E3`, `20.8E3`, `31.25E3`, `41.7E3`, `62.5E3`, `125E3`, `250E3`, and `500E3`.
      Serial.println("LoRa was started.");
      displayzeile[2] = "LoRa was started.";
      displayText();
   }

   // Format for setting a serial port is: Serial1.begin(baud-rate, protocol, RX pin, TX pin);
   // Serial RX-Pin set to 4 (TX but not used, therefore set to -1)
   // DSMR 4.0/4.2 Baudrate = 115200, Data bits = 8, Parity = None, Stop bits = 1
   pinMode(4, INPUT_PULLUP);
   Serial1.begin(115200, SERIAL_8N1, 4, -1);
   Serial1.setTimeout(3000);
   Serial.println("Serialport 1 was started.");
   Serial.println("Setup done.");
}

// --------------- LOOP ---------------
void loop() {

  ReadSerialData();

   // Taster abfragen, wenn Taster gedrückt wurde, Display ein oder ausschalten
   statusTaster = digitalRead(2); 
   if (statusTaster == !statusTasterLetzter) { // Wenn aktueller Tasterstatus anders ist als der letzte Tasterstatus
      if (statusTaster == LOW) { // Wenn Taster gedrückt
         if (displayPowerSave == true){ // Display an- bzw. ausschalten
            displayPowerSave = false;
            oled.ssd1306_command(SSD1306_DISPLAYON);
            Serial.println("Display an.");

         } else {
            displayPowerSave = true;
            oled.ssd1306_command(SSD1306_DISPLAYOFF);
            Serial.println("Display aus.");
         } 
      }            
   }
   statusTasterLetzter = statusTaster; // merken des letzten Tasterstatus

   Data_Age = (millis() - _lastmillis)/1000;
   displayzeile[4] = "Data Age: "+ std::to_string(Data_Age) + " s";
   if ((millis() - _lastmillisdisplay) > (1000)) { // display jede sekunde aktualisieren
      displayText();
      _lastmillisdisplay = millis();

   }
   if (Data_Age > 30){ // wenn länger als 30 s keine daten --> display blinken
      displayblinken();
   } else{
      if (display_on = false){ // damit, wenn blink-phase endet, display an ist
         oled.ssd1306_command(SSD1306_DISPLAYON);
         display_on = true;
      }            
   }

}