//************** BMI EVALUATION SYSTEM **********************
// ***** CODE  VERSION 3 (server version) ****************
#include <Wire.h> // Library for I2C Conmmunications
#include <VL53L0X.h> // Laser Height sensor
#include "LiquidCrystal_I2C.h" // Header for LCD 
#include "HX711.h" // Library for loadcell amplifier
#include "Arduino.h" // arduino lib for nodemcu
#include <ThingSpeak.h> // server library
#include <ESP8266WiFi.h> // Library for WiFi connection

#define channelID 2146386 // Thingspeak channel ID
#define apiKey "Z8MB60FUF2DFJV53" // API for writing data to Thingspeak

#define wifi_status 2  //--> Defining an LED pin (D2 = GPIO4), used for indicating when the process of connecting to a wifi router16
// WiFi Details
const char* ssid = "Electroacoustics";
const char* password = "O112358_1321G";

// ***************** Host Server ****
const char* server = "api.thingspeak.com";
WiFiClient client; //--> Create a WiFiClientSecure object.

VL53L0X heightSensor; // Height sensor object
LiquidCrystal_I2C lcd = LiquidCrystal_I2C(0x3F, 20,4); // object for lcd, default address 0x3F
HX711 LoadCell; // creates an instance of HX711 library

// HX711-Arduino circuit wiring
#define loadcell_DT 12 // pin used for the loadcell amplifier datapin is Pin A0 = ADC0
#define loadcell_SCK  13

// Arduino pins for button input 
#define buttonPin1 14 // BMI measurement button 
#define buttonPin2 15 // BMI result comments and clound data button

bool buttonState1 = 0; //keeps track of  BMI measurement button
int stateHolder1 = 0; // state holder for button1
bool buttonState2 = 0; //keeps track of  BMI result comments  button
int stateHolder2 = 0; // state holder for button1

long lastDebounceTime = 0;  // the last time the output pin changed 
long debounceDelay = 400;    // the button debounce time

// Varibale for mass and height
float height = 0.0; // height in metres
float mass = 0.0; // mass in kg
float bmi = 0.0; // bmi in kg/m^2

// Recomendation for the BMI value
String comment = ""; 

void setup()
{
  pinMode(buttonPin1, INPUT); 
  pinMode(buttonPin2, INPUT);
  pinMode(wifi_status, OUTPUT); //--> LED pin port Direction output
  Serial.begin(115200);
  Wire.begin();
  lcd.begin();
  lcd.noBacklight();
  heightSensor.init();
  heightSensor.setTimeout(500);
  // Start continuous back-to-back mode (take readings as
  // fast as possible).  To use continuous timed mode
  // instead, provide a desired inter-measurement period in
  // ms (e.g. sensor.startContinuous(100)).
  heightSensor.startContinuous();

  // Initialize the loadcell
  LoadCell.begin(loadcell_DT, loadcell_SCK);
  LoadCell.set_scale(-24.06); // set calibration factor
  LoadCell.tare();   // reset the scale to 0

    // ****************** Connecting to wifi  **************
   WiFi.begin(ssid, password); //--> Connect to your WiFi router
    Serial.println("WiFi Connection Setup");
    
    pinMode(wifi_status, OUTPUT); //--> LED pin port Direction output
    digitalWrite(wifi_status, HIGH); //--> Turn off Led On Board

    //----------------------------------------Wait for connection
    Serial.print("Connecting");
    while (WiFi.status() != WL_CONNECTED) {
      Serial.print(".");
      //----------------------------------------Make the Flashing LED on the process of connecting to the wifi router.
      digitalWrite(wifi_status, LOW);
      delay(250);
      digitalWrite(wifi_status, HIGH);
      delay(250);
      //----------------------------------------
    }
    //----------------------------------------
    digitalWrite(wifi_status, HIGH); //--> Turn on the LED when it is connected to the wifi router.
    Serial.println("");
    Serial.print("Successfully connected to : ");
    Serial.println(ssid);
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP()); // can uncomment to hide the WiFi ip address
    Serial.println();
  //----------------------------------------
 
  //client.setInsecure();
  // Reading Limits from Cloud
  ThingSpeak.begin(client);
   
}

// ***************************** HEIGHT MEASUREMENT ************************
float measure_height(){
  float distance = heightSensor.readRangeContinuousMillimeters();
  height = (205 - (distance/10.0)); // refereence height = 205.00 cm
  if(height < 50 ){height = 0.0;} // height = 0, when no one is on the platform
  if (heightSensor.timeoutOccurred()) { Serial.print(" TIMEOUT"); }

  return (height); // the height in centimetres
}

// ******************** MASS MEASUREMENT FUNCTION **************************** 
float measure_mass(){
  float mass_measured = LoadCell.get_units(10); // takes the average of 10 mass readings
  return (mass_measured/1000.0); // the mass in kgs
  }
// ************************** BMI CALCULATION ************************
void measure_bmi(){
  // perform height and mass measurement
    height = measure_height(); 
    mass = measure_mass(); 
    if (mass < 10){mass = 0.0;}

    // Calculate bmi value
    if (height == 0){bmi = 0.0;} // to avoid displaying overflow
    // else evaluate: bmi = mass/height^2 -> (kg/m^2) 
    else {bmi = (mass)/(pow((height/100.0),2));}
    //delay(200); 
    }

 // ****************** BMI Measurement button Function *****************
