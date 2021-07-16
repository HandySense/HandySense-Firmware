#include <Arduino.h>
#include <WiFi.h>
#include <Wire.h>
#include <NTPClient.h>
#include <PubSubClient.h>
#include <WiFiUdp.h>
#include <ArduinoJson.h>
#include <EEPROM.h>
#include <StreamUtils.h>
#include "Adafruit_SHT31.h"
#include <BH1750.h>
#include "Max44009.h"
#include "RTClib.h"
#include "time.h"
#include <WiFiClient.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <Update.h>

#ifdef DEBUG
#define DEBUG_PRINT(x)    //Serial.print(x)
#define DEBUG_PRINTLN(x)  //Serial.println(x)
#else
#define DEBUG_PRINT(x) Serial.print(x)
#define DEBUG_PRINTLN(x) Serial.println(x)
#endif

// สร้างตัวแปรและใช้ Webserver สำหรับใช้ OTA
const char* host = "esp32";
WebServer server(80);

/*
   Login page
*/
const char* loginIndex =
  "<form name='loginForm'>"
  "<table width='20%' bgcolor='A09F9F' align='center'>"
  "<tr>"
  "<td colspan=2>"
  "<center><font size=4><b>ESP32 Login Page</b></font></center>"
  "<br>"
  "</td>"
  "<br>"
  "<br>"
  "</tr>"
  "<td>Username:</td>"
  "<td><input type='text' size=25 name='userid'><br></td>"
  "</tr>"
  "<br>"
  "<br>"
  "<tr>"
  "<td>Password:</td>"
  "<td><input type='Password' size=25 name='pwd'><br></td>"
  "<br>"
  "<br>"
  "</tr>"
  "<tr>"
  "<td><input type='submit' onclick='check(this.form)' value='Login'></td>"
  "</tr>"
  "</table>"
  "</form>"
  "<script>"
  "function check(form)"
  "{"
  "if(form.userid.value=='admin' && form.pwd.value=='admin')"
  "{"
  "window.open('/serverIndex')"
  "}"
  "else"
  "{"
  " alert('Error Password or Username')/*displays error message*/"
  "}"
  "}"
  "</script>";

/*
   Server Index Page
*/
const char* serverIndex =
  "<script src='https://ajax.googleapis.com/ajax/libs/jquery/3.2.1/jquery.min.js'></script>"
  "<form method='POST' action='#' enctype='multipart/form-data' id='upload_form'>"
  "<input type='file' name='update'>"
  "<input type='submit' value='Update'>"
  "</form>"
  "<div id='prg'>progress: 0%</div>"
  "<script>"
  "$('form').submit(function(e){"
  "e.preventDefault();"
  "var form = $('#upload_form')[0];"
  "var data = new FormData(form);"
  " $.ajax({"
  "url: '/update',"
  "type: 'POST',"
  "data: data,"
  "contentType: false,"
  "processData:false,"
  "xhr: function() {"
  "var xhr = new window.XMLHttpRequest();"
  "xhr.upload.addEventListener('progress', function(evt) {"
  "if (evt.lengthComputable) {"
  "var per = evt.loaded / evt.total;"
  "$('#prg').html('progress: ' + Math.round(per*100) + '%');"
  "}"
  "}, false);"
  "return xhr;"
  "},"
  "success:function(d, s) {"
  "console.log('success!')"
  "},"
  "error: function (a, b, c) {"
  "}"
  "});"
  "});"
  "</script>";

// ประกาศใช้ rtc
RTC_DS1307 rtc;
DateTime _now;

// ประกาศใช้เวลาบน Internet
const char* ntpServer = "pool.ntp.org";
const char* nistTime = "time.nist.gov";
const long  gmtOffset_sec = 7 * 3600;
const int   daylightOffset_sec = 0;
int hourNow,
    minuteNow,
    secondNow,
    dayNow,
    monthNow,
    yearNow,
    weekdayNow;

struct tm timeinfo;
String _data; // อาจจะไม่ได้ใช้

// ประกาศตัวแปรสื่อสารกับ web App
byte STX = 02;
byte ETX = 03;
uint8_t START_PATTERN[3] = {0, 111, 222};
const size_t capacity = JSON_OBJECT_SIZE(7) + 320;
DynamicJsonDocument jsonDoc(capacity);

String mqtt_server ,
       mqtt_Client ,
       mqtt_password ,
       mqtt_username ,
       password ,
       mqtt_port,
       ssid ;

String client_old;

// ประกาศใช้ WiFiClient
WiFiClient espClient;
PubSubClient client(espClient);

// ประกาศใช้ WiFiUDP
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);
int curentTimerError = 0;

// ประกาศตัวแปรเรียกใช้ Max44009
Max44009 myLux(0x4A);

// ประกาศตัวแปรเรียกใช้ SHT31
Adafruit_SHT31 sht31 = Adafruit_SHT31();

// ประกาศตัวแปรเก็บค่า Soil_moisture_sensor
#define Soil_moisture_sensorPin   A0
float sensorValue_soil_moisture   = 0.00,
      voltageValue_soil_moisture  = 0.00,
      percent_soil_moisture       = 0.00;

// ประกาศเพื่อเก็บข้อมูล Min Max ของค่าเซ็นเซอร์ Temp และ Soil
char msg_Minsoil[100],
     msg_Maxsoil[100];
char msg_Mintemp[100],
     msg_Maxtemp[100];

int LEDR = 26,  // LED 26= Blue => แสดงสถานะเชื่อมต่อ Wifi
    LEDY = 27;  // LED 27 = Yenllow => แสดงสถานะส่งข้อมูล, โหมดเชื่อต่อ

const unsigned long eventInterval             = 2 * 1000;          // อ่านค่า temp และ soil sensor ทุก ๆ 2 วินาที
const unsigned long eventInterval_brightness  = 6 * 1000;          // อ่านค่า brightness sensor ทุก ๆ 6 วินาที
unsigned long previousTime_Temp_soil          = 0;
unsigned long previousTime_brightness         = 0;

// ประกาศตัวแปรกำหนดการนับเวลาเริ่มต้น
unsigned long previousTime_Update_data        = 0;
const unsigned long eventInterval_publishData = 2 * 60 * 1000;     // เช่น 2*60*1000 ส่งทุก 2 นาที

float difference_soil                         = 20.00,    // ค่าความชื้นดินแตกต่างกัน +-20 % เมื่อไรส่งค่าขึ้น Web app ทันที
      difference_temp                         = 4.00;     // ค่าอุณหภูมิแตกต่างกัน +- 4 C เมื่อไรส่งค่าขึ้น Web app ทันที
float soil_old  = 0.00,
      temp_old  = 0.00;

// ประกาศตัวแปรสำหรับเก็บค่าเซ็นเซอร์
float   temp              = 0;
int     temp_error        = 0;
int     temp_error_count  = 0;

