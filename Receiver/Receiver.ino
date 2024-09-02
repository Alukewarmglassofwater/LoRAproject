/////
//
// Relay code. Listens for TAG messages, formats them, and retransmits them.
// Also forwards messages from other relays. Length is 25 bytes.
// Message format is 
//  SEQ       %5d
//  TYPE (1)  %5d
//  TAGID     %5d
//  RELAYID   %5d
//  TTL       %5d
//  RSSI      %5d
//  
/////

#include <SPI.h>
#include <RH_RF95.h> //this is a comment 

//Singleton instance of radio driver
RH_RF95 rf95;
int led = 13;   // Define LED pin in case we want to use it to demonstrate activity
int SEQ;
int TAGID;
int TTL;
int thisRSSI;
int RELAYID = 1;  // Identity of Relay. Will eventually be read from an SD card.
int RELAY;
int TYPE = 1;   // Message type
char message[100]; //array size declared

int MESSAGELENGTH = 100;
int DELAY = 1000; // Mean time between transmissions (100 milliseconds)
double CSMATIME = 10;  // Check the status of the channel every 10 ms


void setup() {
  pinMode(led, OUTPUT);
  Serial.begin(9600); 
  Serial.println("Receiver Version 1");
  while (!Serial)
    Serial.println("Waiting for serial port");  //Wait for serial port to be available.
  while (!rf95.init())
  {
    Serial.println("Initialisation of LoRa receiver failed");
    delay(1000);
  }
  rf95.setFrequency(915.0);   
  rf95.setTxPower(23, false); 
  rf95.setSignalBandwidth(500000);
  rf95.setSpreadingFactor(12);

}

void loop() {
  uint8_t buf[MESSAGELENGTH];
  uint8_t len = sizeof(buf);
  if (rf95.available())
  {
    // Should be a message for us now   
    if (rf95.recv(buf, &len))
    {
      //Serial.print("Received message : "); Serial.println((char*)buf);  //DEBUG
      digitalWrite(led, HIGH);
    }
    else
    {
      Serial.println("recv failed");
    }
  
    // Unpack message
    char str[MESSAGELENGTH];
    for (int i=0; i < MESSAGELENGTH; i++)
      str[i] = buf[i];
      
    // Now extract subfields 
    sscanf(str, "%5d %5d %5d %5d %5d %5d %s", &SEQ, &TYPE, &TAGID, &RELAY, &TTL, &thisRSSI, &message);
    // Now display
    Serial.print("Seq "); Serial.print(SEQ);
    Serial.print(" Type "); Serial.print(TYPE); Serial.print(" Tag "); Serial.print(TAGID);
    Serial.print(" Relay "); Serial.print(RELAY); Serial.print(" TTL "); Serial.print(TTL);
    Serial.print(" RSSI "); Serial.print(thisRSSI); Serial.print(" RSSI "); Serial.print(rf95.lastRssi());
    Serial.print(" Message "); Serial.print(message); 
    Serial.println(" ");
  } 
  digitalWrite(led, LOW);
}
