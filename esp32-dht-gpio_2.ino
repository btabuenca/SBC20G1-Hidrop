#include <DHTesp.h>         // DHT for ESP32 library
#include <WiFi.h>           // WiFi control for ESP32
#include <ThingsBoard.h>    // ThingsBoard SDK
#include <WiFiClient.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <Update.h>
#include <TimeLib.h>


// Helper macro to calculate array size
#define COUNT_OF(x) ((sizeof(x)/sizeof(0[x])) / ((size_t)(!(sizeof(x) % sizeof(0[x])))))

// WiFi access point
#define WIFI_AP_NAME        "SBC"
// WiFi password
#define WIFI_PASSWORD       "sbc$2020"
const char* host = "esp32";
WebServer server(80);

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

// See https://thingsboard.io/docs/getting-started-guides/helloworld/ 
// to understand how to obtain an access token
#define TOKEN       "6AKctuJgJVSvf8WKRuWu"  // "xqncff4gpitmg1puV4A9"     //"svyJof8VGEFAYo9SUdzQ" xqncff4gpitmg1puV4A9
// ThingsBoard server instance.
#define THINGSBOARD_SERVER  "iot.etsisi.upm.es"

// Baud rate for debug serial
#define SERIAL_DEBUG_BAUD    115200

// Initialize ThingsBoard client
WiFiClient espClient;
// Initialize ThingsBoard instance
ThingsBoard tb(espClient);
// the Wifi radio's status
int status = WL_IDLE_STATUS;

// Array with LEDs that should be lit up one by one
uint8_t leds_cycling[] = { 36, 33, 26 };
// Array with LEDs that should be controlled from ThingsBoard, one by one
uint8_t leds_control[] = { 39, 34, 13  };

// DHT object
DHTesp dht;
// ESP32 pin used to query DHT22
#define DHT_PIN 32
//pin fotorresistor
#define LUZ_PIN 34
//variable almacenar intensidad luz
int VALOR;
int luminosidad;
#define LVL_PIN 15
int nivel_agua; //humedad terreno en bruto
#define Relay_PIN 25
#define Relay_PIN3 4
#define Relay_PIN4 16
#define llenado 21
//Led rojo 
#define Pin_led_1 2
//Led azul
#define Pin_led_2 18
//Led Verde
#define Pin_led_3 19
int todobien = 1;
int dia_semana;
int nutrido=0;
int relleno=0;
// Main application loop delay
int quant = 20;

// Initial period of LED cycling.
int led_delay = 1000;
// Period of sending a temperature/humidity data.
int send_delay = 2000;

// Time passed after LED was turned ON, milliseconds.
int led_passed = 0;
// Time passed after temperature/humidity data was sent, milliseconds.
int send_passed = 0;

// Set to true if application is subscribed for the RPC messages.
bool subscribed = false;
// LED number that is currenlty ON.
int current_led = 0;
//Agua saludable
int salud_agua=0;

// Processes function for RPC call "setValue"
// RPC_Data is a JSON variant, that can be queried using operator[]
// See https://arduinojson.org/v5/api/jsonvariant/subscript/ for more details
RPC_Response processDelayChange(const RPC_Data &data)
{
  Serial.println("Received the set delay RPC method");

  // Process data

  led_delay = data;

  Serial.print("Set new delay: ");
  Serial.println(led_delay);

  return RPC_Response(NULL, led_delay);
}

// Processes function for RPC call "getValue"
// RPC_Data is a JSON variant, that can be queried using operator[]
// See https://arduinojson.org/v5/api/jsonvariant/subscript/ for more details
RPC_Response processGetDelay(const RPC_Data &data)
{
  Serial.println("Received the get value method");

  return RPC_Response(NULL, led_delay);
}

// Processes function for RPC call "setGpioStatus"
// RPC_Data is a JSON variant, that can be queried using operator[]
// See https://arduinojson.org/v5/api/jsonvariant/subscript/ for more details
  RPC_Response processSetGpioState(const RPC_Data &data)
{
  Serial.println("Received the set GPIO RPC method");

  int pin = data["pin"];
  bool enabled = data["enabled"];

  if (pin < COUNT_OF(leds_control)) {
    Serial.print("Setting LED ");
    Serial.print(pin);
    Serial.print(" to state ");
    Serial.println(enabled);

    digitalWrite(leds_control[pin], enabled);
  }

  return RPC_Response(data["pin"], (bool)data["enabled"]);
}

