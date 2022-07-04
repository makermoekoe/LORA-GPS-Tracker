#include <Arduino.h>
#include <FastLED.h>
#include <esp_wifi.h>
#include <WiFi.h>
#include <Wire.h>
#include <U8g2lib.h>
#include <SPI.h>
#include <LoRa.h>
#include <TinyGPS++.h>
#include <PubSubClient.h>
#include "Adafruit_Si7021.h"

Adafruit_Si7021 sensor;

#define APA102_SDI_PIN  38
#define APA102_CLK_PIN  37
#define BTN1_PIN        16
#define BTN2_PIN        12
#define EN_LDO2_PIN     14
#define ADC_PIN         15
#define OLED_RST_PIN    34

#define LORA_RST_PIN    1
#define LORA_MISO_PIN   2
#define LORA_MOSI_PIN   3
#define LORA_SCK_PIN    4
#define LORA_CS_PIN     5
#define LORA_INT_PIN    6
#define LORA_DIO1_PIN   7
#define LORA_DIO2_PIN   8

#define SDA_PIN_M       36
#define SCL_PIN_M       35

#define GPS_PPS_PIN     40
#define GPS_RXD_PIN     42
#define GPS_TXD_PIN     41


#define NUM_LEDS        1
CRGB leds[NUM_LEDS];

#define WIDTH 128
#define HEIGHT 64
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/ OLED_RST_PIN, /* clock=*/ SCL_PIN_M, /* data=*/ SDA_PIN_M);

const char* ssid = "XXX";
const char* password = "XXX";
const char* mqtt_server = "192.168.178.104";

#define LORA_ID   2948
String lora_data;
int last_rssi = 0;

String recv_time = "";
String recv_date = "";
int recv_lat = 0;
int recv_lng = 0;
int recv_bat_voltage = 0;
int recv_bat_perc = 0;
int recv_uv_value = 0;

TinyGPSPlus gps;
float lattitude,longitude;

WiFiClient espClient;
PubSubClient client(espClient);
long lastMsg = 0;
char msg[50];

bool new_data_received = false;
bool new_data_to_mqtt = false;
unsigned long t_new_data_received = 0;

unsigned long lora_sent_at = 0;


float counter = 0;

// 100%, 95%, 90%, 85%, ...
int battery_percentage[] = {4200, 4150, 4110, 4080, 4020, 3980, 3950, 3910, 3870, 3850, 3840, 3820, 3800, 3790, 3770, 3750, 3730, 3710, 3690, 3610, 3270};

int get_battery_voltage(){
  int sum = 0;
  for(int i=0; i<100; i++) {
    sum = sum + analogRead(ADC_PIN);
  }
  float result = float(sum)/100.0;
  return int(result * (1.3665));
}

int get_battery_percentage(int battery_mv){
  int perc = 0;
  for(int i=0; i<=20; i++) if(battery_mv > battery_percentage[20-i]) perc+=5;
  return constrain(perc, 0, 100);
}

String get_gps_time(){
  int h, m, s;
  String gps_time = "";
  if(gps.time.isValid()){
    h = (gps.time.hour() + 2) % 24;
    m = gps.time.minute();
    s = gps.time.second();

    if(h<10) gps_time += "0";
    gps_time += String(h);
    gps_time += ":";
    if(m<10) gps_time += "0";
    gps_time += String(m);
    gps_time += ":";
    if(s<10) gps_time += "0";
    gps_time += String(s);
  }
  else{
    gps_time = "invalid";
  }
  return gps_time;
}

String get_gps_date(){
  int y, mo, d;
  String gps_date = "";
  if(gps.time.isValid()){
    y = gps.date.year();
    mo = gps.date.month();
    d = gps.date.day();

    if(d<10) gps_date += "0";
    gps_date += String(d);
    gps_date += ".";
    if(mo<10) gps_date += "0";
    gps_date += String(mo);
    gps_date += ".";
    gps_date += String(y);
  }
  else{
    gps_date = "invalid";
  }
  return gps_date;
}


