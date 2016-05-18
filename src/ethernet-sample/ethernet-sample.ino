#include <ArduinoJson.h>
#include <SPI.h>
#include <Ethernet.h>
#include <avr/pgmspace.h>

// =============== GLOBAL CONSTANTS ==============
#define SNSR_TEMP_1     10
#define SNSR_TEMP_2     11
#define SNSR_TEMP_AVG   12

// =============== ERRORS =====================
const PROGMEM byte ERROR_MSG_MAX_LEN = 32;
const PROGMEM byte CANNOT_PARSE_JSON_ID = 1;
const PROGMEM char CANNOT_PARSE_JSON_MSG[] = "Cannot parse json";
const PROGMEM byte ERROR_2_ID = 2;
const PROGMEM char ERROR_2_MSG[] = "Error 2";
const PROGMEM char* const ERROR_MSG[] = {
                      CANNOT_PARSE_JSON_MSG,    // 1
                      ERROR_2_MSG               // 2
                      };
char errorBuffer[ERROR_MSG_MAX_LEN];

// ====================== ETHERNET ==========
byte mac[] = { 0x90, 0xA2, 0xDA, 0x0D, 0x85, 0xD9 }; 
byte ip[] = { 192, 168, 2, 10 };                 
EthernetServer server(80);   
const byte msgBuffMaxSize = 255;
               
void setup() {
    Serial.begin(9600);
    while (!Serial);
    Ethernet.begin(mac,ip);     
    server.begin(); 
    Serial.print("server is on ");
    Serial.println(Ethernet.localIP());
}

void loop() {
    ethernetManager();
}

void ethernetManager(){
    EthernetClient client = server.available(); 
    if (client && client.available() && client.connected()) {
        Serial.println("New client request ...");
        char htmlBody[msgBuffMaxSize];
        byte newLineCounter = 0;
        boolean readBody = false;
        short strCounter = 0;
        char c;
        while ((c = client.read()) > 0){
            if (c == '\r'){
              //Serial.write("[r]");
            }
            else if (c == '\n'){
              ++newLineCounter;
              //Serial.write("[n]");
              if (newLineCounter == 2){
                  newLineCounter = 0;
                  readBody = true;
              }
            }
            else if (newLineCounter > 0){
              --newLineCounter;
            }
            if (strCounter >= msgBuffMaxSize){
                break;
            }
            if (readBody){
                htmlBody[strCounter++] = c;
            }
            // Serial.write(c);
        } // end while client read
        htmlBody[strCounter] = 0;
        //Serial.println("bodyCounter=" + String(strCounter));
        Serial.println("---- BODY BEGIN --------");
        Serial.println(htmlBody);
        Serial.println("---- BODY END --------");
        client.println("HTTP/1.1 200 OK");
        client.println("Content-Type: application/json");
        client.println();
        StaticJsonBuffer<200> jsonBuffer; // TODO: 200 ir chari ? tad jāliek msgBuffMaxSize - notestēt
        JsonObject& json = jsonBuffer.parseObject(htmlBody);
        if (!json.success()){
            Serial.println("htmlBody parse failed");
            client.println(jsonErrorResponse(CANNOT_PARSE_JSON_ID));
        }
        else{
            byte method = json["m"];
            switch (method) {
                case 1:
                    getSensorValue(client, json);
                    break;
                case 2:
                    //do something when var equals 2
                    break;
                default: 
                    break;
            }
        }
        delay(1);
        client.stop();
        Serial.println("Client disconnected");
    } // end client available & connected
}

// methodID: 1
void getSensorValue(EthernetClient client, JsonObject& json){
    byte sensorId = json["p"]["sensor"];
    double value = -1;
    switch(sensorId){
        case SNSR_TEMP_1:
            value = 10.1;
            break;
        case SNSR_TEMP_2:
            value = 20.2;
            break;
        case SNSR_TEMP_AVG:
            value = 15.5;
            break;
    }
    client.print("<h2>");
    client.print("GetSensorValue: ");
    client.print(String(value));
    client.print("</h2>");

    Serial.println("methodID: 1");
    Serial.println("sensor: " + String(sensorId));
    Serial.println("value: " + String(value));
}

String jsonErrorResponse(byte errorCode){
    strcpy_P(errorBuffer, (char*)pgm_read_word(&(ERROR_MSG[errorCode-1]))); 
    return "{\"error\":{\"code\":" + String(errorCode) + ",\"msg\":\"" + errorBuffer + "\"}";
}