// RPC handlers
RPC_Callback callbacks[] = {
  { "setValue",         processDelayChange },
  { "getValue",         processGetDelay },
  { "setGpioStatus",    processSetGpioState },
};

// Setup an application
void setup() {
  pinMode(Relay_PIN, OUTPUT);
  pinMode(Relay_PIN3, OUTPUT);
  pinMode(Relay_PIN4, OUTPUT);
  pinMode(LVL_PIN, INPUT);
  pinMode(llenado, INPUT);
  pinMode(Pin_led_1, OUTPUT);
  pinMode(Pin_led_2, OUTPUT);
  pinMode(Pin_led_3, OUTPUT);
  // Initialize serial for debugging
  Serial.begin(SERIAL_DEBUG_BAUD);
  WiFi.begin(WIFI_AP_NAME, WIFI_PASSWORD);
  InitWiFi();
    if (!MDNS.begin(host)) { //http://esp32.local
    Serial.println("Error setting up MDNS responder!");
    while (1) {
      delay(1000);
    }
  }
  Serial.println("mDNS responder started");
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


  // Pinconfig

  for (size_t i = 0; i < COUNT_OF(leds_cycling); ++i) {
    pinMode(leds_cycling[i], OUTPUT);
  }

  for (size_t i = 0; i < COUNT_OF(leds_control); ++i) {
    pinMode(leds_control[i], OUTPUT);
  }

  
  // Initialize temperature sensor
  dht.setup(DHT_PIN, DHTesp::DHT22);
}

