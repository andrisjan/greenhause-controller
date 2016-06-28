#include <SPI.h>
#include <Ethernet.h>
//https://arduino-tweet.appspot.com/
#include <Twitter.h>
// cits variants
//https://www.markkurossi.com/ArduinoTwitter/

#define SLEEP 100

// digital pins
// 10, 11, 12, 13 - ocup by ethernet
#define ETHERNET_DIG_OUT_PIN          10    // set HI to use ethernet
#define WATER_PUMP_SIG_DIG_OUT_PIN    9     // output pin water pump relay signal
#define ON_OFF_SW_DIG_IN_PIN          8     // 
#define ON_OFF_LED_DIG_OUT_PIN        7     // 
#define WATER_SW_DIG_IN_PIN           6     // 
#define WATER_FLOW_INTERUPT           0     // 0 = digital pin 2 
#define WATER_FROW_SIG_DIG_INT_PIN    2
// analog pins
// A0, A1 - ocup by ethernet
#define TEMP_1_ANALOG_IN_PIN          2     // analog input termorezistoram 1

// ================= variables for temperature ========================
const int SAMPLE_CNT = 10;
int tempLoopCounter = 0;
int samplesForTmp1[SAMPLE_CNT];
double temp1;

// ================ variables for water pump =====================
boolean waterPumpOperating = false;
//const byte CAN_WATER = 1; // watering is posible
const byte CAN_WATER = 1;
const byte WATER_ON  = 2; // water flowing
const byte WATER_OFF = 3; // water turned off
byte WATER_STATE = CAN_WATER;
long waterPumpSleep = 86400000; // water will flow once in 24 h
//long waterPumpSleep = 43200000;
//long waterPumpSleep = 7200000;
//long waterPumpSleep = 60000; // water will flow once in 1 min
long waterPumpTimeOffset = 0;

// =================== variables for water flow sensor ===========
double calibrationFactor = 8; // 4.5 ... 60 for different flow sensors
volatile byte pulseCount;  
float flowRate;
unsigned int flowMilliLitres;
unsigned long totalMilliLitres;
unsigned long oldTime;

// ========================= variables for switches ============
bool forceWaterOn = false;

// =================== teweter ==============
byte mac[] = { 0x90, 0xA2, 0xDA, 0x0D, 0x85, 0xD9 }; 
byte ip[] = { 192, 168, 2, 10 }; 
Twitter twitter("744254568444366849-AcxzgdUkwBdgiR4nTwLpjkYQndHdzFo");
long lastTempTweet = 0;
long lastTwittTime = 0;

// ====================== common varables ===================
bool isTurnedOn = false; //
int maxWaterMilliliters = 400;

void setup() {
    Serial.begin(9600);   
    initEthernet();
    initSwitches();
    initTempArray();
    initWaterFlowSensor();
    initWaterPump();
    Serial.print("init ");
}

void loop() {
    manageSwitches();
    temperatureManager();
    waterFlowManager();
    tweeterManager();

    // actions for turned on only
    if (isTurnedOn || WATER_STATE == WATER_OFF){    
        waterPumpManager();
    }
    delay(SLEEP);

    // TODO:
    // max water pump running time - to protect water pump if there is no water in tank
    // 
}

void initEthernet(){
    Ethernet.begin(mac, ip); // initEthernet();
    Serial.print("Server is on ");
    Serial.print(Ethernet.localIP());
    Serial.println(", ethernet ON");  
    delay(1000);
    lastTwittTime = millis();
    int hours = waterPumpSleep / 3600000;
    String msg = "Hello I'm ON. Next watering after " + String(hours) + "h";
    postTweet(msg);
}

/**
 * switches
 */

int initSwitches(){
    pinMode(ON_OFF_SW_DIG_IN_PIN, INPUT);
    pinMode(WATER_SW_DIG_IN_PIN, INPUT);
    pinMode(ON_OFF_LED_DIG_OUT_PIN, OUTPUT);
    digitalWrite(ON_OFF_LED_DIG_OUT_PIN, LOW);
}

int manageSwitches(){
    // turn on / off
    if (!isTurnedOn && digitalRead(ON_OFF_SW_DIG_IN_PIN) == HIGH){
        digitalWrite(ON_OFF_LED_DIG_OUT_PIN, HIGH);
        isTurnedOn = true;
        waterPumpTimeOffset = millis();
    }
    else if (isTurnedOn && digitalRead(ON_OFF_SW_DIG_IN_PIN) == LOW){
        digitalWrite(ON_OFF_LED_DIG_OUT_PIN, LOW);
        isTurnedOn = false;
        WATER_STATE = WATER_OFF;
    }
}

/**
 * ======================== TEMPERETURE MEASURE =========================
 */
