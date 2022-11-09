#include <ESP8266WiFi.h>
#include <Wire.h>
#include <SPI.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>
#include <ESP8266HTTPClient.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <ESPAsyncWebServer.h>
#include <ESPAsyncTCP.h>

int Socket = 0;
int Wama = 0;
//int temp = 0;
int Zeit = 0;
int Min = 0;
int switch_pin=D7; //Das Wort „taster“ steht jetzt für den Wert D7.
int pushed=0;
int switchStatusLast = LOW;  // last status switch
int LEDStatus = LOW;         // current status LED

AsyncWebServer server(80);

// Variables to store temperature values
String hum = "";
String temp = "";

// Timer variables
unsigned long lastTime = 0;  
unsigned long timerDelay = 30000;

int Webswitch = LOW;

int PowerSwitch_on(){
  Serial.println("AN");
  HTTPClient http;
  http.begin("http://192.168.200.115/cm?cmnd=Power%20On");
  int httpCode = http.GET();
  //Serial.println(httpCode);
  //if (httpCode > 0) { //Check the returning code
    //String payload = http.getString();   //Get the request response payload
    //Serial.println(payload);             //Print the response payload
  //}
  http.end();
  Zeit = 10800;
  Socket = 1;
  pushed = 1;
}
int PowerSwitch_off(){
  Serial.println("AUS");
  HTTPClient http;
  http.begin("http://192.168.200.115/cm?cmnd=Power%20off");
  int httpCode = http.GET();
  Serial.println(httpCode);
  http.end();

  Socket = 0;
  pushed = 0;
  Zeit = 0;
}

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1">

  <style>
    html {
     font-family: Arial;
     display: inline-block;
     margin: 0px auto;
     text-align: center;
    }
    h2 { font-size: 3.0rem; }
    p { font-size: 3.0rem; }
    .units { font-size: 1.2rem; }
    .ds-labels{
      font-size: 1.5rem;
      vertical-align:middle;
      padding-bottom: 15px;
    }
    .switch {position: relative; display: inline-block; width: 120px; height: 68px} 
    .switch input {display: none}
    .slider {position: absolute; top: 0; left: 0; right: 0; bottom: 0; background-color: #ccc; border-radius: 34px}
    .slider:before {position: absolute; content: ""; height: 52px; width: 52px; left: 8px; bottom: 8px; background-color: #fff; -webkit-transition: .4s; transition: .4s; border-radius: 68px}
    input:checked+.slider {background-color: #2196F3}
    input:checked+.slider:before {-webkit-transform: translateX(52px); -ms-transform: translateX(52px); transform: translateX(52px)}
  </style>
</head>
<body>
  <h2>ESP Waschkeller</h2>
  %BUTTONPLACEHOLDER%
  <p>
    <i class="fas fa-thermometer-half" style="color:#059e8a;"></i> 
    <span class="ds-labels">Temperatur</span> 
    <span id="temp">%TEMP%</span>
    <sup class="units">&deg;C</sup>
  </p>
  <p> 
    <span class="ds-labels">Luftfeuchtigkeit</span>
    <span id="hum">%hum%</span>
    <sup class="units">%</sup>
  </p>
</body>
<script>
function toggleCheckbox(element) {
  var xhr = new XMLHttpRequest();
  if(element.checked){ xhr.open("GET", "/on", true); }
  else { xhr.open("GET", "/off", true); }
  xhr.send();
}
setInterval(function ( ) {
  var xhttp = new XMLHttpRequest();
  xhttp.onreadystatechange = function() {
    if (this.readyState == 4 && this.status == 200) {
      document.getElementById("hum").innerHTML = this.responseText;
    }
  };
  xhttp.open("GET", "/hum", true);
  xhttp.send();
}, 10000) ;
</script>
</html>)rawliteral";

// Replaces placeholder with button section in your web page
String processor(const String& var){
  //Serial.println(var);
  // Replaces placeholder with DS18B20 values
  if(var == "BUTTONPLACEHOLDER"){
    String buttons ="";
    String outputStateValue = outputState();
    buttons+= "<h4>Luftentfeuchter<span id=\"outputState\"></span></h4><label class=\"switch\"><input type=\"checkbox\" onchange=\"toggleCheckbox(this)\" id=\"output\" " + outputStateValue + "><span class=\"slider\"></span></label>";
    return buttons;
  }
  return String();
}

String outputState(){
  if(digitalRead(switch_pin)){
    return "checked";
  }
  else {
    return "";
  }
  return "";
}


//WiFi Credentials
const char* ssid = "SSID";
const char* password = "SECRET";

#define SEALEVELPRESSURE_HPA (1013.25)

#define i2c_Address 0x3c //initialize with the I2C addr 0x3C Typically eBay OLED's

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels
#define OLED_RESET -1   //   QT-PY / XIAO
Adafruit_SH1106G display = Adafruit_SH1106G(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

#define NUMFLAKES 10
#define XPOS 0
#define YPOS 1
#define DELTAY 2

Adafruit_BME280 bme; // I2C

const long utcOffsetInSeconds = 7200;
const int ldrPin = A0;
const char* PARAM_INPUT_1 = "state";

// Telegram
#define botToken "bot:token"
#define userID "12345"

WiFiClientSecure client;
UniversalTelegramBot bot(botToken, client);

char daysOfTheWeek[7][12] = {"Sonntag", "Montag", "Dienstag", "Mittwoch", "Donnerstag", "Freitag", "Samstag"};

// Define NTP Client to get time
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", utcOffsetInSeconds);

//Adafruit_SH1106 display = SH1106 display(0x3C,D1,D2);
unsigned long delayTime;

void setup() {

  Serial.begin(9600);
  client.setInsecure();

  //Connecting to WiFi
  Serial.print("Connecting to WiFi: ");
  Serial.println(ssid);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }

  Serial.println("");
  Serial.println("Connected!");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
   
  pinMode(ldrPin, INPUT);
  pinMode(switch_pin, INPUT);
  timeClient.begin();
  display.begin(i2c_Address, true); 
  display.display();      
  delay(5000);
  display.clearDisplay();

  
  bool status;
  status = bme.begin(0x76);  
  if (!status) {
    Serial.println("Could not find a valid BME280 sensor, check wiring!");
    while (1);
  }

  // Route for root / web page
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", index_html, processor);
  });
  server.on("/temp", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/plain", temp.c_str());
  });
  server.on("/hum", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/plain", hum.c_str());
  });
  server.on("/on", HTTP_GET, [] (AsyncWebServerRequest  *request) {
    Serial.println("on");
    //PowerSwitch_on();
    request->send_P(200, "text/plain", "OK");
  });
  server.on("/off", HTTP_GET, [] (AsyncWebServerRequest *request) {
    Serial.println("off");
    //PowerSwitch_off();
    request->send_P(200, "text/plain", "OK");
  });
  // Start server
  server.begin();

}


