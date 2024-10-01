#include <EEPROM.h>
#include <SPI.h>
#include <RH_RF95.h>
#include <ChaChaPoly.h>
#include <Crypto.h>
#include <BLAKE2s.h>

// EEPROM Configuration
#define HASH_EEPROM_START 0
#define HASH_SIZE 32
#define MESSAGELENGTH 64  // Must match the transmitter's MESSAGELENGTH

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
char decryptedMessage[128];

// Function Prototypes
void writeHashToEEPROM(uint8_t* hashResult);
void readHashFromEEPROM(uint8_t* hashResult);
void clearEEPROM();
bool checkEEPROMForData();
bool compareHashes(uint8_t* hash1, uint8_t* hash2, size_t length);
void hexStringToBytes(const char* hexString, byte* byteArray, int byteArraySize);
void listenForMessages();
void readKeyFromEEPROM(byte* key);
void checkEEPROMState();

void setup() {
  // Initialize Serial and LED
  pinMode(LED_PIN, OUTPUT);
  Serial.begin(9600);
  Serial.println(F("Receiver Version 1"));

  // Initialize LoRa
  if (!rf95.init()) {
    Serial.println(F("LoRa receiver initialization failed"));
    while (1);  // Halt if LoRa fails to initialize
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

      // Debug: Print the stored hash
      Serial.print(F("Stored seed hash as 32-byte hex array: { "));
      for (size_t i = 0; i < HASH_SIZE; i++) {
        if (i != 0) Serial.print(F(", "));
        Serial.print(F("0x"));
        if (hashResult[i] < 0x10) Serial.print(F("0"));
        Serial.print(hashResult[i], HEX);
      }
      Serial.println(F(" };"));

      // Prompt to wait for messages
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

        // Debug: Print both hashes to compare
        Serial.print(F("Stored Hash: { "));
        for (size_t i = 0; i < HASH_SIZE; i++) {
          if (i != 0) Serial.print(F(", "));
          Serial.print(F("0x"));
          if (storedHash[i] < 0x10) Serial.print(F("0"));
          Serial.print(storedHash[i], HEX);
        }
        Serial.println(F(" };"));

        Serial.print(F("Entered Hash: { "));
        for (size_t i = 0; i < HASH_SIZE; i++) {
          if (i != 0) Serial.print(F(", "));
          Serial.print(F("0x"));
          if (hashResult[i] < 0x10) Serial.print(F("0"));
          Serial.print(hashResult[i], HEX);
        }
        Serial.println(F(" };"));

        if (compareHashes(hashResult, storedHash, HASH_SIZE)) {
          Serial.println(F("Authentication successful."));
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
    Serial.println(F("EEPROM contains data. Please type your password to authenticate or 'clear' to erase EEPROM."));
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

        // Print the length of the received message
        Serial.print(F("Length of Received Message: "));
        Serial.println(len);

        // Null-terminate the received message
        char receivedStr[MESSAGELENGTH + 1];
        strncpy(receivedStr, (char*)buf, len);
        receivedStr[len] = '\0';

        // Print the raw received message
        Serial.print(F("Raw Received Message: "));
        Serial.println(receivedStr);

        // Extract fields including the encrypted message
        // Expected format: "%d %d %d %d %d %d %s"
        int SEQ, TYPE, TAGID, RELAY, TTL, RSSI;
        char encryptedHex[128];  // Adjust size as needed

        int parsed = sscanf(receivedStr, "%d %d %d %d %d %d %s",
                            &SEQ, &TYPE, &TAGID, &RELAY, &TTL, &RSSI, encryptedHex);

        // Print the expected message format
        Serial.println(F("Expected message format: <SEQ> <TYPE> <TAGID> <RELAY> <TTL> <RSSI> <EncryptedHex>"));

        if (parsed < 7) {
          Serial.println(F("Received message format incorrect."));
          digitalWrite(LED_PIN, LOW);
          continue;
        }

        Serial.print(F("Seq: ")); Serial.println(SEQ);
        Serial.print(F("Type: ")); Serial.println(TYPE);
        Serial.print(F("Tag: ")); Serial.println(TAGID);
        Serial.print(F("Relay: ")); Serial.println(RELAY);
        Serial.print(F("TTL: ")); Serial.println(TTL);
        Serial.print(F("RSSI: ")); Serial.println(RSSI);
        Serial.print(F("Encrypted Message (Hex): ")); Serial.println(encryptedHex);

        // Convert hex string back to bytes
        int encryptedLength = strlen(encryptedHex) / 2;
        byte encryptedBytes[MESSAGELENGTH];
        memset(encryptedBytes, 0, sizeof(encryptedBytes));
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

        // Print the decrypted message
        Serial.print(F("Decrypted Message: "));
        Serial.println(decryptedMessage);

        digitalWrite(LED_PIN, LOW);  // Reset LED
        delay(1000);  // Short delay to avoid flooding
      }
    }
  }
}