int pressButton1(){
//sample the state of the button - is it pressed or not?
  buttonState1 = digitalRead(buttonPin1);
  //filter out any noise by setting a time buffer
  if ( (millis() - lastDebounceTime) > debounceDelay) {
    //if the button has been pressed, lets toggle the stateHolder
    if(stateHolder1 > 1){stateHolder1 = 0;}
    else{
      if (buttonState1) {
      stateHolder1 += 1; //change the state of stateHolder
      lastDebounceTime = millis(); //set the current time
    }   
    }
  }//close if(time buffer)
  return stateHolder1;
}

// ******************** BMI result send result to server button Funtion ***********  
int pressButton2(){
//sample the state of the button - is it pressed or not?
  buttonState2 = digitalRead(buttonPin2);
  //filter out any noise by setting a time buffer
  if ( (millis() - lastDebounceTime) > debounceDelay) {
    //if the button has been pressed, lets toggle the stateHolder
    if(stateHolder2 > 2){stateHolder2 = 0;}
    else{
      if (buttonState2) {
      stateHolder2 += 1; //change the state of stateHolder
      lastDebounceTime = millis(); //set the current time
    }   
    }
  }//close if(time buffer)  
  return stateHolder2;
}

void loop()
{
  // Get the bmi measurements
  measure_bmi();
  // read button pressed
  int press1 = pressButton1();
  int press2 = pressButton2(); 
  
  // Check if Button1 state is false and display the welcome message 
  if(!press1){
    lcd.clear();  
    lcd.setCursor(4,0);
    lcd.noBacklight();
    lcd.print("WELCOME TO BMI");
    lcd.setCursor(2,1);
    lcd.print("EVALUATION SYSTEM");
    delay(200);
  }
  // Check if Button1 state is true and measure the BMI
  else{
    if(press2 == 0){
    // Display the Preliminary results
    lcd.clear();
    lcd.backlight();
    lcd.setCursor(3,0);
    lcd.print("BMI EVALUATION");
    lcd.setCursor(0,1);
    // display Height
    lcd.print("Height: ");
    lcd.setCursor(8,1);
    lcd.print(height);
    lcd.setCursor(14,1);
    lcd.print("cm");
    Serial.print("Height: ");
    Serial.print(height);
    Serial.println(" cm");
    // Display mass
    lcd.setCursor(0,2);
    lcd.print("Mass: ");
    lcd.setCursor(5,2);
    lcd.print(mass);
    lcd.setCursor(15,2);
    lcd.print("kg"); 
    Serial.print("Mass: ");
    Serial.print(mass);
    Serial.println(" kg"); 
    // Display BMI
    lcd.setCursor(0,3);
    lcd.print("BMI: ");
    lcd.setCursor(4,3);
    lcd.print(bmi);
    lcd.setCursor(14,3);
    lcd.print("kg/m^2");
    Serial.print("BMI: ");
    Serial.print(bmi);
    Serial.println(" kg/m^2");  
    delay(200); 
    }

    // Display the bmi value and comment(s)
    else if(press2 == 1){
      //  The bmi comments 
      if (bmi == 0){
        lcd.clear();
        lcd.setCursor(0,1);
        lcd.print("Nobody on the Stage");
        delay(200);
        }
      else {
      if(bmi > 0.0 and bmi <= 16.0){comment = "Severely thin!";}
      else if ((bmi > 16.0) and (bmi <= 17.0) ){comment = "Moderately thin";}
      else if ((bmi > 17.0) and (bmi <= 18.5) ){comment = "Mild thinness";}
      else if ((bmi > 18.5) and (bmi <= 25.0) ){comment = "Normal weight";}  
      else if ((bmi > 25.0) and (bmi <= 30.0) ){comment = "Overweight";}
      else if ((bmi > 30.0) and (bmi <= 35.0) ){comment = "Obese Class 1";}   
      else if ((bmi > 35.0) and (bmi <= 40.0) ){comment = "Obese Class 2";}  
      else if (bmi > 40.0) {comment = "Obese Class 3";} 

      lcd.clear();
      lcd.backlight();
      lcd.setCursor(3, 0);
      lcd.print("BMI: ");
      lcd.print(bmi);
      lcd.print(" kg/m^2");
      lcd.setCursor(2,1);
      lcd.print(comment);
      delay(200);
      }
    }
      // check if bmi > 0 and send result to cloud if button2 is pressed
      else if(press2 == 2){
         if(bmi == 0){
            lcd.clear();
            lcd.setCursor(0,1);
            lcd.print("Nobody on the Stage");
            delay(200);
         }
         else{
           // ++++++++++ Ecode the mass and height measurement to send to thingspeak server ++++++++
            Serial.print("Mass : ");
            Serial.println(mass);
            Serial.print("Height : ");
            Serial.println(height);
            Serial.print("BMI : ");
            Serial.println(bmi);
            // Convert the heigth and mass values to string
            String mass1 = String(mass);
            String height1 = String(height);
            // Removing the decimal point in the string
            int massLen = mass1.length();
            int heightLen = height1.length();
            if(massLen < 6){mass1 = "0" + mass1;}
            if(heightLen < 6){height1 = "0" + height1;}
            mass1.remove(3,1);
            height1.remove(3,1);

            String rawData = mass1 + height1; // Concatenate the mass and height strings
            int data = rawData.toInt(); // convert to an integer
            Serial.print(" Value Sent to ThingSpeak: ");
            Serial.println(data);

            ThingSpeak.writeField(channelID, 1, data, apiKey); // send to server
            lcd.clear();
            lcd.setCursor(0,1);
            lcd.print("Data to the server ");
            delay(200);
         }
         press2 = 0;
      }
   }
  }