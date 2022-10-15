// ST7789 WatchBand
// (c) 2022 Khasanov Danila

#define BLACK  0x0000
#define WHITE  0xFFFF
#define YELLOW 0xFFE0
#define GREEN  0x07E0
#define RED    0xF800

#include <WiFi.h>     //библиотека для работы WIFI
#include <WebServer.h>     //библиотека для работы сервера
#include <EEPROM.h>     //библиотека для работы с EEPROM памятью
#include <SPI.h>     //библиотека для работы с SPI протоколом
#include <TFT_eSPI.h>    //библиотека для чипа ST7789
#include <TJpg_Decoder.h>    //библиотека для работы c jpeg
#include <GyverEncoder.h>    //библиотека для работы с энкодером

#include "settings_icon.h"
#include "wifi_icon.h"
#include "bluetooth_icon.h"
#include "gallery_icon.h"
#include "text_icon.h"
#include "battery_icon.h"
#include "theme_icon.h"
#include "version_icon.h"

//TFT_DC            2
//TFT_RES           4
//TFT_SDA           23
//TFT_SCK           18
#define ENC_DT      25
#define ENC_CLK     26
#define ENC_BTN     27
#define button1     34
#define button2     35
#define batteryPin  A0

WebServer server(80);
TFT_eSPI lcd = TFT_eSPI();
Encoder enc(ENC_CLK, ENC_DT, ENC_BTN);

const char* ssid = "ESP32_Watch";
const char* password = "12345678";

bool tft_output(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap)
{
  if ( y >= lcd.height() ) return 0;
  lcd.pushImage(x, y, w, h, bitmap);
  return 1;
}

bool drawState = true;      //флаг первой отрисовки
bool canPush = false;       //флаг перехода на следующий уровень с текущего
bool canPushBack = false;   //флаг перехода на предыдущий уровень с текущего
bool wifiState = true;
int timeMillis;             //таймер переходов
int arrPos = 1;             //позиция курсора
int firstToBackL1 = 1;      //позиция куросра при возврате L1
int firstToBackL2 = 1;      //позиция куросра при возврате L2

#define minVol   2,5   //минимальный вольтаж аккумулятора
#define maxVol   4,2   //максимальный вольтаж аккумулятора

#define mainMenuItems   5   //кол-во пунктов основного меню
#define settingsItems   3   //кол-во пунктов настроек

char *mainMenu[mainMenuItems] = {"Settings", "WIFI", "Bluetooth", "Gallery", "Texts"};   //пункты главного меню
char *settingsMenu[settingsItems] = {"Version", "Theme", "Battery"};   //пункты настроек
char *textsList[] = {"text1", "text2"};   //пункты текстов
char *texts[] = {"hello", "goodbye"};


void setup() {
  Serial.begin(115200);

  EEPROM.begin(512);

  digitalWrite(15, HIGH); // TFT screen chip select

  //инициализируем дисплей
  lcd.begin(15);
  lcd.init();
  lcd.setRotation(0);
  lcd.fillScreen(BLACK);

  //инициализируем сервер
  WiFi.softAP(ssid, password);
  server.enableCORS(true);
  server.on("/check", handleRoot);
  server.on("/text", HTTP_POST, handleText);
  server.on("/image", HTTP_POST, handleImage);
  server.begin();

  //настройка jpeg
  TJpgDec.setJpgScale(1);
  TJpgDec.setSwapBytes(true);
  TJpgDec.setCallback(tft_output);

  enc.setType(TYPE2);
  enc.setTickMode(AUTO);
  enc.setDirection(NORM);

  pinMode(button1, INPUT);
  pinMode(button2, INPUT);
  pinMode(batteryPin, INPUT);

  timeMillis = millis();     //обнуляем таймер

}


void loop() {

  checkMainMenu(button1, button2, mainMenu, mainMenuItems);

}


