#include <EEPROM.h>
#include <SPI.h>
#include <RH_RF95.h>
#include <Arduino.h>
#include <ChaChaPoly.h>
#include <Crypto.h>
#include <BLAKE2s.h>

#define HASH_EEPROM_START 0    // Start position of hash in EEPROM
#define HASH_SIZE 32            // Hash size is 32 bytesws
#define MAX_MESSAGE_LENGTH 13   // Chunked messages size
#define MESSAGELENGTH 100       // Increased buffer size to accommodate MAC
#define TXINTERVAL 5000         // Interval between transmissions
#define CSMATIME 10             // CSMA backoff time

// Singleton instance of radio driver
RH_RF95 rf95;
uint8_t led = 13; // Define LED pin

// Message fields
uint8_t SEQ = 0;  // Sequence number
uint8_t TYPE = 0; // Message type
uint8_t TAGID = 1; // Identity of tag
uint8_t RELAYID = 0; // Relay ID
uint8_t S_NODE = 2;
int thisRSSI = 0;
int DEST_NODE = 0; // Destination node

// Nonce for encryption
byte nonce[12] = {
    0x12, 0x34, 0x56, 0x78, 0x90, 0xab, 0xcd, 0xef,
    0x12, 0x34, 0x56, 0x78
};

ChaChaPoly chachaPoly;
byte ciphertext[MESSAGELENGTH];  // Buffer to hold encrypted data
byte tag[16]; // Buffer for authentication tag

// Function to write hash to EEPROM
void writeHashToEEPROM(uint8_t* hashResult) {
    for (int i = 0; i < HASH_SIZE; i++) {
        EEPROM.write(HASH_EEPROM_START + i, hashResult[i]);
    }
}

// Function to read the stored hash from EEPROM
void readHashFromEEPROM(uint8_t* hashResult) {
    for (int i = 0; i < HASH_SIZE; i++) {
        hashResult[i] = EEPROM.read(HASH_EEPROM_START + i);
    }
}

// Function to clear EEPROM
void clearEEPROM() {
    for (int i = 0; i < EEPROM.length(); i++) {
        EEPROM.write(i, 0xFF);
    }
    Serial.println(F("EEPROM has been cleared."));
}

// Function to check if EEPROM contains any non-zero value
bool checkEEPROMForData() {
    for (int i = 0; i < HASH_SIZE; i++) {
        if (EEPROM.read(HASH_EEPROM_START + i) != 0xFF) {
            return true;
        }
    }
    return false;
}

// Function to compare two hash arrays
bool compareHashes(uint8_t* hash1, uint8_t* hash2, size_t length) {
    for (size_t i = 0; i < length; i++) {
        if (hash1[i] != hash2[i]) {
            return false;
        }
    }
    return true;
}

// Function to read encryption key from EEPROM
void readKeyFromEEPROM(byte* key) {
    for (int i = 0; i < 32; i++) {
        key[i] = EEPROM.read(i);
    }
}

