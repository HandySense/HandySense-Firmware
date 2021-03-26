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
#define DEBUG_PRINT(x)    Serial.print(x)
#define DEBUG_PRINTLN(x)  Serial.println(x)
#endif

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

RTC_DS1307 rtc;
DateTime _now;
Max44009 myLux(0x4A);
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

String mqtt_server_old ,
       mqtt_Client_old ,
       mqtt_password_old ,
       mqtt_username_old ,
       password_old ,
       mqtt_port_old ,
       ssid_old ;

char msg[5000];
char msg_Minsoil[100],
     msg_Maxsoil[100];
char msg_Mintemp[100],
     msg_Maxtemp[100];

char msg_timer[5000];

WiFiClient espClient;
PubSubClient client(espClient);
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);

BH1750 lightMeter;
Adafruit_SHT31 sht31 = Adafruit_SHT31();

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
    weekday;
long current;
struct tm timeinfo;
String _data;

/* Soil_moisture_sensor */
#define Soil_moisture_sensorPin   A0
float sensorValue_soil_moisture   = 0.00,
      voltageValue_soil_moisture  = 0.00,
      percent_soil_moisture       = 0.00;

int LEDR = 26,  // LED 26= Blue
    LEDY = 27;  // LED 27 = Yenllow

const unsigned long limitTime_connectWifi     = 2 * 60 * 1000;
const unsigned long eventInterval             = 2 * 1000;
const unsigned long _reconnectInterval        = 5 * 1000;
const unsigned long limitTime_change          = 1 * 60 * 1000;
unsigned long previousTime                    = 0;
unsigned long previousTimeWifi                = 0;
unsigned long previousTime_offline            = 0;

const unsigned long eventInterval_publishData = 60 * 30; // 30 วินาที, 60*5 ส่งทุก 5 นาที
float difference_soil                         = 15.00,   // ค่าต่าง 15 % เมื่อไรส่งทันที
      difference_temp                         = 3.00;   // ค่าต่าง 3 เมื่อไรส่งทันที

float soil      = 0.00,
      temp      = 0.00,
      humidity  = 0.00,
      lux[2]    = {0.00, 0.00};
int err;

float soil_old  = 0.00,
      temp_old  = 0.00;

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

/* PCB Black and Green */
//#define mode_Add_device             18
//#define connect_WifiStatusToBox     19
//#define connect_ClientStatusToBox   33
//int relay_pin[4] = {25, 4, 16, 17};

/* new PCB Red */
#define mode_setWifi                11
#define mode_Add_device             2
#define connect_WifiStatusToBox     15
#define connect_ClientStatusToBox   33
int relay_pin[4] = {25, 4, 12, 13};

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

//long time_open[4][7][3];
//long time_close[4][7][3];

unsigned int Max_Soil[4], Min_Soil[4];
unsigned int Max_Temp[4], Min_Temp[4];

unsigned int statusTimer_open[4] = {1, 1, 1, 1};
unsigned int statusTimer_close[4] = {1, 1, 1, 1};
unsigned int status_manual[4];
unsigned int relay_address[4] = {0, 1, 2, 3};

unsigned int statusSoil_Open[4] = {1, 1, 1, 1};
unsigned int statusSoil_Close[4] = {1, 1, 1, 1};
unsigned int statusTemp_Open[4] = {1, 1, 1, 1};
unsigned int statusTemp_Close[4] = {1, 1, 1, 1};

int relayMaxsoil_status[4];
int relayMinsoil_status[4];
int relayMaxtemp_status[4];
int relayMintemp_status[4];

