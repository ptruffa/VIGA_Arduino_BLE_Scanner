/*
   Based on Neil Kolban example for IDF: https://github.com/nkolban/esp32-snippets/blob/master/cpp_utils/tests/BLE%20Tests/SampleScan.cpp
   Ported to Arduino ESP32 by Evandro Copercini
*/

#define WIFI_SSID       "mywifiSSID"
#define WIFI_PASSWORD   "mywifipassword"
#define POST_URL        "https://cowatchapi.vigalab.com/beacon/send"
//#define POST_URL        "http://10.10.10.101:3000/"
#define SCAN_TIME       30     // seconds
#define WAIT_WIFI_LOOP  5 // around 4 seconds for 1 loop
#define SLEEP_TIME      300   // seconds
#define MAX_SAVED_REG   250   // seconds

#include <Arduino.h>
#include <sstream>
#include <bits/stdc++.h> 

#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>

#include <WiFi.h>
#include <WiFiMulti.h>
#include <esp_wifi.h>

#include <HTTPClient.h>

#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"

time_t tnow;
char strftime_buf[64];
struct tm timeinfo;

//String maac;
RTC_DATA_ATTR char address[MAX_SAVED_REG][18];
RTC_DATA_ATTR int rssi[MAX_SAVED_REG];
RTC_DATA_ATTR uint8_t temp[MAX_SAVED_REG][2];
RTC_DATA_ATTR time_t timestamp[MAX_SAVED_REG];
RTC_DATA_ATTR uint8_t cowatchCount = 0, tail = 0;
RTC_DATA_ATTR time_t tlast;

WiFiMulti wifiMulti;
std::stringstream ss;
bool data_sent = false;
int wait_wifi_counter = 0;
bool isCowatch = false;
String aux;
class MyAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks
{
  void onResult(BLEAdvertisedDevice advertisedDevice)
  {
    log_i("Advertised Device: %s \n", advertisedDevice.toString().c_str());
  }
};

void setup()
{
  Serial.begin(115200);

  time(&tnow);
  // Set timezone to China Standard Time
  //setenv("TZ", "CST-4", 1);
  //tzset();
  //localtime_r(&tnow, &timeinfo);
  //strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
  //ESP_LOGI(TAG, "The current date/time in Chile is: %s", strftime_buf);
  Serial.println(tnow);
  Serial.println(tlast);
  Serial.println(tnow-tlast);
  log_i("ESP32 BLE Scanner");
  
  
  for(int j = tail; j < cowatchCount; j++){
    Serial.print(j+1);
    Serial.print("-Adr:");
    for(int k = 0; k < 17; k++)
      Serial.print(address[j][k]);
    Serial.print("-");
    Serial.print("rssi:");
    Serial.print(rssi[j]);
    Serial.print("-T:");
    Serial.print(temp[j][0]);
    Serial.print(",");
    Serial.print(temp[j][1]);
    Serial.print("-tstamp:");
    Serial.print(timestamp[j]);
    Serial.println();
  }
  // disable brownout detector to maximize battery life
  log_i("disable brownout detector");
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);

  log_i("BLEDevice::init()");
  BLEDevice::init("");

  // put your main code here, to run repeatedly:
  Serial.println("Escaniando BLE...");
  BLEScan *pBLEScan = BLEDevice::getScan(); //create new scan
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setActiveScan(true); //active scan uses more power, but get results faster
  pBLEScan->setInterval(0x50);
  pBLEScan->setWindow(0x30);

  log_i("Start BLE scan for %d seconds...\n", SCAN_TIME);

  BLEScanResults foundDevices = pBLEScan->start(SCAN_TIME);
  int count = foundDevices.getCount();
  Serial.print("count=");
  Serial.println(count);
  //ss << "[";
  for (int i = 0; i < count; i++)
  {
    if (i > 0)
    {
      //ss << ",";
    }
    BLEAdvertisedDevice d = foundDevices.getDevice(i);
    //ss << "{\"Address\":\"" << d.getAddress().toString() << "\",\"Rssi\":" << d.getRSSI();
    if (d.haveName())
    {
      //ss << ",\"Name\":\"" << d.getName() << "\"";
      std::string auxstring = d.getName();
      
      if(d.getName().compare(0,7,"Cowatch") == 0){
        isCowatch = true;
        Serial.println("esCowatch!");
        Serial.println(cowatchCount);
        //guardar address y RSSI en estructura
        
        aux = d.getAddress().toString().c_str();
        aux.toCharArray(&address[cowatchCount][0], 18);
        Serial.println(aux);
        
      }
    }
    if (d.haveServiceData())
    {
      std::string md = d.getServiceData();
      uint8_t *mdp = (uint8_t *)d.getServiceData().data();
      char *pHex = BLEUtils::buildHexData(nullptr, mdp, md.length());
      //muestra.temp = pHex[0] << 8 + pHex[1];
      //ss << ",\"ServiceData\":\"" << pHex << "\"";
      if(isCowatch == true){
        //extraer data y guardarla en estructura
        Serial.print((mdp[0]));
        Serial.print("-");
        Serial.println((mdp[1]));
        uint16_t test;
        temp[cowatchCount][0] = mdp[0];
        temp[cowatchCount][1] = mdp[1];
        Serial.print((temp[cowatchCount][0]));
        Serial.print(",");
        Serial.println(temp[cowatchCount][1]);
        test = (mdp[0]<<8) + mdp[1];
        Serial.println(test);
      }
      free(pHex);
    
    }
    /*if (d.haveAppearance())
    {
      ss << ",\"Appearance\":" << d.getAppearance();
    }

    if (d.haveManufacturerData())
    {
      std::string md = d.getManufacturerData();
      uint8_t *mdp = (uint8_t *)d.getManufacturerData().data();
      char *pHex = BLEUtils::buildHexData(nullptr, mdp, md.length());
      ss << ",\"ManufacturerData\":\"" << pHex << "\"";
      free(pHex);
    }
    if (d.haveServiceUUID())
    {
      ss << ",\"ServiceUUID\":\"" << d.getServiceUUID().toString() << "\"";
    }
    
    if (d.haveTXPower())
    {
      ss << ",\"TxPower\":" << (int)d.getTXPower();
    }*/
    if(isCowatch == true)
    {
      tlast = tnow;
      timestamp[cowatchCount] = tlast;
      rssi[cowatchCount] = d.getRSSI();
      cowatchCount++;
      
      //localtime_r(&tnow, &timeinfolast);
      //strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfolast);
      Serial.println(tlast);
    }
    //ss << "}";
  }
  //ss << "]";

  log_i("Scan done!");

  wifiMulti.addAP(WIFI_SSID, WIFI_PASSWORD);
}

