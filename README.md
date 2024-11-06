# LoRaProject

## Overview

# This project enables secure LoRa communication between Arduino-based transmitters and receivers with message encryption, chunking, and authentication.
# Components

    - Transmitter:
        - Authentication: Prompts for password on boot. If EEPROM is empty, stores hashed password. Otherwise, verifies against stored hash or clears EEPROM.
        - Message Encryption: Splits message into 2-character chunks, encrypts each with ChaCha using a static nonce and hashed password, and adds a MAC for integrity.

    - Receiver/Relay:
        - Setup & Authentication: Prompts for device node number and verifies incoming messages.
        - Modes: If the destination matches the receiver’s node, displays the message. Otherwise, forwards it in relay mode.

### Team Roles

    - Encryption, Decryption, Node Functionality: Timo and Jackson.
    - Message Authentication, Memory Management, Chunking: Ronnie and Luke.
    - Project Leader: Jackson
    - Arduino Interface: Gajen
