#include <SPI.h>
#include <RH_RF95.h>
#include <Arduino.h>
#include <ChaChaPoly.h>

// Singleton instance of radio driver
RH_RF95 rf95;
int led = 13; // Define LED pin

// Message fields
int SEQ = 0;     // Sequence number
int TYPE = 0;    // Message type
int TAGID = 1;   // Identity of tag
int RELAYID = 0; // Relay ID
int TTL = 5;     // Time to live
int thisRSSI = 0;
int firstIteration = 0; // First iteration flag
uint8_t hashValue[32] = {
    0x32, 0x7E, 0x7E, 0x38, 0x21, 0xF5, 0xF6, 0xD3,
    0x3C, 0x09, 0x01, 0x37, 0xF9, 0x79, 0xBF, 0x48,
    0xEE, 0x62, 0xE9, 0x05, 0x1C, 0x16, 0x10, 0xE1,
    0xD6, 0x46, 0x8E, 0xCB, 0x3C, 0x67, 0xA1, 0x24
};
// password = "admin"

// Encryption details
const char plaintext[] = "Hello, World!";
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
ChaChaPoly chachaPoly;
byte ciphertext[sizeof(plaintext)];  // Buffer to hold encrypted data
byte tag[16]; // Buffer for authentication tag

// Declare global variable for hex representation
char hexCiphertext[sizeof(ciphertext) * 2 + 1];  // +1 for null terminator

// Define buffer length
#define MESSAGELENGTH 128
#define TXINTERVAL 5000
#define CSMATIME 10

void setup() {
  // Initialize LoRa transceiver and encryption
  pinMode(led, OUTPUT);
  Serial.begin(9600);
  Serial.println("Tag version 1");

  if (!rf95.init()) {
    Serial.println("LoRa initialization failed");
    while (1); // Stop execution
  }

  rf95.setFrequency(915.0);
  rf95.setTxPower(5, false);
  rf95.setSignalBandwidth(500000);
  rf95.setSpreadingFactor(12);

  // Initialize ChaChaPoly
  chachaPoly.setKey(key, sizeof(key));
  chachaPoly.setIV(nonce, sizeof(nonce));

  // Encrypt the plaintext
  chachaPoly.encrypt(ciphertext, reinterpret_cast<const uint8_t*>(plaintext), sizeof(plaintext) - 1);

  // Compute the authentication tag
  chachaPoly.computeTag(tag, sizeof(tag));

  // Convert ciphertext to hex string for the message
  for (size_t i = 0; i < sizeof(ciphertext); ++i) {
    sprintf(&hexCiphertext[i * 2], "%02x", ciphertext[i]);
  }
  hexCiphertext[sizeof(ciphertext) * 2] = '\0'; // Null-terminate the string

  
}

void loop() {
  // Generate message intermittently (10 seconds)
  uint8_t buf[MESSAGELENGTH];

  if (firstIteration == 0) {
    TYPE = 9;
    firstIteration = 1;
  } else {
    TYPE = 0;
  }

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
    Serial.println("Tag node looping on isChannelActive()"); // DEBUG
  }

  // Transmit the message
  Serial.print("Transmitted message: ");
  Serial.println((char*)buf); // DEBUG
  rf95.send(buf, strlen((char*)buf));
  rf95.waitPacketSent();
  delay(TXINTERVAL);

  //This is the current code revision
}
