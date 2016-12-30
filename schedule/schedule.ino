// wifi_settings.h should be a file containing the following: 
//
// const char* ssid     = "wifi ssid";
// const char* password = "wifi passwd";
//
// See http://forum.arduino.cc/index.php?topic=37371.0 on where to put the file
//
#include <wifi_settings.h>

#include <Wire.h>
#include <SPI.h>
#include <U8g2lib.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ArduinoJson.h>

#define BUTTON_A 0
#define BUTTON_B 16
#define BUTTON_C 2
#define LED      0

U8G2_SSD1306_128X32_UNIVISION_1_SW_I2C u8g2(U8G2_R0, /* clock=*/ SCL, /* data=*/ SDA, /* reset=*/ U8X8_PIN_NONE);   // Adafruit Feather ESP8266/32u4 Boards + FeatherWing OLED

//
// Check data with: curl -s 'http://sl.se/api/sv/RealTime/GetDepartures/9163' | jq '.data.MetroGroups[].Departures[] | {time: .DisplayTime, stop: .StopPointNumber, dest: .Destination}'
//
String base_url = "http://sl.se/api/sv/RealTime/GetDepartures/";
String metro_station = "9163"; // 9163 == Bandhagen
String stop_point_number = "1661"; // 1661 == Bandhagen towards T-Centralen

int read_interval = 15000; // Read SL data every 30 sec
unsigned long last_read = -read_interval;

String top_dest = "";
String top_time = "";
String scroll_text = "";

inline int max(int a,int b) {return ((a)>(b)?(a):(b)); }
inline int min(int a,int b) {return ((a)<(b)?(a):(b)); }

int char_width = 0;
int scroll_text_width = 0;
int x_scroll_pos = 128;


void setup() {  
  Serial.begin(115200);

  pinMode(BUTTON_A, INPUT_PULLUP);
  pinMode(BUTTON_B, INPUT_PULLUP);
  pinMode(BUTTON_C, INPUT_PULLUP);

  u8g2.begin();
  u8g2.setFont(u8g2_font_4x6_tf);
  char_width = u8g2.getStrWidth("A") + 1;
 
  WiFi.begin(ssid, password);
  
  int dots = 1;
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    u8g2.firstPage();
    do {
      u8g2.setCursor(0, 6);
      u8g2.print("Connecting to ");      
      u8g2.print(ssid);
      
      for (int i = 0; i < dots; i++) {
        u8g2.print(".");
      }
    } while ( u8g2.nextPage() );
  }

  u8g2.firstPage();
  do {
    u8g2.setCursor(0, 6);
    u8g2.print("Connected as ");
    u8g2.print(WiFi.localIP());
  } while ( u8g2.nextPage() );

  read_data();
}

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

void display_data() {
  int chars_to_draw = (min(128, 128 - x_scroll_pos) / char_width) + 1;

  u8g2.setCursor(0, 6);
  u8g2.print(top_dest);
  u8g2.setCursor(128 - u8g2.getStrWidth(top_time.c_str()), 6);
  u8g2.print(top_time);
  
  int start_char = max(0, -x_scroll_pos / char_width);
  int pos = max(x_scroll_pos, x_scroll_pos + (start_char * char_width));

  u8g2.setCursor(pos, 12);
  u8g2.print(scroll_text.substring(start_char, start_char + chars_to_draw));  
}

boolean scrolled_to_end() {
  return x_scroll_pos < -scroll_text_width;
}

void loop() {
  if (! digitalRead(BUTTON_A)) Serial.println("A");
  if (! digitalRead(BUTTON_B)) Serial.println("B");
  if (! digitalRead(BUTTON_C)) Serial.println("C");

  if (scrolled_to_end() && millis() > last_read + read_interval) {
    read_data();
    x_scroll_pos = 128;
  } 
  
  u8g2.firstPage();
  do {
    display_data();
  } while ( u8g2.nextPage() );

  x_scroll_pos--;
  delay(1);
}



