#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <Notecard.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

/* WiFi network name and password */
const char * ssid = "StarHaven";
const char * pwd = "YOUR_PASSWORD_HERE";

// SignalK server address
const char * udpAddress = "192.168.1.255";
// SignalK server port
const int udpPort = 8500;

// Buffer to store incoming NMEA messages
uint8_t buffer[100];

// Struct of buffers for Heading, Deviation and Variation
struct HDVStruct {
  uint8_t IIHDGBuffer[100]; // Heading, Deviation and Variationa
  uint8_t IIHDMBuffer[100]; // Magnetic
  uint8_t IIHDTBuffer[100];
};
HDVStruct HDV;

// Struct of instrument messages
struct InstrumentStruct {
  uint8_t IIMWVBuffer[100]; // Wind Speed and Angle
  uint8_t IIVHWBuffer[100]; // Water Speed and Heading
  uint8_t IIVWRBuffer[100]; // Relative Wind Speed and Angle
  uint8_t IIDBTBuffer[100]; // Depth Below Transducer
  uint8_t IIDPTBuffer[100]; // Depth of Water
  uint8_t IIDBKBuffer[100]; // Depth below keel
  uint8_t IIMTWBuffer[100]; // Mean Temperature of Water
  uint8_t IIZDABuffer[100]; // Time and Date
  uint8_t IIXDRBuffer[100]; // Transducer Measurements
  uint8_t IIROTBuffer[100]; // Rate of Turn
};
InstrumentStruct Instrument;

// UDP instance
WiFiUDP udp;

// Notecard instance
Notecard notecard;
#define UPLOAD_INTERVAL 60000 // 1 minute

// Task handles
TaskHandle_t mainTaskHandle = NULL;
TaskHandle_t logToNotecardTaskHandle = NULL;

// Task function prototypes
void mainTask(void *pvParameters);
void logToNotecardTask(void *pvParameters);

// NMEA0183 message types
enum NMEAMessageType {
    MSG_UNKNOWN = 0,
    /* GPS Messages */
    MSG_GPGGA,    // Global Positioning System Fix Data
    MSG_GPRMC,    // Recommended Minimum Navigation Information
    MSG_GPGLL,    // Geographic Position, Latitude and Longitude
    /* Integrated Instrumentation Messages */
    MSG_IIHDG,    // Heading, Deviation and Variation
    MSG_IIHDM,    // Heading, Deviation and Variation Magnetic
    MSG_IIHDT,    // Heading, Deviation and Variation True
    MSG_IIROT,    // Rate of Turn
    MSG_IIXDR,    // Transducer Measurements
    /* Wind Speed and Angle */
    MSG_IIMWV,    // Wind Speed and Angle
    MSG_IIVHW,    // Water Speed and Heading
    MSG_IIVWR,    // Relative Wind Speed and Angle
    /* Depth Below Transducer */
    MSG_IIDBT,    // Depth Below Transducer
    MSG_IIDPT,    // Depth of Water
    MSG_IIDBK,    // Depth below keel
    /* Mean Temperature of Water */
    MSG_IIMTW,    // Mean Temperature of Water
    /* Time and Date */
    MSG_IIZDA    // Time and Date
};

