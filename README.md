# Firmware-HandySense-board
![](https://komarev.com/ghpvc/?username=your-github-Firmware-HandySense-board&color=brightgreen) 

สารบัญ (Table of contents)
==========================

<!--ts-->
   * [Functionals](#functionals)
     ฟังก์ชันหลักของ HandySense
     * [Manual control](#manual-control)
     * [Timmer control](#timmer-control)
     * [Sensor control](#sensor-control)
   * [Read sensor](#read-sensor)
     * [Temp and humid sensor](#temp-and-humid-sensor)
     * [Light intensity sensor](#light-intensity-sensor)
     * [Soil sensor](#soil-sensor)
   * [Send data](#send-data)
   * [Add device](#add-device)
   * [Multitasking](#multitasking)
   * [Status led](#status-led)
<!--te-->

Functionals
===========


Manual control
----------------------

```js
/* ----------------------- Manual Control --------------------------- */
void ControlRelay_Bymanual(int ch_relay, String message) {

  DEBUG_PRINTLN();
  DEBUG_PRINT("manual_message : ");
  DEBUG_PRINTLN(message);
  DEBUG_PRINT("ch_relay       : ");
  DEBUG_PRINTLN(ch_relay);

  if (status_manual[ch_relay] == 0) {
    status_manual[ch_relay] = 1;
    if (message == "on") {
      Open_relay(ch_relay);
      RelayStatus[ch_relay] = 1;
      DEBUG_PRINTLN("ON manual");
    } else if (message == "off") {
      Close_relay(ch_relay);
      RelayStatus[ch_relay] = 0;
      DEBUG_PRINTLN("OFF manual");
    }
    check_sendData_status = 1;
  }
}

```


Timmer control
----------------------

```js
/* ------------ Control Relay By Timmer ------------- */
void ControlRelay_Bytimmer() {
  int curentTimer;
  int dayofweek;
  if (WiFi.status() == WL_CONNECTED) {
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer, nistTime);
    if (getLocalTime(&timeinfo)) {
      yearNow = timeinfo.tm_year + 1900;
      monthNow = timeinfo.tm_mon + 1;
      dayNow = timeinfo.tm_mday;
      weekdayNow = timeinfo.tm_wday;
      hourNow = timeinfo.tm_hour;
      minuteNow = timeinfo.tm_min;
      secondNow = timeinfo.tm_sec;

      curentTimer = (hourNow * 60) + minuteNow;
      dayofweek = weekdayNow - 1;
    } else {
      _now = rtc.now();
      curentTimer = (_now.hour() * 60) + _now.minute();
      dayofweek = _now.dayOfTheWeek() - 1;
      DEBUG_PRINT("USE RTC 1");
    }
  } else {
    _now = rtc.now();
    curentTimer = (_now.hour() * 60) + _now.minute();
    dayofweek = _now.dayOfTheWeek() - 1;
    DEBUG_PRINT("USE RTC 2");
  }
  //DEBUG_PRINT("curentTimer : "); DEBUG_PRINTLN(curentTimer);
  /* check curentTimer => 0-1440 */
  if (curentTimer < 0 || curentTimer > 1440) {
    curentTimerError = 1;
    DEBUG_PRINT("curentTimerError : ");
    DEBUG_PRINTLN(curentTimerError);
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
            DEBUG_PRINT("curentTimer : ");
            DEBUG_PRINTLN(curentTimer);
            DEBUG_PRINT("oldTimer    : ");
            DEBUG_PRINTLN(oldTimer);
          } else if (time_close[i][dayofweek][j] == curentTimer) {
            RelayStatus[i] = 0;
            check_sendData_status = 1;
            Close_relay(i);
            DEBUG_PRINTLN("timer Off");
            DEBUG_PRINT("curentTimer : ");
            DEBUG_PRINTLN(curentTimer);
            DEBUG_PRINT("oldTimer    : ");
            DEBUG_PRINTLN(oldTimer);
          } else if (time_open[i][dayofweek][j] == 3000 && time_close[i][dayofweek][j] == 3000) {
            //        Close_relay(i);
            //        DEBUG_PRINTLN(" Not check day, Not Working relay");
          }
        }
      }
      oldTimer = curentTimer;
    }
  }
}
```


Sensor control
----------------------

```js
/* ----------------------- soilMinMax_ControlRelay --------------------------- */
void ControlRelay_BysoilMinMax() {
  Get_soil();
  for (int k = 0; k < 4; k++) {
    if (Min_Soil[k] != 0 && Max_Soil[k] != 0) {
      if (soil < Min_Soil[k]) {
        if (statusSoil[k] == 0) {
          Open_relay(k);
          statusSoil[k] = 1;
          RelayStatus[k] = 1;
          check_sendData_status = 1;
          digitalWrite(LEDY, HIGH);
          //check_sendData_toWeb = 1;
          DEBUG_PRINTLN("soil On");
        }
      } else if (soil > Max_Soil[k]) {
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
        if (statusTemp[g] == 1) {
          Close_relay(g);
          statusTemp[g] = 0;
          RelayStatus[g] = 0;
          check_sendData_status = 1;
          digitalWrite(LEDY, HIGH);
          //check_sendData_toWeb = 1;
          DEBUG_PRINTLN("temp Off");
        }
      } else if (temp > Max_Temp[g]) {
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
```

Read sensor
===========

```js
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
```

Temp and humid sensor
---------------------
```js
/* ----------------------- Calculator sensor SHT31  --------------------------- */
void Get_sht31() {
  float buffer_temp = 0;
  float buffer_hum = 0;
  float temp_cal = 0;
  int num_temp = 0;
  buffer_temp = sht31.readTemperature();
  buffer_hum = sht31.readHumidity();
  if (buffer_temp < -40 || buffer_temp > 125 || isnan(buffer_temp)) {  // range -40 to 125 C
    if (temp_error_count >= 10) {
      temp_error = 1;
      digitalWrite(status_sht31_error, HIGH);
      DEBUG_PRINT("temp_error : ");
      DEBUG_PRINTLN(temp_error);
    } else {
      temp_error_count++;
    }
    DEBUG_PRINT("temp_error_count  : ");
    DEBUG_PRINTLN(temp_error_count);
  } else {    
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
    digitalWrite(status_sht31_error, LOW);
  }
  float hum_cal = 0;
  int num_hum = 0;
  if (buffer_hum < 0 || buffer_hum > 100 || isnan(buffer_hum)) {  // range 0 to 100 %RH
    if (hum_error_count >= 10) {
      hum_error = 1;
      //digitalWrite(status_sht31_error, LOW);
      DEBUG_PRINT("hum_error  : ");
      DEBUG_PRINTLN(hum_error);
    } else {
      hum_error_count++;
    }
    DEBUG_PRINT("hum_error_count  : ");
    DEBUG_PRINTLN(hum_error_count);
  } else {    
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
    //digitalWrite(status_sht31_error, HIGH);
  }
}
```

Light intensity sensor 
---------------------
```js
/* ----------------------- Calculator sensor Max44009 --------------------------- */
void Get_max44009() {
  float buffer_lux = 0;
  float lux_cal = 0;
  int num_lux = 0;
  if(tpye_lux == 2){
    buffer_lux = (myLux.getLux()* 2.15) / 1000;
  } else {    
    buffer_lux = (lightMeter.readLightLevel() * 2.15) / 1000; //(KLux)  
  }
  if (buffer_lux < 0 || buffer_lux > 188000 || isnan(buffer_lux)) {  // range 0.045 to 188,000 lux
    if (lux_error_count >= 10) {
      lux_error = 1;
      digitalWrite(status_max44009_error, HIGH);
      DEBUG_PRINT("lux_error  : ");
      DEBUG_PRINTLN(lux_error);
    } else {
      lux_error_count++;
    }
    DEBUG_PRINT("lux_error_count  : ");
    DEBUG_PRINTLN(lux_error_count);
  } else {    
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
    digitalWrite(status_max44009_error, LOW);
  }
}
```

Soil sensor
---------------------
```js
/* ----------------------- Calculator sensor Soil  --------------------------- */
void Get_soil() {
  float buffer_soil = 0;
  sensorValue_soil_moisture = analogRead(Soil_moisture_sensorPin);
  voltageValue_soil_moisture = (sensorValue_soil_moisture * 3.3) / (4095.00);
  buffer_soil = ((-55.82) * voltageValue_soil_moisture) + 113.52;
  if (buffer_soil < 0 || buffer_soil > 100 || isnan(buffer_soil)) {  // range 0 to 100 %
    if (soil_error_count >= 10) {
      soil_error = 1;
      digitalWrite(status_soil_error, HIGH);
      DEBUG_PRINT("soil_error : ");
      DEBUG_PRINTLN(soil_error);
    } else {
      soil_error_count++;
    }
    DEBUG_PRINT("soil_error_count  : ");
    DEBUG_PRINTLN(soil_error_count);
  } else {    
    ma_soil[4] = ma_soil[3];
    ma_soil[3] = ma_soil[2];
    ma_soil[2] = ma_soil[1];
    ma_soil[1] = ma_soil[0];
    ma_soil[0] = buffer_soil;
    soil = (ma_soil[0] + ma_soil[1] + ma_soil[2] + ma_soil[3] + ma_soil[4]) / 5;
    if (soil <= 0) {
      soil = 0;
    } else if (soil >= 100) {
      soil = 100;
    }
    soil_error = 0;
    soil_error_count = 0;
    digitalWrite(status_soil_error, LOW);
  }
}
```

Send data
===========

```js
/* ----------------------- Sent Timer --------------------------- */
void sent_dataTimer(String topic, String message) {
  String _numberTimer = topic.substring(topic.length() - 2).c_str();
  String _payload = "{\"data\":{\"value_timer";
  _payload += _numberTimer;
  _payload += "\":\"";
  _payload += message;
  _payload += "\"}}";
  DEBUG_PRINT("incoming : ");
  DEBUG_PRINTLN((char*)_payload.c_str());
  client.publish("@shadow/data/update", (char*)_payload.c_str());
}

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
    _payload = "{\"data\": {\"led0\":\"" + String(RelayStatus[0]) + "\",\"led1\":\"" + String(RelayStatus[1]) + "\",\"led2\":\"" + String(RelayStatus[2]) + "\",\"led3\":\"" + String(RelayStatus[3]) + "\"}}";
    DEBUG_PRINT("_payload : ");
    DEBUG_PRINTLN(_payload);
    _payload.toCharArray(msgUpdateRalay, (_payload.length() + 1));
    if (client.publish("@shadow/data/update", msgUpdateRalay)) {
      check_sendData_status = 0;
      DEBUG_PRINTLN("Send StatusRelay FULL");
    }
  }
}

/* --------- Respone soilMinMax toWeb --------- */
void send_soilMinMax() {
  String soil_payload;
  char soilMinMax_data[450];
  if (check_sendData_SoilMinMax == 1) {
    soil_payload = "{\"data\": {\"min_soil0\":" + String(Min_Soil[0]) + ",\"max_soil0\":" + String(Max_Soil[0]) + ",\"min_soil1\":" + String(Min_Soil[1]) + ",\"max_soil1\":" + String(Max_Soil[1]) + ",\"min_soil2\":" + String(Min_Soil[2]) + ",\"max_soil2\":" + String(Max_Soil[2]) + ",\"min_soil3\":" + String(Min_Soil[3]) + ",\"max_soil3\":" + String(Max_Soil[3]) + "}}";
    DEBUG_PRINT("_payload : ");
    DEBUG_PRINTLN(soil_payload);
    soil_payload.toCharArray(soilMinMax_data, (soil_payload.length() + 1));
    if (client.publish("@shadow/data/update", soilMinMax_data)) {
      check_sendData_SoilMinMax = 0;
    }
  }
}

/* --------- Respone tempMinMax toWeb --------- */
void send_tempMinMax() {
  String temp_payload;
  char tempMinMax_data[400];
  if (check_sendData_tempMinMax == 1) {
    temp_payload = "{\"data\": {\"min_temp0\":" + String(Min_Temp[0]) + ",\"max_temp0\":" + String(Max_Temp[0]) + ",\"min_temp1\":" + String(Min_Temp[1]) + ",\"max_temp1\":" + String(Max_Temp[1]) + ",\"min_temp2\":" + String(Min_Temp[2]) + ",\"max_temp2\":" + String(Max_Temp[2]) + ",\"min_temp3\":" + String(Min_Temp[3]) + ",\"max_temp3\":" + String(Max_Temp[3]) + "}}";
    DEBUG_PRINT("_payload : ");
    DEBUG_PRINTLN(temp_payload);
    temp_payload.toCharArray(tempMinMax_data, (temp_payload.length() + 1));
    if (client.publish("@shadow/data/update", tempMinMax_data)) {
      check_sendData_tempMinMax = 0;
    }
  }
}
```

Add device
===========

```js
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
```

Multitasking
===========

```js
/* --------- Auto Connect Wifi and server and setup value init ------------- */
void TaskWifiStatus(void * WifiStatus) {
  while (1) {
    connectWifiStatus = cannotConnect;
    WiFi.begin(ssid.c_str(), password.c_str());   
     
    while (WiFi.status() != WL_CONNECTED) {
      delay(100);
      //DEBUG_PRINTLN("WIFI Not connect !!!");
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
      sendStatus_RelaytoWeb();
      send_soilMinMax();
      send_tempMinMax();   
      delay(500);

      if (!client.connected()) {
        client.connect(mqtt_Client.c_str(), mqtt_username.c_str(), mqtt_password.c_str());
        DEBUG_PRINTLN("NETPIE2020 not connect Server");
        delay(100);
      }
      client.loop();
      server.handleClient();
      delay(1);
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
```

Status led
===========

```js
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
```

# Open Hardware Facts
![os](https://github.com/HandySense/HandySense/blob/main/ohs.jpg)