void loop()
{
  // wait for WiFi connection
  if ((wifiMulti.run() == WL_CONNECTED))
  {
    log_i("WiFi Connected");
    Serial.println("WiFi Connected");
    if(cowatchCount > 0){
      // HTTP POST BLE list
      HTTPClient http;
      //http.setAuthorization("2YwmgXZgjJ3P9LK6ulVfRtbYDvw");
      http.begin(POST_URL);
      
      http.addHeader("Content-Type", "application/x-www-form-urlencoded");
      //http.addHeader("user-id", "101");
      //http.addHeader("x-api-key", "bff21727-c0ab-4992-bce7-8a8053d2e3cd");
      for(int i = tail; i < cowatchCount; i++){
        ss.str("");
        //ss << "[{";
        ss << "address="; 
        ss.write(&address[i][0],17); 
        /*for (int j = 0; j < 18; j++)
        {
          ss << (address[i][j]); 
        }*/
        ss << "&"; 
        ss << "rssi=" << rssi[i] << "&";
        if(int(temp[i][1]) >= 10)
          ss << "temperatura="<< int(temp[i][0]) << int(temp[i][1]) << "&";
        else
          ss << "temperatura=" << int(temp[i][0]) << "0" << int(temp[i][1]) << "&";
        ss << "timestamp=" << timestamp[i] 
        //ss << "\"}]";
        
        log_i("[HTTP] Payload:\n%s", ss.str().c_str());
        log_i("[HTTP] Begin...");
        Serial.print("http payload:");
        //ss.str("");
        //ss << "temperatura=44";
        //String httpRequestData = "temperatura=44"; 
        //Serial.println(httpRequestData);
        Serial.println(ss.str().c_str());
        // configure traged server and url
        //http.begin(POST_URL);
        
        // start connection and send HTTP header
        //int httpCode = http.POST(httpRequestData);
        int httpCode = http.POST(ss.str().c_str());
        Serial.print("http code:");
        Serial.println(httpCode);
        // httpCode will be negative on error
        if (httpCode > 0)
        {
          // HTTP header has been send and Server response header has been handled
          log_i("[HTTP] GET... code: %d\n", httpCode);

          // file found at server
          if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_CREATED ) 
          {
            log_i("[HTTP] Response:\n%s", http.getString());
            tail++;
          }
        }
        else
        {
          log_i("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
          Serial.println("ERROR en POST");
        }

        http.end();
        
      }
      data_sent = true;
      if(cowatchCount == 255) cowatchCount = 0;
      if(tail == 255) tail = 0;
    }
    
  }

  // wait WiFi connected
  if (data_sent || (wait_wifi_counter > WAIT_WIFI_LOOP))
  {
    esp_sleep_enable_timer_wakeup(SLEEP_TIME * 1000000); // translate second to micro second

    log_i("Enter deep sleep for %d seconds...\n", SLEEP_TIME);
    Serial.println("Enter esp_deep_sleep_start");
    esp_wifi_stop();
    esp_deep_sleep_start();

  }
  else
  {
    wait_wifi_counter++;

    log_i("Waiting count: %d\n", wait_wifi_counter);
    Serial.println("waiting count");
  }
}
