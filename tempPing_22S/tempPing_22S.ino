/*

tempPing_22S.ino

Temperature and snow pinger datalogger for Utqiagvik April 2022 deployment.

Ian Raphael
ian.th@dartmouth.edu
2022.03.28
*/

#include <OneWire.h>
#include <SerialFlash.h>
#include <DallasTemperature.h>
#include <SD.h>
#include <RTCZero.h>
#include <TimeLib.h>

#define Serial SerialUSB

#define ONE_WIRE_BUS 12 // temp probe data line
const int flashChipSelect = 4;
#define TEMP_POWER 6 // temp probe power line
#define SD_CS 7 // SD card chip select
#define RADIO_PIN 5 // radio CS pin

/***************!! Station settings !!***************/
#define STATION_ID 1 // station ID
#define NUM_TEMP_SENSORS 2 // number of sensors
// #define SAMPLING_INTERVAL_HOUR 0 // sampling interval, in hours
// #define SAMPLING_INTERVAL_MIN 1 // sampling interval, in mins. Set to 0 for even hourly sampling
// #define SAMPLING_INTERVAL_SEC 0 // sampling interval, in secs. Set to 0 for even minutely samplin
String filename = "test.txt"; // data file filename

RTCZero rtc; // real time clock object

/*************** TempSensors class ***************/
// define a class for the temp sensors
class TempSensors {

  // make public access for everything
public:

  /* Attributes */
  int powerPin; // power pin for the sensors
  int dataPin; // data pin for the sensors

  OneWire oneWire; // onewire obect for the dallas temp object to hold
  DallasTemperature sensors; // the temp sensor object
  uint8_t (*addresses)[8]; // pointer to array of device addresses

  int numSensors; // number of temperature sensors in the array
  int stationID; // measurement station ID

  String filename = ""; // a filename for the data file
  String* headerInformation; // the header information for the data file
  int numHeaderLines = 3;


  /*************** TempSensors object destructor ***************/
  // destroy a TempSensors object. Frees the dynamically allocated memory (called
  // automatically once object goes out of scope)
  ~TempSensors() {

    // deallocate the address array
    delete [] addresses;
    // zero out the address
    addresses = 0;

    delete [] headerInformation;
    // zero out the address
    headerInformation = 0;
  }


  /*************** TempSensors object constructor ***************/
  // Constructor. This takes in the input information, addresses all of the sensors,
  // and creates the header information for the data file.
  // IMPT: This uses dynamically allocated memory via `new`! You _must_ free the address
  // array and the header information via the tempsensors destructor method when you are done.
  // TODO: fully sleep the sensors at the end of this function
  TempSensors(int data_pin, int power_pin, int num_tempSensors, int station_ID) {

    // Setup a oneWire instance to communicate with any OneWire devices
    OneWire currOneWire = OneWire(data_pin);

    this->oneWire = currOneWire;

    // Pass our oneWire reference to create a Dallas Temp object
    DallasTemperature currDallasTemp = DallasTemperature(&oneWire);

    this->sensors = currDallasTemp;

    // copy over the other stuff that we need
    powerPin = power_pin;
    numSensors = num_tempSensors;
    stationID = station_ID;

    // init the address array
    addresses = new uint8_t[num_tempSensors][8];

    // // and point the object's addresses attribute here
    // addresses = curr_addresses;

    pinMode(powerPin, OUTPUT);
    digitalWrite(powerPin , HIGH);

    // init temp sensors themselves
    this->sensors.begin();

    // for every sensor
    for (int i = 0; i < numSensors; i++) {

      // if error getting the address
      if (!(this->sensors.getAddress(&addresses[i][0], i))) {

        // print error
        Serial.print("Couldn't find sensor at index ");
        Serial.println(Serial.print(i));
      }
    }

    // now create the data filename
    filename += "station_";
    filename += stationID;
    filename += "_tempArray.txt";

    // define the header information
    headerInformation = new String[numHeaderLines];

    headerInformation[0] = "DS18B20 array";

    headerInformation[1] = "Station ID: ";
    headerInformation[1] += stationID;

    headerInformation[2] = "Date, Time";

    // for every sensor in the array
    for (int i=0; i<numSensors; i++) {

      // add a header column for each sensor
      headerInformation[2] += ", Sensor ";
      headerInformation[2] += i+1;
    }

    // shut down the sensors until we need them
    digitalWrite(powerPin , LOW);
  }


