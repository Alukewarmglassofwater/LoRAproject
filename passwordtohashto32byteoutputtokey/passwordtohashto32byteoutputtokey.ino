#include <EEPROM.h>
#include <Crypto.h>    // Include the Crypto library
#include <BLAKE2s.h>   // Include the BLAKE2s implementation

// Define the secret key (for example, a 32-byte key)
const byte secretKey[32] = {
  0x4f, 0xf6, 0x3b, 0x2c, 0x1a, 0x45, 0x89, 0x6d,
  0x7e, 0x9c, 0x00, 0x6f, 0x37, 0x11, 0x8a, 0x5b,
  0x9d, 0x74, 0x6e, 0xb2, 0x3c, 0xf0, 0x1d, 0x97,
  0x88, 0x6e, 0x3f, 0x52, 0xc8, 0xa0, 0x1d
};

// Function to store the secret key in EEPROM
void saveSecretKeyToEEPROM() {
  for (int i = 0; i < 32; i++) {
    EEPROM.write(i, secretKey[i]);  // Write each byte to EEPROM
  }
  //EEPROM.commit();  // Save changes
}

// Function to read the secret key from EEPROM
void readSecretKeyFromEEPROM(byte* key) {
  for (int i = 0; i < 32; i++) {
    key[i] = EEPROM.read(i);  // Read each byte from EEPROM
  }
}

// Function to obfuscate the hash output with the key (XOR method)
void obfuscateHash(uint8_t* hash, byte* key, size_t length) {
  for (size_t i = 0; i < length; i++) {
    hash[i] ^= key[i];  // XOR each byte of the hash with the key
  }
}

void setup() {
  // Initialize serial communication
  Serial.begin(9600);

  // Initialize EEPROM with a size of 512 bytes (on some Arduino models)
  EEPROM.begin(512);

  // Store the secret key in EEPROM (this would typically be done once)
  saveSecretKeyToEEPROM();

  // Read the secret key from EEPROM
  byte retrievedKey[32];
  readSecretKeyFromEEPROM(retrievedKey);

  // Display the retrieved key (for debugging purposes)
  Serial.println("Retrieved key from EEPROM:");
  for (size_t i = 0; i < 32; i++) {
    if (retrievedKey[i] < 16) {
      Serial.print('0');  // Add leading zero
    }
    Serial.print(retrievedKey[i], HEX);
    Serial.print(" ");
  }
  Serial.println();

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

  // Obfuscate the hash output using the retrieved key from EEPROM
  obfuscateHash(hashResult, retrievedKey, sizeof(hashResult));

  // Print the obfuscated hash
  Serial.println("Obfuscated BLAKE2s hash:");
  for (size_t i = 0; i < sizeof(hashResult); i++) {
    if (hashResult[i] < 16) {
      Serial.print('0');
    }
    Serial.print(hashResult[i], HEX);
    Serial.print(" ");
  }
  Serial.println();
}

void loop() {
  // Nothing to do here
}