float   humidity          = 0;
int     hum_error         = 0;
int     hum_error_count   = 0;

float   lux_44009         = 0;
int     lux_error         = 0;
int     lux_error_count   = 0;

float   soil              = 0;
int     soil_error        = 0;
int     soil_error_count  = 0;

// Array สำหรับทำ Movie Arg. ของค่าเซ็นเซอร์ทุก ๆ ค่า
float ma_temp[5];
float ma_hum[5];
float ma_soil[5];
float ma_lux[5];

// สำหรับเก็บค่าเวลาจาก Web App
int t[20];
#define state_On_Off_relay        t[0]
#define value_monday_from_Web     t[1]
#define value_Tuesday_from_Web    t[2]
#define value_Wednesday_from_Web  t[3]
#define value_Thursday_from_Web   t[4]
#define value_Friday_from_Web     t[5]
#define value_Saturday_from_Web   t[6]
#define value_Sunday_from_Web     t[7]
#define value_hour_Open           t[8]
#define value_min_Open            t[9]
#define value_hour_Close          t[11]
#define value_min_Close           t[12]

#define OPEN        1
#define CLOSE       0

#define Open_relay(j)    digitalWrite(relay_pin[j], HIGH)
#define Close_relay(j)   digitalWrite(relay_pin[j], LOW)

#define connect_WifiStatusToBox     23

/* new PCB Red */
int relay_pin[4] = {25, 4, 12, 13};
#define status_sht31_error          5
#define status_max44009_error       18
#define status_soil_error           19

// ตัวแปรเก็บค่าการตั้งเวลาทำงานอัตโนมัติ
unsigned int time_open[4][7][3] = {{{2000, 2000, 2000}, {2000, 2000, 2000}, {2000, 2000, 2000},
    {2000, 2000, 2000}, {2000, 2000, 2000}, {2000, 2000, 2000}, {2000, 2000, 2000}
  },
  { {2000, 2000, 2000}, {2000, 2000, 2000}, {2000, 2000, 2000},
    {2000, 2000, 2000}, {2000, 2000, 2000}, {2000, 2000, 2000}, {2000, 2000, 2000}
  },
  { {2000, 2000, 2000}, {2000, 2000, 2000}, {2000, 2000, 2000},
    {2000, 2000, 2000}, {2000, 2000, 2000}, {2000, 2000, 2000}, {2000, 2000, 2000}
  },
  { {2000, 2000, 2000}, {2000, 2000, 2000}, {2000, 2000, 2000},
    {2000, 2000, 2000}, {2000, 2000, 2000}, {2000, 2000, 2000}, {2000, 2000, 2000}
  }
};
unsigned int time_close[4][7][3] = {{{2000, 2000, 2000}, {2000, 2000, 2000}, {2000, 2000, 2000},
    {2000, 2000, 2000}, {2000, 2000, 2000}, {2000, 2000, 2000}, {2000, 2000, 2000}
  },
  { {2000, 2000, 2000}, {2000, 2000, 2000}, {2000, 2000, 2000},
    {2000, 2000, 2000}, {2000, 2000, 2000}, {2000, 2000, 2000}, {2000, 2000, 2000}
  },
  { {2000, 2000, 2000}, {2000, 2000, 2000}, {2000, 2000, 2000},
    {2000, 2000, 2000}, {2000, 2000, 2000}, {2000, 2000, 2000}, {2000, 2000, 2000}
  },
  { {2000, 2000, 2000}, {2000, 2000, 2000}, {2000, 2000, 2000},
    {2000, 2000, 2000}, {2000, 2000, 2000}, {2000, 2000, 2000}, {2000, 2000, 2000}
  }
};

float Max_Soil[4], Min_Soil[4];
float Max_Temp[4], Min_Temp[4];

unsigned int statusTimer_open[4] = {1, 1, 1, 1};
unsigned int statusTimer_close[4] = {1, 1, 1, 1};
unsigned int status_manual[4];

unsigned int statusSoil[4];
unsigned int statusTemp[4];

// สถานะการทำงานของ Relay ด้ววยค่า Min Max
int relayMaxsoil_status[4];
int relayMinsoil_status[4];
int relayMaxtemp_status[4];
int relayMintemp_status[4];

int RelayStatus[4];
TaskHandle_t WifiStatus, WaitSerial;
unsigned int oldTimer;

// สถานะการรเชื่อมต่อ wifi
#define cannotConnect   0
#define wifiConnected   1
#define serverConnected 2
#define editDeviceWifi  3
int connectWifiStatus = cannotConnect;

int check_sendData_status = 0;
int check_sendData_toWeb  = 0;
int check_sendData_SoilMinMax = 0;
int check_sendData_tempMinMax = 0;

#define INTERVAL_MESSAGE 600000 //10 นาที
#define INTERVAL_MESSAGE2 1500000 //1 ชม
unsigned long time_restart = 0;

/* ----------------------- OTA Function => One on One --------------------------- */
void OTA_update() {
  DEBUG_PRINTLN("");
  DEBUG_PRINT("Connected to "); DEBUG_PRINTLN(ssid);
  DEBUG_PRINT("IP address: ");  DEBUG_PRINTLN(WiFi.localIP());
  if (!MDNS.begin(host)) {}
  DEBUG_PRINTLN("mDNS responder started");
  /*return index page which is stored in serverIndex */
  server.on("/", HTTP_GET, []() {
    server.sendHeader("Connection", "close");
    server.send(200, "text/html", loginIndex);
  });
  server.on("/serverIndex", HTTP_GET, []() {
    server.sendHeader("Connection", "close");
    server.send(200, "text/html", serverIndex);
  });
  /*handling uploading firmware file */
  server.on("/update", HTTP_POST, []() {
    server.sendHeader("Connection", "close");
    server.send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK");
    ESP.restart();
  }, []() {
    HTTPUpload& upload = server.upload();
    if (upload.status == UPLOAD_FILE_START) {
      Serial.printf("Update: %s\n", upload.filename.c_str());
      if (!Update.begin(UPDATE_SIZE_UNKNOWN)) { //start with max available size
        Update.printError(Serial);
      }
    } else if (upload.status == UPLOAD_FILE_WRITE) {
      /* flashing firmware to ESP*/
      if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
        Update.printError(Serial);
      }
    } else if (upload.status == UPLOAD_FILE_END) {
      if (Update.end(true)) { //true to set the size to the current progress
        Serial.printf("Update Success: %u\nRebooting...\n", upload.totalSize);
      } else {
        Update.printError(Serial);
      }
    }
  });
  server.begin();
}

