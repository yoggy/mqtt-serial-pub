//
//  mqtt-serial-pub.ino - 
//
//  Requirements:
//    - Arduino Client for MQTT 
//      - https://github.com/knolleary/pubsubclient/
//    - EspSoftwareSerial
//      - https://github.com/plerup/espsoftwareserial/
//  
//  How to:
//    $ git clone https://github.com/yoggy/mqtt-serial-pub.git
//    $ mqtt-serial-pub
//    $ cp config.ino.sample config.ino
//    $ vi config.ino
//    ※ edit mqtt_host, mqtt_username, mqtt_password, topic...
//    $ open mqtt-serial-pub.ino
//  
//  Copyright and license:
//    Copyright(c) 2018 yoggy<yoggy0@gmail.com>
//    Released under the MIT license
//    http://opensource.org/licenses/mit-license.php;
//
#include <Wire.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>   // https://github.com/knolleary/pubsubclient/
#include <SoftwareSerial.h> // https://github.com/plerup/espsoftwareserial/

// Wif config (from config.ino)
extern char *wifi_ssid;
extern char *wifi_password;
extern char *mqtt_server;
extern int  mqtt_port;

extern char *mqtt_client_id;
extern bool mqtt_use_auth;
extern char *mqtt_username;
extern char *mqtt_password;

extern char *mqtt_base_topic;

WiFiClient wifi_client;
PubSubClient mqtt_client(mqtt_server, mqtt_port, NULL, wifi_client);

#define READ_BUF_SIZE 32
char read_buf[READ_BUF_SIZE];
int read_buf_idx = 0;

char message_buf[64];

SoftwareSerial software_serial(13, 15, false, 256);  // RX pin, TX pin, inverse_logic, bufsize

uint32_t st_m = 0;
uint32_t epoch = 0;
uint32_t st_epoch = 0;
uint32_t last_publish_epoch = 0;

void log(char *msg) {
  Serial.println(msg);
  aqm0802_clear();
  aqm0802_print(msg);
}

void setup() {
  clear_read_buffer();

  Serial.begin(115200);
  software_serial.begin(19200);
  Wire.begin(14, 12); //SDA, SCL
  aqm0802_init();

  log("mqtt-serial-pub");
  delay(1000);
  log("MQTT Client ID:");
  delay(1000);
  log(mqtt_client_id);
  delay(2000);

  log("connecting mqtt...");
  delay(1000);
  aqm0802_clear();

  WiFi.begin(wifi_ssid, wifi_password);
  WiFi.mode(WIFI_STA);
  int wifi_count = 0;
  while (WiFi.status() != WL_CONNECTED) {
    switch (wifi_count % 4) {
      case 0:
        aqm0802_print("|");
        break;
      case 1:
        aqm0802_print("/");
        break;
      case 2:
        aqm0802_print("-");
        break;
      case 3:
        aqm0802_print("\\");
        break;
    }
    wifi_count ++;
    delay(300);

    if (wifi_count > 100) reboot();
  }

  bool rv = false;
  if (mqtt_use_auth == true) {
    rv = mqtt_client.connect(mqtt_client_id, mqtt_username, mqtt_password);
  }
  else {
    rv = mqtt_client.connect(mqtt_client_id);
  }
  if (rv == false) {
    reboot();
  }

  log("CONNECT");
  delay(1000);
}

void reboot() {
  log("REBOOT!");
  delay(1000);

  ESP.reset();

  while (true) {
    log("REBOOT!");
    delay(500);
    aqm0802_clear();
    delay(500);
  };
}

void loop() {
  if (!mqtt_client.connected()) {
    reboot();
  }
  mqtt_client.loop();

  process_serial();
  process_idle();
}

void process_idle() {
  char buf[64];

  uint32_t diff_m = millis() - st_m;
  if (diff_m > 1000) {
    epoch ++;

    // publish ping message
    if (epoch - st_epoch >= 60) {
      Serial.println("ping.");

      sprintf(buf, "{\"ping\":%d}", millis());
      mqtt_publish("ping", buf);

      st_epoch = epoch;
    }

    // display idle sceen...
    if (epoch - last_publish_epoch >= 10) {
      aqm0802_clear();
      switch (epoch % 2) {
        case 0:
          aqm0802_print("\r"); // AQM0802 displays dot charactor as "\r".  
          break;
        case 1:
          aqm0802_print(" ");
          break;
      }
    }

    st_m = millis();
  }
}

void process_serial() {
  // read loop...
  if (software_serial.available() > 0) {
    while (software_serial.available() > 0) {
      char c = software_serial.read();
      int n = c;
      Serial.println(n, HEX);

      // CR:0x0D, LF:0x0A,
      if (c == 0x0D) {
        // noting to do...
      }
      else if (c == 0x0A) {
        mqtt_publish_buffer();
      }
      else {
        read_buf[read_buf_idx] = c;
        read_buf_idx ++;
        if (read_buf_idx == READ_BUF_SIZE - 1) {
          mqtt_publish_buffer();
        }
      }
    }
  }
}

void clear_read_buffer() {
  memset(read_buf, 0, READ_BUF_SIZE);
  read_buf_idx = 0;
}

void mqtt_publish_buffer() {
  memset(message_buf, 0, sizeof(message_buf));

  // publish message
  sprintf(message_buf, "{\"recv\":\"%s\"}", read_buf);
  mqtt_publish("recv", message_buf);

  // display message
  char buf[64];
  memset(buf, 0, 64);
  sprintf(buf, "publish:%s", read_buf);
  aqm0802_clear();
  aqm0802_print(buf);

  clear_read_buffer();

  last_publish_epoch = epoch;
}

void mqtt_publish(char *sub_topic, char *message) {
  char topic[64];
  sprintf(topic, "%s%s", mqtt_base_topic, sub_topic);

  mqtt_client.publish(topic, message);

  // serial console...
  Serial.print("publish: topic=");
  Serial.print(topic);
  Serial.print(", message=");
  Serial.println(message);
}


