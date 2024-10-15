#include <EEPROM.h>
#include <SPI.h>
#include <RH_RF95.h>
#include <ChaChaPoly.h>
#include <Crypto.h>
#include <BLAKE2s.h>

// EEPROM Configuration
#define HASH_EEPROM_START 0
#define NODE_NUM_EEPROM_ADDRESS 64  // Location for where node number is stored
#define HASH_SIZE 32 // Size of the hash. 32 bytes.
#define MESSAGELENGTH 83  // Value seems to not overallocate RAM. Anything above 83 leads to some sort of memory overflow. 

// Decrypted string configuration 
#define DECRYPTED_MESSAGE_EEPROM_START 128 // Start location for where decrypted string is stored on EEPROM
#define MAX_DECRYPTED_MESSAGE_LENGTH 128 // Max decrypted string length

// LoRa Transmission settings
#define RF95_FREQ 915.0 // Frequency LoRa operates on in Australia. Reserved frequency. 
#define TXPOWER 23 // Transmission power. Seems to work. 25 is the max. Don't want to push the transmitter too hard. 
#define BANDWIDTH 500000 // 500kHz. Was set beforehand in startup code given.
#define SPREADING_FACTOR 12 // Max range for short messages. Ideal for mining scenario. (7- 12 range) Less power, less data, longer distance, better resistance to interference. 
#define CSMATIME 10 // 10 ms gap upon sensing medium is clear to send. 

// Singleton instance of radio driver 
RH_RF95 rf95;

const uint8_t LED_PIN = 13; // Sets LED to correct pin

// 12 Byte static Nonce
const byte nonce[12] = {
    0x12, 0x34, 0x56, 0x78,
    0x90, 0xab, 0xcd, 0xef,
    0x12, 0x34, 0x56, 0x78
};

// Initializes ChaChaPoly session
ChaChaPoly chachaPoly;

char encryptedMessageHex[128]; // Holds encrypted message ciphertext
char decryptedMessage[MAX_DECRYPTED_MESSAGE_LENGTH]; // Holds decrypted message

int NODE_NUM = 0;  // Initialize and assign NODE_NUM variable

// Function prototypes
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
    pinMode(LED_PIN, OUTPUT); // Set LED up so we can use it
    Serial.begin(9600); // Initialize serial connection on 9600 baud

    // Read node number from storage
    readNodeNumberFromEEPROM();

    // Initialize LoRa
    if (!rf95.init()) {
        Serial.println(F("LoRa receiver initialization failed"));
        while (1); // Stop if LoRa board communication does not exist
    }

    // Set pre-defined parameters to the LoRa transmitter
    rf95.setFrequency(RF95_FREQ);
    rf95.setTxPower(TXPOWER, false);
    rf95.setSignalBandwidth(BANDWIDTH);
    rf95.setSpreadingFactor(SPREADING_FACTOR);

    // Check EEPROM
    checkEEPROMState();
}

