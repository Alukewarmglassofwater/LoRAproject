#include <EEPROM.h>
#include <SPI.h>
#include <RH_RF95.h>
#include <ChaChaPoly.h>
#include <Crypto.h>
#include <BLAKE2s.h>

// EEPROM Configuration
#define HASH_EEPROM_START 0
#define NODE_NUM_EEPROM_ADDRESS 64  // New address to store the node number
#define HASH_SIZE 32
#define MESSAGELENGTH 83       // Must match the transmitter's MESSAGELENGTH

// Configuration for storing decrypted messages
#define DECRYPTED_MESSAGE_EEPROM_START 128 // Start address for decrypted messages
#define MAX_DECRYPTED_MESSAGE_LENGTH 128 // Maximum length for the stored message

// LoRa Configuration
#define RF95_FREQ 915.0
#define TXPOWER 23
#define BANDWIDTH 500000
#define SPREADING_FACTOR 12
#define CSMATIME 10

// Singleton instance of radio driver
RH_RF95 rf95;

// LED Pin
const uint8_t LED_PIN = 13;

// Nonce for encryption (must match transmitter)
const byte nonce[12] = {
    0x12, 0x34, 0x56, 0x78,
    0x90, 0xab, 0xcd, 0xef,
    0x12, 0x34, 0x56, 0x78
};

// ChaChaPoly instance
ChaChaPoly chachaPoly;

// Buffers for encrypted and decrypted messages
char encryptedMessageHex[128];
char decryptedMessage[MAX_DECRYPTED_MESSAGE_LENGTH]; // Use defined maximum length

int NODE_NUM = 0;  // Variable to store the node number

// Function Prototypes
void writeHashToEEPROM(uint8_t* hashResult);
void readHashFromEEPROM(uint8_t* hashResult);
void clearEEPROM();
void clearStoredMessage(); // New function to clear stored messages
bool checkEEPROMForData();
bool compareHashes(uint8_t* hash1, uint8_t* hash2, size_t length);
void hexStringToBytes(const char* hexString, byte* byteArray, int byteArraySize);
void listenForMessages();
void readKeyFromEEPROM(byte* key);
void checkEEPROMState();
void promptForNodeNumber();
void readNodeNumberFromEEPROM();
void writeNodeNumberToEEPROM(int nodeNum);
void transmitMessage(const char* message); // New function to transmit messages
void saveDecryptedMessageToEEPROM(const char* message, int length); // Updated function signature
void displayStoredMessage(); // New function to display the stored message

void setup() {
    // Initialize Serial and LED
    pinMode(LED_PIN, OUTPUT);
    Serial.begin(9600);
    // Serial.println(F("Receiver Version 1"));

    // Read the node number from EEPROM
    readNodeNumberFromEEPROM();

    // Initialize LoRa
    if (!rf95.init()) {
        Serial.println(F("LoRa receiver initialization failed"));
        while (1); // Halt if LoRa fails to initialize
    }

    rf95.setFrequency(RF95_FREQ);
    rf95.setTxPower(TXPOWER, false);
    rf95.setSignalBandwidth(BANDWIDTH);
    rf95.setSpreadingFactor(SPREADING_FACTOR);

    // Initial prompt based on EEPROM state
    checkEEPROMState();
}

