#include <Crypto.h>   // Include the Crypto library
#include <BLAKE2s.h>  // Include the BLAKE2s implementation
#include <EEPROM.h>

// Function to read the secret key from EEPROM
void readSecretKeyFromEEPROM(byte* key) {
  for (int i = 0; i < 32; i++) {
    key[i] = EEPROM.read(i);  // Read each byte from EEPROM
  }
}

void setup() {
    // Initialize serial communication
    Serial.begin(9600);

    // Wait for the serial port to open
    while (!Serial);

    // Create the BLAKE2s hash object
    BLAKE2s hashObject;

    // The input data we want to hash
    const char* password = "admin";

    // Update the hash with the password
    hashObject.update((const uint8_t*)password, strlen(password));

    // Create a buffer to store the hash result
    uint8_t hashResult[32];  // BLAKE2s typically outputs 32 bytes

    // Finalize the hash to get the result
    hashObject.finalize(hashResult, sizeof(hashResult));

    // Print the hash result in uint8_t format (hexadecimal values)
    Serial.println("BLAKE2s hash of 'admin' in uint8_t format:");
    Serial.print("uint8_t hashResult[32] = { ");
    for (size_t i = 0; i < sizeof(hashResult); ++i) {
        if (i != 0) {
            Serial.print(", ");
        }
        // Print each byte in hexadecimal format, 0-padded for single digits
        Serial.print("0x");
        if (hashResult[i] < 0x10) {
            Serial.print('0');
        }
        Serial.print(hashResult[i], HEX);
    }
    Serial.println(" };");




    // -------------------------------------------
    // Read the secret key from EEPROM
//    byte retrievedKey[32];
//    readSecretKeyFromEEPROM(retrievedKey);
//
//    // Display the retrieved key (for debugging purposes)
//    Serial.println("Retrieved key from EEPROM:");
//    for (size_t i = 0; i < 32; i++) {
//      if (retrievedKey[i] < 16) {
//        Serial.print('0');  // Add leading zero
//      }
//      Serial.print(retrievedKey[i], HEX);
//      Serial.print(" ");
//    }
//    Serial.println();
}

void loop() {
    // Nothing to do here
}
