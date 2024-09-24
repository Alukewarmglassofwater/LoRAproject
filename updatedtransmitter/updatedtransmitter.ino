#include <EEPROM.h>
#include <SPI.h>
#include <RH_RF95.h>
#include <Arduino.h>
#include <ChaChaPoly.h>
#include <Crypto.h>
#include <BLAKE2s.h>

#define HASH_EEPROM_START 0    // EEPROM start address to store the hash
#define HASH_SIZE 32           // Hash size is 32 bytes
#define MESSAGELENGTH 64       // Reduce the message buffer length
#define TXINTERVAL 5000
#define CSMATIME 10

// Singleton instance of radio driver
RH_RF95 rf95;
uint8_t led = 13; // Define LED pin

// Message fields
uint8_t SEQ = 0;  // Sequence number
uint8_t TYPE = 0; // Message type
uint8_t TAGID = 1; // Identity of tag
uint8_t RELAYID = 0; // Relay ID
uint8_t TTL = 5; // Time to live
int thisRSSI = 0;
uint8_t firstIteration = 0; // First iteration flag

// Nonce for encryption
byte nonce[12] = {
  0x12, 0x34, 0x56, 0x78, 0x90, 0xab, 0xcd, 0xef,
  0x12, 0x34, 0x56, 0x78
};

ChaChaPoly chachaPoly;
byte ciphertext[MESSAGELENGTH];  // Buffer to hold encrypted data
byte tag[16]; // Buffer for authentication tag

// Declare global variable for hex representation (reduced size)
char hexCiphertext[MESSAGELENGTH * 2 + 1];  // +1 for null terminator

// Function to write hash to EEPROM
void writeHashToEEPROM(uint8_t* hashResult) {
  for (int i = 0; i < HASH_SIZE; i++) {
    EEPROM.write(HASH_EEPROM_START + i, hashResult[i]);  // Write each byte to EEPROM
  }
}

// Function to read the stored hash from EEPROM
void readHashFromEEPROM(uint8_t* hashResult) {
  for (int i = 0; i < HASH_SIZE; i++) {
    hashResult[i] = EEPROM.read(HASH_EEPROM_START + i);  // Read each byte from EEPROM
  }
}

// Function to print the hash as a C-style byte array
void printHashAsHexArray(uint8_t* hashResult, size_t length) {
  Serial.print(F("uint8_t hashResult[32] = {"));
  for (size_t i = 0; i < length; i++) {
    if (i != 0) {
      Serial.print(F(", "));
    }
    Serial.print(F("0x"));
    if (hashResult[i] < 0x10) {
      Serial.print(F("0"));  // Add leading zero for single digit
    }
    Serial.print(hashResult[i], HEX);
  }
  Serial.println(F(" };"));
}

// Function to clear EEPROM
void clearEEPROM() {
  for (int i = 0; i < EEPROM.length(); i++) {
    EEPROM.write(i, 0xFF);  // Set each byte to 0xFF (default unprogrammed state)
  }
  Serial.println(F("EEPROM has been cleared."));
}

// Function to check if EEPROM contains any non-zero value
bool checkEEPROMForData() {
  for (int i = 0; i < HASH_SIZE; i++) {
    if (EEPROM.read(HASH_EEPROM_START + i) != 0xFF) {  // 0xFF is the default EEPROM value when unprogrammed
      return true;  // Found something in EEPROM
    }
  }
  return false;  // EEPROM is empty (all default values)
}

// Function to compare two hash arrays
bool compareHashes(uint8_t* hash1, uint8_t* hash2, size_t length) {
  for (size_t i = 0; i < length; i++) {
    if (hash1[i] != hash2[i]) {
      return false;  // Hashes do not match
    }
  }
  return true;  // Hashes match
}

// Function to read encryption key from EEPROM
void readKeyFromEEPROM(byte* key) {
  for (int i = 0; i < 32; i++) {
    key[i] = EEPROM.read(i);
  }
}

void setup() {
  // Initialize serial communication and LoRa transceiver
  Serial.begin(9600);
  pinMode(led, OUTPUT);
  
  if (!rf95.init()) {
    Serial.println(F("LoRa initialization failed"));
    while (1); // Stop execution
  }

  rf95.setFrequency(915.0);
  rf95.setTxPower(5, false);
  rf95.setSignalBandwidth(500000);
  rf95.setSpreadingFactor(12);

  // Check if EEPROM is populated
  if (checkEEPROMForData()) {
    Serial.println(F("EEPROM contains data. Please type your password to authenticate or 'clear' to erase EEPROM."));
  } else {
    Serial.println(F("EEPROM is empty. Please enter a seed for your encryption key:"));
  }
}

