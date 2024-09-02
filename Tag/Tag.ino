/////
//
// Tag code. Generates a message every 10 seconds for transmission via relays to Receiver
// Message format is 
//  SEQ
//  TYPE (0,9)  // is a message from the tag, 9 is a reset which is sent first time on startup
//  TAGID
//  RELAYID (0)
//  TTL     (5)
//  RSSI    (0)
//  
/////

#include <SPI.h>
#include <RH_RF95.h>

//Singleton instance of radio driver
RH_RF95 rf95;
int led = 13;   // Define LED pin in case we want to use it to demonstrate activity

//Message fields
int SEQ = 0;     // Sequence number
int TYPE = 0;   // Message type
int TAGID = 1;  // Identity of tag. Will eventually be read from an SD card.
int RELAYID = 0;
int TTL = 5;
int thisRSSI = 0;
int firstIteration = 0;   // First time through main loop send a reset (kludge, I know. Should go in setup.
char message[] = "thisisatest";


int MESSAGELENGTH = 100;
int TXINTERVAL = 5000; // Time between transmissions (in ms)
double CSMATIME = 10;  // Check the status of the channel every 10 ms

void setup() {
  // Initialise LoRa transceiver
  pinMode(led, OUTPUT);
  Serial.begin(9600); 
  Serial.println("Tag version 1");
  while (!Serial)
    Serial.println("Waiting for serial port");  //Wait for serial port to be available.
  while (!rf95.init()){
    Serial.println("Initialisation of LoRa receiver failed");
    delay(1000);
  }
  rf95.setFrequency(915.0);   //PB set to use 915 MHz
  rf95.setTxPower(5, false);
  rf95.setSignalBandwidth(500000);
  rf95.setSpreadingFactor(12);

}

void loop() {
  // Generate message intermittently (10 seconds)
  uint8_t buf[MESSAGELENGTH];
  uint8_t len = sizeof(buf);
  char str[MESSAGELENGTH]; 
  if (firstIteration == 0) // kludge to send out a type 9 as first message. Should eventually be in setup.
    {
      TYPE = 9;
      firstIteration = 1;
    }
  else
    TYPE = 0;
    
  SEQ++;
  sprintf(str, "%5d %5d %5d %5d %5d %5d %s", SEQ, TYPE, TAGID, RELAYID, TTL, thisRSSI, message);
  for (int i=0; i < MESSAGELENGTH; i++)
    buf[i] = str[i];   
  rf95.setModeIdle(); // some obscure bug causing loss of every second message  
    
  // Channel should be idle but if not wait for it to go idle
  while (rf95.isChannelActive())
  {
    delay(CSMATIME);   // wait for channel to go idle by checking frequently
    Serial.println("Tag node looping on isChannelActive()"); //DEBUG
  }
    
  // Transmit message
  Serial.print("Transmitted message: ");  //DEBUG
  Serial.println((char*)buf);           //DEBUG
  rf95.send(buf, sizeof(buf));
  rf95.waitPacketSent();
  delay(TXINTERVAL);
}