/* --------- Callback function get data from web ---------- */
void callback(String topic, byte* payload, unsigned int length) {
  //DEBUG_PRINT("Message arrived [");
  DEBUG_PRINT("Message arrived [");
  DEBUG_PRINT(topic);
  DEBUG_PRINT("] ");
  String message;
  for (int i = 0; i < length; i++) {
    message = message + (char)payload[i];
  }
  /* ------- topic timer ------- */
  if (topic.substring(0, 14) == "@private/timer") {
    timmer_setting(topic, payload, length);
    sent_dataTimer(topic, message);
  }
  /* ------- topic manual_control relay ------- */
  else if (topic.substring(0, 12) == "@private/led") {
    status_manual[0] = 0;
    status_manual[1] = 0;
    status_manual[2] = 0;
    status_manual[3] = 0;
    ControlRelay_Bymanual(topic, message, length);
  }
  /* ------- topic Soil min max ------- */
  else if (topic.substring(0, 17) == "@private/max_temp" || topic.substring(0, 17) == "@private/min_temp") {
    TempMaxMin_setting(topic, message, length);
  }
  /* ------- topic Temp min max ------- */
  else if (topic.substring(0, 17) == "@private/max_soil" || topic.substring(0, 17) == "@private/min_soil") {
    SoilMaxMin_setting(topic, message, length);
  }
}

/* ----------------------- Sent Timer --------------------------- */
void sent_dataTimer(String topic, String message) {
  String _numberTimer = topic.substring(topic.length() - 2).c_str();
  String _payload = "{\"data\":{\"value_timer";
  _payload += _numberTimer;
  _payload += "\":\"";
  _payload += message;
  _payload += "\"}}";
  DEBUG_PRINT("incoming : "); DEBUG_PRINTLN((char*)_payload.c_str());
  client.publish("@shadow/data/update", (char*)_payload.c_str());
}

/* --------- UpdateData_To_Server --------- */
// void UpdateData_To_Server() {
//   String DatatoWeb;
//   char msgtoWeb[200];
//   if (check_sendData_toWeb == 1) {
//     DatatoWeb = "{\"data\": {\"temperature\":" + String(temp) +
//                 ",\"humidity\":" + String(humidity) + ",\"lux\":" +
//                 String(lux_44009) + ",\"soil\":" + String(soil)  + "}}";

//     DEBUG_PRINT("DatatoWeb : "); DEBUG_PRINTLN(DatatoWeb);
//     DatatoWeb.toCharArray(msgtoWeb, (DatatoWeb.length() + 1));
//     if (client.publish("@shadow/data/update", msgtoWeb)) {
//       check_sendData_toWeb = 0;
//       DEBUG_PRINTLN(" Send Data Complete ");
//     }
//     digitalWrite(LEDY, HIGH);
//   }
// }

/* --------- UpdateData_To_Server --------- */
void UpdateData_To_Server() {
  String DatatoWeb;
    char msgtoWeb[200];
    DatatoWeb = "{\"data\": {\"temperature\":" + String(temp) +
                ",\"humidity\":" + String(humidity) + ",\"lux\":" +
                String(lux_44009) + ",\"soil\":" + String(soil)  + "}}";

    DEBUG_PRINT("DatatoWeb : "); DEBUG_PRINTLN(DatatoWeb);
    DatatoWeb.toCharArray(msgtoWeb, (DatatoWeb.length() + 1));
    if (client.publish("@shadow/data/update", msgtoWeb)) {
      DEBUG_PRINTLN(" Send Data Complete ");
    }
}

/* --------- sendStatus_RelaytoWeb --------- */
void sendStatus_RelaytoWeb() {
  String _payload;
  char msgUpdateRalay[200];
  if (check_sendData_status == 1) {
    _payload = "{\"data\": {\"led0\":\"" + String(RelayStatus[0]) +
               "\",\"led1\":\"" + String(RelayStatus[1]) +
               "\",\"led2\":\"" + String(RelayStatus[2]) +
               "\",\"led3\":\"" + String(RelayStatus[3]) + "\"}}";
    DEBUG_PRINT("_payload : "); DEBUG_PRINTLN(_payload);
    _payload.toCharArray(msgUpdateRalay, (_payload.length() + 1));
    if (client.publish("@shadow/data/update", msgUpdateRalay)) {
      check_sendData_status = 0;
      DEBUG_PRINTLN(" Send Complete Relay ");
    }
  }
}

/* --------- Respone soilMinMax toWeb --------- */
void send_soilMinMax() {
  String soil_payload;
  char soilMinMax_data[450];
  if (check_sendData_SoilMinMax == 1) {
    soil_payload =  "{\"data\": {\"min_soil0\":" + String(Min_Soil[0]) + ",\"max_soil0\":" + String(Max_Soil[0]) +
                    ",\"min_soil1\":" + String(Min_Soil[1]) + ",\"max_soil1\":" + String(Max_Soil[1]) +
                    ",\"min_soil2\":" + String(Min_Soil[2]) + ",\"max_soil2\":" + String(Max_Soil[2]) +
                    ",\"min_soil3\":" + String(Min_Soil[3]) + ",\"max_soil3\":" + String(Max_Soil[3]) + "}}";
    DEBUG_PRINT("_payload : "); DEBUG_PRINTLN(soil_payload);
    soil_payload.toCharArray(soilMinMax_data, (soil_payload.length() + 1));
    if (client.publish("@shadow/data/update", soilMinMax_data)) {
      check_sendData_SoilMinMax = 0;
      DEBUG_PRINTLN(" Send Complete min max ");
    }
  }
}

/* --------- Respone tempMinMax toWeb --------- */
void send_tempMinMax() {
  String temp_payload;
  char tempMinMax_data[400];
  if (check_sendData_tempMinMax == 1) {
    temp_payload =  "{\"data\": {\"min_temp0\":" + String(Min_Temp[0]) + ",\"max_temp0\":" + String(Max_Temp[0]) +
                    ",\"min_temp1\":" + String(Min_Temp[1]) + ",\"max_temp1\":" + String(Max_Temp[1]) +
                    ",\"min_temp2\":" + String(Min_Temp[2]) + ",\"max_temp2\":" + String(Max_Temp[2]) +
                    ",\"min_temp3\":" + String(Min_Temp[3]) + ",\"max_temp3\":" + String(Max_Temp[3]) + "}}";
    DEBUG_PRINT("_payload : "); DEBUG_PRINTLN(temp_payload);
    temp_payload.toCharArray(tempMinMax_data, (temp_payload.length() + 1));
    if (client.publish("@shadow/data/update", tempMinMax_data)) {
      check_sendData_tempMinMax = 0;
    }
  }
}