void checkMainMenu(int btn1, int btn2, char *arr[], int arrSize) {   //пункт основного меню
  checkBattery(batteryPin);   //проверяем батарею(1)
  server.handleClient();   //запускаем слушателя

  canPushFunc();

  if (drawState) {   //прорисовка
    writePoints(arr, arrSize, 1);   //выводим пункты
    writeCursor(arr, firstToBackL1);   //выводим курсор
    Serial.println("Proricovka");
    drawState = false;
  }

  encMove(arr, arrSize);

  //проверяем позицию курсора и переходим на следующий(при нажатии на кнопку)
  if (digitalRead(button1) == HIGH && canPush) {
    firstToBackL1 = arrPos;   //позиция курсора в момент перехода
    lcd.fillScreen(BLACK);
    canPush = false;   //обнуляем
    canPushBack = false;   //обнуляем
    drawState = true;   //для открывающей отрисовки
    timeMillis = millis();   //для таймера нажатия
    switch (arrPos) {
      case 1:
        while (!(digitalRead(button2) == HIGH && canPushBack)) {
          settings(button1, button2, settingsMenu, settingsItems);
        }
        break;
      case 2:
        while (!(digitalRead(button2) == HIGH && canPushBack)) {
          wifi(button1, button2);
        }
        break;
      case 3:
        while (!(digitalRead(button2) == HIGH && canPushBack)) {
          checkBattery(batteryPin);   //проверяем батарею(1)
          server.handleClient();   //запускаем слушателя

          canPushBackFunc();

          lcd.setCursor(15, 13);
          lcd.print("Bluetooth");
        }
        break;
      case 4:
        while (!(digitalRead(button2) == HIGH && canPushBack)) {
          gallery();
        }
        break;
      default:
        while (!(digitalRead(button2) == HIGH && canPushBack)) {
          text();
        }
        break;
    }
    Serial.println("-");
    lcd.fillScreen(BLACK);
    canPush = false;   //обнуляем
    canPushBack = false;   //обнуляем
    drawState = true;   //для возвратной прорисовки
    timeMillis = millis();   //для таймера нажатия
  }
}

void settings(int btn1, int btn2, char *arr[], int arrSize) {   //пункт настроек
  checkBattery(batteryPin);   //проверяем батарею(1)
  server.handleClient();   //запускаем слушателя

  canPushFunc();
  canPushBackFunc();

  if (drawState) {   //прорисовка
    writePointsSettings(arr, arrSize, 1);   //выводим пункты
    writeCursor(arr, firstToBackL2);   //выводим курсор
    drawState = false;
  }

  encMoveSettings(arr, arrSize);

  //проверяем позицию курсора и переходим на следующий(при нажатии на кнопку)
  if (digitalRead(btn1) == HIGH  && canPush) {
    Serial.println(arrPos);
    firstToBackL2 = arrPos;
    lcd.fillScreen(BLACK);
    canPush = false;   //обнуляем
    canPushBack = false;   //обнуляем
    drawState = true;   //для открывающей прорисовки
    timeMillis = millis();   //для таймера нажатия
    switch (arrPos) {
      case 1:
        while (!(digitalRead(button2) == HIGH && canPushBack)) {
          canPushBackFunc();
          writeTitle("Version: 1.3.7", 2);
        }
        break;
      case 2:
        while (!(digitalRead(button2) == HIGH && canPushBack)) {
          canPushBackFunc();
          writeTitle("Theme settings", 2);
        }
        break;
      case 3:
        while (!(digitalRead(button2) == HIGH && canPushBack)) {
          canPushBackFunc();
          writeTitle("Battery parameters", 2);
        }
        break;
    }
    Serial.println("-");
    lcd.fillScreen(BLACK);
    canPush = false;   //обнуляем
    canPushBack = false;   //обнуляем
    drawState = true;   //для возвратоной прорисовки
    timeMillis = millis();   //для таймера нажатия
  }
}

void wifi(int btn1, int btn2) {
  checkBattery(batteryPin);   //проверяем батарею(1)
  server.handleClient();   //запускаем слушателя

  canPushBackFunc();
  canPushFunc();

  writeTitle("WIFI settings", 2);

  lcd.setTextColor(WHITE, BLACK);
  lcd.setCursor(22, 50);
  lcd.print("wifi AP: ");
  if (wifiState) {
    lcd.setTextColor(GREEN, BLACK);
    lcd.print("on");
    lcd.drawRoundRect(20, 48, 135, 20, 5, WHITE);
  } else {
    lcd.setTextColor(RED, BLACK);
    lcd.print("off");
    lcd.drawRoundRect(20, 48, 147, 20, 5, WHITE);
  }
  lcd.setTextColor(WHITE, BLACK);
  lcd.setCursor(22, 90);
  lcd.print("ssid: ");
  lcd.print(ssid);
  lcd.setCursor(22, 120);
  lcd.print("password: ");
  lcd.print(password);

  //проверяем позицию курсора и переходим на следующий(при нажатии на кнопку)
  if (digitalRead(btn1) == HIGH  && canPush) {
    wifiState = !(wifiState);
    lcd.fillScreen(BLACK);
    canPush = false;   //обнуляем
    timeMillis = millis();   //для таймера нажатия
  }
}

void gallery() {
  checkBattery(batteryPin);   //проверяем батарею(1)
  server.handleClient();   //запускаем слушателя

  canPushBackFunc();

  writeTitle("Gallery", 2);
}

void text() {
  checkBattery(batteryPin);   //проверяем батарею(1)
  server.handleClient();   //запускаем слушателя

  canPushBackFunc();

  writeTitle("Text", 2);

  String text = readStringFromEEPROM(0);
  lcd.setTextColor(WHITE, BLACK);
  lcd.setCursor(22, 90);
  lcd.print(text);
}


