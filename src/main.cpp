#include <Wire.h>
#include <SPI.h>
#include <U8g2lib.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ArduinoJson.h>

// For LED stuff
#include <Adafruit_GFX.h>
#include "Adafruit_LEDBackpack.h"

#include <settings.h>

Adafruit_8x16minimatrix matrix = Adafruit_8x16minimatrix();

// Adafruit Feather ESP8266/32u4 Boards + FeatherWing OLED
//U8G2_SSD1306_128X32_UNIVISION_1_SW_I2C u8g2(U8G2_R0, /* clock=*/ SCL, /* data=*/ SDA, /* reset=*/ U8X8_PIN_NONE);

// Sweetpea esp210 + SSD1306
U8G2_SSD1306_128X64_NONAME_1_SW_I2C u8g2(U8G2_R0, 14, 2);

//
// Check data with: curl -s 'http://sl.se/api/sv/RealTime/GetDepartures/9163' | jq '.data.MetroGroups[].Departures[] | {time: .DisplayTime, stop: .StopPointNumber, dest: .Destination}'
//
String base_url = "http://sl.se/api/sv/RealTime/GetDepartures/";
String metro_station = "9163"; // 9163 == Bandhagen
String stop_point_number = "1661"; // 1661 == Bandhagen towards T-Centralen

int read_interval = 30000; // Read SL data every 30 sec
unsigned long last_read = -read_interval;

String top_dest = "";
String top_time = "";
String scroll_text = "";

inline int max(int a,int b) {return ((a)>(b)?(a):(b)); }
inline int min(int a,int b) {return ((a)<(b)?(a):(b)); }

int screen_width = 128;
int char_width = 0;
int char_height = 0;
int scroll_text_width = 0;
int x_scroll_pos = screen_width;

const int led_disp_width = 16;
const int led_char_width = 6;
int led_scroll_pos = led_disp_width;

void read_data() {
  Serial.println("Loading SL data...");
  HTTPClient http;
  http.begin(base_url + metro_station);
  int httpCode = http.GET();
  if(httpCode != HTTP_CODE_OK) {
    Serial.printf("Bad response code %d\n", httpCode);
    http.end();
    return;
  }

  String payload = http.getString();
  http.end();

  DynamicJsonBuffer jsonBuffer;
  JsonObject& root = jsonBuffer.parseObject(payload);
  if (!root.success()) {
    Serial.println("Could not parse JSON");
    return;
  }

  int groups = root["data"]["MetroGroups"].size();
  boolean first = true;
  scroll_text = "";

  for (int i = 0; i < groups; i++) {
    for (int j = 0; j < root["data"]["MetroGroups"][i]["Departures"].size(); j++) {
      String stop_point = root["data"]["MetroGroups"][i]["Departures"][j]["StopPointNumber"];
      String time = root["data"]["MetroGroups"][i]["Departures"][j]["DisplayTime"];
      String dest = root["data"]["MetroGroups"][i]["Departures"][j]["Destination"];
      dest.replace("Ä", "A");//"\xe4");
      dest.replace("ä", "a");//"\xe4");
      dest.replace("Å", "A");//"\xe4");
      dest.replace("å", "a");//"\xe4");
      dest.replace("Ö", "O");//"\xf6");
      dest.replace("ö", "o");//"\xf6");

      if (stop_point == stop_point_number) {
        if (first) {
          top_dest = dest;
          top_time = time;
          first = false;

        } else {
          scroll_text += dest + " " + time + "  ";
        }
      }
    }
  }
  scroll_text_width = scroll_text.length() * char_width;
  last_read = millis();
}

boolean tryWifi(String ssid, String pw) {
  WiFi.begin(ssid.c_str(), pw.c_str());
  int dots = 0;
  while (WiFi.status() != WL_CONNECTED) {
    dots++;
    if (dots > 10) {
      return false;
    }

    u8g2.firstPage();
    do {
      u8g2.setCursor(0, char_height);
      u8g2.print("Trying ");
      u8g2.print(ssid);

      for (int i = 0; i < dots; i++) {
        u8g2.print(".");
      }
    } while ( u8g2.nextPage() );
    delay(1000);
  }
  return true;
}

void setupWifi() {
  size_t num_wifis = (sizeof wifis / sizeof wifis[0]);
  int wifi_idx = 0;
  while (true) {
    if (tryWifi(wifis[wifi_idx][0], wifis[wifi_idx][1])) {
      break;
    }
    wifi_idx = (wifi_idx + 1) % num_wifis;
  }

  u8g2.firstPage();
  do {
    u8g2.setCursor(0, char_height);
    u8g2.print("Got ");
    u8g2.print(WiFi.localIP());
  } while ( u8g2.nextPage() );
  delay(1000);
}

void setup() {
  Serial.begin(115200);

  //Setup LED
  matrix.begin(0x70);
  matrix.clear();
  matrix.setTextSize(1);
  matrix.setTextWrap(false);
  matrix.setTextColor(LED_ON);
  matrix.setRotation(1);
  matrix.writeDisplay();
  matrix.setBrightness(5);

  //Setup OLED
  u8g2.begin();
  u8g2.setFont(u8g2_font_6x10_tf);
  char_height = 9;
  char_width = u8g2.getStrWidth("A") + 1;

  setupWifi();
  read_data();
  delay(500);
}

void display_data() {
  int chars_to_draw = (min(screen_width, screen_width - x_scroll_pos) / char_width) + 1;

  u8g2.setCursor(0, char_height);
  u8g2.print(top_dest);
  u8g2.setCursor(screen_width - u8g2.getStrWidth(top_time.c_str()), char_height);
  u8g2.print(top_time);

  int start_char = max(0, -x_scroll_pos / char_width);
  int pos = max(x_scroll_pos, x_scroll_pos + (start_char * char_width));

  u8g2.setCursor(pos, char_height * 2);
  u8g2.print(scroll_text.substring(start_char, start_char + chars_to_draw));
}

void display_leds() {
  String led_text = top_dest + " " + top_time;
  int width = led_text.length() * led_char_width;
  matrix.clear();
  matrix.setCursor(led_scroll_pos, 0);
  matrix.print(led_text);
  matrix.writeDisplay();

  led_scroll_pos--;
  if (led_scroll_pos < -width) {
    led_scroll_pos = led_disp_width;
  }
}

boolean scrolled_to_end() {
  return x_scroll_pos < -scroll_text_width;
}

void loop() {
  if (scrolled_to_end() && millis() > last_read + read_interval) {
    read_data();
    x_scroll_pos = screen_width;
  }

  u8g2.firstPage();
  do {
    display_data();
  } while ( u8g2.nextPage() );

  display_leds();
  x_scroll_pos--;
  delay(1);
}