/* ----------------------- Setting Timer --------------------------- */
void timmer_setting(String topic, byte * payload, unsigned int length) {
  int timer, relay;
  char* str;
  unsigned int count = 0;
  char message_time[50];
  timer = topic.substring(topic.length() - 1).toInt();
  relay = topic.substring(topic.length() - 2, topic.length() - 1).toInt();
  DEBUG_PRINTLN();
  DEBUG_PRINT("timeer     : "); DEBUG_PRINTLN(timer);
  DEBUG_PRINT("relay      : "); DEBUG_PRINTLN(relay);
  for (int i = 0; i < length; i++) {
    message_time[i] = (char)payload[i];
  }
  DEBUG_PRINTLN(message_time);
  str = strtok(message_time, " ,,,:");
  while (str != NULL) {
    t[count] = atoi(str);
    count++;
    str = strtok(NULL, " ,,,:");
  }
  if (state_On_Off_relay == 1) {
    for (int k = 0; k < 7; k++) {
      if (t[k + 1] == 1) {
        time_open[relay][k][timer] = (value_hour_Open * 60) + value_min_Open;
        time_close[relay][k][timer] = (value_hour_Close * 60) + value_min_Close;
      }
      else {
        time_open[relay][k][timer] = 3000;
        time_close[relay][k][timer] = 3000;
      }
      int address = ((((relay * 7 * 3) + (k * 3) + timer) * 2) * 2) + 2100;
      EEPROM.write(address, time_open[relay][k][timer] / 256);
      EEPROM.write(address + 1, time_open[relay][k][timer] % 256);
      EEPROM.write(address + 2, time_close[relay][k][timer] / 256);
      EEPROM.write(address + 3, time_close[relay][k][timer] % 256);
      EEPROM.commit();
      DEBUG_PRINT("time_open  : "); DEBUG_PRINTLN(time_open[relay][k][timer]);
      DEBUG_PRINT("time_close : "); DEBUG_PRINTLN(time_close[relay][k][timer]);
    }
  }
  else if (state_On_Off_relay == 0) {
    for (int k = 0; k < 7; k++) {
      time_open[relay][k][timer] = 3000;
      time_close[relay][k][timer] = 3000;
      int address = ((((relay * 7 * 3) + (k * 3) + timer) * 2) * 2) + 2100;
      EEPROM.write(address, time_open[relay][k][timer] / 256);
      EEPROM.write(address + 1, time_open[relay][k][timer] % 256);
      EEPROM.write(address + 2, time_close[relay][k][timer] / 256);
      EEPROM.write(address + 3, time_close[relay][k][timer] % 256);
      EEPROM.commit();
      DEBUG_PRINT("time_open  : "); DEBUG_PRINTLN(time_open[relay][k][timer]);
      DEBUG_PRINT("time_close : "); DEBUG_PRINTLN(time_close[relay][k][timer]);
    }
  }
  else {
    DEBUG_PRINTLN("Not enabled timer, Day !!!");
  }
}

/* ------------ Control Relay By Timmer ------------- */
void ControlRelay_Bytimmer() {
  int curentTimer;
  int dayofweek;
  if (WiFi.status() == WL_CONNECTED) {
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer, nistTime);
    if (getLocalTime(&timeinfo)) {
      yearNow     = timeinfo.tm_year + 1900;
      monthNow    = timeinfo.tm_mon + 1;
      dayNow      = timeinfo.tm_mday;
      weekdayNow  = timeinfo.tm_wday;
      hourNow     = timeinfo.tm_hour;
      minuteNow   = timeinfo.tm_min;
      secondNow   = timeinfo.tm_sec;

      curentTimer = (hourNow * 60) + minuteNow;
      dayofweek = weekdayNow - 1;
    } else {
      _now = rtc.now();
      curentTimer = (_now.hour() * 60) + _now.minute();
      dayofweek = _now.dayOfTheWeek() - 1;
      DEBUG_PRINT("USE RTC 1");
    }
  }
  else {
    _now = rtc.now();
    curentTimer = (_now.hour() * 60) + _now.minute();
    dayofweek = _now.dayOfTheWeek() - 1;
    DEBUG_PRINT("USE RTC 2");
  }
  //DEBUG_PRINT("curentTimer : "); DEBUG_PRINTLN(curentTimer);
  /* check curentTimer => 0-1440 */
  if (curentTimer < 0 || curentTimer > 1440) {
    curentTimerError = 1;
    DEBUG_PRINT("curentTimerError : "); DEBUG_PRINTLN(curentTimerError);
  } else {
    curentTimerError = 0;
    if (dayofweek == -1) {
      dayofweek = 6;
    }
    //DEBUG_PRINT("dayofweek   : "); DEBUG_PRINTLN(dayofweek);
    if (curentTimer != oldTimer) {
      for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 3; j++) {
          if (time_open[i][dayofweek][j] == curentTimer) {
            RelayStatus[i] = 1;
            check_sendData_status = 1;
            Open_relay(i);
            DEBUG_PRINTLN("timer On");
            DEBUG_PRINT("curentTimer : "); DEBUG_PRINTLN(curentTimer);
            DEBUG_PRINT("oldTimer    : "); DEBUG_PRINTLN(oldTimer);
          }
          else if (time_close[i][dayofweek][j] == curentTimer) {
            RelayStatus[i] = 0;
            check_sendData_status = 1;
            Close_relay(i);
            DEBUG_PRINTLN("timer Off");
            DEBUG_PRINT("curentTimer : "); DEBUG_PRINTLN(curentTimer);
            DEBUG_PRINT("oldTimer    : "); DEBUG_PRINTLN(oldTimer);
          }
          else if (time_open[i][dayofweek][j] == 3000 && time_close[i][dayofweek][j] == 3000) {
            //        Close_relay(i);
            //        DEBUG_PRINTLN(" Not check day, Not Working relay");
          }
        }
      }
      oldTimer = curentTimer;
    }
  }
}

/* ----------------------- Manual Control --------------------------- */
void ControlRelay_Bymanual(String topic, String message, unsigned int length) {
  String manual_message = message;
  int manual_relay = topic.substring(topic.length() - 1).toInt();
  DEBUG_PRINTLN();
  DEBUG_PRINT("manual_message : "); DEBUG_PRINTLN(manual_message);
  DEBUG_PRINT("manual_relay   : "); DEBUG_PRINTLN(manual_relay);
  if (status_manual[manual_relay] == 0) {
    status_manual[manual_relay] = 1;
    if (manual_message == "on") {
      Open_relay(manual_relay);
      RelayStatus[manual_relay] = 1;
      DEBUG_PRINTLN("ON man");
    }
    else if (manual_message == "off") {
      Close_relay(manual_relay);
      RelayStatus[manual_relay] = 0;
      DEBUG_PRINTLN("OFF man");
    }
    check_sendData_status = 1;
  }
}