void writeTitle(String text, int textSize) {
  lcd.setTextColor(YELLOW, BLACK);
  lcd.setTextSize(textSize);
  lcd.setCursor(18, 18);
  lcd.print(text);
}


void canPushFunc() {
  if (!canPush) {
    if (millis() - timeMillis >= 700 || digitalRead(button1) == LOW) {
      canPush = true;
    } else {
      canPush = false;
    }
  }
}

void canPushBackFunc() {
  if (!canPushBack) {
    if (millis() - timeMillis >= 700 || digitalRead(button2) == LOW) {
      canPushBack = true;
    } else {
      canPushBack = false;
    }
  }
}


void encMove(char *arr[], int arrSize) {     //перемещение курсора
  if (enc.isLeft()) {
    arrPos = arrPos - 1;
    if (arrPos >= 1) {
      lcd.fillScreen(BLACK);
      writePoints(arr, arrSize, 1);   //выводим пункты
      writeCursor(arr, arrPos);   //выводим курсор
      Serial.println("left");
      Serial.println(arrPos);
    } else {
      arrPos = arrPos + 1;
      Serial.println("stop");
      Serial.println(arrPos);
    }
  } if (enc.isRight()) {
    arrPos = arrPos + 1;
    if (arrPos <= arrSize) {
      lcd.fillScreen(BLACK);
      writePoints(arr, arrSize, 1);   //выводим пункты
      writeCursor(arr, arrPos);   //выводим курсор
      Serial.println("right");
      Serial.println(arrPos);
    } else {
      arrPos = arrPos - 1;
      Serial.println("stop");
      Serial.println(arrPos);
    }
  }
}

void encMoveSettings(char *arr[], int arrSize) {     //перемещение курсора
  if (enc.isLeft()) {
    arrPos = arrPos - 1;
    if (arrPos >= 1) {
      lcd.fillScreen(BLACK);
      writePointsSettings(arr, arrSize, 1);   //выводим пункты
      writeCursor(arr, arrPos);   //выводим курсор
      Serial.println("left");
      Serial.println(arrPos);
    } else {
      arrPos = arrPos + 1;
      Serial.println("stop");
      Serial.println(arrPos);
    }
  } if (enc.isRight()) {
    arrPos = arrPos + 1;
    if (arrPos <= arrSize) {
      lcd.fillScreen(BLACK);
      writePointsSettings(arr, arrSize, 1);   //выводим пункты
      writeCursor(arr, arrPos);   //выводим курсор
      Serial.println("right");
      Serial.println(arrPos);
    } else {
      arrPos = arrPos - 1;
      Serial.println("stop");
      Serial.println(arrPos);
    }
  }
}


void writePoints (char *arrText[], int arrSize, int textSize) {
  lcd.setTextColor(WHITE, BLACK);
  lcd.setTextSize(textSize);
  uint16_t w = 26, h = 26;
  for (int i = 1; i <= arrSize; i++) {
    if (i <= 3) {
      mainImages(w, h, i, 30);
      lcd.setCursor(12 + 72 * (i - 1) + (72 / 2 - round(strlen(arrText[i - 1])) / 2 - 20), 56);
      lcd.print(arrText[i - 1]);
    } else {
      mainImages(w, h, i, 96);
      lcd.setCursor(12 + 72 * (i - 4) + (72 / 2 - round(strlen(arrText[i - 1])) / 2 - 20), 122);
      lcd.print(arrText[i - 1]);
    }
  }
}

void writePointsSettings (char *arrText[], int arrSize, int textSize) {
  lcd.setTextColor(WHITE, BLACK);
  lcd.setTextSize(textSize);
  uint16_t w = 26, h = 26;
  for (int i = 1; i <= arrSize; i++) {
    if (i <= 3) {
      settingsImages(w, h, i, 30);
      lcd.setCursor(12 + 72 * (i - 1) + (72 / 2 - round(strlen(arrText[i - 1])) / 2 - 20), 56);
      lcd.print(arrText[i - 1]);
    }
  }
}

