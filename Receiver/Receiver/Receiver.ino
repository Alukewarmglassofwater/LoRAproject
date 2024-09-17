#include <SPI.h>
#include <RH_RF95.h>
#include <ChaChaPoly.h>  // Include the ChaChaPoly library

// Singleton instance of radio driver
RH_RF95 rf95;
int led = 13;   // Define LED pin
int SEQ, TAGID, TTL, thisRSSI, RELAYID = 1, RELAY, TYPE;

// Encryption details (replace with your actual key and IV)
byte key[32] = {
  0x4f, 0xf6, 0x3b, 0x2c, 0x1a, 0x45, 0x89, 0x6d,
  0x7e, 0x9c, 0x00, 0x6f, 0x37, 0x11, 0x8a, 0x5b,
  0x9d, 0x74, 0x6e, 0xb2, 0x3c, 0xf0, 0x1d, 0x97,
  0x88, 0x6e, 0x3f, 0x52, 0xc8, 0xa0, 0x1d
};
byte nonce[12] = {
  0x12, 0x34, 0x56, 0x78, 0x90, 0xab, 0xcd, 0xef,
  0x12, 0x34, 0x56, 0x78
};
byte tag[16];  // Placeholder for authentication tag

// Initialize ChaChaPoly
ChaChaPoly chachaPoly;

// Helper function to convert hex string to bytes
void hexStringToBytes(const char* hexString, byte* byteArray, int byteArraySize) {
  for (int i = 0; i < byteArraySize; i++) {
    sscanf(&hexString[i * 2], "%2hhx", &byteArray[i]);  // Convert each pair of hex chars to a byte
  }
}

void setup() {
  pinMode(led, OUTPUT);
  Serial.begin(9600);
  Serial.println("Receiver Version 1");

  while (!Serial) {
    Serial.println("Waiting for serial port");
  }

  while (!rf95.init()) {
    Serial.println("Initialization of LoRa receiver failed");
    delay(1000);
  }

  rf95.setFrequency(915.0);
  rf95.setTxPower(23, false);
  rf95.setSignalBandwidth(500000);
  rf95.setSpreadingFactor(12);
}

void loop() {
  uint8_t buf[128];  // Larger buffer to handle incoming message
  memset(buf, 0, sizeof(buf));  // Clear the buffer before using
  uint8_t len = sizeof(buf);

  char encryptedMessageHex[128];  // Buffer for hex-encoded encrypted message
  char decryptedMessage[128];  // Buffer to store decrypted message

  if (rf95.available()) {
    if (rf95.recv(buf, &len)) {
      digitalWrite(led, HIGH);

      // Unpack the message
      char str[128];  // Adjust size as needed
      strncpy(str, (char*)buf, len);
      str[len] = '\0';  // Null-terminate

      // Extract the fields (including the hex-encoded encrypted message)
      sscanf(str, "%5d %5d %5d %5d %5d %5d %s", &SEQ, &TYPE, &TAGID, &RELAY, &TTL, &thisRSSI, encryptedMessageHex);

      Serial.print("Seq: "); Serial.println(SEQ);
      Serial.print("Type: "); Serial.println(TYPE);
      Serial.print("Tag: "); Serial.println(TAGID);
      Serial.print("Relay: "); Serial.println(RELAY);
      Serial.print("TTL: "); Serial.println(TTL);
      Serial.print("RSSI: "); Serial.println(thisRSSI);
      Serial.print("Encrypted Message (Hex): "); Serial.println(encryptedMessageHex);

      // Convert hex-encoded encrypted message back to bytes
      byte encryptedMessage[64];  // Adjust size based on expected encrypted message length
      memset(encryptedMessage, 0, sizeof(encryptedMessage));
      memset(decryptedMessage, 0, sizeof(decryptedMessage));  // Clearing buffers
      int encryptedMessageLength = strlen(encryptedMessageHex) / 2;  // Each hex char represents half a byte
      hexStringToBytes(encryptedMessageHex, encryptedMessage, encryptedMessageLength);

        // Initialize ChaChaPoly with key and nonce
      chachaPoly.setKey(key, sizeof(key));
      chachaPoly.setIV(nonce, sizeof(nonce));  // Nonce stays the same each time for consistent decryption

      // Decrypt the message
      chachaPoly.decrypt(decryptedMessage, encryptedMessage, encryptedMessageLength);

      // Ensure null termination for decrypted message
      decryptedMessage[encryptedMessageLength] = '\0';  // Null-terminate the decrypted message

      // Print the decrypted message
      Serial.print("Decrypted Message: ");
      Serial.println(decryptedMessage);

      delay(10000);  // Delay to avoid flooding serial output
    }
    digitalWrite(led, LOW);
  }
}