// Convert NMEA message to enum type
NMEAMessageType getMessageType(const uint8_t* buffer) {
    if (buffer[0] != '$') return MSG_UNKNOWN;

    // Check for GPS messages
    if (buffer[1] == 'G' && buffer[2] == 'P') {
        if (strncmp((char*)&buffer[3], "GGA", 3) == 0) return MSG_GPGGA;
        if (strncmp((char*)&buffer[3], "RMC", 3) == 0) return MSG_GPRMC;
        if (strncmp((char*)&buffer[3], "GLL", 3) == 0) return MSG_GPGLL;
    }
    // Check for Integrated Instrumentation messages
    else if (buffer[1] == 'I' && buffer[2] == 'I') {
        if (strncmp((char*)&buffer[3], "MWV", 3) == 0) return MSG_IIMWV;
        if (strncmp((char*)&buffer[3], "VHW", 3) == 0) return MSG_IIVHW;
        if (strncmp((char*)&buffer[3], "VWR", 3) == 0) return MSG_IIVWR;
        if (strncmp((char*)&buffer[3], "MTW", 3) == 0) return MSG_IIMTW;
        if (strncmp((char*)&buffer[3], "ZDA", 3) == 0) return MSG_IIZDA;
        if (strncmp((char*)&buffer[3], "XDR", 3) == 0) return MSG_IIXDR;
        if (strncmp((char*)&buffer[3], "HDT", 3) == 0) return MSG_IIHDT;
        if (strncmp((char*)&buffer[3], "HDM", 3) == 0) return MSG_IIHDM;
        if (strncmp((char*)&buffer[3], "HDG", 3) == 0) return MSG_IIHDG;
        if (strncmp((char*)&buffer[3], "ROT", 3) == 0) return MSG_IIROT;
        if (strncmp((char*)&buffer[3], "DBT", 3) == 0) return MSG_IIDBT;
        if (strncmp((char*)&buffer[3], "DPT", 3) == 0) return MSG_IIDPT;
        if (strncmp((char*)&buffer[3], "DBK", 3) == 0) return MSG_IIDBK;
    }
    return MSG_UNKNOWN;
}

void processPacket(){
  memset(buffer, 0, 100);
  //processing incoming packet, must be called before reading the buffer
  udp.parsePacket();
  // Check if we have a packet and if it's not too large
  int packetSize = udp.available();
  if(packetSize > 0 && packetSize <= 100) {
    if(udp.read(buffer, packetSize) > 0){
      NMEAMessageType msgType = getMessageType(buffer);
      bool sendToNotecard = false;

      switch(msgType) {
        case MSG_GPGGA:
          sendToNotecard = true;
          break;
        case MSG_GPRMC:
          sendToNotecard = true;
          break;
        case MSG_GPGLL:
          sendToNotecard = true;
          break;
        case MSG_IIMWV:
          memcpy(Instrument.IIMWVBuffer, buffer, sizeof(buffer));
          break;
        case MSG_IIVHW:
          memcpy(Instrument.IIVHWBuffer, buffer, sizeof(buffer));
          break;
        case MSG_IIVWR:
          memcpy(Instrument.IIVWRBuffer, buffer, sizeof(buffer));
          break;
        case MSG_IIMTW:
          memcpy(Instrument.IIMTWBuffer, buffer, sizeof(buffer));
          break;
        case MSG_IIZDA:
          memcpy(Instrument.IIZDABuffer, buffer, sizeof(buffer));
          break;
        case MSG_IIXDR:
          memcpy(Instrument.IIXDRBuffer, buffer, sizeof(buffer));
          break;
        /* Heading, Deviation and Variation Data */
        case MSG_IIHDT:
          memcpy(HDV.IIHDTBuffer, buffer, sizeof(buffer));
          break;
        case MSG_IIHDM:
          memcpy(HDV.IIHDMBuffer, buffer, sizeof(buffer));
          break;
        case MSG_IIHDG:
          memcpy(HDV.IIHDGBuffer, buffer, sizeof(buffer));
          break;
        case MSG_IIROT:
          memcpy(Instrument.IIROTBuffer, buffer, sizeof(buffer));
          break;
        case MSG_IIDBT:
          memcpy(Instrument.IIDBTBuffer, buffer, sizeof(buffer));
          break;
        case MSG_IIDPT:
          memcpy(Instrument.IIDPTBuffer, buffer, sizeof(buffer));
          break;
        case MSG_IIDBK:
          memcpy(Instrument.IIDBKBuffer, buffer, sizeof(buffer));
          break;
        default:
          // Unknown message type
          break;
      }

      if(sendToNotecard){
        // Serial.write(buffer, packetSize);
        Serial2.write(buffer, packetSize);
      }
    }
  }
}

void setup(){
  Serial.begin(115200);

  // Initialize buffers
  memset(buffer, 0, sizeof(buffer));
  memset(&HDV, 0, sizeof(HDV));

  // Create the main task
  xTaskCreate(
    mainTask,          // Task function
    "MainTask",        // Task name
    10000,            // Stack size (bytes)
    NULL,             // Task parameters
    1,                // Task priority
    &mainTaskHandle   // Task handle
  );

  // Create the notecard task
  xTaskCreate(
    logToNotecardTask,      // Task function
    "logToNotecardTask",    // Task name
    4096,             // Stack size (bytes)
    NULL,             // Task parameters
    1,                // Task priority
    &logToNotecardTaskHandle // Task handle
  );
}

