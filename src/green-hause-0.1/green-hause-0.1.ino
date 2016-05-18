#include <math.h>
#include <ArduinoJson.h>
#include <SPI.h>
#include <Ethernet.h>
#include <avr/pgmspace.h>
//#include <MemoryFree.h>

const int SLEEP = 100; // TODO jāizdomā kā regulēt procesu izpildes laiku - relatīvi pret sleep time
// =============== SENSOR IDs ==============
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

// ============== PIN DEFs ===============
// digital pins
// 10, 11, 12, 13 - ocup by ethernet
#define WIN_UP_SIG_DIG_OUT_PIN        9     // output pin loga pacelšanas releja signāls
#define WIN_DOWN_SIG_DIG_OUT_PIN      8     // output pin loga nolaišanas releja signāls
#define WIN_UP_SW_DIG_IN_PIN          7     // input pin loga pacelšanas signālam
#define WIN_DOWN_SW_DIG_IN_PIN        6     // input pin loga nolaišanas signālam
#define WIN_AUTO_MODE_SW_DIG_IN_PIN   5     // input pin logu automātiskā režīma slēdzis
// analog pins
// A0, A1 - ocup by ethernet
#define TEMP_1_ANALOG_IN_PIN      2     // analog input termorezistoram 1
#define TEMP_2_ANALOG_IN_PIN      3     // analog input termorezistoram 2     
   
// ================= variables for temperature ========================
const int SAMPLE_CNT = 10;
int tempLoopCounter = 0;
int samplesForTmp1[SAMPLE_CNT];
int samplesForTmp2[SAMPLE_CNT];
double temp1;
double temp2;
double tempAverage;

// ======================= variables for windows ========================
const double WINDOWS_OPEN_TEMP_LEVEL = 24.0;  
const double WINDOWS_CLOSE_TEMP_LEVEL = 23.0;
const int WIN_OFF = 0;
const int WIN_OPENING = 1;
const int WIN_CLOSING = 2;
const int WIN_BUSY = 10;                      // status lai izvairītos no loga raustīšanās
boolean windowsOperating = false;             // flag that windows relays are operating and we cannot measure temperature
int windowStatus = WIN_OFF;
const int WINDOWS_LOOP_LENGHT = 30;           // how many loops window open/close operation will be active. 30 * 0.1 = 3 sec
int windowsLoopCounter = 0;
const int WINDOW_LOOP_RESET_LENGHT = 100;     // after how many loops windows can be opened again
int windowLoopReset = 0; 
boolean winOperatesInAuto = false;            // if true - can open / close windows based on temp

// ====================== variables for ethernet ==========
byte mac[] = { 0x90, 0xA2, 0xDA, 0x0D, 0x85, 0xD9 }; 
byte ip[] = { 192, 168, 2, 10 };                 
EthernetServer server(80);   
const byte msgBuffMaxSize = 255;

void setup() {
    Serial.begin(9600);      // open the serial port at 9600 bps:  
    setUpEthernet();
    setUpWindows();
    initTempArray();
}

void loop() {

    // TODO: ziņojuma parakstīšana HMAC
    // TODO: datu glabāšana failā
    // logu atvēršanas/ aizvēršanas operācijas absolūtais laiks - konfigurējams
    ethernetManager();
    temperatureManager();
    windowManager();
    //Serial.print("freeMemory()=");
    //Serial.println(freeMemory());
    delay(SLEEP);
}

/**
 * ethernet methods
 */
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
//        Serial.println("---- BODY BEGIN --------");
//        Serial.println(htmlBody);
//        Serial.println("---- BODY END --------");
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
                    // TODO: getSensorData()
                    break;
                case 3:
                    operateWindows(client, json);
                    break;
                default: 
                    break;
            }
        }
        delay(1);
        client.stop();
    } // end client available & connected
}

// methodID: 1
void getSensorValue(EthernetClient client, JsonObject& json){
    byte sensorId = json["p"]["sensor"];
    double value = -1;
    switch(sensorId){
        case SNSR_TEMP_1:
            value = temp1;
            break;
        case SNSR_TEMP_2:
            value = temp2;
            break;
        case SNSR_TEMP_AVG:
            value = tempAverage;
            break;
    }
    String result = "{\"result\":" + String(value) + "}";
    client.print(result);
}

// methodID: 3
void operateWindows(EthernetClient client, JsonObject& json){
    byte action = json["p"]["action"];
    switch(action){
        case 1: // open
            windowStatus = WIN_OPENING;
            break;
        case 2: // close
            windowStatus = WIN_CLOSING;
            break;
    }
    String result = "{\"result\":1}";
    client.print(result);
}

String jsonErrorResponse(byte errorCode){
    strcpy_P(errorBuffer, (char*)pgm_read_word(&(ERROR_MSG[errorCode-1]))); 
    return "{\"error\":{\"code\":" + String(errorCode) + ",\"msg\":\"" + errorBuffer + "\"}";
}

void setUpEthernet(){
    Ethernet.begin(mac,ip);     
    server.begin(); 
    Serial.print("server is on ");
    Serial.println(Ethernet.localIP());
}

