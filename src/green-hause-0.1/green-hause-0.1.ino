
#include <math.h>

//const int PIN_TURN_ON = 13;      // output pin shēmas ieslēgšnai
const int SLEEP = 100;
// digital pins
const int WIN_UP_SW_DIG_IN_PIN = 9;     // input pin loga pacelšanas signālam
const int WIN_UP_SIG_DIG_OUT_PIN = 11;   // output pin loga pacelšanas releja signāls
const int WIN_DOWN_SW_DIG_IN_PIN = 10;  // input pin loga nolaišanas signālam
const int WIN_DOWN_SIG_DIG_OUT_PIN = 12; // output pin loga nolaišanas releja signāls
// analog pins
const int TEMP_1_ANALOG_IN_PIN = 0;     // analog input termorezistoram 1
const int TEMP_2_ANALOG_IN_PIN = 1;     // analog input termorezistoram 2     
   
// variables for temperature
const int SAMPLE_CNT = 10;
int tempLoopCounter = 0;
int samplesForTmp1[SAMPLE_CNT];
int samplesForTmp2[SAMPLE_CNT];
double temp1;
double temp2;
double tempAverage;

// variables for windows
const double WINDOWS_OPEN_TEMP_LEVEL = 26.0;  
const double WINDOWS_CLOSE_TEMP_LEVEL = 24.0;
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

void setup() {
  Serial.begin(9600);      // open the serial port at 9600 bps:  
  Serial.println("setup ...");
  setUpWindows();
  initTempArray();
}

void loop() {

    // TODO: controller() metode vai vnk kods kas apstrādā REST komandas

    // TODO: tempService() klase [sākumā vnk metodes] darbībām ar temperatūru
    // updateSensorData() // nolasa termorezistoru vērtības
    // getTemp() // atgriež agregētu t vērtību no vairākiem termorezistoriem
    // metodes datu saglabāšanai failā, nolasīšanai, izdzēšanai
 
    temperatureManager();
    windowManager();
    
    delay(SLEEP);
}

/**
 * temperature related methods
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
    String result = "temp1: " + String(temp1) + ", temp2: " + String(temp2) + ", tempAverage: " + String(tempAverage);
    Serial.println(result); 
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
 * window open / close related methods
 */
void windowManager(){
    // open/close windows based on manual switch
    if (digitalRead(WIN_UP_SW_DIG_IN_PIN) == HIGH) {
        openWindows();
        return;
    }
    else if (digitalRead(WIN_DOWN_SW_DIG_IN_PIN) == HIGH){
        closeWindows();
        return;
    }

    // open/close windows based on temp
    if (windowStatus == WIN_OFF && tempAverage > WINDOWS_OPEN_TEMP_LEVEL){
        windowStatus = WIN_OPENING;
    }
    else if (windowStatus == WIN_OFF && tempAverage < WINDOWS_CLOSE_TEMP_LEVEL){
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

    Serial.println("windowStatus: " + String(windowStatus) + ", windowsLoopCounter: " + String(windowsLoopCounter));
}

void openWindows(){
    Serial.println("UP");
    windowsOperating = true;
    digitalWrite(WIN_DOWN_SIG_DIG_OUT_PIN, LOW);
    digitalWrite(WIN_UP_SIG_DIG_OUT_PIN, HIGH);
}

void closeWindows(){
    Serial.println("DOWN");
    windowsOperating = true;
    digitalWrite(WIN_UP_SIG_DIG_OUT_PIN, LOW);
    digitalWrite(WIN_DOWN_SIG_DIG_OUT_PIN, HIGH);
}

void turnOffWindows(){
    Serial.println("OFF");
    windowsOperating = false;
    digitalWrite(WIN_DOWN_SIG_DIG_OUT_PIN, LOW);
    digitalWrite(WIN_UP_SIG_DIG_OUT_PIN, LOW);
}
 
void setUpWindows(){
    pinMode(WIN_UP_SW_DIG_IN_PIN, INPUT);
    pinMode(WIN_DOWN_SW_DIG_IN_PIN, INPUT);
    pinMode(WIN_UP_SIG_DIG_OUT_PIN, OUTPUT);
    digitalWrite(WIN_UP_SIG_DIG_OUT_PIN, LOW);
    pinMode(WIN_DOWN_SIG_DIG_OUT_PIN, OUTPUT);
    digitalWrite(WIN_DOWN_SIG_DIG_OUT_PIN, LOW);
}