void mainTask(void *pvParameters) {
  //Connect to the WiFi network
  WiFi.begin(ssid, pwd);
  Serial.println("");

  // Wait for connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  // Initialize Serial2
  Serial2.begin(9600, SERIAL_8N1, 16, 17); // Connected to Notecard AUX_TX, AUX_RX

  notecard.begin();

  //This initializes udp and transfer buffer
  udp.begin(udpPort);

  /* Notecard Configuration */
  J *req = notecard.newRequest("hub.set");
  JAddStringToObject(req, "product", "com.blues.abucknall:passage_log");
  JAddStringToObject(req, "mode", "continuous");
  JAddBoolToObject(req, "sync", true);
  notecard.sendRequestWithRetry(req, 5);

  // Attempt to use WiFi if internet connection is available
  req = notecard.newRequest("card.wifi");
  JAddStringToObject(req, "ssid", ssid);
  JAddStringToObject(req, "password", pwd);
  notecard.sendRequest(req);

  // Fallback to cellular if WiFi is not available, fallback to NTN if cellular is not available
  req = notecard.newRequest("card.transport");
  JAddStringToObject(req, "method", "wifi-cell-ntn");
  JAddBoolToObject(req, "allow", true);
  notecard.sendRequest(req);

  // Set the mode to external gps
  req = notecard.newRequest("card.aux.serial");
  JAddStringToObject(req, "mode", "gps");
  notecard.sendRequest(req);

  // Set the location mode to continuous
  req = notecard.newRequest("card.location.mode");
  JAddStringToObject(req, "mode", "continuous");
  notecard.sendRequest(req);

  // Start location tracking
  req = notecard.newRequest("card.location.track");
  JAddBoolToObject(req, "start", true);
  JAddBoolToObject(req, "heartbeat", true);
  // Only sync when we also send HDV and Instrumentation data
  JAddBoolToObject(req, "sync", true);
  notecard.sendRequest(req);

  Serial.println("Starting application");
  // Main task loop
  while(1) {
    processPacket();
    vTaskDelay(pdMS_TO_TICKS(10)); // 10ms delay
  }
}

// Helper function to combine HDV messages
void combineHDVMessages(char* dest, size_t destSize) {
    dest[0] = '\0';  // Start with empty string
    // Add each message if it's not empty, and clear the buffer after adding it
    if (HDV.IIHDGBuffer[0] != '\0') {
        strncat(dest, (char*)HDV.IIHDGBuffer, destSize - strlen(dest) - 1);
        memset(HDV.IIHDGBuffer, 0, sizeof(HDV.IIHDGBuffer));
    }
    if (HDV.IIHDMBuffer[0] != '\0') {
        strncat(dest, (char*)HDV.IIHDMBuffer, destSize - strlen(dest) - 1);
        memset(HDV.IIHDMBuffer, 0, sizeof(HDV.IIHDMBuffer));
    }
    if (HDV.IIHDTBuffer[0] != '\0') {
        strncat(dest, (char*)HDV.IIHDTBuffer, destSize - strlen(dest) - 1);
        memset(HDV.IIHDTBuffer, 0, sizeof(HDV.IIHDTBuffer));
    }
}