// Set used fonts with width and height of each character
struct fonts {
  const uint8_t* font;
  int width;
  int height;
};

fonts font_xl = {u8g2_font_profont29_mf, 16, 19};
fonts font_m = {u8g2_font_profont17_mf, 9, 11};
fonts font_s = {u8g2_font_profont12_mf, 6, 8};
fonts font_xs = {u8g2_font_profont10_mf, 5, 6};

// Custom type to set up a page on the OLED display. Baisc structure is a page of three fields.
// A top one with the power, a left one with the voltage and a right one with the current.
struct display_page {
  String title;
  float *value;
  String msg;
  bool is_value;
};

float val1 = 0.0;
float placeholder = 0.0;
String msg2display_top = "";
String msg2display_bot = "";

display_page page_1 = {"Counter", &counter, "", true};
display_page page_2 = {"Test", &val1, "", true};

// Menu and Pages
#define N_PAGES 2
int current_page = 0;
bool new_page = true;
unsigned long t_new_page = 0;

struct page {
  int p;
  display_page &page;
};

page pages[N_PAGES] = {
  {0, page_1},
  {1, page_2},
};

// Alignment for the OLED display
enum box_alignment {
  Center,
  Left,
  Right
};

// Custom type for setting up a box which can be drawn to the oled display. It is defined by
// the top left corner and the width and height of the box.
struct box {
  int32_t left, top, width, height;
  int32_t radius;
  bool has_frame;

  void draw(bool fill) {
    int frame = has_frame ? 4 : 0;
    if (fill) u8g2.drawRBox(left + frame, top + frame, width - (2*frame), height - (2*frame), radius);
    else u8g2.drawRFrame(left, top, width, height, radius);
  }

  void print_text(String str, fonts f, box_alignment align, float pos) {
    u8g2.setFont(f.font);
    int x = 0;
    if(align == Center) x = left + 0.5 * (width - (f.width * str.length() - 1));
    else if(align == Left) x = left + 5;
    else if(align == Right) x = left + width - f.width * str.length() - 5;
    u8g2.setCursor(x, top + pos * height + f.height / 2);
    u8g2.print(str);
  }
};

// Initialize the boxes for the OLED
box top_left = {0, 0, 64, 10, 0, false};
box row1 = {0, 0, 128, 10, 0, false};
box row2 = {0, row1.top + row1.height, 128, row1.height, 0, false};
box row3 = {0, row2.top + row2.height, 128, row1.height, 0, false};
box row4 = {0, row3.top + row3.height, 128, row1.height, 0, false};
box row5 = {0, row4.top + row4.height, 128, row1.height, 0, false};
box row6 = {0, row5.top + row5.height, 128, row1.height, 0, false};

// Update the OLED display with the three boxes according to the given pages
void update_display(){
  u8g2.clearBuffer();

  if(new_page){
    if(millis() <= t_new_page + 800){
      top_left.print_text(pages[current_page].page.title, font_m, Center, 0.5);
    }
    else{
      new_page = false;
    }
  }

  if(!new_page){
    String str0 = String(get_gps_date()) + " - " + String(get_gps_time());
    String str1 = String(0, 2) + " C, Received data";
    String str2 = String(recv_date) + " - " + String(recv_time);
    String str3 = "Battery: " + String(recv_bat_voltage) + " mV, " + String(recv_bat_perc) + "%";
    String str4 = "UV: " + String(recv_uv_value);
    String str5 = "RSSI: " + String(last_rssi);

    row1.print_text(str0, font_xs, Center, 0.5);
    row2.print_text(str1, font_xs, Center, 0.5);
    row3.print_text(str2, font_xs, Center, 0.5);
    row4.print_text(str3, font_xs, Center, 0.5);
    row5.print_text(str4, font_xs, Center, 0.5);
    row6.print_text(str5, font_xs, Center, 0.5);
  }
  u8g2.sendBuffer();
}


