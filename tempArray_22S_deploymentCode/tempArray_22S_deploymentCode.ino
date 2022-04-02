/*

tempArray_22S_deploymentCode.ino

Temperature datalogger for Utqiagvik April 2022 deployment.

Ian Raphael
ian.th@dartmouth.edu
2022.04.02
*/

/***************!! Station settings !!***************/
#define STATION_ID 1 // station ID
#define NUM_TEMP_SENSORS 2 // number of sensors
uint8_t SAMPLING_INTERVAL_HOUR = 0;// number of hours between samples
uint8_t SAMPLING_INTERVAL_MIN = 0; // number of minutes between samples
uint8_t SAMPLING_INTERVAL_SEC = 15; // number of seconds between samples

/*************** packages ***************/
#include <OneWire.h>
#include <SerialFlash.h>
#include <DallasTemperature.h>
#include <SD.h>
#include <RTCZero.h>
#include <TimeLib.h>

/*************** defines, macros, globals ***************/
#define Serial SerialUSB

#define ONE_WIRE_BUS 12 // temp probe data line
const int flashChipSelect = 4;
#define TEMP_POWER 6 // temp probe power line
#define SD_CS 7 // SD card chip select
#define SD_CD 10 // SD card chip DETECT. Indicates presence/absence of SD card. High when card is inserted.
#define RADIO_PIN 5 // radio CS pin
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
  int numHeaderLines = 4;


  /*************** TempSensors object destructor ***************/
  // destroy a TempSensors object. Frees the dynamically allocated memory (called
  // automatically once object goes out of scope)
  ~TempSensors() {

    // // deallocate the address array
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
    addresses = new DeviceAddress[num_tempSensors];

    // // and point the object's addresses attribute here
    // addresses = curr_addresses;

    pinMode(powerPin, OUTPUT);
    digitalWrite(powerPin , HIGH);

    // init temp sensors themselves
    this->sensors.begin();

    // for every sensor
    for (int i = 0; i < numSensors; i++) {

      // if error getting the address
      if (!(this->sensors.getAddress(addresses[i], i))) {

        // print error
        Serial.print("Couldn't find sensor at index ");
        Serial.println(Serial.print(i));
      }
    }

    // now create the data filename
    filename += "stn";
    filename += stationID;
    filename += ".txt";
    // filename += "_tempArray.txt";

    // define the header information
    headerInformation = new String[numHeaderLines];

    headerInformation[0] = "DS18B20 array";

    headerInformation[1] = "Station ID: ";
    headerInformation[1] += stationID;

    headerInformation[2] = "Sensor addresses: {";
    appendAddrToStr(addresses[0], &headerInformation[2]);
    for (int i=1;i<numSensors;i++){
      headerInformation[2] += ", ";
      appendAddrToStr(addresses[i], &headerInformation[2]);
    }
    headerInformation[2] += "}";

    headerInformation[3] = "Date, Time";
    // for every sensor in the array
    for (int i=0; i<numSensors; i++) {

      // add a header column for each sensor
      headerInformation[3] += ", Sensor ";
      headerInformation[3] += i+1;
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

    // write the power pin high
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
      readString += this->sensors.getTempC(addresses[i]);
    }

    // return the readstring. TODO: add timestamp to the string
    return readString;
  }

  /************ print array ************/
  // void printArr(uint8_t *ptr, int len) {
  //   for (int i=0; i<len; i++) {
  //     Serial.print(ptr[i], HEX);
  //     Serial.print(" ");
  //   }
  // }

  void appendAddrToStr(uint8_t *addrPtr, String *strPtr) {
    for (int i=0; i<8; i++) {
      // *strPtr += "0x";
      *strPtr += String(addrPtr[i],HEX);
      if (i<7) {
        *strPtr += " ";
      }
    }
  }
};



/************ TEST SCRIPT ************/

// setup function
void setup() {

  // setup the board
  boardSetup();

  // ***** IMPORTANT DELAY FOR CODE UPLOAD BEFORE USB PORT DETACH DURING SLEEP *****
  delay(5000);

  // Start serial communications
  Serial.begin(9600);
  while (!Serial); // Wait for serial comms

  // init the SD
  init_SD();

  // init RTC
  init_RTC();
  // alarm_one();

  //
  Serial.println("Board initialized successfully. Initial file creation and test data-write: ");
  Serial.println("–––––––––––––––");
  alarm_one_routine();
  Serial.println("–––––––––––––––");
  Serial.println("Subsequent data sampling will occur beginning on the 0th multiple of the sampling period.");
  Serial.println("For example, if you are sampling every half hour, the next sample will occur at the top of the next hour, and subsequently every 30 minutes.");
  Serial.println("If you are sampling every 30 seconds, the next sample will occur at the top of the next minute, and subsequently every 30 seconds.");
  Serial.println("–––––––––––––––");
  Serial.println("Have a great field deployment :)");
  Serial.println("");
}

/************ main loop ************/
void loop(void)
{
  // nothing happens here...check alarm_one_routine
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

  // put the flash chip to sleep since we are using SD
  SerialFlash.begin(flashChipSelect);
  SerialFlash.sleep();

}