int initTempArray(){
    int tmpInitVal = analogRead(TEMP_1_ANALOG_IN_PIN); 
    for (int i=0; i<SAMPLE_CNT; ++i){
        samplesForTmp1[i] = tmpInitVal;
    }
}
 
void temperatureManager() {
    // do not measure temp while pumping water, because of voltage drop
    if (waterPumpOperating){
        return;
    }
    samplesForTmp1[tempLoopCounter] = analogRead(TEMP_1_ANALOG_IN_PIN);
    int temp1Sum = 0;
    for(short i=0; i<SAMPLE_CNT; ++i){
        temp1Sum += samplesForTmp1[i];
    }
    temp1 = rawToCelciusTemp(temp1Sum / SAMPLE_CNT);
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



/**
 *  ========================= WATER PUMP MANEGEMENT ================= 
 */
void initWaterPump(){
    pinMode(WATER_PUMP_SIG_DIG_OUT_PIN, OUTPUT);
    digitalWrite(WATER_PUMP_SIG_DIG_OUT_PIN, LOW);
}

void waterPumpManager(){
    long currentTime = millis();
    Serial.println("WATER_STATE: " + String(WATER_STATE) + ", currentTime: " + String(currentTime) + ", waterPumpTimeOffset: " + String(waterPumpTimeOffset) + ", totalFlow: " + String(totalMilliLitres));
    if (digitalRead(WATER_SW_DIG_IN_PIN) == HIGH){
        digitalWrite(WATER_PUMP_SIG_DIG_OUT_PIN, HIGH);
    }
    else if (WATER_STATE == CAN_WATER && currentTime - waterPumpTimeOffset > waterPumpSleep){
        waterPumpTimeOffset = currentTime;
        WATER_STATE = WATER_ON;
    }
    else{
        digitalWrite(WATER_PUMP_SIG_DIG_OUT_PIN, LOW);
    }

    if (WATER_STATE == WATER_ON){
        if (totalMilliLitres < maxWaterMilliliters){
            digitalWrite(WATER_PUMP_SIG_DIG_OUT_PIN, HIGH);
        }
        else{
            WATER_STATE = WATER_OFF; // stop
        }
    }
    if (WATER_STATE == WATER_OFF){
        digitalWrite(WATER_PUMP_SIG_DIG_OUT_PIN, LOW);
        lastTwittTime = millis();
        String msg = "Just watered with " + String(totalMilliLitres) + " ml of water";
        postTweet(msg);
        totalMilliLitres = 0;
        WATER_STATE = CAN_WATER;
    }
}


// ====================== WATER FLOW SENSOR ==================
void initWaterFlowSensor(){
    pinMode(WATER_FROW_SIG_DIG_INT_PIN, INPUT);
    digitalWrite(WATER_FROW_SIG_DIG_INT_PIN, HIGH);
    pulseCount        = 0;
    flowRate          = 0.0;
    flowMilliLitres   = 0;
    totalMilliLitres  = 0;
    oldTime           = 0;
    attachInterrupt(WATER_FLOW_INTERUPT, waterFlowPulseCounter, FALLING);
}
// interupt service routine
void waterFlowPulseCounter(){
    pulseCount++;
}

void waterFlowManager(){
    // calc once per second
    if((millis() - oldTime) > 1000){ 
        detachInterrupt(WATER_FLOW_INTERUPT);
        flowRate = ((1000.0 / (millis() - oldTime)) * pulseCount) / calibrationFactor;
        oldTime = millis();
        flowMilliLitres = (flowRate / 60) * 1000;
        totalMilliLitres += flowMilliLitres;
//        Serial.print("  Output Liquid Quantity: ");        
//        Serial.print(totalMilliLitres);
//        Serial.print("mL, temp: ");
//        Serial.println(temp1); 
        pulseCount = 0;
        attachInterrupt(WATER_FLOW_INTERUPT, waterFlowPulseCounter, FALLING);
    }  
}

/**
 * tweeter manager
 */
void tweeterManager(){
    // temperature once in a hour
    if ((lastTempTweet == 0 || (millis() - lastTempTweet) > 3600000)
          && (millis() - lastTwittTime) > 60000){
        lastTempTweet = millis();
        lastTwittTime = millis();
        String msg = "Temp: " + String(temp1);
        postTweet(msg);
    }
}

void postTweet(String msg){
    int rnd = random(1000);
    String msgFull = "[#" + String(rnd) + "] " + msg;
    char buf[msgFull.length()];
    msgFull.toCharArray(buf, msgFull.length());
    if (twitter.post(buf)) {
      int status = twitter.wait();
      if (status == 200) {
        Serial.println("OK.");
      } else {
        Serial.print("failed : code ");
        Serial.println(status);
      }
    } else {
      Serial.println("connection failed.");
    } 
}