void loop() {
    // Check for user input
    if (Serial.available() > 0) { // If serial connection is working, run below
        String inputText = Serial.readStringUntil('\n'); // Take input from user and null terminate string
        inputText.trim();  // Remove whitespace just in case

        // Initialize BLAKE2s hash object
        BLAKE2s hashObject;
        uint8_t hashResult[HASH_SIZE]; // Set hashResult array to correct size and set each index to hold unsigned integer type of 8 bits. Forces everything in it to be a binary value
        uint8_t storedHash[HASH_SIZE]; // Same as above

        if (!checkEEPROMForData()) { // If checkEEPROMForData is false (all 0's) do below
            // EEPROM is empty: user needs to set encryption key via password
            hashObject.update((const uint8_t*)inputText.c_str(), inputText.length()); // Feed user input into hashing algorithm
            hashObject.finalize(hashResult, sizeof(hashResult)); // Runs hashing function and stores result in hashResult

            writeHashToEEPROM(hashResult); // Write result to EEPROM
            Serial.println(F("Encryption key seed stored in EEPROM.")); // Indicate to user hash has been written

            // Prompt for node number
            promptForNodeNumber();

            Serial.print(F("Current Node Number: ")); // Tells user current node Number
            Serial.println(NODE_NUM);  // Display current node number stored in EEPROM
            Serial.println(F("Waiting for message...")); // Prompts user Arduino is waiting to receive a message
            listenForMessages();  // Wait for incoming messages
        } else { // If EEPROM is populated do below
            if (inputText.equalsIgnoreCase("clear")) { // If enters 'clear'
                clearEEPROM(); // Clear EEPROM
                checkEEPROMState();  // Double-check it is actually clear before continuing and print EEPROM is full/clear to user
            } else {
                // Authenticate user
                hashObject.update((const uint8_t*)inputText.c_str(), inputText.length()); // Update hash object with user input
                hashObject.finalize(hashResult, sizeof(hashResult)); // Run hash object and store output in hashResult

                readHashFromEEPROM(storedHash); // Read storedHash and update the hashResult function

                if (compareHashes(hashResult, storedHash, HASH_SIZE)) { // If authentication is successful
                    Serial.println(F("Authentication successful."));
                    Serial.print(F("Current Node Number: "));
                    Serial.println(NODE_NUM);  // Display current node number
                    Serial.println(F("Waiting for message..."));
                    listenForMessages();  // Start listening for incoming messages
                } else { // If compareHashes is false then run below code
                    Serial.println(F("Authentication failed."));
                    checkEEPROMState();  // Re-prompt after failed authentication
                }
            }
        }

        delay(500);  // Small delay before next input
    }
}

// Prompts for node number
void promptForNodeNumber() {
    while (true) {
        Serial.print(F("Assign node number to this device. 2-99 is valid: "));
        while (!Serial.available());  // Only run if serial is working
        NODE_NUM = Serial.parseInt(); // Take in an integer value from user and write to NODE_NUM
        if (NODE_NUM >= 2 && NODE_NUM <= 99) {
            writeNodeNumberToEEPROM(NODE_NUM); // Store the node number in EEPROM
            Serial.print(F("Node number assigned: "));
            Serial.println(NODE_NUM); // Prints actual node number from EEPROM
            break; 
        } else { // If entered node number is out of range prompt user with below
            Serial.println(F("Invalid node number. Please enter a valid number."));
        }
    }
}

// Function to read the node number from EEPROM
void readNodeNumberFromEEPROM() {
    NODE_NUM = EEPROM.read(NODE_NUM_EEPROM_ADDRESS); // Read EEPROM from the node_num location
    if (NODE_NUM < 2 || NODE_NUM > 99) {
        NODE_NUM = 0;  // Reset to 0 if the value is invalid
    }
}

// Write node number to EEPROM
void writeNodeNumberToEEPROM(int nodeNum) {
    EEPROM.write(NODE_NUM_EEPROM_ADDRESS, nodeNum);
}

// Write hash to EEPROM
void writeHashToEEPROM(uint8_t* hashResult) {
    for (int i = 0; i < HASH_SIZE; i++) {
        EEPROM.write(HASH_EEPROM_START + i, hashResult[i]);
    }
}

// Read the stored hash from EEPROM
void readHashFromEEPROM(uint8_t* hashResult) {
    for (int i = 0; i < HASH_SIZE; i++) {
        hashResult[i] = EEPROM.read(HASH_EEPROM_START + i);
    }
}

// Clear entire EEPROM
void clearEEPROM() {
    for (int i = 0; i < EEPROM.length(); i++) {
        EEPROM.write(i, 0xFF);  // Set all bytes to 0 in EEPROM
    }
    Serial.println(F("EEPROM has been cleared."));
}

