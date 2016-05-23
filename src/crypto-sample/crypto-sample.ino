//#include <Crypto.h>
#include <SHA256.h>

#define HASH_SIZE 32

SHA256 sha256;
char key[] = "key";
char data[] = "data lot of very long data bla bla bladata lot of very long data bla bla bladata lot of very long data bla bla bladata lot of very long data bla bla bladata lot of very long data bla bla bla";

void setup() {
    Serial.begin(9600);
    Serial.println("crypto sample init");

    uint8_t result[HASH_SIZE];

    sha256.resetHMAC(key, sizeof(key)-1);
    sha256.update(data, sizeof(data)-1);
    sha256.finalizeHMAC(key, sizeof(key)-1, result, HASH_SIZE);

    for (byte i=0; i<HASH_SIZE; i++){
        Serial.println(result[i], HEX);
    }

}

void loop() {
  // put your main code here, to run repeatedly:

}