void loop() {
  // Check if user input is available
  if (Serial.available() > 0) {
    // Read the user input from the serial monitor
    String inputText = Serial.readStringUntil('\n');
    inputText.trim();  // Remove extra spaces and newline characters

    // Create the BLAKE2s hash object
    BLAKE2s hashObject;

    // Create a buffer to store the hash result (32 bytes)
    uint8_t hashResult[32];
    uint8_t storedHash[32];

    // If EEPROM is empty, take the input as the seed for the encryption key
    if (!checkEEPROMForData()) {
      // Update the hash with the user-provided seed
      hashObject.update((const uint8_t*)inputText.c_str(), inputText.length());

      // Finalize the hash to get the result
      hashObject.finalize(hashResult, sizeof(hashResult));

      // Store the hashed seed in EEPROM
      writeHashToEEPROM(hashResult);
      Serial.println(F("Encryption key seed stored in EEPROM."));

      // Print the hashed seed as a 32-byte hex array
      Serial.print(F("Stored seed hash as 32-byte hex array: "));
      printHashAsHexArray(hashResult, sizeof(hashResult));
    }
    // If EEPROM is populated, require authentication or clear the EEPROM
    else {
      if (inputText.equalsIgnoreCase("clear")) {
        // If the user types "clear", erase the EEPROM
        clearEEPROM();
      } else {
        // Update the hash with the user-provided password
        hashObject.update((const uint8_t*)inputText.c_str(), inputText.length());

        // Finalize the hash to get the result
        hashObject.finalize(hashResult, sizeof(hashResult));

        // Read the stored hash from EEPROM
        readHashFromEEPROM(storedHash);

        // Compare the entered hash with the stored hash
        if (compareHashes(hashResult, storedHash, HASH_SIZE)) {
          Serial.println(F("Authentication successful."));

          // After successful authentication, allow the user to enter a message to encrypt
          Serial.println(F("Please enter a message to encrypt:"));

          // Wait for message input
          while (!Serial.available()) {
            delay(100);
          }

          String message = Serial.readStringUntil('\n');
          message.trim();

          if (message.length() > 0) {
            // Prepare encryption key from EEPROM
            byte key[32];
            readKeyFromEEPROM(key);

            // Initialize ChaChaPoly with the stored key and nonce
            chachaPoly.setKey(key, sizeof(key));
            chachaPoly.setIV(nonce, sizeof(nonce));

            // Encrypt the message
            size_t len = message.length();
            chachaPoly.encrypt(ciphertext, reinterpret_cast<const uint8_t*>(message.c_str()), len);

            // Compute the authentication tag
            chachaPoly.computeTag(tag, sizeof(tag));

            // Convert ciphertext to hex string for the message
            for (size_t i = 0; i < len; ++i) {
              sprintf(&hexCiphertext[i * 2], "%02x", ciphertext[i]);
            }
            hexCiphertext[len * 2] = '\0'; // Null-terminate the string

            // Prepare to transmit the message
            uint8_t buf[MESSAGELENGTH];
            SEQ++;

            // Create the message
            char str[MESSAGELENGTH];
            snprintf(str, sizeof(str), "%5d %5d %5d %5d %5d %5d %s", SEQ, TYPE, TAGID, RELAYID, TTL, thisRSSI, hexCiphertext);

            // Ensure the buffer is large enough and properly null-terminated
            memset(buf, 0, sizeof(buf)); // Clear the buffer first
            strncpy((char*)buf, str, sizeof(buf) - 1);
            buf[sizeof(buf) - 1] = '\0'; // Null-terminate

            rf95.setModeIdle(); // Ensure channel is idle
            while (rf95.isChannelActive()) {
              delay(CSMATIME);
              Serial.println(F("Tag node looping on isChannelActive()")); // DEBUG
            }

            // Transmit the message
            Serial.print(F("Transmitted message: "));
            Serial.println((char*)buf); // DEBUG
            rf95.send(buf, strlen((char*)buf));
            rf95.waitPacketSent();
            delay(TXINTERVAL);
          }
        } else {
          Serial.println(F("Authentication failed. Hash does not match."));
        }
      }
    }

    // Prompt for the next action
    if (checkEEPROMForData()) {
      Serial.println(F("Please type your password to authenticate or 'clear' to erase EEPROM."));
    } else {
      Serial.println(F("EEPROM is empty. Please enter a seed for your encryption key:"));
    }
    delay(500);  // Small delay before reading next input
  }
}