/* ----------------------- SoilMaxMin_setting --------------------------- */
void SoilMaxMin_setting(String topic, String message, unsigned int length) {
  String soil_message = message;
  String soil_topic = topic;
  int Relay_SoilMaxMin = topic.substring(topic.length() - 1).toInt();
  if (soil_topic.substring(9, 12) == "max") {
    relayMaxsoil_status[Relay_SoilMaxMin] = topic.substring(topic.length() - 1).toInt();
    Max_Soil[Relay_SoilMaxMin] = soil_message.toInt();
    EEPROM.write(Relay_SoilMaxMin + 2000,  Max_Soil[Relay_SoilMaxMin]);
    EEPROM.commit();
    check_sendData_SoilMinMax = 1;
    DEBUG_PRINT("Max_Soil : "); DEBUG_PRINTLN(Max_Soil[Relay_SoilMaxMin]);
  }
  else if (soil_topic.substring(9, 12) == "min") {
    relayMinsoil_status[Relay_SoilMaxMin] = topic.substring(topic.length() - 1).toInt();
    Min_Soil[Relay_SoilMaxMin] = soil_message.toInt();
    EEPROM.write(Relay_SoilMaxMin + 2004,  Min_Soil[Relay_SoilMaxMin]);
    EEPROM.commit();
    check_sendData_SoilMinMax = 1;
    DEBUG_PRINT("Min_Soil : "); DEBUG_PRINTLN(Min_Soil[Relay_SoilMaxMin]);
  }
}

/* ----------------------- TempMaxMin_setting --------------------------- */
void TempMaxMin_setting(String topic, String message, unsigned int length) {
  String temp_message = message;
  String temp_topic = topic;
  int Relay_TempMaxMin = topic.substring(topic.length() - 1).toInt();
  if (temp_topic.substring(9, 12) == "max") {
    Max_Temp[Relay_TempMaxMin] = temp_message.toInt();
    EEPROM.write(Relay_TempMaxMin + 2008, Max_Temp[Relay_TempMaxMin]);
    EEPROM.commit();
    check_sendData_tempMinMax = 1;
    DEBUG_PRINT("Max_Temp : "); DEBUG_PRINTLN(Max_Temp[Relay_TempMaxMin]);
  }
  else if (temp_topic.substring(9, 12) == "min") {
    Min_Temp[Relay_TempMaxMin] = temp_message.toInt();
    EEPROM.write(Relay_TempMaxMin + 2012,  Min_Temp[Relay_TempMaxMin]);
    EEPROM.commit();
    check_sendData_tempMinMax = 1;
    DEBUG_PRINT("Min_Temp : "); DEBUG_PRINTLN(Min_Temp[Relay_TempMaxMin]);
  }
}

/* ----------------------- soilMinMax_ControlRelay --------------------------- */
void ControlRelay_BysoilMinMax() {
  Get_soil();
  for (int k = 0; k < 4; k++) {
    if (Min_Soil[k] != 0 && Max_Soil[k] != 0) {
      if (soil < Min_Soil[k]) {
        //DEBUG_PRINT("statusSoilMin"); DEBUG_PRINT(k); DEBUG_PRINT(" : "); DEBUG_PRINTLN(statusTemp[k]);
        if (statusSoil[k] == 0) {
          Open_relay(k);
          statusSoil[k] = 1;
          RelayStatus[k] = 1;
          check_sendData_status = 1;
          digitalWrite(LEDY, HIGH);
          //check_sendData_toWeb = 1;
          DEBUG_PRINTLN("soil On");
        }
      }
      else if (soil > Max_Soil[k]) {
        //DEBUG_PRINT("statusSoilMax"); DEBUG_PRINT(k); DEBUG_PRINT(" : "); DEBUG_PRINTLN(statusTemp[k]);
        if (statusSoil[k] == 1) {
          Close_relay(k);
          statusSoil[k] = 0;
          RelayStatus[k] = 0;
          check_sendData_status = 1;
          digitalWrite(LEDY, HIGH);
          //check_sendData_toWeb = 1;
          DEBUG_PRINTLN("soil Off");
        }
      }
    }
  }
}

/* ----------------------- tempMinMax_ControlRelay --------------------------- */
void ControlRelay_BytempMinMax() {
  Get_sht31();
  for (int g = 0; g < 4; g++) {
    if (Min_Temp[g] != 0 && Max_Temp[g] != 0) {
      if (temp < Min_Temp[g]) {
        //DEBUG_PRINT("statusTempMin"); DEBUG_PRINT(g); DEBUG_PRINT(" : "); DEBUG_PRINTLN(statusTemp[g]);
        if (statusTemp[g] == 1) {
          Close_relay(g);
          statusTemp[g] = 0;
          RelayStatus[g] = 0;
          check_sendData_status = 1;
          digitalWrite(LEDY, HIGH);
          //check_sendData_toWeb = 1;
          DEBUG_PRINTLN("temp Off");
        }
      }
      else if (temp > Max_Temp[g]) {
        //DEBUG_PRINT("statusTempMax"); DEBUG_PRINT(g); DEBUG_PRINT(" : "); DEBUG_PRINTLN(statusTemp[g]);
        if (statusTemp[g] == 0) {
          Open_relay(g);
          statusTemp[g] = 1;
          RelayStatus[g] = 1;
          check_sendData_status = 1;
          digitalWrite(LEDY, HIGH);
          //check_sendData_toWeb = 1;
          DEBUG_PRINTLN("temp On");
        }
      }
    }
  }
}

/* ----------------------- Mode for calculator sensor i2c --------------------------- */
int Mode(float* getdata) {
  int maxValue = 0;
  int maxCount = 0;
  for (int i = 0; i < sizeof(getdata); ++i) {
    int count = 0;
    for (int j = 0; j < sizeof(getdata); ++j) {
      if (round(getdata[j]) == round(getdata[i]))
        ++count;
    }
    if (count > maxCount) {
      maxCount = count;
      maxValue = round(getdata[i]);
    }
  }
  return maxValue;
}