void loop() {
    // Check for user input
    if (Serial.available() > 0) {
        String inputText = Serial.readStringUntil('\n');
        inputText.trim();  // Remove leading/trailing whitespace

        // Initialize BLAKE2s hash object
        BLAKE2s hashObject;
        uint8_t hashResult[HASH_SIZE];
        uint8_t storedHash[HASH_SIZE];

        if (!checkEEPROMForData()) {
            // EEPROM is empty: user needs to enter a seed
            hashObject.update((const uint8_t*)inputText.c_str(), inputText.length());
            hashObject.finalize(hashResult, sizeof(hashResult));

            // Store the hash in EEPROM
            writeHashToEEPROM(hashResult);
            Serial.println(F("Encryption key seed stored in EEPROM."));

            // Prompt for node number
            promptForNodeNumber();

            // // Debug: Print the stored hash
            // Serial.print(F("Stored seed hash as 32-byte hex array: { "));
            // for (size_t i = 0; i < HASH_SIZE; i++) {
            //     if (i != 0) Serial.print(F(", "));
            //     Serial.print(F("0x"));
            //     if (hashResult[i] < 0x10) Serial.print(F("0"));
            //     Serial.print(hashResult[i], HEX);
            // }
            // Serial.println(F(" };"));

            // Prompt to wait for messages
            Serial.print(F("Current Node Number: "));
            Serial.println(NODE_NUM);  // Display current node number
            Serial.println(F("Waiting for message..."));
            listenForMessages();  // Start listening for incoming messages
        } else {
            // EEPROM is populated: user needs to authenticate or clear EEPROM
            if (inputText.equalsIgnoreCase("clear")) {
                clearEEPROM();
                checkEEPROMState();  // Re-prompt after clearing EEPROM
            } else {
                // Authenticate user
                hashObject.update((const uint8_t*)inputText.c_str(), inputText.length());
                hashObject.finalize(hashResult, sizeof(hashResult));

                readHashFromEEPROM(storedHash);

                // // Debug: Print both hashes to compare
                // Serial.print(F("Stored Hash: { "));
                // for (size_t i = 0; i < HASH_SIZE; i++) {
                //     if (i != 0) Serial.print(F(", "));
                //     Serial.print(F("0x"));
                //     if (storedHash[i] < 0x10) Serial.print(F("0"));
                //     Serial.print(storedHash[i], HEX);
                // }
                // Serial.println(F(" };"));

                // Serial.print(F("Entered Hash: { "));
                // for (size_t i = 0; i < HASH_SIZE; i++) {
                //     if (i != 0) Serial.print(F(", "));
                //     Serial.print(F("0x"));
                //     if (hashResult[i] < 0x10) Serial.print(F("0"));
                //     Serial.print(hashResult[i], HEX);
                // }
                // Serial.println(F(" };"));

                if (compareHashes(hashResult, storedHash, HASH_SIZE)) {
                    Serial.println(F("Authentication successful."));
                    Serial.print(F("Current Node Number: "));
                    Serial.println(NODE_NUM);  // Display current node number
                    Serial.println(F("Waiting for message..."));
                    listenForMessages();  // Start listening for incoming messages
                } else {
                    Serial.println(F("Authentication failed."));
                    checkEEPROMState();  // Re-prompt after failed authentication
                }
            }
        }

        delay(500);  // Small delay before next input
    }
}

// Function to prompt for node number
void promptForNodeNumber() {
    while (true) {
        Serial.print(F("Assign node number to this device. 2-99 is valid: "));
        while (!Serial.available());  // Wait for user input
        NODE_NUM = Serial.parseInt();
        if (NODE_NUM >= 2 && NODE_NUM <= 98) {
            writeNodeNumberToEEPROM(NODE_NUM); // Store the node number in EEPROM
            Serial.print(F("Node number assigned: "));
            Serial.println(NODE_NUM);
            break;
        } else {
            Serial.println(F("Invalid node number. Please enter a valid number."));
        }
    }
}

// Function to read the node number from EEPROM
void readNodeNumberFromEEPROM() {
    NODE_NUM = EEPROM.read(NODE_NUM_EEPROM_ADDRESS);
    if (NODE_NUM < 2 || NODE_NUM > 98) {
        NODE_NUM = 0;  // Reset to 0 if the value is invalid
    }
}

// Function to write the node number to EEPROM
void writeNodeNumberToEEPROM(int nodeNum) {
    EEPROM.write(NODE_NUM_EEPROM_ADDRESS, nodeNum);
}

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
        EEPROM.write(i, 0xFF);  // Set all bytes to 0xFF (default unprogrammed state)
    }
    Serial.println(F("EEPROM has been cleared."));
}

