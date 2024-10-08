#include <EEPROM.h>
#include <SPI.h>
#include <RH_RF95.h>
#include <Arduino.h>
#include <ChaChaPoly.h>
#include <Crypto.h>
#include <BLAKE2s.h>

#define HASH_EEPROM_START 0          // EEPROM start address to store the hash
#define HASH_SIZE 32                  // Hash size is 32 bytes
#define MESSAGELENGTH 64              // Base message buffer length
#define CHUNK_SIZE 32                 // Size of each chunk to be encrypted and sent
#define TXINTERVAL 5000
#define CSMATIME 10

// Singleton instance of radio driver
RH_RF95 rf95;
uint8_t led = 13; // Define LED pin

// Message fields
uint8_t SEQ = 0;  
uint8_t TYPE = 0; 
uint8_t TAGID = 1; 
uint8_t RELAYID = 0; 
uint8_t TTL = 5; 
int thisRSSI = 0;

// Nonce for encryption
byte nonce[12] = {
    0x12, 0x34, 0x56, 0x78, 
    0x90, 0xab, 0xcd, 0xef,
    0x12, 0x34, 0x56, 0x78
};

ChaChaPoly chachaPoly;
byte ciphertext[CHUNK_SIZE];  // Buffer to hold encrypted data
byte tag[16]; // Buffer for authentication tag

// Function to print the hash as a hex string
void printHashAsHex(const uint8_t* hash) {
    Serial.print("Hash: { ");
    for (int i = 0; i < HASH_SIZE; i++) {
        if (i != 0) Serial.print(", ");
        Serial.print("0x");
        if (hash[i] < 0x10) Serial.print("0");
        Serial.print(hash[i], HEX);
    }
    Serial.println(" }");
}

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

// Function to send a chunk of the message
void sendChunk(const byte* chunk, size_t length) {
    uint8_t buf[MESSAGELENGTH];
    snprintf((char*)buf, sizeof(buf), "%5d %5d %5d %5d %5d %5d ", SEQ, TYPE, TAGID, RELAYID, TTL, thisRSSI);
  
    // Append chunk to the buffer
    for (size_t i = 0; i < length; i++) {
        sprintf((char*)&buf[strlen((char*)buf)], "%02x", chunk[i]);
    }

    rf95.setModeIdle(); // Ensure channel is idle
    while (rf95.isChannelActive()) {
        delay(CSMATIME);
        Serial.println(F("Waiting for channel to be free..."));
    }

    // Transmit the message
    Serial.print(F("Transmitting chunk: "));
    Serial.println((char*)buf); // DEBUG
    rf95.send(buf, strlen((char*)buf));
    rf95.waitPacketSent();
    delay(TXINTERVAL);
}

void setup() {
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
        String inputText = Serial.readStringUntil('\n');
        inputText.trim();  // Remove extra spaces and newline characters

        BLAKE2s hashObject;
        uint8_t hashResult[32];
        uint8_t storedHash[32];

        if (!checkEEPROMForData()) {
            // Update the hash with the user-provided seed
            hashObject.update((const uint8_t*)inputText.c_str(), inputText.length());
            hashObject.finalize(hashResult, sizeof(hashResult));

            // Store the hashed seed in EEPROM
            writeHashToEEPROM(hashResult);
            Serial.println(F("Encryption key seed stored in EEPROM."));
            printHashAsHex(hashResult);  // Print the hashed password immediately after hashing
        } else {
            if (inputText.equalsIgnoreCase("clear")) {
                clearEEPROM();
            } else {
                // Update the hash with the user-provided password
                hashObject.update((const uint8_t*)inputText.c_str(), inputText.length());
                hashObject.finalize(hashResult, sizeof(hashResult));
                readHashFromEEPROM(storedHash);

                if (compareHashes(hashResult, storedHash, HASH_SIZE)) {
                    Serial.println(F("Authentication successful."));
                    Serial.println(F("Please enter a message to encrypt:"));

                    while (!Serial.available()) {
                        delay(100);
                    }

                    String message = Serial.readStringUntil('\n');
                    message.trim();

                    if (message.length() > 0) {
                        byte key[32];
                        readKeyFromEEPROM(key);
                        chachaPoly.setKey(key, sizeof(key));
                        chachaPoly.setIV(nonce, sizeof(nonce));

                        size_t totalChunks = (message.length() + CHUNK_SIZE - 1) / CHUNK_SIZE;

                        for (size_t chunkIndex = 0; chunkIndex < totalChunks; chunkIndex++) {
                            size_t chunkStart = chunkIndex * CHUNK_SIZE;
                            size_t chunkLength = min(CHUNK_SIZE, message.length() - chunkStart);

                            byte chunk[CHUNK_SIZE] = {0};
                            message.getBytes(chunk, CHUNK_SIZE, chunkStart);
                            chachaPoly.encrypt(ciphertext, chunk, chunkLength);
                            sendChunk(ciphertext, chunkLength);
                        }
                    }
                } else {
                    Serial.println(F("Authentication failed. Hash does not match."));
                }
            }
        }

        // Print the stored hash from EEPROM
        uint8_t readHash[HASH_SIZE];
        readHashFromEEPROM(readHash);
        printHashAsHex(readHash);  // Print the hash read from EEPROM

        // Prompt for the next action
        if (checkEEPROMForData()) {
            Serial.println(F("Please type your password to authenticate or 'clear' to erase EEPROM."));
        } else {
            Serial.println(F("EEPROM is empty. Please enter a seed for your encryption key:"));
        }
        delay(500);
    }
}