// Function to transmit encrypted message chunks
void transmitMessageChunks(const String& message) {
    int messageLength = message.length();
    int totalChunks = (messageLength + MAX_MESSAGE_LENGTH - 1) / MAX_MESSAGE_LENGTH;

    // Iterate for the total message length
    for (int chunkIndex = 0; chunkIndex < totalChunks; chunkIndex++) {
        String chunk = message.substring(chunkIndex * MAX_MESSAGE_LENGTH, (chunkIndex + 1) * MAX_MESSAGE_LENGTH); // Create a substring (chunk) of MAX_MESSAGE_LENGTH
        
        // Prepare encryption key from EEPROM
        byte key[32];
        readKeyFromEEPROM(key);

        // Initialize ChaChaPoly with the stored key and nonce
        chachaPoly.setKey(key, sizeof(key)); // Sets key
        chachaPoly.setIV(nonce, sizeof(nonce)); // Sets nonce

        // Encrypt the message chunk
        size_t len = chunk.length();
        chachaPoly.encrypt(ciphertext, reinterpret_cast<const uint8_t*>(chunk.c_str()), len); // Encrypt message into ciphertext
        chachaPoly.computeTag(tag, sizeof(tag)); // Compute the tag

        // Append the tag to the ciphertext
        memcpy(ciphertext + len, tag, sizeof(tag)); // Append tag to ciphertext
        len += sizeof(tag); // Update length to include tag

        // Convert ciphertext to hex string for the message
        char hexCiphertext[len * 2 + 1];  // +1 for null terminator 
        for (size_t i = 0; i < len; ++i) {
            sprintf(&hexCiphertext[i * 2], "%02x", ciphertext[i]); // Convert each element in ciphertext into hex string
        }
        hexCiphertext[len * 2] = '\0'; // Null-terminate the string

        // Prepare to transmit the message
        uint8_t buf[MESSAGELENGTH];
        SEQ++;

        // Set STOP field: 0 for ongoing messages, 1 for the last message
        uint8_t STOP = (chunkIndex == totalChunks - 1) ? 1 : 0;

        // Create the message with metadata and encrypted chunk
        snprintf((char*)buf, MESSAGELENGTH, "%d %d %d %d %d %d %d %d %s", SEQ, TYPE, TAGID, RELAYID, S_NODE, thisRSSI, DEST_NODE, STOP, hexCiphertext);

        rf95.setModeIdle(); // Ensure channel is idle
        while (rf95.isChannelActive()) {
            delay(CSMATIME);
        }

        // Flash the LED while transmitting
        digitalWrite(led, HIGH);  // Turn on the LED
        Serial.print(F("Transmitting chunk: "));
        Serial.println(chunk);  // For debugging

        rf95.send(buf, strlen((char*)buf));
        rf95.waitPacketSent();
        
        digitalWrite(led, LOW);   // Turn off the LED after transmission
        delay(TXINTERVAL);  // Delay before transmitting the next chunk
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
        Serial.println(F("EEPROM is empty. Please enter a password to hash:"));
    }
}

void loop() {
    // Check if user input is available
    if (Serial.available() > 0) {
        String inputText = Serial.readStringUntil('\n');
        inputText.trim();  // Remove extra spaces and newline characters

        BLAKE2s hashObject;
        uint8_t hashResult[HASH_SIZE];
        uint8_t storedHash[HASH_SIZE];

        // If EEPROM is empty, take the input as the password to hash
        if (!checkEEPROMForData()) {
            hashObject.update((const uint8_t*)inputText.c_str(), inputText.length());
            hashObject.finalize(hashResult, sizeof(hashResult));
            writeHashToEEPROM(hashResult);
            Serial.println(F("Password hash stored in EEPROM."));
        } else {
            if (inputText.equalsIgnoreCase("clear")) {
                clearEEPROM();
            } else {
                hashObject.update((const uint8_t*)inputText.c_str(), inputText.length());
                hashObject.finalize(hashResult, sizeof(hashResult));
                readHashFromEEPROM(storedHash);

                if (compareHashes(hashResult, storedHash, HASH_SIZE)) {
                    Serial.println(F("Authentication successful."));

                    // Prompt for destination node input 
                    Serial.println(F("Enter intended destination node. 2-99 is valid:")); // 1 is the transmitter

                    // Wait for the destination node input
                    while (!Serial.available()) {
                        delay(100);
                    }
                    DEST_NODE = Serial.parseInt();

                    // Clear any remaining characters in the serial buffer (especially newline)
                    while (Serial.available()) {
                        Serial.read();
                    }

                    if (DEST_NODE >= 2 && DEST_NODE <= 99) {
                        Serial.print(F("Destination node set to: "));
                        Serial.println(DEST_NODE);

                        Serial.println(F("Please enter a message to encrypt:"));

                        // Wait for message input
                        while (!Serial.available()) {
                            delay(100);
                        }

                        String message = Serial.readStringUntil('\n');
                        message.trim();

                        if (message.length() > 0) {
                            transmitMessageChunks(message);  // Transmit the message in chunks
                        }

                    } else {
                        Serial.println(F("Invalid destination node. Please try again."));
                    }  
                } else {
                    Serial.println(F("Authentication failed. Hash does not match."));
                }
            }
        }

        if (checkEEPROMForData()) {
            Serial.println(F("Please type your password to authenticate or 'clear' to erase EEPROM."));
        } else {
            Serial.println(F("EEPROM is empty. Please enter a password to hash:"));
        }
        delay(500);  // Small delay before reading next input
    }
}