/* ----------------------- Calculator sensor SHT31  --------------------------- */
void Get_sht31() {
  float buffer_temp = 0;
  float buffer_hum  = 0;
  float temp_cal    = 0;
  int num_temp      = 0;
  buffer_temp = sht31.readTemperature();
  buffer_hum = sht31.readHumidity();
  if (buffer_temp < -40 || buffer_temp > 125 || isnan(buffer_temp)) { // range -40 to 125 C
    if (temp_error_count >= 10) {
      temp_error = 1;
      digitalWrite(status_sht31_error, LOW);
      DEBUG_PRINT("temp_error : "); DEBUG_PRINTLN(temp_error);
    } else {
      temp_error_count++;
    }
    DEBUG_PRINT("temp_error_count  : "); DEBUG_PRINTLN(temp_error_count);
  } else {
    digitalWrite(status_sht31_error, HIGH);
    ma_temp[4] = ma_temp[3];
    ma_temp[3] = ma_temp[2];
    ma_temp[2] = ma_temp[1];
    ma_temp[1] = ma_temp[0];
    ma_temp[0] = buffer_temp;

    int mode_value_temp = Mode(ma_temp);
    for (int i = 0; i < sizeof(ma_temp); i++) {
      if (abs(mode_value_temp - ma_temp[i]) < 1) {
        temp_cal = temp_cal + ma_temp[i];
        num_temp++;
      }
    }
    temp = temp_cal / num_temp;
    temp_error = 0;
    temp_error_count = 0;
  }
  float hum_cal     = 0;
  int num_hum       = 0;
  if (buffer_hum < 0 || buffer_hum > 100  || isnan(buffer_hum)) { // range 0 to 100 %RH
    if (hum_error_count >= 10) {
      hum_error = 1;
      digitalWrite(status_sht31_error, LOW);
      DEBUG_PRINT("hum_error  : "); DEBUG_PRINTLN(hum_error);
    } else {
      hum_error_count++;
    }
    DEBUG_PRINT("hum_error_count  : "); DEBUG_PRINTLN(hum_error_count);
  } else {
    digitalWrite(status_sht31_error, HIGH);
    ma_hum[4] = ma_hum[3];
    ma_hum[3] = ma_hum[2];
    ma_hum[2] = ma_hum[1];
    ma_hum[1] = ma_hum[0];
    ma_hum[0] = buffer_hum;

    int mode_value_hum = Mode(ma_hum);
    for (int j = 0; j < sizeof(ma_hum); j++) {
      if (abs(mode_value_hum - ma_hum[j]) < 1) {
        hum_cal = hum_cal + ma_hum[j];
        num_hum++;
      }
    }
    humidity = hum_cal / num_hum;
    hum_error = 0;
    hum_error_count = 0;
  }
}

/* ----------------------- Calculator sensor Max44009 --------------------------- */
void Get_max44009() {
  float buffer_lux  = 0;
  float lux_cal     = 0;
  int num_lux       = 0;
  buffer_lux = myLux.getLux() / 1000;
  if (buffer_lux < 0 || buffer_lux > 188000 || isnan(buffer_lux)) { // range 0.045 to 188,000 lux
    if (lux_error_count >= 10) {
      lux_error = 1;
      digitalWrite(status_max44009_error, LOW);
      DEBUG_PRINT("lux_error  : "); DEBUG_PRINTLN(lux_error);
    } else {
      lux_error_count++;
    }
    DEBUG_PRINT("lux_error_count  : "); DEBUG_PRINTLN(lux_error_count);
  } else {
    digitalWrite(status_max44009_error, HIGH);
    ma_lux[4] = ma_lux[3];
    ma_lux[3] = ma_lux[2];
    ma_lux[2] = ma_lux[1];
    ma_lux[1] = ma_lux[0];
    ma_lux[0] = buffer_lux;

    int mode_value_lux = Mode(ma_lux);
    for (int i = 0; i < sizeof(ma_lux); i++) {
      if (abs(mode_value_lux - ma_lux[i]) < 1) {
        lux_cal = lux_cal + ma_lux[i];
        num_lux++;
      }
    }
    lux_44009 = lux_cal / num_lux;
    lux_error = 0;
    lux_error_count = 0;
  }
}

/* ----------------------- Calculator sensor Soil  --------------------------- */
void Get_soil() {
  float buffer_soil = 0;
  sensorValue_soil_moisture = analogRead(Soil_moisture_sensorPin);
  voltageValue_soil_moisture = (sensorValue_soil_moisture * 3.3) / (4095.00);
  buffer_soil = ((-58.82) * voltageValue_soil_moisture) + 123.52;
  if (buffer_soil < 0 || buffer_soil > 100 || isnan(buffer_soil)) { // range 0 to 100 %
    if (soil_error_count >= 10) {
      soil_error = 1;
      digitalWrite(status_soil_error, LOW);
      DEBUG_PRINT("soil_error : "); DEBUG_PRINTLN(soil_error);
    } else {
      soil_error_count++;
    }
    DEBUG_PRINT("soil_error_count  : "); DEBUG_PRINTLN(soil_error_count);
  } else {
    digitalWrite(status_soil_error, HIGH);
    ma_soil[4] = ma_soil[3];
    ma_soil[3] = ma_soil[2];
    ma_soil[2] = ma_soil[1];
    ma_soil[1] = ma_soil[0];
    ma_soil[0] = buffer_soil;
    soil = (ma_soil[0] + ma_soil[1] + ma_soil[2] + ma_soil[3] + ma_soil[4]) / 5;
    if (soil <= 0) {
      soil = 0;
    }
    else if (soil >= 100) {
      soil = 100;
    }
    soil_error = 0;
    soil_error_count = 0;
  }
}

void printLocalTime() {
  if (!getLocalTime(&timeinfo)) {
    DEBUG_PRINTLN("Failed to obtain time");
    return;
  } //DEBUG_PRINTLN(&timeinfo, "%A, %d %B %Y %H:%M:%S");
}

/* ----------------------- Set All Config --------------------------- */
void setAll_config() {
  for (int b = 0; b < 4; b++) {
    Max_Soil[b] = EEPROM.read(b + 2000);
    Min_Soil[b] = EEPROM.read(b + 2004);
    Max_Temp[b] = EEPROM.read(b + 2008);
    Min_Temp[b] = EEPROM.read(b + 2012);
    if (Max_Soil[b] >= 255) {
      Max_Soil[b] = 0;
    }
    if (Min_Soil[b] >= 255) {
      Min_Soil[b] = 0;
    }
    if (Max_Temp[b] >= 255) {
      Max_Temp[b] = 0;
    }
    if (Min_Temp[b] >= 255) {
      Min_Temp[b] = 0;
    }
    DEBUG_PRINT("Max_Soil   ");  DEBUG_PRINT(b); DEBUG_PRINT(" : "); DEBUG_PRINTLN(Max_Soil[b]);
    DEBUG_PRINT("Min_Soil   ");  DEBUG_PRINT(b); DEBUG_PRINT(" : "); DEBUG_PRINTLN(Min_Soil[b]);
    DEBUG_PRINT("Max_Temp   ");  DEBUG_PRINT(b); DEBUG_PRINT(" : "); DEBUG_PRINTLN(Max_Temp[b]);
    DEBUG_PRINT("Min_Temp   ");  DEBUG_PRINT(b); DEBUG_PRINT(" : "); DEBUG_PRINTLN(Min_Temp[b]);
  }
  int count_in = 0;
  for (int eeprom_relay = 0; eeprom_relay < 4; eeprom_relay++) {
    for (int eeprom_timer = 0; eeprom_timer < 3; eeprom_timer++) {
      for (int dayinweek = 0; dayinweek < 7; dayinweek++) {
        int eeprom_address = ((((eeprom_relay * 7 * 3) + (dayinweek * 3) + eeprom_timer) * 2) * 2) + 2100;
        time_open[eeprom_relay][dayinweek][eeprom_timer] = (EEPROM.read(eeprom_address) * 256) + (EEPROM.read(eeprom_address + 1));
        time_close[eeprom_relay][dayinweek][eeprom_timer] = (EEPROM.read(eeprom_address + 2) * 256) + (EEPROM.read(eeprom_address + 3));

        if (time_open[eeprom_relay][dayinweek][eeprom_timer] >= 2000) {
          time_open[eeprom_relay][dayinweek][eeprom_timer] = 3000;
        }
        if (time_close[eeprom_relay][dayinweek][eeprom_timer] >= 2000) {
          time_close[eeprom_relay][dayinweek][eeprom_timer] = 3000;
        }
        DEBUG_PRINT("cout       : "); DEBUG_PRINTLN(count_in);
        DEBUG_PRINT("time_open  : "); DEBUG_PRINTLN(time_open[eeprom_relay][dayinweek][eeprom_timer]);
        DEBUG_PRINT("time_close : "); DEBUG_PRINTLN(time_close[eeprom_relay][dayinweek][eeprom_timer]);
        count_in++;
      }
    }
  }
}