// Main application loop
void loop() {
    
  server.handleClient();
  delay(quant);

  led_passed += quant;
  send_passed += quant;
  
  // Check if next LED should be lit up
  /*if (led_passed > led_delay) {
    // Turn off current LED
    digitalWrite(leds_cycling[current_led], LOW);
    led_passed = 0;
    current_led = current_led >= 2 ? 0 : (current_led + 1);
    // Turn on next LED in a row
    digitalWrite(leds_cycling[current_led], HIGH);
  }*/
  digitalWrite(leds_cycling[26], HIGH);
  // Reconnect to WiFi, if needed
  if (WiFi.status() != WL_CONNECTED) {
    reconnect();
    return;
  }

  // Reconnect to ThingsBoard, if needed
  if (!tb.connected()) {
    subscribed = false;

    // Connect to the ThingsBoard
    Serial.print("Connecting to: ");
    Serial.print(THINGSBOARD_SERVER);
    Serial.print(" with token ");
    Serial.println(TOKEN);
    if (!tb.connect(THINGSBOARD_SERVER, TOKEN)) {
      Serial.println("Failed to connect");
      return;
    }
  }

  // Subscribe for RPC, if needed
  if (!subscribed) {
    Serial.println("Subscribing for RPC...");

    // Perform a subscription. All consequent data processing will happen in
    // callbacks as denoted by callbacks[] array.
    if (!tb.RPC_Subscribe(callbacks, COUNT_OF(callbacks))) {
      Serial.println("Failed to subscribe for RPC");
      return;
    }

    Serial.println("Subscribe done");
    subscribed = true;
  }

  // Check if it is a time to send DHT22 temperature and humidity
  if (send_passed > send_delay) {
    Serial.println("Sending data...");

    // Uploads new telemetry to ThingsBoard using MQTT. 
    // See https://thingsboard.io/docs/reference/mqtt-api/#telemetry-upload-api 
    // for more details

    TempAndHumidity lastValues = dht.getTempAndHumidity();    
    if (isnan(lastValues.humidity) || isnan(lastValues.temperature)) {
      Serial.println("Failed to read from DHT sensor!");
    } else {
      tb.sendTelemetryFloat("temperature", lastValues.temperature);
      tb.sendTelemetryFloat("humidity", lastValues.humidity);
    }
    VALOR = analogRead(LUZ_PIN);
    if (isnan(VALOR)) {
      Serial.println("Failed to read from Luminosity Sensor!");
    }
    else {
      luminosidad = map(VALOR, 0, 4095, 0 , 100);
      tb.sendTelemetryInt("Luminosidad", luminosidad);
      Serial.println(VALOR);
    }
    nivel_agua = digitalRead(LVL_PIN);
    if (isnan(LVL_PIN)) {
      Serial.println("Failed to read from Luminosity Sensor!");
    }
    Serial.println("nivel agua");
    Serial.println(nivel_agua);
    tb.sendTelemetryInt("Nivel del Agua", nivel_agua);

    dia_semana = weekday();
    if (dia_semana == 5 && nutrido == 0){
       digitalWrite(Relay_PIN4,HIGH);
      delay (5000);
      nutrido =1; 
      digitalWrite(Relay_PIN4,LOW);
      Serial.println("dia semana");
      Serial.println(dia_semana);
    }
     
     
    if (dia_semana == 6){
      nutrido = 0;
    }
    relleno = digitalRead (llenado);
    Serial.println("mirar aqui");
      Serial.println(relleno);
    if(salud_agua==0){  
       digitalWrite(Pin_led_1,LOW);
      digitalWrite(Pin_led_3,LOW);
      digitalWrite(Pin_led_2,HIGH);  
      if (relleno == 1){
        salud_agua = 1;
        relleno = 0;
      }
     
    }
    
    if (!nivel_agua){
      digitalWrite(Relay_PIN3,LOW);
      digitalWrite(Relay_PIN,HIGH);
      digitalWrite(Pin_led_1,LOW);
      digitalWrite(Pin_led_3,LOW);
      digitalWrite(Pin_led_2,HIGH);
      delay(100);
      digitalWrite(Pin_led_2,LOW);
      delay(100);
      digitalWrite(Pin_led_2,HIGH);
      delay(100);
      digitalWrite(Pin_led_2,LOW);
    
    }
    else {
      digitalWrite(Relay_PIN,LOW);
      digitalWrite(Relay_PIN3,HIGH);
      
      
    }


    if(todobien && salud_agua && nivel_agua){
      digitalWrite(Pin_led_1,LOW);
      digitalWrite(Pin_led_2,LOW);
      digitalWrite(Pin_led_3,HIGH);
    }
    else digitalWrite(Pin_led_3,LOW);
    if(!tb.connect(THINGSBOARD_SERVER, TOKEN)) {
      digitalWrite(Pin_led_1,LOW);
      digitalWrite(Pin_led_3,LOW);
      digitalWrite(Pin_led_2,HIGH);
    }
    
    /*if (luminosidad<30){
    digitalWrite(Pin_led_2,HIGH);
    }
    else {
    digitalWrite(Pin_led_2,LOW);
    }*/
    send_passed = 0;
  }

  // Process messages
  tb.loop();
}

void InitWiFi()
{
  Serial.println("Connecting to AP ...");
  // attempt to connect to WiFi network

  WiFi.begin(WIFI_AP_NAME, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    digitalWrite(Pin_led_1,LOW);
    digitalWrite(Pin_led_3,LOW);
    digitalWrite(Pin_led_2,HIGH);
    delay(50);
    digitalWrite(Pin_led_2,LOW);
    digitalWrite(Pin_led_1,HIGH);
    delay(50);
    digitalWrite(Pin_led_1,LOW);
    digitalWrite(Pin_led_2,HIGH);
    delay(50);
    digitalWrite(Pin_led_2,LOW);
    digitalWrite(Pin_led_1,HIGH);
  }
  Serial.println("Connected to AP");
  digitalWrite(Pin_led_1,LOW);
}

void reconnect() {
  // Loop until we're reconnected
  status = WiFi.status();
  if ( status != WL_CONNECTED) {
    WiFi.begin(WIFI_AP_NAME, WIFI_PASSWORD);
    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
    }
    Serial.println("Connected to AP");
  }
}
