# LoRaProject

## Overview

### This project enables secure LoRa communication between Arduino-based transmitters and receivers with message encryption, chunking, and authentication.
#### Components

- Transmitter:
    - Authentication: Prompts for password on boot. If EEPROM is empty, stores hashed password. Otherwise, verifies against stored hash or clears EEPROM.
    - Message Encryption: Splits message into 2-character chunks, encrypts each with ChaCha using a static nonce and hashed password, and adds a MAC for integrity.

- Receiver/Relay:
    - Setup & Authentication: Prompts for device node number and verifies incoming messages.
    - Modes: If the destination matches the receiver’s node, displays the message. Otherwise, forwards it in relay mode.

#### Team Roles

- Encryption, Decryption, Node Functionality: Timo and Jackson.
- Message Authentication, Memory Management, Chunking: Ronnie and Luke.
- Project Leader: Jackson
- Arduino Interface: Gajen

### Quick Setup Instructions

#### Hardware Setup:
- Transmitter: Load the Transmitter code onto an Arduino with a LoRa shield.
- Receiver/Relay: Load the Receiver/Relay code onto another Arduino with a LoRa shield.

 #### Required Libraries:
- Crypto: Provides Arduiono optimized ChaChaPoly encryption and Blake2s hashing.
- RH_RF95.h (Radiohead): Interfaces with the LoRa shield.
- SPI.h: Enables serial communication with the LoRa shield.
- EEPROM.h: Manages EEPROM storage for password storage and verification.

#### Initial Setup:
- Transmitter: On first boot, if EEPROM is empty, the Transmitter will prompt for a password, hash it with Blake2s, and store it in EEPROM.
- Receiver/Relay: On startup, the Receiver will prompt the user to set a node number and authenticate.

#### Usage:
- Transmitter: Enter the destination node and a message (up to 50 characters). The message is split into chunks, encrypted, and sent to the specified node.
- Receiver/Relay: The Receiver checks for valid source and destination nodes, decrypts and reassembles the message. If the message is intended for its node, it displays it; otherwise, it forwards the message in relay mode.

#### Code Differences:
- The Receiver/Relay includes additional code for message reception, reassembly, and forwarding.
