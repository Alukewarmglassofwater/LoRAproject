#include <Arduino.h>
#include <ChaChaPoly.h>  // Include the ChaChaPoly library

// Define a 256-bit key (32 bytes)
byte key[32] = {
  0x4f, 0xf6, 0x3b, 0x2c, 0x1a, 0x45, 0x89, 0x6d,
  0x7e, 0x9c, 0x00, 0x6f, 0x37, 0x11, 0x8a, 0x5b,
  0x9d, 0x74, 0x6e, 0xb2, 0x3c, 0xf0, 0x1d, 0x97,
  0x88, 0x6e, 0x3f, 0x52, 0xc8, 0xa0, 0x1d
};

// Define a 12-byte nonce
byte nonce[12] = {
  0x12, 0x34, 0x56, 0x78, 0x90, 0xab, 0xcd, 0xef,
  0x12, 0x34, 0x56, 0x78
};

// Create an instance of ChaChaPoly
ChaChaPoly chachaPoly;

// Define plaintext and buffers for encryption and decryption
const char plaintext[] = "Hello, World!";
byte ciphertext[sizeof(plaintext)];  // Buffer to hold encrypted data
byte decrypted[sizeof(plaintext)];   // Buffer to hold decrypted data
byte tag[16];  // Buffer for authentication tag

void setup() {
  // Initialize Serial for debugging
  Serial.begin(9600);
  
  // Set up the ChaChaPoly object
  chachaPoly.setKey(key, sizeof(key));
  chachaPoly.setIV(nonce, sizeof(nonce));
  
  // Encrypt the plaintext
  chachaPoly.encrypt(ciphertext, reinterpret_cast<const uint8_t*>(plaintext), sizeof(plaintext) - 1);
  
  // Print the ciphertext in hexadecimal
  Serial.print("Ciphertext: ");
  for (size_t i = 0; i < sizeof(ciphertext); i++) {
    Serial.print(ciphertext[i], HEX);
    Serial.print(" ");
  }
  Serial.println();
  
  // Compute the authentication tag
  chachaPoly.computeTag(tag, sizeof(tag));
  
  // Print the tag in hexadecimal
  Serial.print("Tag: ");
  for (size_t i = 0; i < sizeof(tag); i++) {
    Serial.print(tag[i], HEX);
    Serial.print(" ");
  }
  Serial.println();
  
  // Set up the ChaChaPoly object for decryption
  chachaPoly.setKey(key, sizeof(key));
  chachaPoly.setIV(nonce, sizeof(nonce));
  
  // Decrypt the ciphertext
  chachaPoly.decrypt(reinterpret_cast<uint8_t*>(decrypted), ciphertext, sizeof(ciphertext));
  
  // Null-terminate the decrypted data for printing
  decrypted[sizeof(plaintext) - 1] = '\0';
  
  // Print the decrypted text
  Serial.print("Decrypted: ");
  Serial.println(reinterpret_cast<char*>(decrypted));
}

void loop() {
  // Nothing to do here
}