/************ init_SD ************/
void init_SD(){

  delay(100);
  // set SS pins high for turning off radio
  pinMode(RADIO_PIN, OUTPUT);
  delay(500);
  digitalWrite(RADIO_PIN, HIGH);
  delay(500);
  pinMode(SD_CS, OUTPUT);
  delay(500);
  digitalWrite(SD_CS, LOW);
  delay(1000);
  SD.begin(SD_CS);
  delay(1000);
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

/************ alarm_one ************/
/*
* Function to set RTC alarm
*
* TODO: at this point, we are incrementing all intervals at once. This is only
* accurate as long as we are not doing a mixed definition sampling interval,
* e.g. as long as we define our sampling interval as some number of minutes
* ONLY (every 15 mins, e.g.), and not as some number of hours and minutes
*
* takes:
* bool initFlag: indicates whether this is the initial instance of the alarm.
* If it is, then the sample times as zero to sample first at the top of the interval
*/
// bool initFlag
void alarm_one() {

  // if any of the intervals are defined as zero, redefine them as their max value
  if (SAMPLING_INTERVAL_SEC == 0){
    SAMPLING_INTERVAL_SEC = 60;
  }
  if (SAMPLING_INTERVAL_MIN == 0) {
    SAMPLING_INTERVAL_MIN = 60;
  }
  if (SAMPLING_INTERVAL_HOUR == 0) {
    SAMPLING_INTERVAL_HOUR = 24;
  }


  static uint8_t sampleSecond;
  static uint8_t sampleMinute;
  static uint8_t sampleHour;
  static uint8_t nSecSamples = 0;
  static uint8_t nMinSamples = 0;
  static uint8_t nHourSamples = 0;

  // if our sample definitions are null, create them
  // if (initFlag == TRUE){
  // if (sampleSecond == NULL){
  //   sampleSecond = 0;
  //   sampleMinute = 0;
  //   sampleHour = 0;
  // } else {
  //   // otherwise define the next sample to be taken. e.g. if we are sampling every 15
  //   // seconds, and the next will be the second sample (n=1), then we will have:
  //   // sampleSecond = (1*15)%60 = 15.
  //   sampleSecond = (nSecSamples * SAMPLING_INTERVAL_SEC) % 60;
  //   sampleMinute = (nMinSamples * SAMPLING_INTERVAL_MIN) % 60;
  //   sampleHour = (nHourSamples * SAMPLING_INTERVAL_HOUR) % 24;
  // }

  sampleSecond = (nSecSamples * SAMPLING_INTERVAL_SEC) % 60;
  sampleMinute = (nMinSamples * SAMPLING_INTERVAL_MIN) % 60;
  sampleHour = (nHourSamples * SAMPLING_INTERVAL_HOUR) % 24;

  // set the counter for the number of subinterval samples we've taken
  nSecSamples = ((sampleSecond + SAMPLING_INTERVAL_SEC) % 60)/SAMPLING_INTERVAL_SEC;
  nMinSamples = ((sampleMinute + SAMPLING_INTERVAL_MIN) % 60)/SAMPLING_INTERVAL_MIN;
  nHourSamples = ((sampleHour + SAMPLING_INTERVAL_HOUR) % 24)/SAMPLING_INTERVAL_HOUR;

  // then set the alarm and define the interrupt
  rtc.setAlarmTime(sampleHour,sampleMinute,sampleSecond);
  rtc.enableAlarm(rtc.MATCH_SS);
  rtc.attachInterrupt(alarm_one_routine);
}


/************ alarm_one_routine ************/
/*
* Routine to perform upon alarm_one interrupt. This is basically the main body
* of the code
*/
void alarm_one_routine() {

  // get a static temp sensors object. Should persist throughout program lifetime
  static TempSensors tempSensors_object = TempSensors(ONE_WIRE_BUS, TEMP_POWER, NUM_TEMP_SENSORS, STATION_ID);
  // static String filename = tempSensors_object.filename;

  // //TODO: for some reason this causes program to hang, without ever entering this loop.
  // if the SD card is out, hold until it's back in
  // while(!SD_CD) {
  //   Serial.println("SD card not inserted...insert to continue");
  //   delay(500);
  //   Serial.print(".");
  //   delay(500);
  //   Serial.print(".");
  //   delay(500);
  //   Serial.println(".");
  // }

  // if the file doesn't already exist
  if (!SD.exists(tempSensors_object.filename)){

    Serial.print("Creating datafile: ");
    Serial.println(tempSensors_object.filename);

    // create the file
    File dataFile = SD.open(tempSensors_object.filename, FILE_WRITE);

    // write the header information
    // if the file is available
    if (dataFile) {

      // print the header information to the file
      for (int i=0;i<tempSensors_object.numHeaderLines;i++) {

        dataFile.println(tempSensors_object.headerInformation[i]);

        // also print to serial to confirm
        Serial.println(tempSensors_object.headerInformation[i]);
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
  int secs = rtc.getSeconds();

  // append hours, mins, secs to the timestamp, 0 padding if necessary
  if (hours < 10) {
    timeString = String(0) + String(hours) + ":";
  } else {
    timeString = String(hours) + ":";
  }

  if (mins < 10) {
    timeString += String(0) + String(mins) + ":";
  } else {
    timeString += String(mins) + ":";
  }

  if (secs < 10){
    timeString += String(0) + String(secs);
  } else {
    timeString += String(secs);
  }


  // read the data
  String dataString = tempSensors_object.readTempSensors(dateString,timeString);

  // test print the data
  Serial.println(dataString);

  // init the sd card
  // init_SD();

  // open the file for writing
  File dataFile = SD.open(tempSensors_object.filename, FILE_WRITE);

  // if the file is available
  if (dataFile) {

    // write the data
    dataFile.println(dataString);

    // close the file
    dataFile.close();

    // flash LED to indicate successful data write
    for (int i=0;i<5;i++){
      digitalWrite(13,HIGH);
      delay(2000);
      digitalWrite(13,LOW);
      delay(500);
    }
  }

  // schedule the next alarm
  alarm_one();
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
