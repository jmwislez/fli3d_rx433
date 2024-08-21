/*
 * Fli3d: radio receiver functionality
 * 
 * Receives Fli3d radio packets and send over Wifi/UDP to Yamcs
 * To be used with ESP8266 NodeMCU, with WL101-341 433 MHz receiver on pin D1
 * 
 */
 
#define SW_VERSION "Fli3d rx433 v1.0.0 (20240804)"
#define PLATFORM_ESP8266_RADIO

#define WIFI_TIMEOUT 20
#define LED1 D4  
#define LED2 D4 // should be D0 but not available on all NODEMCU boards
#define RADIO_BAUD 1000
#define RX_PIN D1
#define TX_PIN D2 // not connected
#define PTT_PIN D3 // not connected

#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <RH_ASK.h>
#include <SPI.h>
#include "fli3d_secrets.h"  

struct __attribute__ ((packed)) ccsds_rx433_t {
  uint8_t     apid_H:3;           // 0:5
  bool        sec_hdr:1;          // 0: 4
  bool        type:1;             // 0:  3
  uint8_t     version:3;          // 0:   0
  uint8_t     apid_L;             // 1
  uint8_t     seq_ctr_H:6;        // 2:2
  uint8_t     seq_flag:2;         // 2: 0
  uint8_t     seq_ctr_L;          // 3
  uint8_t     pkt_len_H;          // 4
  uint8_t     pkt_len_L;          // 5
  uint32_t    millis:24;          // 6-8
  uint16_t    packet_ctr;         // 9-10
  uint16_t    baud;               // 11-12
  uint16_t    good_433_ctr;       // 13-14
  uint16_t    rx_buffer_sts;      // 15-16
}; 

RH_ASK radio_receiver (RADIO_BAUD, RX_PIN, TX_PIN, PTT_PIN, false);

WiFiUDP wifiUDP;
char yamcs_server[20];
ccsds_rx433_t rx433;

void setup() {
  Serial.begin(115200);
  Serial.println(" ");
  Serial.println("Fli3d 433MHz Yamcs frontend initializing ...");

  int num_wifi_available = WiFi.scanNetworks();
  Serial.print("Found ");
  Serial.print(num_wifi_available);
  Serial.print(" WiFi networks, checking with ");
  Serial.print(NUMBER_OF_WIFI);
  Serial.println(" known networks.");

  uint8_t i = 0;
  uint8_t j = 0;
  uint8_t k = 0;

  while (j < NUMBER_OF_WIFI and WiFi.status() != WL_CONNECTED) {       // try known WiFi in order of preference
    k = 0;
    while (k++ < num_wifi_available) {                                 // look for known WiFi in detected WiFi list
      if (!strcmp(default_wifi_ssid[j], WiFi.SSID(k).c_str())) {       // if found
        WiFi.begin(default_wifi_ssid[j], default_wifi_password[j]);    // try to connect
        if (i!=0) {
          Serial.println("");
        }
        Serial.print("Trying to connect to ");
        Serial.print(default_wifi_ssid[j]);
        i = 0;
        while (i++ < WIFI_TIMEOUT and WiFi.status() != WL_CONNECTED) { // give it some time
          delay (500);
          Serial.print (".");
        }
      }
    }
    if (WiFi.status() != WL_CONNECTED) {
      Serial.print(default_wifi_ssid[j]);
      Serial.println (" network not found");
      j++;
    }
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("");
    Serial.print("Connected with IP ");
    Serial.println(WiFi.localIP().toString());
    strcpy (yamcs_server, default_yamcs_server[j]);
    Serial.print("Setting yamcs server UDP port to ");
    Serial.print(yamcs_server);
    Serial.print(":");
    Serial.println(default_yamcs_radio_port);
  } 
 
  if (!radio_receiver.init ()) {
    Serial.println("Initialisation of RH_ASK radio receiver failed");
  }
  else {
    Serial.println("RH_ASK radio receiver initialised");  
  }
  rx433.apid_H = 0;
  rx433.sec_hdr = false;
  rx433.type = 0;
  rx433.version = 0;
  rx433.apid_L = 41;
  rx433.seq_ctr_H  = 0;
  rx433.seq_flag = 3;
  rx433.seq_ctr_L = 0;
  rx433.pkt_len_H = 0;
  rx433.pkt_len_L = sizeof(ccsds_rx433_t);
  rx433.millis = millis();
  rx433.packet_ctr = 0;
  rx433.baud = RADIO_BAUD;
  rx433.good_433_ctr = 0;
  rx433.rx_buffer_sts = 0;
  pinMode(LED1, OUTPUT);
  pinMode(LED2, OUTPUT);
}

void loop() {
  static uint8_t radio_data[RH_ASK_MAX_MESSAGE_LEN];
  static uint8_t radio_datalen = sizeof (radio_data);
  static uint8_t c = 0;
  static uint8_t s = 0;
  static uint32_t t = millis();
  static bool packet_received;

  if (radio_receiver.recv (radio_data, &radio_datalen)) { 
    // Message with a good checksum received, dump it.
    radio_receiver.printBuffer ("Received: ", radio_data, radio_datalen);
    wifiUDP.beginPacket (yamcs_server, default_yamcs_radio_port);
    wifiUDP.write (radio_data, radio_datalen);
    wifiUDP.endPacket ();
    rx433.good_433_ctr += 1;
    rx433.rx_buffer_sts = radio_receiver.RxBufferStatus();
    packet_received = true;
  }
  
  if (millis() > t) {
    t+=100;
    digitalWrite(LED1, HIGH);
    digitalWrite(LED2, HIGH);
    if (packet_received) {
      digitalWrite(LED2, LOW);
      Serial.print("+");
      packet_received = false;
    }
    else {
      Serial.print("-");
    }
    if (++c == 10) { 
      Serial.print(" ");
      c = 0;
      rx433.millis = millis();
      rx433.seq_ctr_L++;
      if (rx433.seq_ctr_L == 0) {
        rx433.seq_ctr_H++;
      }
      rx433.packet_ctr++;
      wifiUDP.beginPacket (yamcs_server, default_yamcs_radio_port);
      wifiUDP.write ((char*)&rx433, sizeof(ccsds_rx433_t));
      wifiUDP.endPacket ();
      digitalWrite(LED1, LOW);
      if (++s == 10) {
        Serial.println("");
        s = 0;
      }
    }
  }
}