  /*************** TempSensors readTempSensors ***************/
  // Reads an array of temp sensors, returning a string with a timestamp and the
  // reading for each sensor.
  // inputs: timestamp
  // TODO: wake/sleep the sensors in this function
  String readTempSensors(String date, String time) {

    // w
    digitalWrite(powerPin, HIGH);

    // Call sensors.requestTemperatures() to issue a global temperature request to all devices on the bus
    sensors.requestTemperatures();

    // declare a string to hold the read data
    String readString = "";

    // throw the timestamp on there
    readString = date + ", " + time;

    // for every sensor on the line
    for (int i=0; i<numSensors; i++){

      // add its data to the string
      readString += ", ";
      readString += this->sensors.getTempC(&addresses[i][0]);
    }

    // return the readstring. TODO: add timestamp to the string
    return readString;
  }

  /************ print array ************/
  void printArr(uint8_t *ptr, int len) {
    for (int i=0; i<len; i++) {
      Serial.print(ptr[i], HEX);
      Serial.print(" ");
    }
  }
};



/************ TEST SCRIPT ************/

// // declare temp sensors pointer as a global
// TempSensors *test;

// setup function
void setup() {

  // setup the board
  boardSetup();

  // ***** IMPORTANT DELAY FOR CODE UPLOAD BEFORE USB PORT DETACH DURING SLEEP *****
  delay(5000);

  // Start serial communications
  Serial.begin(9600);
  while (!Serial); // Wait for serial comms

  // init RTC
  init_RTC();
  // alarm_one();
}

/************ main loop ************/
void loop(void)
{

  // Initialize USB and attach to host (not required if not in use)
  // USBDevice.init();
  // USBDevice.attach();

  // delay(15000);

  // get a static temp sensors object. Should persist throughout program lifetime
  static TempSensors tempSensors_object = TempSensors(ONE_WIRE_BUS, TEMP_POWER, NUM_TEMP_SENSORS, STATION_ID);

  // init the SD
  init_SD();

  // if the file doesn't already exist
  if (!SD.exists(filename)){

    Serial.print("Creating datafile: ");
    Serial.println(filename);

    // create the file
    File dataFile = SD.open(filename, FILE_WRITE);

    // write the header information
    // if the file is available
    if (dataFile) {

      // print the header information to the file
      for (int i=0;i<tempSensors_object.numHeaderLines;i++) {

        dataFile.println(tempSensors_object.headerInformation[i]);
        // TODO: include sensor ids in metadata info
        //TODO: ensure that we are appending data & not flushing file index if powered down
      }

      // close the file
      dataFile.close();
    }
  }

  // get date and timestamps
  String dateString = "";
  dateString = String(20) + String(rtc.getYear()) + ".";

  int month = rtc.getMonth();
  int day = rtc.getDay();

  // append months and days to datestamp, 0 padding if necessary
  if (month < 10) {
    dateString += String(0) + String(month) + ".";
  } else {
    dateString += String(month) + ".";
  }

  if (day < 10) {
    dateString += String(0) + String(day);
  } else {
    dateString += String(day);
  }

  String timeString = "";
  int hours = rtc.getHours();
  int mins = rtc.getMinutes();

  // append hours and minutes to the timestamp, 0 padding if necessary
  if (hours < 10) {
    timeString = String(0) + String(hours) + ":";
  } else {
    timeString = String(hours) + ":";
  }

  if (mins < 10) {
    timeString += String(0) + String(mins);
  } else {
    timeString += String(mins);
  }

  // read the data
  String dataString = tempSensors_object.readTempSensors(dateString,timeString);

  // print the header information
  for (int i=0;i<tempSensors_object.numHeaderLines;i++) {

    Serial.println(tempSensors_object.headerInformation[i]);
  }

  // test print the data
  Serial.println(dataString);

  // init the sd card
  init_SD();

  // open the file for writing
  File dataFile = SD.open(filename, FILE_WRITE);

  // if the file is available
  if (dataFile) {

    // write the data
    dataFile.println(dataString);

    // close the file
    dataFile.close();
    //TODO: include LED flash to indicate successful data write
  }

  // Detach USB from host (not required if not in use)
  // USBDevice.detach();

  // delay(15000);

  // Sleep until next alarm match
  // rtc.standbyMode();

}