/**
 * temperature methods
 */
void temperatureManager() {
    // do not measure temp while opening/closing windows, because of voltage drop
    if (windowsOperating){
        return;
    }
    samplesForTmp1[tempLoopCounter] = analogRead(TEMP_1_ANALOG_IN_PIN);
    samplesForTmp2[tempLoopCounter] = analogRead(TEMP_2_ANALOG_IN_PIN);
    int temp1Sum = 0;
    int temp2Sum = 0;
    for(int i=0; i<SAMPLE_CNT; ++i){
        temp1Sum += samplesForTmp1[i];
        temp2Sum += samplesForTmp2[i];
    }
    temp1 = rawToCelciusTemp(temp1Sum / SAMPLE_CNT);
    temp2 = rawToCelciusTemp(temp2Sum / SAMPLE_CNT);
    tempAverage = (temp1 + temp2) / 2;
    if (tempLoopCounter == SAMPLE_CNT - 1){
        tempLoopCounter = 0;
    }
    else{
        ++tempLoopCounter;
    }
}

double rawToCelciusTemp(float raw){
    double temp = log(10500.0 * ((1024.0 / raw - 1))); 
    temp = 1 / (0.001129148 + (0.000234125 + (0.0000000876741 * temp * temp ))* temp );
    return (temp - 273.15);            // Convert Kelvin to Celcius
}

int initTempArray(){
    int tmpInitVal = analogRead(TEMP_1_ANALOG_IN_PIN); 
    for (int i=0; i<SAMPLE_CNT; ++i){
        samplesForTmp1[i] = tmpInitVal;
        samplesForTmp2[i] = tmpInitVal;
    }
}

/**
 * windows methods
 */
void windowManager(){
    // open/close windows based on manual switch - always priority 
    if (digitalRead(WIN_UP_SW_DIG_IN_PIN) == HIGH) {
        openWindows();
        return;
    }
    else if (digitalRead(WIN_DOWN_SW_DIG_IN_PIN) == HIGH){
        closeWindows();
        return;
    }
    if (digitalRead(WIN_AUTO_MODE_SW_DIG_IN_PIN) == LOW){
        winOperatesInAuto = false;
    }
    else {
        winOperatesInAuto = true;
    }
    // open/close windows based on temp
    if (winOperatesInAuto && windowStatus == WIN_OFF && tempAverage > WINDOWS_OPEN_TEMP_LEVEL){
        windowStatus = WIN_OPENING;
    }
    else if (winOperatesInAuto && windowStatus == WIN_OFF && tempAverage < WINDOWS_CLOSE_TEMP_LEVEL){
        windowStatus = WIN_CLOSING;
    }
    if (windowStatus == WIN_BUSY){
        turnOffWindows();
        if (windowsLoopCounter > 0){ // TODO: periodu kurā neko nedara vajag ilgāku
            --windowsLoopCounter;
        }
        else{
            windowStatus = WIN_OFF;
        }
    }
    else if (windowStatus == WIN_OPENING && windowsLoopCounter < WINDOWS_LOOP_LENGHT){
        ++windowsLoopCounter;
        openWindows();
    }
    else if (windowStatus == WIN_CLOSING && windowsLoopCounter < WINDOWS_LOOP_LENGHT){
        ++windowsLoopCounter;
        closeWindows(); 
    }
    else{
        windowStatus = WIN_BUSY;
    }
    //Serial.println("windowStatus: " + String(windowStatus) + ", windowsLoopCounter: " + String(windowsLoopCounter));
}

void openWindows(){
    //Serial.println("UP");
    windowsOperating = true;
    digitalWrite(WIN_DOWN_SIG_DIG_OUT_PIN, LOW);
    digitalWrite(WIN_UP_SIG_DIG_OUT_PIN, HIGH);
}

void closeWindows(){
    //Serial.println("DOWN");
    windowsOperating = true;
    digitalWrite(WIN_UP_SIG_DIG_OUT_PIN, LOW);
    digitalWrite(WIN_DOWN_SIG_DIG_OUT_PIN, HIGH);
}

void turnOffWindows(){
    //Serial.println("OFF");
    windowsOperating = false;
    digitalWrite(WIN_DOWN_SIG_DIG_OUT_PIN, LOW);
    digitalWrite(WIN_UP_SIG_DIG_OUT_PIN, LOW);
}
 
void setUpWindows(){
    pinMode(WIN_UP_SW_DIG_IN_PIN, INPUT);
    pinMode(WIN_DOWN_SW_DIG_IN_PIN, INPUT);
    pinMode(WIN_AUTO_MODE_SW_DIG_IN_PIN, INPUT);
    pinMode(WIN_UP_SIG_DIG_OUT_PIN, OUTPUT);
    digitalWrite(WIN_UP_SIG_DIG_OUT_PIN, LOW);
    pinMode(WIN_DOWN_SIG_DIG_OUT_PIN, OUTPUT);
    digitalWrite(WIN_DOWN_SIG_DIG_OUT_PIN, LOW);
}