// Check if EEPROM contains data
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
        if (hash1[i] != hash2[i]) { // If any value in either array is not equal return false
            return false;
        }
    }
    return true; // If all values are equal then return true
}

// Convert hex string to bytes
void hexStringToBytes(const char* hexString, byte* byteArray, int byteArraySize) { // Convert string to hex then hex to bytes in byteArray for storage.
    for (int i = 0; i < byteArraySize; i++) {
        sscanf(&hexString[i * 2], "%2hhx", &byteArray[i]);
    }
}

// Read encryption key from EEPROM
void readKeyFromEEPROM(byte* key) { // Pass in pointer to memory location of key
    for (int i = 0; i < 32; i++) {
        key[i] = EEPROM.read(i); // Read from memory and store encryption key in key array.
    }
}

// Check EEPROM state and prompt user
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
        uint8_t buf[MESSAGELENGTH]; // Buffer for the incoming message
        memset(buf, 0, sizeof(buf)); // Allocate memory for buffer on stack
        uint8_t len = sizeof(buf); // Length = size of buffer

        if (rf95.available()) { // If LoRa is present
            if (rf95.recv(buf, &len)) { // If receiving store contents into buf
                digitalWrite(LED_PIN, HIGH);  // Set LED to HIGH

                // Check message isn't too long that it exceeds buffer memory space
                if (len >= MESSAGELENGTH) {
                    Serial.println(F("Received message too long!"));
                    digitalWrite(LED_PIN, LOW);
                    continue; // Skip this message
                }

                // Null-terminate received message so compiler knows where the end of the string is
                char receivedStr[MESSAGELENGTH + 1];
                strncpy(receivedStr, (char*)buf, len); // Write buffer contents to receivedStr 
                receivedStr[len] = '\0';

                // Extract fields, encrypted message and the STOP field
                int SEQ, TYPE, TAGID, RELAY, S_NODE, RSSI, DEST_NODE, STOP;
                char encryptedHex[128]; // Array for encrypted message to be received

                // Extract plain text messages 
                int parsed = sscanf(receivedStr, "%d %d %d %d %d %d %d %d %s",
                                    &SEQ, &TYPE, &TAGID, &RELAY, &S_NODE, &RSSI, &DEST_NODE, &STOP, encryptedHex); 

                if (parsed < 9) { // All 9 'sections' must be received. If not do below
                    Serial.println(F("Received message format incorrect."));
                    digitalWrite(LED_PIN, LOW);
                    continue;
                }

                 // Check if S_NODE is 1
                if (S_NODE == 1) {
                Serial.println(F("Message received from NODE 1"));


                // Debug print the message fields
                // Serial.print(F("Type: ")); Serial.println(TYPE);
                // Serial.print(F("Tag: ")); Serial.println(TAGID);
                // Serial.print(F("Relay: ")); Serial.println(RELAY);
                Serial.print(F("S_NODE: ")); Serial.println(S_NODE);
                // Serial.print(F("RSSI: ")); Serial.println(RSSI);
                Serial.print(F("Received - Seq: ")); Serial.println(SEQ); // Print sequence number
                Serial.print(F("DEST_NODE: ")); Serial.println(DEST_NODE); // Print destination node number
                Serial.print(F("STOP: ")); Serial.println(STOP); // Print STOP flag
                Serial.print(F("Received Encrypted Message (Hex): ")); Serial.println(encryptedHex); 

                // Convert hex string back to bytes
                int encryptedLength = strlen(encryptedHex) / 2;
                byte encryptedBytes[encryptedLength];
                hexStringToBytes(encryptedHex, encryptedBytes, encryptedLength);

                // Read the stored encryption key from EEPROM
                byte key[32];
                readKeyFromEEPROM(key);

                // Initialize ChaChaPoly with the key and nonce
                chachaPoly.setKey(key, sizeof(key)); // Set key
                chachaPoly.setIV(nonce, sizeof(nonce)); // Set nonce

                // Extract the last 16 bytes as the tag from the received message
                byte receivedTag[16];
                memcpy(receivedTag, encryptedBytes + encryptedLength - 16, 16); // Get the last 16 bytes for the tag

                // Decrypt the message (without the last 16 bytes, as those are for the tag)
                chachaPoly.decrypt((uint8_t*)decryptedMessage, encryptedBytes, encryptedLength - 16);
                decryptedMessage[encryptedLength - 16] = '\0'; // Null-terminate decrypted message

                // Verify the tag
                byte computedTag[16];
                chachaPoly.computeTag(computedTag, sizeof(computedTag)); // Compute the tag for the decrypted message


                // Check if the computed tag matches the received tag
                if (memcmp(receivedTag, computedTag, sizeof(computedTag)) != 0) {
                    Serial.println(F("CORRUPTED MESSAGE"));
                } else {
                    // Save the decrypted message to EEPROM
                    saveDecryptedMessageToEEPROM(decryptedMessage, encryptedLength - 16); // Pass the length without the tag

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

                    // Clear decrypted message from memory after use
                    memset(decryptedMessage, 0, sizeof(decryptedMessage));

                    // Convert ciphertext to hex string for transmission (reverse of above decrypted text storage process)
                    char hexCiphertext[encryptedLength * 2 + 1]; // +1 for null terminator
                    for (int i = 0; i < encryptedLength; ++i) {
                        sprintf(&hexCiphertext[i * 2], "%02x", ciphertext[i]); //convert each element in ciphertext byte array into hex string (AA -> 0xAA, easier to debug)
                    }

                    hexCiphertext[encryptedLength * 2] = '\0'; // Null-terminate the string

                    // Transmit the re-encrypted message
                    transmitMessage(hexCiphertext);
                    Serial.println(F("~Message forwarded~"));

                    }

                    // If STOP is 1, display the stored message and clear it
                    if (STOP == 1) {
                        Serial.println(F("Last message received. Displaying stored message:"));
                        displayStoredMessage(); // Call the function to display the message
                        clearStoredMessage(); // Clear the stored message from EEPROM
                    }
                }
                
              } else {
                Serial.println(F("!MESSAGE SENT FROM UNAUTHORIZED SOURCE!"));

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
    
    rf95.setModeIdle(); // Set transceiver to idle state. 
    while (rf95.isChannelActive()) { //if channel is busy run below code which adds a delay before transmission occurs
        delay(CSMATIME); 
    }

    // Transmit the message
    rf95.send(buf, strlen((char*)buf)); //sends message
    rf95.waitPacketSent(); //waits for transmission to be complete
    Serial.print(F("Forwarding re-encrypted message: (Hex) ")); //prints to user
    Serial.println(message); //prints the message that was sent
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

    // If EEPROM is empty (filled with 0xFF), write the message at the start of DECRYPTED_MESSAGE_EEPROM_START
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

        // Ensure don't exceed the maximum allowed length
        for (int i = 0; i < length && (address < (DECRYPTED_MESSAGE_EEPROM_START + MAX_DECRYPTED_MESSAGE_LENGTH)); i++) {
            EEPROM.write(address++, message[i]);
        }
        // Null-terminate the stored message
        EEPROM.write(address, '\0');
    }
}

//  display the stored message
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

    Serial.print(F("Stored Message: ")); //print
    Serial.println(storedMessage); //print stored message. Should be call received string segments in one. 
}

// New function to clear only the stored decrypted messages in EEPROM
void clearStoredMessage() {
    for (int i = DECRYPTED_MESSAGE_EEPROM_START; i < DECRYPTED_MESSAGE_EEPROM_START + MAX_DECRYPTED_MESSAGE_LENGTH; i++) {
        EEPROM.write(i, 0xFF); // Set each byte to 0xFF to indicate empty
    }
    Serial.println(F("Stored decrypted messages have been cleared.")); //print to user
}