/* ----------------------- Delete All Config --------------------------- */
void Delete_All_config() {
  for (int b = 2000; b < 4096; b++) {
    EEPROM.write(b, 255);
    EEPROM.commit();
  }
}

/* ----------------------- Add and Edit device || Edit Wifi --------------------------- */
void Edit_device_wifi() {
  connectWifiStatus = editDeviceWifi;
  EepromStream eeprom(0, 1024);
  Serial.write(START_PATTERN, 3);               // ส่งข้อมูลชนิดไบต์ ส่งตัวอักษรไปบนเว็บ
  Serial.flush();                               // เครียบัฟเฟอร์ให้ว่าง
  deserializeJson(jsonDoc, eeprom);             // คือการรับหรืออ่านข้อมูล jsonDoc ใน eeprom
  client_old = jsonDoc["client"].as<String>();  // เก็บค่า client เก่าเพื่อเช็คกับ client ใหม่ ในการ reset Wifi ไม่ให้ลบค่า Config อื่นๆ
  Serial.write(STX);                            // 02 คือเริ่มส่ง
  serializeJsonPretty(jsonDoc, Serial);         // ส่งข่อมูลของ jsonDoc ไปบนเว็บ
  Serial.write(ETX);
  delay(1000);
  Serial.write(START_PATTERN, 3);               // ส่งข้อมูลชนิดไบต์ ส่งตัวอักษรไปบนเว็บ
  Serial.flush();
  jsonDoc["server"]   = NULL;
  jsonDoc["client"]   = NULL;
  jsonDoc["pass"]     = NULL;
  jsonDoc["user"]     = NULL;
  jsonDoc["password"] = NULL;
  jsonDoc["port"]     = NULL;
  jsonDoc["ssid"]     = NULL;
  jsonDoc["command"]  = NULL;
  Serial.write(STX);                      // 02 คือเริ่มส่ง
  serializeJsonPretty(jsonDoc, Serial);   // ส่งข่อมูลของ jsonDoc ไปบนเว็บ
  Serial.write(ETX);                      // 03 คือจบ
}

/* -------- webSerialJSON function ------- */
void webSerialJSON() {
  while (Serial.available() > 0) {
    Serial.setTimeout(10000);
    EepromStream eeprom(0, 1024);
    DeserializationError err = deserializeJson(jsonDoc, Serial);
    if (err == DeserializationError::Ok) {
      String command  =  jsonDoc["command"].as<String>();
      bool isValidData  =  !jsonDoc["client"].isNull();
      if (command == "restart") {
        delay(100);
        ESP.restart();
      }
      if (isValidData) {
        /* ------------------WRITING----------------- */
        serializeJson(jsonDoc, eeprom);
        eeprom.flush();
        // ถ้าไม่เหมือนคือเพิ่มอุปกรณ์ใหม่ // ถ้าเหมือนคือการเปลี่ยน wifi
        if (client_old != jsonDoc["client"].as<String>()) {
          Delete_All_config();
        }
        delay(100);
        ESP.restart();
      }
    }  else  {
      Serial.read();
    }
  }
}

/* --------- อินเตอร์รัป แสดงสถานะการเชื่อม wifi ------------- */
int buff_count_LED_serverConnected;
void IRAM_ATTR Blink_LED() {
  if (connectWifiStatus == editDeviceWifi) {
    digitalWrite(LEDR, HIGH);
    digitalWrite(connect_WifiStatusToBox, HIGH);
    digitalWrite(LEDY, !digitalRead(LEDY));
  } else  if (connectWifiStatus == cannotConnect) {
    digitalWrite(LEDY, HIGH);
    digitalWrite(LEDR, !digitalRead(LEDR));
    digitalWrite(connect_WifiStatusToBox, HIGH);
  } else if (connectWifiStatus == serverConnected) {
    digitalWrite(LEDR, LOW);
    digitalWrite(LEDY, LOW);
    digitalWrite(connect_WifiStatusToBox, LOW);
  } else if (connectWifiStatus == wifiConnected) {
    buff_count_LED_serverConnected++;
    if (buff_count_LED_serverConnected < 7) {
      digitalWrite(LEDR, !digitalRead(LEDR));
    } else if (buff_count_LED_serverConnected < 10) {
      digitalWrite(LEDR, LOW);
    } else buff_count_LED_serverConnected = 0;
  }
}