// Helper function to combine instrument messages
void combineInstrumentMessages(char* dest, size_t destSize) {
    dest[0] = '\0';  // Start with empty string
    // Add each message if it's not empty, and clear the buffer after adding it
    if (Instrument.IIMWVBuffer[0] != '\0') {
        strncat(dest, (char*)Instrument.IIMWVBuffer, destSize - strlen(dest) - 1);
        memset(Instrument.IIMWVBuffer, 0, sizeof(Instrument.IIMWVBuffer));
    }
    if (Instrument.IIVHWBuffer[0] != '\0') {
        strncat(dest, (char*)Instrument.IIVHWBuffer, destSize - strlen(dest) - 1);
        memset(Instrument.IIVHWBuffer, 0, sizeof(Instrument.IIVHWBuffer));
    }
    if (Instrument.IIVWRBuffer[0] != '\0') {
        strncat(dest, (char*)Instrument.IIVWRBuffer, destSize - strlen(dest) - 1);
        memset(Instrument.IIVWRBuffer, 0, sizeof(Instrument.IIVWRBuffer));
    }
    if (Instrument.IIDBTBuffer[0] != '\0') {
        strncat(dest, (char*)Instrument.IIDBTBuffer, destSize - strlen(dest) - 1);
        memset(Instrument.IIDBTBuffer, 0, sizeof(Instrument.IIDBTBuffer));
    }
    if (Instrument.IIDPTBuffer[0] != '\0') {
        strncat(dest, (char*)Instrument.IIDPTBuffer, destSize - strlen(dest) - 1);
        memset(Instrument.IIDPTBuffer, 0, sizeof(Instrument.IIDPTBuffer));
    }
    if (Instrument.IIDBKBuffer[0] != '\0') {
        strncat(dest, (char*)Instrument.IIDBKBuffer, destSize - strlen(dest) - 1);
        memset(Instrument.IIDBKBuffer, 0, sizeof(Instrument.IIDBKBuffer));
    }
    if (Instrument.IIMTWBuffer[0] != '\0') {
        strncat(dest, (char*)Instrument.IIMTWBuffer, destSize - strlen(dest) - 1);
        memset(Instrument.IIMTWBuffer, 0, sizeof(Instrument.IIMTWBuffer));
    }
    if (Instrument.IIZDABuffer[0] != '\0') {
        strncat(dest, (char*)Instrument.IIZDABuffer, destSize - strlen(dest) - 1);
        memset(Instrument.IIZDABuffer, 0, sizeof(Instrument.IIZDABuffer));
    }
    if (Instrument.IIXDRBuffer[0] != '\0') {
        strncat(dest, (char*)Instrument.IIXDRBuffer, destSize - strlen(dest) - 1);
        memset(Instrument.IIXDRBuffer, 0, sizeof(Instrument.IIXDRBuffer));
    }
    if (Instrument.IIROTBuffer[0] != '\0') {
        strncat(dest, (char*)Instrument.IIROTBuffer, destSize - strlen(dest) - 1);
        memset(Instrument.IIROTBuffer, 0, sizeof(Instrument.IIROTBuffer));
    }
}

void logToNotecardTask(void *pvParameters) {
  // Wait for main task to initialize Notecard
  vTaskDelay(pdMS_TO_TICKS(5000));  // 5 second delay to ensure main task is ready

  while(1) {
    // Get the location
    double latitude = 0;
    double longitude = 0;
    uint32_t timestamp = 0;
    J *rsp = notecard.requestAndResponse(notecard.newRequest("card.location"));
    if (rsp != NULL)
    {
        latitude = JGetNumber(rsp, "latitude");
        longitude = JGetNumber(rsp, "longitude");
        timestamp = JGetNumber(rsp, "timestamp");
        notecard.deleteResponse(rsp);
    }

    // Combine messages before sending
    char combinedHDVBuffer[300];
    char combinedInstrumentBuffer[300];
    combineHDVMessages(combinedHDVBuffer, sizeof(combinedHDVBuffer));
    combineInstrumentMessages(combinedInstrumentBuffer, sizeof(combinedInstrumentBuffer));

    // Create a new request
    Serial.println("Adding HDV to Notecard");
    Serial.println(combinedHDVBuffer);
    Serial.println("Adding Instrument to Notecard");
    Serial.println(combinedInstrumentBuffer);
    J *req = notecard.newRequest("note.add");
    if (req != NULL)
    {
        JAddBoolToObject(req, "sync", true);
        J *body = JAddObjectToObject(req, "body");
        if (body != NULL)
        {
            JAddStringToObject(body, "HDV", combinedHDVBuffer);
            JAddStringToObject(body, "INS", combinedInstrumentBuffer);
        }
        notecard.sendRequest(req);
    }
    // Wait for 60 seconds
    vTaskDelay(pdMS_TO_TICKS(UPLOAD_INTERVAL));
  }
}

void loop(){
  // Empty - we're using FreeRTOS tasks instead
}