void go_to_deepsleep(){
  delay(300);
  digitalWrite(EN_LDO2_PIN, LOW);
  delay(100);

  esp_sleep_enable_timer_wakeup(10000000);
  esp_sleep_enable_ext0_wakeup(GPIO_NUM_16, 0);
  esp_deep_sleep_start();
}


void callback(char* topic, byte* message, unsigned int length) {
  Serial.print("MQTT message arrived on topic: ");
  Serial.print(topic);
  Serial.print(": ");
  String messageTemp;

  for (int i = 0; i < length; i++) {
    Serial.print((char)message[i]);
    messageTemp += (char)message[i];
  }
  Serial.println();
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    leds[0] = CRGB::Red;
    FastLED.show();
    // Attempt to connect
    if (client.connect("MQTTClient_Lora", "moekoe", "moekoe@mqtt")) {
      Serial.println("connected");
      client.subscribe("home/#");
      leds[0] = CRGB::Black;
      FastLED.show();
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}

int32_t getWiFiChannel(const char *ssid) {
  if (int32_t n = WiFi.scanNetworks()) {
      for (uint8_t i=0; i<n; i++) {
          if (!strcmp(ssid, WiFi.SSID(i).c_str())) {
              return WiFi.channel(i);
          }
      }
  }
  return 0;
}


void setup() {
  Serial.begin(115200);
  Serial1.begin(9600, SERIAL_8N1, GPS_RXD_PIN, GPS_TXD_PIN, false);

  pinMode(EN_LDO2_PIN, OUTPUT);
  digitalWrite(EN_LDO2_PIN, HIGH);

  pinMode(GPS_PPS_PIN, INPUT);
  pinMode(BTN1_PIN, INPUT);
  pinMode(BTN2_PIN, INPUT);
  pinMode(ADC_PIN, INPUT);
  analogReadResolution(12);

  delay(100);
  FastLED.addLeds<APA102, APA102_SDI_PIN, APA102_CLK_PIN, BGR>(leds, NUM_LEDS);
  FastLED.setBrightness(200);
  delay(100);

  leds[0] = CRGB::Green;
  FastLED.show();

  int32_t channel = getWiFiChannel(ssid);
  esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);

  WiFi.mode(WIFI_AP_STA);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    leds[0] = CRGB::Green;
    FastLED.show();
    delay(250);
    Serial.print(".");
    leds[0] = CRGB::Black;
    FastLED.show();
    delay(250);
  }


  SPI.begin(LORA_SCK_PIN, LORA_MISO_PIN, LORA_MOSI_PIN, LORA_CS_PIN);
  LoRa.setPins(LORA_CS_PIN, LORA_RST_PIN, LORA_DIO1_PIN);

  if (!LoRa.begin(866E6)) {
    Serial.println("LoRa fail!");
    while (1);
  }
  Serial.println("LoRa OK!");

  Wire.begin(SDA_PIN_M, SCL_PIN_M);
  delay(100);
  u8g2.begin();
  delay(100);
  u8g2.clearBuffer();
  u8g2.sendBuffer();
  delay(100);

  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
}

unsigned long t_display = millis() + 100;
unsigned long t_send = millis() + 500;
unsigned long t_pps = millis() + 600;
unsigned long t_lora_send = millis() + 800;