void setup() {
  hw_timer_t * timer = NULL;
  timer = timerBegin(0, 80, true);
  timerAttachInterrupt(timer, &Blink_LED, true);
  timerAlarmWrite(timer, 500000, true);
  timerAlarmEnable(timer);
  Serial.begin(115200);
  EEPROM.begin(4096);
  Wire.begin();
  Wire.setClock(10000);
  pinMode(Soil_moisture_sensorPin, INPUT);
  pinMode(relay_pin[0], OUTPUT);
  pinMode(relay_pin[1], OUTPUT);
  pinMode(relay_pin[2], OUTPUT);
  pinMode(relay_pin[3], OUTPUT);
  pinMode(LEDR, OUTPUT);
  pinMode(LEDY, OUTPUT);
  pinMode(connect_WifiStatusToBox,    OUTPUT);
  pinMode(status_sht31_error,         OUTPUT);
  pinMode(status_max44009_error,      OUTPUT);
  pinMode(status_soil_error,          OUTPUT);
  digitalWrite(connect_WifiStatusToBox, HIGH);
  digitalWrite(status_sht31_error,      HIGH);
  digitalWrite(status_max44009_error,   HIGH);
  digitalWrite(status_soil_error,       HIGH);
  digitalWrite(relay_pin[0], LOW);
  digitalWrite(relay_pin[1], LOW);
  digitalWrite(relay_pin[2], LOW);
  digitalWrite(relay_pin[3], LOW);
  if (! sht31.begin(0x44)) {}
  for (int x = 0; x < 20; x++) {
    digitalWrite(LEDY, 0);    digitalWrite(LEDR, 1);    delay(50);
    digitalWrite(LEDY, 1);    digitalWrite(LEDR, 0);    delay(50);
  }
  digitalWrite(LEDY, HIGH);     digitalWrite(LEDR, HIGH);
  Edit_device_wifi();
  EepromStream eeprom(0, 1024);           // ประกาศ Object eepromSteam ที่ Address 0 ขนาด 1024 byte
  deserializeJson(jsonDoc, eeprom);       // คือการรับหรืออ่านข้อมูล jsonDoc ใน eeprom
  if (!jsonDoc.isNull()) {                      // ถ้าใน jsonDoc มีค่าแล้ว
    mqtt_server   = jsonDoc["server"].as<String>();
    mqtt_Client   = jsonDoc["client"].as<String>();
    mqtt_password = jsonDoc["pass"].as<String>();
    mqtt_username = jsonDoc["user"].as<String>();
    password      = jsonDoc["password"].as<String>();
    mqtt_port     = jsonDoc["port"].as<String>();
    ssid          = jsonDoc["ssid"].as<String>();
  }
  xTaskCreatePinnedToCore(TaskWifiStatus, "WifiStatus", 4096, NULL, 1, &WifiStatus, 1);
  xTaskCreatePinnedToCore(TaskWaitSerial, "WaitSerial", 8192, NULL, 1, &WaitSerial, 1);
  setAll_config();
  delay(500);
  sensorValue_soil_moisture = analogRead(Soil_moisture_sensorPin);
  voltageValue_soil_moisture = (sensorValue_soil_moisture * 3.3) / (4095.00);
  ma_soil[0] = ma_soil[1] = ma_soil[2] = ma_soil[3] = ma_soil[4] = ((-58.82) * voltageValue_soil_moisture) + 123.52;
  ma_temp[0] =  ma_temp[1] =  ma_temp[2] =  ma_temp[3] =  ma_temp[4] = sht31.readTemperature();
  ma_hum[0] = ma_hum[1] = ma_hum[2] = ma_hum[3] = ma_hum[4] = sht31.readHumidity();
  ma_lux[0] = ma_lux[1] = ma_lux[2] = ma_lux[3] = ma_lux[4] = myLux.getLux() / 1000;
}

void loop() {
  client.loop();
  server.handleClient();
  delay(1);
  unsigned long currentTime = millis();
  if (currentTime - previousTime_Temp_soil >= eventInterval) {
    ControlRelay_Bytimmer();
    ControlRelay_BysoilMinMax();
    ControlRelay_BytempMinMax();

    DEBUG_PRINTLN("");
    DEBUG_PRINT("Temp : ");       DEBUG_PRINT(temp);      DEBUG_PRINT(" C, ");
    DEBUG_PRINT("Hum  : ");       DEBUG_PRINT(humidity);  DEBUG_PRINT(" %RH, ");
    DEBUG_PRINT("Brightness : "); DEBUG_PRINT(lux_44009); DEBUG_PRINT(" Klux, ");
    DEBUG_PRINT("Soil  : ");      DEBUG_PRINT(soil);      DEBUG_PRINTLN(" %");

    previousTime_Temp_soil = currentTime;
  }
  if (currentTime - previousTime_brightness >= eventInterval_brightness) {
    Get_max44009();
    previousTime_brightness = currentTime;
  }
  unsigned long currentTime_Update_data = millis();
  if (currentTime_Update_data - previousTime_Update_data >= (eventInterval_publishData)) {
    //check_sendData_toWeb = 1;
    UpdateData_To_Server();
    previousTime_Update_data = currentTime_Update_data;
    soil_old = soil;
    temp_old = temp;
  }
  //  if (abs(soil - soil_old) > difference_soil) {
  //    digitalWrite(LEDY, HIGH);
  //    check_sendData_toWeb = 1;
  //    soil_old = soil;
  //  }
  //  if (abs(temp - temp_old) > difference_temp) {
  //    digitalWrite(LEDY, HIGH);
  //    check_sendData_toWeb = 1;
  //    temp_old = temp;
  //  }
}

/* --------- Auto Connect Wifi and server and setup value init ------------- */
void TaskWifiStatus(void * pvParameters) {
  while (1) {
    connectWifiStatus = cannotConnect;
    WiFi.begin(ssid.c_str(), password.c_str());   
     
    while (WiFi.status() != WL_CONNECTED) {
      DEBUG_PRINTLN("WIFI Not connect !!!");

      /* -- ESP Reset -- */
      if (millis() - time_restart > INTERVAL_MESSAGE2) { // ผ่านไป 1 ชม. ยังไม่ได้เชื่อมต่อ Wifi ให้ Reset ตัวเอง
        time_restart = millis();
        ESP.restart();
      }
      delay(100);
    }

    connectWifiStatus = wifiConnected;
    client.setServer(mqtt_server.c_str(), mqtt_port.toInt());
    client.setCallback(callback);
    timeClient.begin();

    client.connect(mqtt_Client.c_str(), mqtt_username.c_str(), mqtt_password.c_str());
    delay(100);

    while (!client.connected() ) {
      client.connect(mqtt_Client.c_str(), mqtt_username.c_str(), mqtt_password.c_str());
      DEBUG_PRINTLN("NETPIE2020 can not connect");
      delay(100);
    }

    if (client.connect(mqtt_Client.c_str(), mqtt_username.c_str(), mqtt_password.c_str())) {
      connectWifiStatus = serverConnected;

      DEBUG_PRINTLN("NETPIE2020 connected");
      client.subscribe("@private/#");

      configTime(gmtOffset_sec, daylightOffset_sec, ntpServer, nistTime);
      printLocalTime();
      yearNow = timeinfo.tm_year + 1900;
      monthNow = timeinfo.tm_mon + 1;
      dayNow = timeinfo.tm_mday;
      hourNow = timeinfo.tm_hour;
      minuteNow = timeinfo.tm_min;
      secondNow = timeinfo.tm_sec;
      rtc.adjust(DateTime(yearNow, monthNow, dayNow, hourNow, minuteNow, secondNow));

      OTA_update();
    }
    while (WiFi.status() == WL_CONNECTED) { // เชื่อมต่อ wifi แล้ว ไม่ต้องทำอะไรนอกจากส่งค่า
      //UpdateData_To_Server();
      sendStatus_RelaytoWeb();
      send_soilMinMax();
      send_tempMinMax();   
      delay(500);
    }
  }
}

/* --------- Auto Connect Serial ------------- */
void TaskWaitSerial(void * WaitSerial) {
  while (1) {
    if (Serial.available())   webSerialJSON();
    delay(500);
  }
}
