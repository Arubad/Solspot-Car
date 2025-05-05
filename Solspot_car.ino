#include <Arduino.h>
#ifdef ESP32
#include <WiFi.h>
#include <AsyncTCP.h>
#elif defined(ESP8266)
#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#endif
#include <ESPAsyncWebServer.h>
#include <ESP32Servo.h>
#include <iostream>
#include <sstream>

#define RIGHT_FRONT_SERVO_PIN  27
#define RIGHT_BACK_SERVO_PIN   26
#define LEFT_FRONT_SERVO_PIN   25
#define LEFT_BACK_SERVO_PIN    33
Servo rightFrontServo;
Servo rightBackServo;
Servo leftFrontServo;
Servo leftBackServo;

struct MOTOR_PINS {
  int pinEn;  
  int pinIN1;
  int pinIN2;    
};

std::vector<MOTOR_PINS> motorPins = {
  {12, 14, 13},  // RIGHT_MOTOR Pins (EnA, IN1, IN2)
  {15, 18, 19},  // LEFT_MOTOR  Pins (EnB, IN3, IN4)
};

#define R 1
#define L 2
#define LEFT 3
#define RIGHT 4
#define STOP 0

#define RIGHT_MOTOR 0
#define LEFT_MOTOR 1

#define FORWARD 1
#define BACKWARD -1

const int PWMFreq = 1000; /* 1 KHz */
const int PWMResolution = 8;
const int PWMSpeedChannel = 4;

const char* ssid     = "No";
const char* password = "123456789";

AsyncWebServer server(80);
AsyncWebSocket wsCarInput("/CarInput");

const char* htmlHomePage PROGMEM = R"HTMLHOMEPAGE(
<!DOCTYPE html>
<html>
  <head>
  <meta name="viewport" content="width=device-width, initial-scale=1, maximum-scale=1, user-scalable=no">
    <style>
    .arrows {
      font-size:40px;
      color:red;
    }
    td.button {
      background-color:black;
      border-radius:25%;
      box-shadow: 5px 5px #888888;
    }
    td.button:active {
      transform: translate(5px,5px);
      box-shadow: none; 
    }
    .noselect {
      -webkit-touch-callout: none; /* iOS Safari */
        -webkit-user-select: none; /* Safari */
         -khtml-user-select: none; /* Konqueror HTML */
           -moz-user-select: none; /* Firefox */
            -ms-user-select: none; /* Internet Explorer/Edge */
                user-select: none; /* Non-prefixed version, currently
                                      supported by Chrome and Opera */
    }
    .slidecontainer {
      width: 100%;
    }
    .slider {
      -webkit-appearance: none;
      width: 100%;
      height: 20px;
      border-radius: 5px;
      background: #d3d3d3;
      outline: none;
      opacity: 0.7;
      -webkit-transition: .2s;
      transition: opacity .2s;
    }
    .slider:hover {
      opacity: 1;
    }
  
    .slider::-webkit-slider-thumb {
      -webkit-appearance: none;
      appearance: none;
      width: 40px;
      height: 40px;
      border-radius: 50%;
      background: red;
      cursor: pointer;
    }
    .slider::-moz-range-thumb {
      width: 40px;
      height: 40px;
      border-radius: 50%;
      background: red;
      cursor: pointer;
    }
    </style>
  
  </head>
  <body class="noselect" align="center" style="background-color:white">
     
    <h1 style="color: teal;text-align:center;">AASVSK</h1>
    <h2 style="color: teal;text-align:center;">PARALLEL
    
      &#128663; Control</h2>
    
    <table id="mainTable" style="width:400px;margin:auto;table-layout:fixed" CELLSPACING=10>
      <tr>
        <td></td>
        <td class="button" ontouchstart='sendButtonInput("MoveCar","1")' ontouchend='sendButtonInput("MoveCar","0")'><span class="arrows" >&#8679;</span></td>
        <td></td>
      </tr>
      <tr>
        <td class="button" ontouchstart='sendButtonInput("MoveCar","3")' ontouchend='sendButtonInput("MoveCar","0")'><span class="arrows" >&#8678;</span></td>
        <td class="button"></td>    
        <td class="button" ontouchstart='sendButtonInput("MoveCar","4")' ontouchend='sendButtonInput("MoveCar","0")'><span class="arrows" >&#8680;</span></td>
      </tr>
      <tr>
        <td></td>
        <td class="button" ontouchstart='sendButtonInput("MoveCar","2")' ontouchend='sendButtonInput("MoveCar","0")'><span class="arrows" >&#8681;</span></td>
        <td></td>
      </tr>
      <tr/><tr/>
      <tr/><tr/>
      <tr>
        <td style="text-align:left;font-size:25px"><b>TURN:</b></td>
        <td colspan=2>
         <div class="slidecontainer">
            <input type="range" min="90" max="180" value="90" class="slider" id="TURN" oninput='sendButtonInput("TURN",value)'>
          </div>
        </td>
      </tr> 
      <tr/><tr/>
      <tr>
        <td style="text-align:left;font-size:25px"><b>Speed:</b></td>
        <td colspan=2>
         <div class="slidecontainer">
            <input type="range" min="0" max="255" value="150" class="slider" id="Speed" oninput='sendButtonInput("Speed",value)'>
          </div>
        </td>
      </tr>       
    </table>
  
    <script>
      var webSocketCarInputUrl = "ws:\/\/" + window.location.hostname + "/CarInput";      
      var websocketCarInput;
      
      function initCarInputWebSocket() 
      {
        websocketCarInput = new WebSocket(webSocketCarInputUrl);
        websocketCarInput.onopen    = function(event)
        {
          var speedButton = document.getElementById("Speed");
          sendButtonInput("Speed", speedButton.value);
          var elevateButton = document.getElementById("TURN");
          sendButtonInput("TURN", elevateButton.value);          
        };
        websocketCarInput.onclose   = function(event){setTimeout(initCarInputWebSocket, 2000);};
        websocketCarInput.onmessage = function(event){};        
      }
      
      function sendButtonInput(key, value) 
      {
        var data = key + "," + value;
        websocketCarInput.send(data);
      }
    
      window.onload = initCarInputWebSocket;
      document.getElementById("mainTable").addEventListener("touchend", function(event){
        event.preventDefault()
      });      
    </script>
  </body>    
