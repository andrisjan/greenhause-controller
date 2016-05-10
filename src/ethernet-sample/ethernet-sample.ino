#include <ArduinoJson.h>
#include <SPI.h>
#include <Ethernet.h>


//const int LED_DIG_OUT_PIN = 13;

byte mac[] = { 0x90, 0xA2, 0xDA, 0x0D, 0x85, 0xD9 }; 
byte ip[] = { 192, 168, 1, 10 };                 
EthernetServer server(80);                  
byte blankLineCounter = 0;
void setup() {
  Serial.begin(9600);
  Ethernet.begin(mac,ip);     
  server.begin();          
  Serial.print("server is at ");
  Serial.println(Ethernet.localIP());
  
  //pinMode(LED_DIG_OUT_PIN, OUTPUT);
}

void loop() {
   //digitalWrite(LED_DIG_OUT_PIN, HIGH);
   //delay(1000);
   //digitalWrite(LED_DIG_OUT_PIN, LOW);
   //delay(1000);
    EthernetClient client = server.available();   // look for the client
    
    if (client) {
      Serial.println("12new client");
      int size = 1024;
      char header[size];
      char body[size];
      byte newLineCounter = 0;
      boolean readBody = false;
      int bodyCounter = 0;
      int headerCounter = 0;
        if (client.available()) {
          char c;
          while ((c = client.read()) > 0){
            if (c == '\r'){
              continue;
              Serial.write("\\r");
            }
            if (c == '\n'){
              ++newLineCounter;
              Serial.write("\\n");
              if (newLineCounter == 2){
                  newLineCounter = 0;
                  Serial.print("kkknext line should start message body");
                  readBody = true;
                  continue;
              }
              continue;
            }
            if (newLineCounter > 0){
              --newLineCounter;
            }
            if (bodyCounter >= size /*|| headerCounter >= size*/){
              break;
            }
            if (readBody){
              body[bodyCounter++] = c;
            }
            else{
              //header[headerCounter++] = c;
            }
            Serial.write(c);
          } // end while
          body[bodyCounter] = 0;
          header[headerCounter] = 0;
        }

      delay(1);
      // close the connection:
      client.stop();
      Serial.println("client disconnected");
//      if (headerCounter > 0){
//        //Serial.println("---- HEADER BEGIN --------");
//        //Serial.println(header);
//        //Serial.println("---- HEADER END --------");
//      }

        Serial.println("---- BODY BEGIN --------");
        Serial.println(body);
        Serial.println("---- BODY END --------");

    }
}