void loop() { 

  /*if ((millis() - lastTime) > timerDelay) {
    temp = bme.readTemperature(),1;
    hum = bme.readHumidity(),1;
    lastTime = millis();
  }*/

  temp = bme.readTemperature();
  hum = bme.readHumidity();

  int ldrStatus = analogRead(ldrPin);
  int switchStatus = digitalRead(switch_pin);

  if (switchStatus != switchStatusLast){
        delay(3);
        switchStatus = digitalRead(switch_pin);
        if (switchStatus != switchStatusLast){
          if ((switchStatus == HIGH) && (switchStatusLast == LOW)){
            LEDStatus = ! LEDStatus;
            if (LEDStatus == 1){ 
              int on;
              on = PowerSwitch_on();
            }
            if (LEDStatus == 0){
              int off; 
              off = PowerSwitch_off();
            }

            switchStatus = switchStatusLast;
          } 
        }
      }

  Zeit = Zeit-1;

  if ((Zeit <= 0) && (Socket == 1) && (pushed ==1)){
    HTTPClient http;
    http.begin("http://192.168.200.115/cm?cmnd=Power%20off");
    int httpCode = http.GET();
    http.end();
    Socket = 0;
    pushed = 0;
    LEDStatus = ! LEDStatus;
  }

  timeClient.update();
  display.clearDisplay();
  display.setTextColor(SH110X_WHITE);
  display.setCursor(0, 0);
  
  display.setTextSize(1);
  display.print("  ");
  display.print(daysOfTheWeek[timeClient.getDay()]);
  display.print("    ");
  display.print(timeClient.getHours());
  display.print(":");
  display.println(timeClient.getMinutes());

  if (Zeit > 0){
    Min = Zeit/60;
    display.print("  LE fuer ");display.print(Min);display.println(" Min an");
  }
  else{
    display.println("");
  }

  display.setTextSize(2);
  display.setTextColor(SH110X_WHITE);
  display.print("  "); display.print(bme.readTemperature(),1); display.println(" C");
  display.print("  "); display.print(bme.readHumidity(),1); display.println(" %");
  display.setTextSize(1);

  if ((ldrStatus > 200) && (Wama == 0)){
    bot.sendMessage(userID, "Waschmaschine fertig", "");
    Wama = 1;
  }
  if (Wama == 1){
    display.println("Waschmaschine fertig");
  }
  if (Wama == 0){
    display.println(" ");
  }
  if ((ldrStatus < 100) && (Wama == 1)){
    Wama = 0;
  }

  if ((bme.readHumidity() > 60.00) && (Socket == 0) && (pushed == 0)){
    HTTPClient http;
    http.begin("http://192.168.200.115/cm?cmnd=Power%20On");
    int httpCode = http.GET();
    http.end();
    Socket = 1;
  }
  if ((bme.readHumidity() < 50.00) && (Socket == 1) && (pushed == 0)) {
    HTTPClient http;
    http.begin("http://192.168.200.115/cm?cmnd=Power%20off");
    int httpCode = http.GET();
    http.end();
    Socket = 0;
  }
  if (Socket == 1){
    display.println(" Luftentfeuchter AN");
  }
  if (Socket == 0){
    display.println(" Luftentfeuchter AUS");
  }

  display.display();
  delay(1000);

}