</html>
)HTMLHOMEPAGE";


void rotateMotor(int motorNumber, int motorDirection)
{
  if (motorDirection == FORWARD)
  {
    digitalWrite(motorPins[motorNumber].pinIN1, HIGH);
    digitalWrite(motorPins[motorNumber].pinIN2, LOW);    
  }
  else if (motorDirection == BACKWARD)
  {
    digitalWrite(motorPins[motorNumber].pinIN1, LOW);
    digitalWrite(motorPins[motorNumber].pinIN2, HIGH);     
  }
  else
  {
    digitalWrite(motorPins[motorNumber].pinIN1, LOW);
    digitalWrite(motorPins[motorNumber].pinIN2, LOW);       
  }
}

void moveCar(int inputValue)
{
  Serial.printf("Got value as %d\n", inputValue);  
  switch(inputValue)
  {
    case R:
      rotateMotor(RIGHT_MOTOR, FORWARD);
      rotateMotor(LEFT_MOTOR, FORWARD);                  
      break;

    case L:
      rotateMotor(RIGHT_MOTOR, BACKWARD);
      rotateMotor(LEFT_MOTOR, BACKWARD);  
      break;

    case LEFT:
      rotateMotor(RIGHT_MOTOR, FORWARD);
      rotateMotor(LEFT_MOTOR, BACKWARD);  
      break;

    case RIGHT:
      rotateMotor(RIGHT_MOTOR, BACKWARD);
      rotateMotor(LEFT_MOTOR, FORWARD); 
      break;

    case STOP:
      rotateMotor(RIGHT_MOTOR, STOP);
      rotateMotor(LEFT_MOTOR, STOP);    
      break;

    default:
      rotateMotor(RIGHT_MOTOR, STOP);
      rotateMotor(LEFT_MOTOR, STOP);    
      break;
  }
}


void elevateCar(int rightFrontServoValue, int rightBackServoValue, int leftFrontServoValue, int leftBackServoValue)
{
  rightFrontServo.write(rightFrontServoValue);
  rightBackServo.write(rightBackServoValue);
  leftFrontServo.write(leftFrontServoValue);
  leftBackServo.write(leftBackServoValue);  
}


void handleRoot(AsyncWebServerRequest *request)
{
  request->send_P(200, "text/html", htmlHomePage);
}

void notifyCarInputClients(String carKeyInput)
{
  wsCarInput.textAll(carKeyInput);  
}

void handleWebSocketCarInputMessage(void *arg, uint8_t *data, size_t len)
{
  AwsFrameInfo info = (AwsFrameInfo)arg;
  if(info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) 
  {
    data[len] = 0;
    Serial.printf("Got Text as %s\n", (char*)data);
    String textInput = (char*)data;
    std::stringstream dataStream(textInput.c_str());
    std::string key, value;
    
    if (std::getline(dataStream, key, ',') && std::getline(dataStream, value, ','))
    {
      int intValue = atoi(value.c_str());
      if (key == "MoveCar")
      {
        moveCar(intValue);
      }
      else if (key == "TURN")
      {
        elevateCar(intValue, intValue, intValue, intValue);        
      }
      else if (key == "Speed")
      {
        ledcWrite(PWMSpeedChannel, intValue);
      }
    }
  }
}

void onEventCarInput(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) 
{
  switch (type) 
  {
    case WS_EVT_CONNECT:
      Serial.printf("ws[%s][%u] connect\n", server->url(), client->id());
      client->printf("Hello Client %u :)", client->id());
      client->ping();
      break;
    case WS_EVT_DISCONNECT:
      Serial.printf("ws[%s][%u] disconnect\n", server->url(), client->id());
      break;
    case WS_EVT_DATA:
      handleWebSocketCarInputMessage(arg, data, len);
      break;
    case WS_EVT_PONG:
    case WS_EVT_ERROR:
      break;
  }
}


void setup()
{
  Serial.begin(115200);

  pinMode(motorPins[RIGHT_MOTOR].pinIN1, OUTPUT);
  pinMode(motorPins[RIGHT_MOTOR].pinIN2, OUTPUT);
  pinMode(motorPins[LEFT_MOTOR].pinIN1, OUTPUT);
  pinMode(motorPins[LEFT_MOTOR].pinIN2, OUTPUT);

  rightFrontServo.attach(RIGHT_FRONT_SERVO_PIN);
  rightBackServo.attach(RIGHT_BACK_SERVO_PIN);
  leftFrontServo.attach(LEFT_FRONT_SERVO_PIN);
  leftBackServo.attach(LEFT_BACK_SERVO_PIN);
  
  ledcSetup(PWMSpeedChannel, PWMFreq, PWMResolution);
  ledcAttachPin(motorPins[RIGHT_MOTOR].pinEn, PWMSpeedChannel);
  ledcWrite(PWMSpeedChannel, 100);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) 
  {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  Serial.println("Connected to WiFi");

  wsCarInput.onEvent(onEventCarInput);
  server.addHandler(&wsCarInput);

  server.on("/", HTTP_GET, handleRoot);
  server.begin();
  Serial.println("HTTP server started");
}


void loop() 
{
  wsCarInput.cleanupClients();
}