void loop() {

  if (Serial1.available() > 0){
    if (gps.encode(Serial1.read())){
      // displayInfo();
    }
  }

  if(millis() >= t_display + 500){
    t_display = millis();
    update_display();
    counter++;
  }

  if(digitalRead(BTN1_PIN) == 0){
    leds[0] = CRGB(0,0,100);
    FastLED.show();
    while(digitalRead(BTN1_PIN) == 0) delay(10);

    go_to_deepsleep();

    leds[0] = CRGB(0,0,0);
    FastLED.show();
  }

  if(digitalRead(BTN2_PIN) == 0){
    leds[0] = CRGB(0,100,0);
    FastLED.show();

    String str = "Hey there at " + String(millis());
    lora_sent_at = millis();

    LoRa.beginPacket();
    LoRa.print(str);
    LoRa.endPacket();

    while(digitalRead(BTN2_PIN) == 0) delay(10);
    leds[0] = CRGB(0,0,0);
    FastLED.show();
  }


  int packetSize = LoRa.parsePacket();
  if (packetSize) {
    new_data_received = true;

    Serial.print("Paket empfangen ");
    leds[0] = CRGB::Blue;
    FastLED.show();

    //Paket auslesen
    while (LoRa.available()) {
      lora_data = LoRa.readString();
    }
    Serial.println(lora_data);

    int end_sep = lora_data.indexOf("#");

    if(lora_data.startsWith("LORA") && end_sep > 0){
      Serial.println("Correct String format");
      int next_sep_start = 5;
      int last_sep_start = 4;
      int data_field_counter = 0;
      while(next_sep_start + 1 < end_sep){
        next_sep_start = lora_data.indexOf(";", next_sep_start+1);
        String tmp = lora_data.substring(last_sep_start+1, next_sep_start);
        last_sep_start = next_sep_start;

        String topic = tmp.substring(0, tmp.indexOf("-"));
        String value = tmp.substring(tmp.indexOf("-") + 1);

        if(topic == "GT"){
          recv_time = value;
        }
        else if(topic == "GD"){
          recv_date = value;
        }
        else if(topic == "GLAT"){
          recv_lat = value.toInt();
        }
        else if(topic == "GLNG"){
          recv_lng = value.toInt();
        }
        else if(topic == "BV"){
          recv_bat_voltage = value.toInt();
        }
        else if(topic == "BP"){
          recv_bat_perc = value.toInt();
        }
        else if(topic == "UV"){
          recv_uv_value = value.toInt();
        }

        data_field_counter++;
      }

      Serial.print("data_field_counter: ");
      Serial.println(data_field_counter);
    }

    //RSSI ausgeben
    int rssi = LoRa.packetRssi();
    Serial.print(" mit RSSI ");
    Serial.println(rssi);
    last_rssi = rssi;
    delay(100);
    leds[0] = CRGB::Black;
    FastLED.show();
  }

  if(new_data_received){
    leds[0] = CRGB::Red;
    FastLED.show();
    new_data_received = false;
    String topic_value = "home/lora" + String(LORA_ID) + "/value";
    String topic_battery = "home/lora" + String(LORA_ID) + "/battery";
    String topic_batterylevel = "home/lora" + String(LORA_ID) + "/batterylevel";
    char topic_value_char[topic_value.length() + 1];
    char topic_battery_char[topic_battery.length() + 1];
    char topic_batterylevel_char[topic_batterylevel.length() + 1];
    topic_value.toCharArray(topic_value_char, topic_value.length() + 1);
    topic_battery.toCharArray(topic_battery_char, topic_battery.length() + 1);
    topic_batterylevel.toCharArray(topic_batterylevel_char, topic_batterylevel.length() + 1);

    char value_str[10];
    dtostrf(recv_uv_value, 1, 2, value_str);
    client.publish(topic_value_char, value_str);
    delay(10);

    char bat_str[10];
    dtostrf(recv_bat_voltage, 1, 2, bat_str);
    client.publish(topic_battery_char, bat_str);
    delay(10);

    char batlev_str[10];
    dtostrf(recv_bat_perc, 1, 2, batlev_str);
    client.publish(topic_batterylevel_char, batlev_str);

    delay(100);
    leds[0] = CRGB::Black;
    FastLED.show();
  }

  if (!client.connected()) {
    reconnect();
  }

  client.loop();
}