// Function to check if EEPROM contains data
bool checkEEPROMForData() {
    for (int i = 0; i < HASH_SIZE; i++) {
        if (EEPROM.read(HASH_EEPROM_START + i) != 0xFF) {
            return true;  // EEPROM has data
        }
    }
    return false;  // EEPROM is empty
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

// Helper function to convert hex string to bytes
void hexStringToBytes(const char* hexString, byte* byteArray, int byteArraySize) {
    for (int i = 0; i < byteArraySize; i++) {
        sscanf(&hexString[i * 2], "%2hhx", &byteArray[i]);
    }
}

// Function to read encryption key from EEPROM
void readKeyFromEEPROM(byte* key) {
    for (int i = 0; i < 32; i++) {
        key[i] = EEPROM.read(i);
    }
}

// Function to check EEPROM state and prompt user accordingly
void checkEEPROMState() {
    if (checkEEPROMForData()) {
        Serial.print(F("EEPROM contains data. Current Node Number: "));
        Serial.println(NODE_NUM);
        Serial.println(F("Please type your password to authenticate or 'clear' to erase EEPROM."));
    } else {
        Serial.println(F("EEPROM is empty. Please enter a seed for your encryption key:"));
    }
}

// Function to listen for incoming messages and decrypt them
void listenForMessages() {
    while (true) {  // Infinite loop to keep listening for messages
        uint8_t buf[MESSAGELENGTH];
        memset(buf, 0, sizeof(buf));
        uint8_t len = sizeof(buf);

        if (rf95.available()) {
            if (rf95.recv(buf, &len)) {
                digitalWrite(LED_PIN, HIGH);  // Indicate reception

                // Ensure we don't exceed the buffer
                if (len >= MESSAGELENGTH) {
                    Serial.println(F("Received message too long!"));
                    digitalWrite(LED_PIN, LOW);
                    continue; // Skip this message
                }

                // Null-terminate the received message
                char receivedStr[MESSAGELENGTH + 1];
                strncpy(receivedStr, (char*)buf, len);
                receivedStr[len] = '\0';

                // Extract fields including the encrypted message and the STOP field
                int SEQ, TYPE, TAGID, RELAY, TTL, RSSI, DEST_NODE, STOP;
                char encryptedHex[128];  // Adjust size as needed

                // Update sscanf format to match the new sender's format with STOP
                int parsed = sscanf(receivedStr, "%d %d %d %d %d %d %d %d %s",
                                    &SEQ, &TYPE, &TAGID, &RELAY, &TTL, &RSSI, &DEST_NODE, &STOP, encryptedHex);

                if (parsed < 9) { // Updated to check for STOP as well
                    Serial.println(F("Received message format incorrect."));
                    digitalWrite(LED_PIN, LOW);
                    continue;
                }

                // Debug print the message fields
                Serial.print(F("Received - Seq: ")); Serial.println(SEQ);
                // Serial.print(F("Type: ")); Serial.println(TYPE);
                // Serial.print(F("Tag: ")); Serial.println(TAGID);
                // Serial.print(F("Relay: ")); Serial.println(RELAY);
                // Serial.print(F("TTL: ")); Serial.println(TTL);
                // Serial.print(F("RSSI: ")); Serial.println(RSSI);
                Serial.print(F("DEST_NODE: ")); Serial.println(DEST_NODE);
                Serial.print(F("STOP: ")); Serial.println(STOP);
                Serial.print(F("Received Encrypted Message (Hex): ")); Serial.println(encryptedHex);

                // Convert hex string back to bytes
                int encryptedLength = strlen(encryptedHex) / 2; // Correct length
                byte encryptedBytes[encryptedLength];
                hexStringToBytes(encryptedHex, encryptedBytes, encryptedLength);

                // Read the key from EEPROM
                byte key[32];
                readKeyFromEEPROM(key);

                // Initialize ChaChaPoly with the key and nonce
                chachaPoly.setKey(key, sizeof(key));
                chachaPoly.setIV(nonce, sizeof(nonce));

                // Decrypt the message
                chachaPoly.decrypt((uint8_t*)decryptedMessage, encryptedBytes, encryptedLength);        

                decryptedMessage[encryptedLength] = '\0';  // Null-terminate

                // Save the decrypted message to EEPROM
                saveDecryptedMessageToEEPROM(decryptedMessage, encryptedLength); // Pass the length

                // Check DEST_NODE and respond accordingly
                if (DEST_NODE == NODE_NUM) {
                    Serial.println(F("Message received for this node."));
                    Serial.print(F("Decrypted Message: "));
                    Serial.println(decryptedMessage);
                } else {
                    Serial.println(F("MESSAGE NOT INTENDED FOR NODE - ACTING AS RELAY"));
                    Serial.println(F("##Message display for assignment purposes only##"));
                    Serial.print(F("Decrypted Message: "));
                    Serial.println(decryptedMessage);

                  // Re-encrypt the entire decrypted message
                    byte ciphertext[encryptedLength]; // Buffer for re-encryption
                    chachaPoly.encrypt(ciphertext, (uint8_t*)decryptedMessage, encryptedLength);

                    // Convert ciphertext to hex string for transmission
                    char hexCiphertext[encryptedLength * 2 + 1]; // +1 for null terminator
                    for (int i = 0; i < encryptedLength; ++i) {
                        sprintf(&hexCiphertext[i * 2], "%02x", ciphertext[i]);
                    }
                    hexCiphertext[encryptedLength * 2] = '\0'; // Null-terminate the string

                    // Transmit the re-encrypted message
                    transmitMessage(hexCiphertext);
                    Serial.println(F("~~Message forwarded~~"));

                
                }

                // If STOP is 1, display the stored message and clear it
                if (STOP == 1) {
                    Serial.println(F("Last message received. Displaying stored message:"));
                    displayStoredMessage(); // Call the function to display the message
                    clearStoredMessage(); // Clear the stored message from EEPROM
                }

                digitalWrite(LED_PIN, LOW);  // Reset LED
                delay(1000);  // Short delay to avoid flooding
            } else {
                Serial.println(F("Failed to receive message."));
            }
        }
        delay(100);  // Small delay to avoid flooding the loop
    }
}

// Function to transmit messages
void transmitMessage(const char* message) {
    uint8_t buf[MESSAGELENGTH];
    snprintf((char*)buf, MESSAGELENGTH, "%s", message);
    
    rf95.setModeIdle(); // Ensure channel is idle
    while (rf95.isChannelActive()) {
        delay(CSMATIME);
    }

    // Transmit the message
    rf95.send(buf, strlen((char*)buf));
    rf95.waitPacketSent();
    Serial.print(F("Forwarding re-encrypted message: (Hex) "));
    Serial.println(message);
}

// Updated function to save decrypted messages to EEPROM
void saveDecryptedMessageToEEPROM(const char* message, int length) {
    int address = DECRYPTED_MESSAGE_EEPROM_START;
    
    // Check if the EEPROM at the starting position is filled with 0xFF
    bool isEmpty = true;
    for (int i = 0; i < MAX_DECRYPTED_MESSAGE_LENGTH; i++) {
        if (EEPROM.read(address + i) != 0xFF) {
            isEmpty = false; // Found a value other than 0xFF
            break;
        }
    }

    // If EEPROM is empty (filled with 0xFF), write the message at the start
    if (isEmpty) {
        for (int i = 0; i < length && (address < (DECRYPTED_MESSAGE_EEPROM_START + MAX_DECRYPTED_MESSAGE_LENGTH)); i++) {
            EEPROM.write(address++, message[i]);
        }
        // Null-terminate the stored message
        EEPROM.write(address, '\0');
    } else {
        // Find the end of the previous message in EEPROM
        while (address < (DECRYPTED_MESSAGE_EEPROM_START + MAX_DECRYPTED_MESSAGE_LENGTH)) {
            if (EEPROM.read(address) == '\0') {
                break; // Stop if a null terminator is found
            }
            address++; // Move to the next address
        }

        // Ensure we don't exceed the maximum allowed length
        for (int i = 0; i < length && (address < (DECRYPTED_MESSAGE_EEPROM_START + MAX_DECRYPTED_MESSAGE_LENGTH)); i++) {
            EEPROM.write(address++, message[i]);
        }
        // Null-terminate the stored message
        EEPROM.write(address, '\0');
    }
}

// New function to display the stored message
void displayStoredMessage() {
    char storedMessage[MAX_DECRYPTED_MESSAGE_LENGTH];
    int i = 0;

    // Read the stored message from EEPROM
    while (i < MAX_DECRYPTED_MESSAGE_LENGTH) {
        storedMessage[i] = EEPROM.read(DECRYPTED_MESSAGE_EEPROM_START + i);
        if (storedMessage[i] == '\0') break; // Stop at null terminator
        i++;
    }
    storedMessage[i] = '\0'; // Ensure the message is null-terminated

    Serial.print(F("Stored Message: "));
    Serial.println(storedMessage);
}

// New function to clear only the stored decrypted messages in EEPROM
void clearStoredMessage() {
    for (int i = DECRYPTED_MESSAGE_EEPROM_START; i < DECRYPTED_MESSAGE_EEPROM_START + MAX_DECRYPTED_MESSAGE_LENGTH; i++) {
        EEPROM.write(i, 0xFF); // Set each byte to 0xFF to indicate empty
    }
    Serial.println(F("Stored decrypted messages have been cleared."));
}