void mainImages(uint16_t w, uint16_t h, int i, int height) {   //выборка изображения меню
  switch (i) {
    case 1:
      TJpgDec.getJpgSize(&w, &h, settings_icon, sizeof(settings_icon));
      TJpgDec.drawJpg(12 + 72 * 0 + (72 / 2 - w / 2), height, settings_icon, sizeof(settings_icon));
      break;
    case 2:
      TJpgDec.getJpgSize(&w, &h, wifi_icon, sizeof(wifi_icon));
      TJpgDec.drawJpg(12 + 72 * 1 + (72 / 2 - w / 2), height, wifi_icon, sizeof(wifi_icon));
      break;
    case 3:
      TJpgDec.getJpgSize(&w, &h, bluetooth_icon, sizeof(bluetooth_icon));
      TJpgDec.drawJpg(12 + 72 * 2 + (72 / 2 - w / 2), height, bluetooth_icon, sizeof(bluetooth_icon));
      break;
    case 4:
      TJpgDec.getJpgSize(&w, &h, gallery_icon, sizeof(gallery_icon));
      TJpgDec.drawJpg(12 + 72 * 0 + (72 / 2 - w / 2), height, gallery_icon, sizeof(gallery_icon));
      break;
    case 5:
      TJpgDec.getJpgSize(&w, &h, text_icon, sizeof(text_icon));
      TJpgDec.drawJpg(12 + 72 * 1 + (72 / 2 - w / 2), height, text_icon, sizeof(text_icon));
      break;
  }
}

void settingsImages(uint16_t w, uint16_t h, int i, int height) {   //выборка изображения меню настроек
  switch (i) {
    case 1:
      TJpgDec.getJpgSize(&w, &h, version_icon, sizeof(version_icon));
      TJpgDec.drawJpg(12 + 72 * 0 + (72 / 2 - w / 2), height, version_icon, sizeof(version_icon));
      break;
    case 2:
      TJpgDec.getJpgSize(&w, &h, theme_icon, sizeof(theme_icon));
      TJpgDec.drawJpg(12 + 72 * 1 + (72 / 2 - w / 2), height, theme_icon, sizeof(theme_icon));
      break;
    case 3:
      TJpgDec.getJpgSize(&w, &h, battery_icon, sizeof(battery_icon));
      TJpgDec.drawJpg(12 + 72 * 2 + (72 / 2 - w / 2), height, battery_icon, sizeof(battery_icon));
      break;
  }
}

void writeCursor(char *arr[], int arrPos) {
  lcd.setTextColor(WHITE, BLACK);
  if (arrPos <= 3) {
    lcd.drawRoundRect(12 + 72 * (arrPos - 1) + (72 / 2 - round(strlen(arr[arrPos - 1]) / 2) - 20), 30 - 2, round(strlen(arr[arrPos - 1])) * 8, 44, 5, WHITE);
  } else {
    lcd.drawRoundRect(12 + 72 * (arrPos - 4) + (72 / 2 - round(strlen(arr[arrPos - 1]) / 2) - 20), 96 - 2, round(strlen(arr[arrPos - 1])) * 8, 44, 5, WHITE);
  }
}


void writePointsLine(char *arr[], int arrSize, int textSize) {
  lcd.setTextColor(WHITE, BLACK);
  lcd.setTextSize(textSize);
  for (int i = 1; i <= arrSize; i++) {
    lcd.setCursor(40, i * 18 + 30);
    lcd.print(arr[i - 1]);
    lcd.print("\n");
  }
}

void writeCursorLine(char *arr[], int arrPos, int cursorSize) {
  lcd.drawRoundRect(32, arrPos * 18 + 29, strlen(arr[arrPos - 1]) * 14, 18, 5, WHITE);
  lcd.setTextColor(WHITE, BLACK);
  lcd.setTextSize(cursorSize);
  lcd.setCursor(8, arrPos * 18 + 30);
  lcd.print(">");
}


void checkBattery(int BTpin) {
  float batteryVol = (analogRead(BTpin) * 5) / 1024.0;
  int batteryST = (batteryVol - minVol) * (100 - 0) / (maxVol - minVol) + 0;
  batteryST = constrain(batteryST, 0, 100);
  lcd.setTextColor(WHITE, BLACK);
  lcd.setTextSize(1);
  lcd.setCursor(200, 8);
  lcd.print(String(batteryST) + "%");
  //Serial.println(batteryST);
}


void writeStringToEEPROM(int addrOffset, const String &strToWrite)
{
  byte len = strToWrite.length();
  EEPROM.write(addrOffset, len);
  for (int i = 0; i < len; i++)
  {
    EEPROM.write(addrOffset + 1 + i, strToWrite[i]);
  }
  EEPROM.commit();
}

String readStringFromEEPROM(int addrOffset)
{
  int newStrLen = EEPROM.read(addrOffset);
  char data[newStrLen + 1];
  for (int i = 0; i < newStrLen; i++)
  {
    data[i] = EEPROM.read(addrOffset + 1 + i);
  }
  data[newStrLen] = '\0';
  return String(data);
}


void handleRoot() {
  server.send(200, "text/plain", "ESP32 ready!");
}

void handleImage() {
  String data = server.arg("data");
  Serial.println("Image: " + data);
  server.send(200, "text/plain", "data was successfully received!");
}

void handleText() {
  String data = server.arg("data");
  Serial.println("Text: " + data);
  writeStringToEEPROM(0, data);
  server.send(200, "text/plain", "data was successfully received!");
}
