// libraries to connect to server
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebSrv.h>   //updated library as ESPAsyncWebserver.h has been overridden in use of esp32
#include "rgb_lcd.h"

// hard coded in ssid and passwords to fit the server connection (credentials)

// utilizing char* (immutable) and const being mutable, it points to the content location (aka pointer reference)
rgb_lcd lcd;

const char* ssid = "wifiName";
const char* password = "password";
const char* conditionInput = "state";

// pin of button + led
const int outputLed = 2;
const int buttonPin = 4;

// Variables will change:
int ledCurrentstate = LOW;   // the current state of the output pin
int buttonState;             // the current reading from the input pin
int prevButtonS = LOW;  
char* Ltxt = "Hope you enjoyed this demonstration!";
int cursorM = 0; 

// unsigned variables allow more stored values
unsigned long lastDebounceTime = 0;  // the last time the output pin was toggled
unsigned long debounceDelay = 50;    // the debounce time; increase if the output flickers

// create AsyncWebServer class that links to server port 80, which is default for unecrypted http web pages
AsyncWebServer server(80);

// enables raw string data and usage of html to make a website to make a simple switch toggle
// was unable to utilize many aspects of html, as managing where the server is being called from and the properties of the server is still unknown for me 
const char htmlPiece[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML>
<html>
  <head>
    <!-- Title for website which includes tab name -->
    <title>Glorious button clicker</title>

    <!-- meta is moreof hashtags for the website, viewpoint is more aligned to user visible area scaling on different devices (which I won't be using as it will be only tested through a laptop browser) -->
    <meta name="viewport" content="width=device-width, initial-scale=1">

    <!-- adding my own flavicon, kept failing to display and probably due to not being able to access website root -->
    <link rel="icon" type="image/x-icon" href="/favicon.ico">

    <!-- where most visual settings have been set -->
    <style>
      html {font-family: Monospace; display: inline-block; text-align: center;}
      h2 {font-size: 3.0rem;}
      p {font-size: 3.0rem;}
      body {max-width: 600px; margin:0px auto; padding-bottom: 25px;}
      .switch {position: relative; display: inline-block; width: 120px; height: 68px} 
      .switch input {display: none}
      .slider {position: absolute; top: 0; left: 0; right: 0; bottom: 0; background-color: #ccc; border-radius: 34px}
      .slider:before {position: absolute; content: ""; height: 52px; width: 52px; left: 8px; bottom: 8px; background-color: #fff; -webkit-transition: .4s; transition: .4s; border-radius: 68px}
      input:checked+.slider {background-color: #CD5C5C}
      input:checked+.slider:before {-webkit-transform: translateX(52px); -ms-transform: translateX(52px); transform: translateX(52px)}
    </style>
  </head>
  <body>
    <h2>Purposeful button clicker</h2>
    %Reloaded_site%
    <h3> (meaningful button presses) </h3>
    <script>function toggleCheckbox (element) {
      var xhr = new XMLHttpRequest();
      if(element.checked){ xhr.open("GET", "/update?state=1", true); }
      else { xhr.open("GET", "/update?state=0", true); }
      xhr.send();
    }

    setInterval(function ( ) {
      var xhttp = new XMLHttpRequest();
      xhttp.onreadystatechange = function() {
        if (this.readyState == 4 && this.status == 200) {
          var checkInput;
          var outputUpdate;
          if( this.responseText == 1){ 
            checkInput = true;
            outputUpdate = "On";
          }
          else { 
            checkInput = false;
            outputUpdate = "Off";
          }
          document.getElementById("output").checked = checkInput;
          document.getElementById("outputState").innerHTML = outputUpdate;
        }
      };
      xhttp.open("GET", "/state", true);
      xhttp.send();
    }, 1000 ) ;
    </script>
  </body>
</html>
)rawliteral";

// replaces placeholder with button section in your web page 
String processor(const String& var){
  Serial.println(var);   // printing to console can tell us everytime website is reloaded
  if(var == "Reloaded_site"){
    String buttons ="";
    String outputStateValue = outputState();
    // declares the state of which the button is in, 1 or 0, Off or On
    buttons+= "<h4>The button is now <span id=\"outputState\"><span></h4><label class=\"switch\"><input type=\"checkbox\" onchange=\"toggleCheckbox(this)\" id=\"output\" " + outputStateValue + "><span class=\"slider\"></span></label>";
    return buttons;   // isn't a void function and we return buttons to 
  }
  return String();
}

// this function checks for the button press condition
String outputState(){
  if(digitalRead(outputLed)){
    // checked goes back to the main button press which tells the website to output true 
    return "checked";
  }
  else {
    // return empty if the output was read as off 
    return "";
  }
  return "";
}

void setup(){
  // serial port for debugging purposes
  Serial.begin(115200);

  // setting lcd screen
  lcd.begin(16,2);

  // setting pins for led and button
  pinMode(outputLed, OUTPUT);
  digitalWrite(outputLed, LOW);
  pinMode(buttonPin, INPUT);
  
  // cnnect to wifi/data
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(2000);
    Serial.println("Connecting to WiFi..");
  }

  // displays the local ip address to input into a browser
  Serial.println(WiFi.localIP());

  // also can be writting through a void class function but using [] brackets (lambda function) help minimize the coding necessary for this function as it's purpose is to send a request to the server 
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", htmlPiece, processor);
  });

  // also to send a request to a server base where you use a get function, same functionality applies with the [] brackets, they are the callback functions
  server.on("/update", HTTP_GET, [] (AsyncWebServerRequest *request) {
    String inputMessage;
    String inputParam;
    // GET input1 value on <ESP_IP>/update?state=<inputMessage>
    if (request->hasParam(conditionInput)) {
      inputMessage = request->getParam(conditionInput)->value();
      inputParam = conditionInput;
      digitalWrite(outputLed, inputMessage.toInt());
      ledCurrentstate = !ledCurrentstate;
    }
    else {
      // fall back option
      inputMessage = "No message sent";
      inputParam = "none";
    }
    Serial.println(inputMessage);
    request->send(200, "text/plain", "OK");
  });

  // Send a GET request to <ESP_IP>/state
  server.on("/state", HTTP_GET, [] (AsyncWebServerRequest *request) {
    request->send(200, "text/plain", String(digitalRead(outputLed)).c_str());
  });

  // opens server
  server.begin();
}
  
void loop() {
  // lcd screen section:
  lcd.cursor();
  lcd.setCursor(0,0);
  if (ledCurrentstate == LOW){
    lcd.print("button is off");
    Serial.println(1);
    delay(1000);
  }
  else{
    lcd.print("button is on");
    Serial.println(0);
    delay(1000);
  }
  lcd.clear();
  lcd.setCursor(0,1);
  
  ScrollingDisplay();
  delay(50);

  //button to wifi section
  // read the state of the switch into a local variable
  int reading = digitalRead(buttonPin);

  // check to see if you just pressed the button
  // (i.e. the input went from LOW to HIGH), and you've waited long enough
  // since the last press to ignore any noise:

  // If the switch changed, due to noise or pressing:
  if (reading != prevButtonS) {
    // reset the debouncing timer
    lastDebounceTime = millis();
  }

  if ((millis() - lastDebounceTime) > debounceDelay) {
    // whatever the reading is at, it's been there for longer than the debounce
    // delay, so take it as the actual current state:

    // if the button state has changed:
    if (reading != buttonState) {
      buttonState = reading;

      // only toggle the LED if the new button state is HIGH
      if (buttonState == HIGH) {
        ledCurrentstate = !ledCurrentstate;
      }
    }
  }

  // set the LED:
  digitalWrite(outputLed, ledCurrentstate);

  // overwrites reading to make it the previous input for button
  prevButtonS = reading;
}


// function for scrolling across the lcd display
void ScrollingDisplay(){
  int lentxt = strlen(Ltxt);
  if (cursorM == (lentxt-1)){
    cursorM = 0;
  }
  lcd.setCursor(0, lentxt);
  if (cursorM < lentxt -16){
    for (int ichar = cursorM; ichar < cursorM + 16; ichar++){
      lcd.print(Ltxt[ichar]);
      }
  }
  else{
    for (int ichar = cursorM; ichar <(lentxt-1); ichar++){
      lcd.print(Ltxt[ichar]);
    }
    for (int ichar = 0; ichar <= 16 - (lentxt - 1);){
      lcd.print(Ltxt[ichar]);
    }  
  }
  cursorM++;
}





