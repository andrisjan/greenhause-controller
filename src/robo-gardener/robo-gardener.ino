
#define SLEEP 100

// digital pins
// 10, 11, 12, 13 - ocup by ethernet
#define ETHERNET_DIG_OUT_PIN          10    // set HI to use ethernet
#define WATER_PUMP_SIG_DIG_OUT_PIN    9     // output pin water pump relay signal
#define WATER_FLOW_INTERUPT           0     // 0 = digital pin 2 
#define WATER_FROW_SIG_DIG_INT_PIN    2
// analog pins
// A0, A1 - ocup by ethernet
#define TEMP_1_ANALOG_IN_PIN      2     // analog input termorezistoram 1

// ================= variables for temperature ========================
const int SAMPLE_CNT = 10;
int tempLoopCounter = 0;
int samplesForTmp1[SAMPLE_CNT];
double temp1;

// ================ variables for water pump =====================
boolean waterPumpOperating = false;
const byte CAN_WATER = 1; // state when watering is posible
const byte WATER_ON  = 2; // state when water flowing
byte WATER_STATE = CAN_WATER;

// =================== variables for water flow sensor ===========
double calibrationFactor = 4.5;
volatile byte pulseCount;  
float flowRate;
unsigned int flowMilliLitres;
unsigned long totalMilliLitres;
unsigned long oldTime;

void setup() {
    Serial.begin(9600);    
    initTempArray();
    initWaterFlowSensor();
    initWaterPump();
}

void loop() {
    temperatureManager();
    waterFlowManager();
    waterPumpManager();
    delay(SLEEP);

    // TODO:
    // tweet temp once in a hour
    // 
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

    double tempLevel = 28;
    // TODO: rezi x stundās, dienas laikā 
    if (WATER_STATE == CAN_WATER && temp1 > tempLevel){
        WATER_STATE = WATER_ON;
    }
    int maxWaterInMl = 1000;
    if (WATER_STATE == WATER_ON){
        if (totalMilliLitres < maxWaterInMl){
            digitalWrite(WATER_PUMP_SIG_DIG_OUT_PIN, HIGH);
        }
        else{
            WATER_STATE = 9; // stop
            totalMilliLitres = 0;
        }
    }
    else{
        digitalWrite(WATER_PUMP_SIG_DIG_OUT_PIN, LOW);
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
        Serial.print("  Output Liquid Quantity: ");        
        Serial.print(totalMilliLitres);
        Serial.print("mL, temp: ");
        Serial.println(temp1); 
        pulseCount = 0;
        attachInterrupt(WATER_FLOW_INTERUPT, waterFlowPulseCounter, FALLING);
    }  
}

