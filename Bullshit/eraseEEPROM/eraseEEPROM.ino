
#include <EEPROM.h>

int a = 0;
int value;
void setup() {

   // Initialize Serial communication for debugging
   Serial.begin(9600);
   // initialize the LED pin as an output.
   pinMode(13, OUTPUT);

    // erase everything
   for (int i = 0 ; i < EEPROM.length() ; i++) {
      EEPROM.write(i, 0);
   }


   // turn the LED on when we're done
   digitalWrite(13, HIGH);
}
void loop(){
  value = EEPROM.read(a);

  Serial.print(a);
  Serial.print("\t");
  Serial.print(value);
  Serial.println();

  a = a + 1;

  if (a == 512)
    a = 0;

  delay(500);
  
}
//  
//}
//
//
//int a = 0;
//int value;
//
//void setup()
//{
//  Serial.begin(9600);
//}

//int a = 0;
//int value;
//
//void setup()
//{
//  Serial.begin(9600);
//}
//
//void loop()
//{
//  value = EEPROM.read(a);
//
//  Serial.print(a);
//  Serial.print("\t");
//  Serial.print(value);
//  Serial.println();
//
//  a = a + 1;
//
//  if (a == 512)
//    a = 0;
//
//  delay(500);
//}
