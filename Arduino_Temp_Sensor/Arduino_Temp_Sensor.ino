#include <Wire.h>
#include<DS1631.h>

#ifndef sbi
#define sbi(sfr, bit) (_SFR_BYTE(sfr) |= _BV(bit))
#endif

DS1631 Temp1(0);
const unsigned serialBufferSize = 1024;
char inData[serialBufferSize]; // For reading serial aa
int inDataCount = 0;

int analogPinForVoltage = A0;

int relayPins[4] = {2, 3, 4, 5};

unsigned long lastMillis = 0;

void setup() {
  // Setup the serial connection
  Serial.begin(9600);
  
  Serial.println("Startup: Temp sensor test.");
  
  Wire.begin(); 
  
  // Set the twi bits to 1 to cut the frequency of i2c by 64
  sbi(TWSR, TWPS0);
  sbi(TWSR, TWPS1);
  
  // Set the Two Wire Bit Rate register to the max value (slowest operation)
  TWBR = 255;
  
  int config = Temp1.readConfig();
  Serial.println(config, BIN); // Prints the config, reading in binary.
  Temp1.writeConfig(13); // Sets to 12 bit, 1 shot mode. (stolen from example)
  
  config  = Temp1.readConfig();
  Serial.println(config, BIN); // Prints the config, reading in binary. This time _after_ it was changed. Basically verifies that it was correctly updated.
  
  // Initialize the output bits before we make them "live" to avoid momentary glitches at startup
  for (int r=0; r<4; r++)
  {
      // Sets the pin mode for the relays after setting their initial value
      digitalWrite(relayPins[r], true);
      pinMode(relayPins[r], OUTPUT);
  }
}


void loop() {
  
  const unsigned long TemperatureReportRate     = 1000;
  const unsigned long TemperatureConvertTimeout = 1000;  // Docs say it can take up to 750 ms for a full precision conversion

  // Figures out how long since last time the temperature was set, and if it's over 1000ms, send it again.  
  bool tempRunning = false;
  unsigned long now = millis();
  unsigned long difference = now - lastMillis;
  
  if (tempRunning)
  {
    if (Temp1.conversionDone())
    {
      float temp = Temp1.readTempF();
      // After reading the temperature, put the DS1631 back into low-power idle state.
      Temp1.stopConversion();   
      
      Serial.print("D");         // "1" means we're sending a temperature reading
      Serial.println(temp, 4);   // 4 specifies the amount of decimals to include in the number.
      
      // Restart our clock to wait until time for the next temperature sample
      lastMillis = now;
    }
    else
    {
      // Has it been too long and we should give up and try again?
      if (difference >= TemperatureReportRate + TemperatureConvertTimeout)
      {
        // We waited too long for an answer and didn't get one.  Try again.
        tempRunning = false;
      }
    }
  }
  else
  {
    if (difference >= TemperatureReportRate) {
      // Start the digitial thermometer A/D conversion process
      Temp1.startConversion();  // Running a full precision conversion can take up to 750 ms
      tempRunning = true;
      
      // Report the analog reading right away
      int voltage = analogRead(analogPinForVoltage);
      Serial.print("V");
      Serial.println(voltage);    
    }    
  }
  
  // Handle any command we got
  processCommands();
}


// Reads a string from serial. NEED TO IMPLEMENT A STOP CHARACTER!!!
char * readString() {
  int h = Serial.available(); // Stores whether the serial is available in h
  
  // Read out each byte from the serial port
  while (h>0)
  {
     char received = (char) Serial.read(); // Get the next character from the serial line.
       
     // Is this a CR or LF line ending marker?
     if ((received == '\r') || (received == '\n'))
     {
       if (inDataCount == 0)
       {
         // We got an empty string.  Return NULL and get anything left in the buffer next time.
         return NULL;
       }
       else
       {
         // We got a line ending, so add a NULL terminator
         inData[inDataCount] = 0;  // add a NULL terminator
         
         // Reset to the beginning of the buffer for next time
         inDataCount = 0;
         
         // Now return what we have accumulated and pick up the rest (if any) next time
         return inData;
       }
     }
       
     // If we have room, add this character to the end of our buffer (leaving room for a final NULL terminator)
     if (inDataCount+2 < serialBufferSize)
     {
       // Append the data to the end of inData for each byte in the buffer
       inData[inDataCount] = received;
       inDataCount++;
     }
     
     h--;
  }
    
  // If we don't have a complete string for delivery, then return NULL
  return NULL;
}


/*
/ Format for relay message:
/ R [RELAY] [STATE]
*/

// Gets the updates for the relays from Serial
void processCommands() {   
  char * in = readString(); 
  
  if (in != NULL)
  {
    char prefix = in[0];
    if (prefix == 'R') {
      if (strlen(in) < 5)
      {
        // Bogus command structure!
        return;
      }
      
      //Serial.println("Getting relay updates. " + in);
      int relay = int(in[2]) - 48;  // -48 to convert from ASCII to integer value
      boolean state = (in[4] != '0');
         
      digitalWrite(relayPins[relay], !state);
    }
  }
}