String manual_relayStatus[4];
int configTime_net;
TaskHandle_t Task1;
unsigned int oldTimer;

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
  /* ------- topic notify_status ------- */
  else if (topic == "@private/notify_status") {
    String notify_status = message;
    String notifyStatus_payload = "{\"data\":{\"notify_status";
    notifyStatus_payload += "\":\"";
    notifyStatus_payload += notify_status;
    notifyStatus_payload += "\"}}";
    DEBUG_PRINT("notifyStatus_payload : "); DEBUG_PRINTLN((char*)notifyStatus_payload.c_str());
    client.publish("@shadow/data/update", (char*)notifyStatus_payload.c_str());
  }
  /* ------- topic notify_relay ------- */
  //  else if (topic.substring(0, 21) == "@private/notify_relay") {
  //    String notify_message = message;
  //    int notify_relay = topic.substring(topic.length() - 1).toInt();
  //    for (int n = 0; n < 4; n++) {
  //      if (notify_relay == n) {
  //        String notify_payload = "{\"data\":{\"notify_relay";
  //        notify_payload += notify_relay;
  //        notify_payload += "\":\"";
  //        notify_payload += notify_message;
  //        notify_payload += "\"}}";
  //        DEBUG_PRINT("notify_payload : "); DEBUG_PRINTLN((char*)notify_payload.c_str());
  //        client.publish("@shadow/data/update", (char*)notify_payload.c_str());
  //      }
  //    }
  //  }
  else if (topic.substring(0, 20) == "@private/change_wifi") {
    String WifiPass_message = message;
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

/* --------- sendStatus_RelaytoWeb --------- */
void sendStatus_RelaytoWeb(int RelayNumber, String _status) {
  String _payload;
  if (_status == "on") {
    if (RelayNumber == 0) {
      _payload = "{\"data\": {\"led0\":" + String(1) + "}}";
    } else if (RelayNumber == 1) {
      _payload = "{\"data\": {\"led1\":" + String(1) + "}}";
    } else if (RelayNumber == 2) {
      _payload = "{\"data\": {\"led2\":" + String(1) + "}}";
    } else {
      _payload = "{\"data\": {\"led3\":" + String(1) + "}}";
    }
  } else {
    if (RelayNumber == 0) {
      _payload = "{\"data\": {\"led0\":" + String(0) + "}}";
    } else if (RelayNumber == 1) {
      _payload = "{\"data\": {\"led1\":" + String(0) + "}}";
    } else if (RelayNumber == 2) {
      _payload = "{\"data\": {\"led2\":" + String(0) + "}}";
    } else {
      _payload = "{\"data\": {\"led3\":" + String(0) + "}}";
    }
  }
  _payload.toCharArray(msg, (_payload.length() + 1));
  client.publish("@shadow/data/update", msg);
}

void send_soilMinMax(int k) {
  int _value = k;
  String data_Maxsoil, data_Minsoil;
  if (k == 0) {
    data_Minsoil = "{\"data\": {\"min_soil0\":" + String(Min_Soil[_value]) + "}}";
    data_Maxsoil = "{\"data\": {\"max_soil0\":" + String(Max_Soil[_value]) + "}}";
  }
  else if (k == 1) {
    data_Minsoil = "{\"data\": {\"min_soil1\":" + String(Min_Soil[_value]) + "}}";
    data_Maxsoil = "{\"data\": {\"max_soil1\":" + String(Max_Soil[_value]) + "}}";
  }
  else if (k == 2) {
    data_Minsoil = "{\"data\": {\"min_soil2\":" + String(Min_Soil[_value]) + "}}";
    data_Maxsoil = "{\"data\": {\"max_soil2\":" + String(Max_Soil[_value]) + "}}";
  }
  else if (k == 3) {
    data_Minsoil = "{\"data\": {\"min_soil3\":" + String(Min_Soil[_value]) + "}}";
    data_Maxsoil = "{\"data\": {\"max_soil3\":" + String(Max_Soil[_value]) + "}}";
  }
  data_Minsoil.toCharArray(msg_Minsoil, (data_Minsoil.length() + 1));
  data_Maxsoil.toCharArray(msg_Maxsoil, (data_Maxsoil.length() + 1));
  client.publish("@shadow/data/update", msg_Minsoil);
  client.publish("@shadow/data/update", msg_Maxsoil);
}

void send_tempMinMax(int k) {
  int _value = k;
  String data_Maxtemp, data_Mintemp;
  if (k == 0) {
    data_Mintemp = "{\"data\": {\"min_temp0\":" + String(Min_Temp[_value]) + "}}";
    data_Maxtemp = "{\"data\": {\"max_temp0\":" + String(Max_Temp[_value]) + "}}";
  }
  else if (k == 1) {
    data_Mintemp = "{\"data\": {\"min_temp1\":" + String(Min_Temp[_value]) + "}}";
    data_Maxtemp = "{\"data\": {\"max_temp1\":" + String(Max_Temp[_value]) + "}}";
  }
  else if (k == 2) {
    data_Mintemp = "{\"data\": {\"min_temp2\":" + String(Min_Temp[_value]) + "}}";
    data_Maxtemp = "{\"data\": {\"max_temp2\":" + String(Max_Temp[_value]) + "}}";
  }
  else if (k == 3) {
    data_Mintemp = "{\"data\": {\"min_temp3\":" + String(Min_Temp[_value]) + "}}";
    data_Maxtemp = "{\"data\": {\"max_temp3\":" + String(Max_Temp[_value]) + "}}";
  }
  data_Mintemp.toCharArray(msg_Mintemp, (data_Mintemp.length() + 1));
  data_Maxtemp.toCharArray(msg_Maxtemp, (data_Maxtemp.length() + 1));
  client.publish("@shadow/data/update", msg_Mintemp);
  client.publish("@shadow/data/update", msg_Maxtemp);
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
  _now = rtc.now();
  int curentTimer = (_now.hour() * 60) + _now.minute();
  unsigned int dayofweek = _now.dayOfTheWeek() - 1;
  //delay(20);
  if (dayofweek == -1) {
    dayofweek = 6;
  }
  if (curentTimer != oldTimer) {
    for (int i = 0; i < 4; i++) {
      for (int j = 0; j < 3; j++) {
        if (time_open[i][dayofweek][j] == curentTimer) {
          Open_relay(i);
          sendStatus_RelaytoWeb(i, "on");
          manual_relayStatus[i] = "on";
          DEBUG_PRINTLN("timer On");
          DEBUG_PRINT("curentTimer : "); DEBUG_PRINTLN(curentTimer);
          DEBUG_PRINT("oldTimer    : "); DEBUG_PRINTLN(oldTimer);
        }
        else if (time_close[i][dayofweek][j] == curentTimer) {
          Close_relay(i);
          sendStatus_RelaytoWeb(i, "off");
          manual_relayStatus[i] = "off";
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

/* ----------------------- Manual Control --------------------------- */
void ControlRelay_Bymanual(String topic, String message, unsigned int length) {
  String manual_message = message;
  int manual_relay = topic.substring(topic.length() - 1).toInt();
  DEBUG_PRINTLN();
  DEBUG_PRINT("manual_message : "); DEBUG_PRINTLN(manual_message);
  DEBUG_PRINT("manual_relay   : "); DEBUG_PRINTLN(manual_relay);
  for (int i = 0; i < 4; i++) {
    if (manual_relay == relay_address[i]) {
      if (manual_message == "on") {
        if (status_manual[i] == 0) {
          Open_relay(i);
          status_manual[i] = 1;
          sendStatus_RelaytoWeb(manual_relay, "on");
          manual_relayStatus[i] = manual_message;
          DEBUG_PRINTLN("ON man");
        }
      }
      else if (manual_message == "off") {
        if (status_manual[i] == 0) {
          Close_relay(i);
          status_manual[i] = 1;
          sendStatus_RelaytoWeb(manual_relay, "off");
          manual_relayStatus[i] = manual_message;
          DEBUG_PRINTLN("OFF man");
        }
      }
    }
  }
}

/* ----------------------- SoilMaxMin_setting --------------------------- */
void SoilMaxMin_setting(String topic, String message, unsigned int length) {
  String soil_message = message;
  //DEBUG_PRINT("soil        : "); DEBUG_PRINTLN(soil_message);
  String soil_topic = topic;
  if (soil_topic.substring(9, 12) == "max") {
    for (int i = 0; i < 4; i++) {
      if (topic.substring(topic.length() - 1).toInt() == i) {
        relayMaxsoil_status[i] = topic.substring(topic.length() - 1).toInt();
        Max_Soil[i] = soil_message.toInt();
        EEPROM.write(i + 2000,  Max_Soil[i]);
        EEPROM.commit();
        //Max_Soil[i] = EEPROM.read(i + 2000);
        send_soilMinMax(i);
        DEBUG_PRINT("Max_Soil : "); DEBUG_PRINTLN(Max_Soil[i]);
      }
    }
  }
  else {
    for (int i = 0; i < 4; i++) {
      if (topic.substring(topic.length() - 1).toInt() == i) {
        relayMinsoil_status[i] = topic.substring(topic.length() - 1).toInt();
        Min_Soil[i] = soil_message.toInt();
        EEPROM.write(i + 2004,  Min_Soil[i]);
        EEPROM.commit();
        //Min_Soil[i] = EEPROM.read(i + 2004);
        send_soilMinMax(i);
        DEBUG_PRINT("Min_Soil : "); DEBUG_PRINTLN(Min_Soil[i]);
      }
    }
  }
}

/* ----------------------- TempMaxMin_setting --------------------------- */
void TempMaxMin_setting(String topic, String message, unsigned int length) {
  String temp_message = message;
  String temp_topic = topic;
  if (temp_topic.substring(9, 12) == "max") {
    for (int i = 0; i < 4; i++) {
      if (topic.substring(topic.length() - 1).toInt() == i) {
        Max_Temp[i] = temp_message.toInt();
        EEPROM.write(i + 2008, Max_Temp[i]);
        EEPROM.commit();
        //Max_Temp[i] = EEPROM.read(i + 2008);
        send_tempMinMax(i);
        DEBUG_PRINT("Max_Temp : "); DEBUG_PRINTLN(Max_Temp[i]);
      }
    }
  }
  else {
    for (int i = 0; i < 4; i++) {
      if (topic.substring(topic.length() - 1).toInt() == i) {
        Min_Temp[i] = temp_message.toInt();
        EEPROM.write(i + 2012,  Min_Temp[i]);
        EEPROM.commit();
        //Min_Temp[i] = EEPROM.read(i + 2012);
        send_tempMinMax(i);
        DEBUG_PRINT("Min_Temp : "); DEBUG_PRINTLN(Min_Temp[i]);
      }
    }
  }
}

/* ----------------------- soilMinMax_ControlRelay --------------------------- */
void ControlRelay_BysoilMinMax() {
  for (int k = 0; k < 4; k++) {
    if (Min_Soil[k] != 0 && Max_Soil[k] != 0) {
      if (soil < Min_Soil[k]) {
        if (statusSoil_Open[k] == 1) {
          Open_relay(k);
          statusSoil_Open[k] = 2;
          statusSoil_Close[k] = 1;
          sendStatus_RelaytoWeb(relayMinsoil_status[k], "on");
          manual_relayStatus[k] = "on";
          Get_dataToweb();
          digitalWrite(LEDY, 1);
          DEBUG_PRINT("soil < Min : "); DEBUG_PRINTLN(_data);
          _data.toCharArray(msg, (_data.length() + 1));
          client.publish("@shadow/data/update", msg);
          DEBUG_PRINTLN("soil On");
        }
      }
      else if (soil > Max_Soil[k]) {
        if (statusSoil_Close[k] == 1) {
          Close_relay(k);
          statusSoil_Open[k] = 1;
          statusSoil_Close[k] = 2;
          sendStatus_RelaytoWeb(relayMaxsoil_status[k], "off");
          manual_relayStatus[k] = "off";
          Get_dataToweb();
          digitalWrite(LEDY, 1);
          DEBUG_PRINT("soil > Max : "); DEBUG_PRINTLN(_data);
          _data.toCharArray(msg, (_data.length() + 1));
          client.publish("@shadow/data/update", msg);
          DEBUG_PRINTLN("soil Off");
        }
      }
      else {
        statusSoil_Open[k] = 1;
        statusSoil_Close[k] = 1;
      }
    }
  }
}

/* ----------------------- tempMinMax_ControlRelay --------------------------- */
void ControlRelay_BytempMinMax() {
  for (int g = 0; g < 4; g++) {
    if (Min_Temp[g] != 0 && Max_Temp[g] != 0) {
      if (temp < Min_Temp[g]) {
        if (statusTemp_Close[g] == 1) {
          Close_relay(g);
          statusTemp_Open[g] = 1;
          statusTemp_Close[g] = 2;
          sendStatus_RelaytoWeb(relayMintemp_status[g], "off");
          manual_relayStatus[g] = "off";
          Get_dataToweb();
          digitalWrite(LEDY, 1);
          DEBUG_PRINT("temp < Min : "); DEBUG_PRINTLN(_data);
          _data.toCharArray(msg, (_data.length() + 1));
          client.publish("@shadow/data/update", msg);
          DEBUG_PRINTLN("temp Off");
        }
      }
      else if (temp > Max_Temp[g]) {
        if (statusTemp_Open[g] == 1) {
          Open_relay(g);
          statusTemp_Open[g] = 2;
          statusTemp_Close[g] = 1;
          sendStatus_RelaytoWeb(relayMintemp_status[g], "on");
          manual_relayStatus[g] = "on";
          Get_dataToweb();
          digitalWrite(LEDY, 1);
          DEBUG_PRINT("temp > Max : "); DEBUG_PRINTLN(_data);
          _data.toCharArray(msg, (_data.length() + 1));
          client.publish("@shadow/data/update", msg);
          DEBUG_PRINTLN("temp On");
        }
      }
      else {
        statusTemp_Open[g] = 1;
        statusTemp_Close[g] = 1;
      }
    }
  }
}

void get_value_soil_moisture_sensor() {
  sensorValue_soil_moisture = analogRead(Soil_moisture_sensorPin);
  voltageValue_soil_moisture = (sensorValue_soil_moisture * 3.3) / (4095.00);
  soil = ((-50) * voltageValue_soil_moisture) + 100;
  if (soil <= 0) {
    soil = 0;
  }
  else if (soil >= 100) {
    soil = 100;
  }
}

void get_lux() {
  lux[1] = myLux.getLux() / 1000;
  err = myLux.getError();
  if (err != 0) {
    lux[1] = lux[0];
  }
  lux[0] = lux[1];
}

void Get_dataToweb() {
  _data = "{\"data\": {\"temperature\":" + String(temp) +
          ",\"humidity\":" + String(humidity) + ",\"lux\":" +
          String(lux[1]) + ",\"soil\":" + String(soil)  + "}}";
}

/* -------- webSerialJSON function ------- */
void webSerialJSON() {
  while (Serial.available() > 0) { // ทำงานขณะ Serial มีการส่งค่า
    Serial.setTimeout(10000); // กำหนดเวลาทำงานข้อ Serial ถ้าเกิน 20 วินาที ให้ออกจากจุดนี้
    DeserializationError err = deserializeJson(jsonDoc, Serial); // อ่านหรือรับข้อมูล jsonDoc ใน Serial แล้วเอาไปใส่ err
    EepromStream eeprom(0, 1024);
    if (err == DeserializationError::Ok) { // ถ้ามีการับข้อมูลแล้ว
      String command  =  jsonDoc["command"].as<String>();
      bool isValidData  =  !jsonDoc["client"].isNull(); //jsonDoc["client"] ยังไม่มีค่าแล้ว
      if (command == "restart") {
        WiFi.disconnect();
        delay(500);
        ESP.restart();
      }
      if (isValidData || jsonDoc["client"] == 0 || jsonDoc["client"] == NULL) { // ถ้าใน jsonDoc["client"] ยังไม่มีค่าแล้ว
        /* ------------------WRITING----------------- */
        serializeJson(jsonDoc, eeprom); // เขียนข้อมุลลง eeprom
        eeprom.flush(); // คือการเรียกใช้ EEPROM.commit() บน ESP
        delay(500);
        ESP.restart();
      }
    }  else  {
      Serial.read();
    }
  }
}

void printLocalTime() {
  if (!getLocalTime(&timeinfo)) {
    DEBUG_PRINTLN("Failed to obtain time");
    return;
  } //DEBUG_PRINTLN(&timeinfo, "%A, %d %B %Y %H:%M:%S");
}

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

void Reset_wifi() {
  for (int x = 0; x < 10; x++) {
    digitalWrite(LEDY, 1);
    delay(50);
    digitalWrite(LEDY, 0);
    delay(50);
  }
  ssid_old          = jsonDoc["ssid"].as<String>();
  password_old      = jsonDoc["password"].as<String>();
  jsonDoc["password"] = NULL;
  jsonDoc["ssid"]     = NULL;
  jsonDoc["command"]  = NULL;
  EepromStream eeprom(0, 1024);
  serializeJson(jsonDoc, eeprom);
  eeprom.flush();
  delay(1000);

  int t = 0;
  while (true) {
    if (t >= 60000) { // 1 นาที
      jsonDoc["password"] = password_old;
      jsonDoc["ssid"]     = ssid_old;
      EepromStream eeprom(0, 1024);
      serializeJson(jsonDoc, eeprom);
      eeprom.flush();
      delay(1000);
      WiFi.begin(ssid.c_str(), password.c_str());
      digitalWrite(connect_WifiStatusToBox, LOW);
      digitalWrite(LEDR, 1);
      break;
    }
    t++;
  }
}

void Add_device() {
  for (int x = 0; x < 10; x++) {
    digitalWrite(LEDR, 1);
    delay(50);
    digitalWrite(LEDR, 0);
    delay(50);
  }
  jsonDoc["server"]   = NULL;
  jsonDoc["client"]   = NULL;
  jsonDoc["pass"]     = NULL;
  jsonDoc["user"]     = NULL;
  jsonDoc["password"] = NULL;
  jsonDoc["port"]     = NULL;
  jsonDoc["ssid"]     = NULL;
  jsonDoc["command"]  = NULL;
  EepromStream eeprom(0, 1024);
  serializeJson(jsonDoc, eeprom);
  eeprom.flush();
  delay(1000);
  ESP.restart();
}

void Edit_device() {
  for (int x = 0; x < 10; x++) {
    digitalWrite(LEDR, 1);
    delay(50);
    digitalWrite(LEDR, 0);
    delay(50);
  }
  mqtt_server_old   = jsonDoc["server"].as<String>();
  mqtt_Client_old   = jsonDoc["client"].as<String>();
  mqtt_password_old = jsonDoc["pass"].as<String>();
  mqtt_username_old = jsonDoc["user"].as<String>();
  password_old      = jsonDoc["password"].as<String>();
  mqtt_port_old     = jsonDoc["port"].as<String>();
  ssid_old          = jsonDoc["ssid"].as<String>();
  jsonDoc["server"]   = NULL;
  jsonDoc["client"]   = NULL;
  jsonDoc["pass"]     = NULL;
  jsonDoc["user"]     = NULL;
  jsonDoc["password"] = NULL;
  jsonDoc["port"]     = NULL;
  jsonDoc["ssid"]     = NULL;
  jsonDoc["command"]  = NULL;
  EepromStream eeprom(0, 1024);
  serializeJson(jsonDoc, eeprom);
  eeprom.flush();
  delay(1000);
  for (int x = 0; x < 20; x++) {
    digitalWrite(LEDY, 0);
    digitalWrite(LEDR, 1);
    delay(50);
    digitalWrite(LEDY, 1);
    digitalWrite(LEDR, 0);
    delay(50);
  }
  Serial.write(START_PATTERN, 3);         // ส่งข้อมูลชนิดไบต์ ส่งตัวอักษรไปบนเว็บ
  Serial.flush();                         // เครียบัฟเฟอร์ให้ว่าง
  deserializeJson(jsonDoc, eeprom);       // คือการรับหรืออ่านข้อมูล jsonDoc ใน eeprom
  Serial.write(STX);                      // 02 คือเริ่มส่ง
  serializeJsonPretty(jsonDoc, Serial);   // ส่งข่อมูลของ jsonDoc ไปบนเว็บ
  Serial.write(ETX);                      // 03 คือจบ

  if (!jsonDoc.isNull()) {                      // ถ้าใน jsonDoc มีค่าแล้ว
    WiFi.disconnect();
    delay(1000);
    if ( WiFi.status() != WL_CONNECTED) {       // ถ้ายังไม่เชื่อมต่อ wifi
      while ( WiFi.status() != WL_CONNECTED) {  // ทำงาน ขณะที่ยังไม่เชื่อมต่อ
        unsigned long currentTime = millis();
        webSerialJSON();
        delay(1000);
        if (currentTime - previousTime >= limitTime_change) {
          DEBUG_PRINTLN("1 min out");
          jsonDoc["server"]   = mqtt_server_old;
          jsonDoc["client"]   = mqtt_Client_old;
          jsonDoc["pass"]     = mqtt_password_old;
          jsonDoc["user"]     = mqtt_username_old;
          jsonDoc["password"] = password_old;
          jsonDoc["port"]     = mqtt_port_old;
          jsonDoc["ssid"]     = ssid_old;
          EepromStream eeprom(0, 1024);
          serializeJson(jsonDoc, eeprom);
          eeprom.flush();
          delay(1000);
          previousTime = currentTime;
          ESP.restart();
        }
      }
    }
  }
}

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

void setup() {
  Wire.begin();
  Serial.begin(115200);
  Wire.setClock(50000);
  Serial.print("Start max44009_test01 : ");
  Serial.println(MAX44009_LIB_VERSION);
  //Wire.setClock(25000);
  lightMeter.begin();
  DEBUG_PRINTLN(F("BH1750 Test begin"));
  pinMode(Soil_moisture_sensorPin, INPUT);
  pinMode(relay_pin[0], OUTPUT);
  pinMode(relay_pin[1], OUTPUT);
  pinMode(relay_pin[2], OUTPUT);
  pinMode(relay_pin[3], OUTPUT);
  pinMode(LEDR, OUTPUT);
  pinMode(LEDY, OUTPUT);
  pinMode(connect_WifiStatusToBox,   OUTPUT);
  pinMode(connect_ClientStatusToBox, OUTPUT);
  digitalWrite(relay_pin[0], LOW);
  digitalWrite(relay_pin[1], LOW);
  digitalWrite(relay_pin[2], LOW);
  digitalWrite(relay_pin[3], LOW);
  for (int x = 0; x < 20; x++) {
    digitalWrite(LEDY, 0);
    digitalWrite(LEDR, 1);
    delay(50);
    digitalWrite(LEDY, 1);
    digitalWrite(LEDR, 0);
    delay(50);
  }
  digitalWrite(LEDY, 1);
  digitalWrite(LEDR, 1);
  EEPROM.begin(4096);
  Serial.write(START_PATTERN, 3);         // ส่งข้อมูลชนิดไบต์ ส่งตัวอักษรไปบนเว็บ
  Serial.flush(); // เครียบัฟเฟอร์ให้ว่าง
  EepromStream eeprom(0, 1024);           // ประกาศ Object eepromSteam ที่ Address 0 ขนาด 1024 byte
  deserializeJson(jsonDoc, eeprom);       // คือการรับหรืออ่านข้อมูล jsonDoc ใน eeprom
  // ส่ง jsonDoc ไปบนเว็บ
  Serial.write(STX);                      // 02 คือเริ่มส่ง
  serializeJsonPretty(jsonDoc, Serial);   // ส่งข่อมูลของ jsonDoc ไปบนเว็บ
  Serial.write(ETX);                      // 03 คือจบ
  if (! sht31.begin(0x44)) {}

  mqtt_server   = jsonDoc["server"].as<String>();
  mqtt_Client   = jsonDoc["client"].as<String>();
  mqtt_password = jsonDoc["pass"].as<String>();
  mqtt_username = jsonDoc["user"].as<String>();
  password      = jsonDoc["password"].as<String>();
  mqtt_port     = jsonDoc["port"].as<String>();
  ssid          = jsonDoc["ssid"].as<String>();
  WiFi.begin(ssid.c_str(), password.c_str());
  WiFi.waitForConnectResult();
  if (!jsonDoc.isNull()) {                      // ถ้าใน jsonDoc มีค่าแล้ว
    if ( WiFi.status() != WL_CONNECTED) {       // ถ้ายังไม่เชื่อมต่อ wifi
      while ( WiFi.status() != WL_CONNECTED) {  // ทำงาน ขณะที่ยังไม่เชื่อมต่อ
        unsigned long currentTime = millis();
        webSerialJSON();
        DEBUG_PRINTLN("Connecting...");
        delay(1000);
        if (digitalRead(mode_Add_device) == HIGH) {
          DEBUG_PRINTLN("Reset device");
          Add_device();
        }
        if (currentTime - previousTime >= _reconnectInterval) {
          DEBUG_PRINTLN("Connect wifi...");
          WiFi.begin(ssid.c_str(), password.c_str()); // ทำการเชื่อมต่อ wifi
          previousTime = currentTime;
        }
        if (currentTime - previousTimeWifi >= limitTime_connectWifi) {
          DEBUG_PRINTLN("Working...");
          previousTime = previousTimeWifi;
          delay(1000);
          break;
        }
      }
    }
  } else { // ถ้าใน jsonDoc ไม่มีค่า
    while (true) {
      WiFi.disconnect();
      webSerialJSON();
    }
  }
  client.setServer(mqtt_server.c_str(), mqtt_port.toInt());
  client.setCallback(callback);
  timeClient.begin();
  xTaskCreatePinnedToCore(Task1code, "Task1", 2048, NULL, 1, &Task1, 1);
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer, nistTime);
  printLocalTime();
  yearNow = timeinfo.tm_year + 1900;
  monthNow = timeinfo.tm_mon + 1;
  dayNow = timeinfo.tm_mday;
  hourNow = timeinfo.tm_hour;
  minuteNow = timeinfo.tm_min;
  secondNow = timeinfo.tm_sec;
  rtc.adjust(DateTime(yearNow, monthNow, dayNow, hourNow, minuteNow, secondNow));
  setAll_config();
  digitalWrite(LEDY, 1);
  digitalWrite(LEDR, 1);
  OTA_update();
  delay(500);
}

void Task1code(void * pvParameters) {
  DEBUG_PRINT("Task1 running on core "); DEBUG_PRINTLN(xPortGetCoreID());
  while (1) {
    if (WiFi.status() != WL_CONNECTED) {
      WiFi.begin(ssid.c_str(), password.c_str());
      digitalWrite(connect_WifiStatusToBox, LOW);
      digitalWrite(LEDR, 1);
      DEBUG_PRINTLN("WIFI not connect");
    } else {
      //DEBUG_PRINTLN("Connected wifi");
      digitalWrite(LEDR, 0);
      digitalWrite(connect_WifiStatusToBox, HIGH);
    }
    if (!client.connected()) {
      digitalWrite(connect_ClientStatusToBox, LOW);
      digitalWrite(LEDY, 1);
      client.connect(mqtt_Client.c_str(), mqtt_username.c_str(), mqtt_password.c_str());
      delay(1000);
      if (client.connect(mqtt_Client.c_str(), mqtt_username.c_str(), mqtt_password.c_str())) {
        DEBUG_PRINTLN("NETPIE2020 connected");
        client.subscribe("@private/#");
        configTime_net = 1;
      }
    }
    else {
      digitalWrite(connect_ClientStatusToBox, HIGH);
      digitalWrite(LEDY, 0);
    }
    delay(3000);
  }
}

void loop() {
  client.loop();
  server.handleClient();
  delay(1);
  unsigned long currentTime = millis();
  if (currentTime - previousTime_offline >= eventInterval) {
    //lux = lightMeter.readLightLevel() / 1000; //(KLux)
    //lux[1] = myLux.getLux();
    //    get_lux();
    //    temp = sht31.readTemperature();
    //    humidity = sht31.readHumidity();
    //    get_value_soil_moisture_sensor();
    /* --- กรณีไม่ได้ต่อ sensor --- */
    temp = random(10, 100);
    humidity = random(10, 20);
    lux[1] = random(0, 20);
    soil = random(0, 100);
    previousTime_offline = currentTime;
    delay(500);
  }
  ControlRelay_Bytimmer();
  ControlRelay_BysoilMinMax();
  ControlRelay_BytempMinMax();
  Get_dataToweb();
  if (configTime_net == 1) {
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer, nistTime);
    printLocalTime();
    yearNow = timeinfo.tm_year + 1900;
    monthNow = timeinfo.tm_mon + 1;
    dayNow = timeinfo.tm_mday;
    hourNow = timeinfo.tm_hour;
    minuteNow = timeinfo.tm_min;
    secondNow = timeinfo.tm_sec;
    /* ตั้งค่าวันและเวลา ณ ปัจจุบัน: ปี, เดือน, วัน, ชั่วโมง, นาที, วินาที ตามลำดับ */
    rtc.adjust(DateTime(yearNow, monthNow, dayNow, hourNow, minuteNow, secondNow));
    for (int h = 0; h < 4; h++) {
      String status_update = "{\"data\":{\"led";
      status_update += h;
      status_update += "\":\"";
      if (manual_relayStatus[h] == "on") {
        status_update += "1";
      } else if (manual_relayStatus[h] == "off") {
        status_update += "0";
      }
      status_update += "\"}}";
      DEBUG_PRINT("RelayNumber : "); DEBUG_PRINTLN((char*)status_update.c_str());
      status_update.toCharArray(msg, (status_update.length() + 1));
      client.publish("@shadow/data/update", msg);
    }
    DEBUG_PRINTLN("Set time RTC");
    DEBUG_PRINTLN("Connected internet, Complete !!!");
    configTime_net = 2;
  }
  else if (abs(soil - soil_old) > difference_soil) {
    digitalWrite(LEDY, 1);
    DEBUG_PRINT("dif soil : "); DEBUG_PRINTLN(_data);
    _data.toCharArray(msg, (_data.length() + 1));
    client.publish("@shadow/data/update", msg);
    soil_old = soil;
  }
  //  else if (abs(temp - temp_old) > difference_temp) {
  //    digitalWrite(LEDY, 1);
  //    DEBUG_PRINT("dif temp : "); DEBUG_PRINTLN(_data);
  //    _data.toCharArray(msg, (_data.length() + 1));
  //    client.publish("@shadow/data/update", msg);
  //    temp_old = temp;
  //  }
  else if (currentTime - previousTime >= (eventInterval_publishData * 1000)) {
    digitalWrite(LEDY, 1);
    DEBUG_PRINTLN(_data);
    _data.toCharArray(msg, (_data.length() + 1));
    client.publish("@shadow/data/update", msg);
    previousTime = currentTime;
  }
  if (digitalRead(mode_Add_device) == HIGH) {
    DEBUG_PRINTLN("Change wifi");
    delay(1000);
    while (true) {
      if (digitalRead(mode_Add_device) == HIGH) {
        DEBUG_PRINTLN("Edit device");
        Edit_device();
        DEBUG_PRINTLN("Edit device Done");
        break;
      }
      //Reset_wifi();
    }
  }
}