/************ Board setup ************/
void boardSetup() {

  unsigned char pinNumber;

  // Pull up all unused pins to prevent floating vals
  for (pinNumber = 0; pinNumber < 23; pinNumber++) {
    pinMode(pinNumber, INPUT_PULLUP);
  }
  for (pinNumber = 32; pinNumber < 42; pinNumber++) {
    pinMode(pinNumber, INPUT_PULLUP);
  }
  pinMode(25, INPUT_PULLUP);
  pinMode(26, INPUT_PULLUP);
  pinMode(13, OUTPUT);
  digitalWrite(13, LOW);
  analogReadResolution(12);

  SerialFlash.begin(flashChipSelect);
  SerialFlash.sleep();

  // USBDevice.detach();

  // rtc.standbyMode();

}

/************ init_SD ************/
void init_SD(){

  delay(100);
  // set SS pins high for turning off radio
  pinMode(RADIO_PIN, OUTPUT);
  delay(50);
  digitalWrite(RADIO_PIN, HIGH);
  delay(50);
  pinMode(SD_CS, OUTPUT);
  delay(50);
  digitalWrite(SD_CS, LOW);
  delay(100);
  SD.begin(SD_CS);
  delay(200);
  if (!SD.begin(SD_CS)) {
    Serial.println("initialization failed!");
    while(1);
  }
}


/************ init_RTC() ************/
/*
* Function to sync the RTC to compile-time timestamp
*/
bool init_RTC() {

  // get the date and time the compiler was run
  uint8_t dateArray[3];
  uint8_t timeArray[3];

  if(getDate(__DATE__,dateArray) && getTime(__TIME__,timeArray)){

    rtc.begin();
    rtc.setTime(timeArray[1], timeArray[2], timeArray[3]);
    rtc.setDate(dateArray[3], dateArray[2], dateArray[1]);

  } else { // if failed, hang
    Serial.println("failed to init RTC");
    while(1);
  }
}
//
// void alarm_one() {
//   rtc.setAlarmTime(SAMPLING_INTERVAL_HOUR,SAMPLING_INTERVAL_MIN,SAMPLING_INTERVAL_SEC);
//   // rtc.enableAlarm(rtc.MATCH_MMSS);
//   rtc.enableAlarm(rtc.MATCH_SS);
//   rtc.attachInterrupt(alarm_one_routine);
//

//----------------------------------------

void alarm_one_routine() {


}


/************ getTime() ************/
/*
* Function to parse time from system __TIME__ string. From Paul Stoffregen
* DS1307RTC library example SetTime.ino
*/
bool getTime(const char *str,uint8_t* timeArray)
{
  int Hour, Min, Sec;

  if (sscanf(str, "%d:%d:%d", &Hour, &Min, &Sec) != 3) return false;

  // pack everything into a return array
  timeArray[1] = (uint8_t) Hour;
  timeArray[2] = (uint8_t) Min;
  timeArray[3] = (uint8_t) Sec;

  return true;
}


/************ getDate() ************/
/*
* Function to parse date from system __TIME__ string. Modified from Paul
* Stoffregen's DS1307RTC library example SetTime.ino
*/
bool getDate(const char *str,uint8_t *dateArray)
{
  char Month[12];
  int Day, Year;
  uint8_t monthIndex;

  const char *monthName[12] = {
    "Jan", "Feb", "Mar", "Apr", "May", "Jun",
    "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
  };

  if (sscanf(str, "%s %d %d", Month, &Day, &Year) != 3) return false;
  for (monthIndex = 0; monthIndex < 12; monthIndex++) {
    if (strcmp(Month, monthName[monthIndex]) == 0) break;
  }
  if (monthIndex >= 12) return false;

  // convert the int year into a string
  String twoDigitYear = String(Year);
  // extract the last two digits of the year
  twoDigitYear = twoDigitYear.substring(2);

  // pack everything into a return array as uint8_ts
  dateArray[1] = (uint8_t) twoDigitYear.toInt();
  dateArray[2] = monthIndex+1;
  dateArray[3] = (uint8_t) Day;

  return true;
}
