#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <LiquidCrystal_I2C.h>
#include <TinyGPS++.h>
#include <math.h>
// ----- STEP 3 STARTS HERE -----
#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include <addons/TokenHelper.h>
#include "secrets.h"

FirebaseData fbdo;
FirebaseAuth fbAuth;
FirebaseConfig fbConfig;

struct Snapshot { char status[24];
  int sats; bool hdopOk; float hdop;
  bool hasFix; double lat, lon;
  float gpsSpeed, accelMag, accelChange, gyroMag; 
  };
struct StatusEvent { char from[24]; char to[24]; int sats; float hdop; float spd; };

Snapshot snap;
SemaphoreHandle_t snapMutex;
QueueHandle_t eventQueue, smsQueue;
volatile int simMode = 0;

const char* toCode(const String& s) { 
  if (s == "NORMAL")             return "NORMAL";
  if (s == "GNSS DEGRADED")      return "GNSS_DEGRADED";
  if (s == "SUSPECTED SPOOFING") return "SUSPECTED_SPOOFING";
  if (s == "GNSS BLACKOUT")      return "GNSS_BLACKOUT";
  return "GPS_ACQUIRING"; }
// ----- STEP 3 ENDS HERE -----
// ---------- HARDWARE ----------
Adafruit_MPU6050 mpu;
LiquidCrystal_I2C lcd(0x27, 16, 2);

TinyGPSPlus gps;
HardwareSerial GPSserial(1);
HardwareSerial ModemSerial(2);

// CHANGE THIS to your phone number
const char PHONE_NUMBER[] = "+918801501605";

// ---------- GPS SETTINGS ----------
const unsigned long GPS_TIMEOUT = 10000;
unsigned long lastValidGPS = 0;
bool gpsFixAcquired = false;

// ---------- GNSS QUALITY ----------
bool gnssDegraded = false;
int poorGPSCount = 0;
int goodGPSCount = 0;

bool TEST_DEGRADED = false;
bool TEST_SPOOFING = false;

// ---------- ALERT STATE ----------
String previousStatus = "GPS ACQUIRING";

// ---------- MODEM RESPONSE ----------
bool waitForModem(const char *expected,
                  unsigned long timeout) {
  String response = "";
  unsigned long start = millis();

  while (millis() - start < timeout) {
    while (ModemSerial.available()) {
      char c = ModemSerial.read();
      Serial.write(c);
      response += c;

      if (response.indexOf(expected) >= 0) {
        return true;
      }

      if (response.length() > 500) {
        response.remove(0, 250);
      }
    }
    delay(1);
  }

  Serial.println("\nModem response timeout.");
  return false;
}

// ---------- SEND SMS ----------
bool sendSMS(String message) {
  Serial.println("\nPreparing SMS...");

  ModemSerial.println("AT");
  if (!waitForModem("OK", 3000)) return false;

  ModemSerial.println("AT+CMGF=1");
  if (!waitForModem("OK", 3000)) return false;

  ModemSerial.print("AT+CMGS=\"");
  ModemSerial.print(PHONE_NUMBER);
  ModemSerial.println("\"");

  if (!waitForModem(">", 10000)) return false;

  ModemSerial.print(message);
  ModemSerial.write(26);  // Ctrl+Z sends SMS

  if (!waitForModem("+CMGS:", 30000)) {
    Serial.println("\nSMS was not confirmed.");
    return false;
  }

  waitForModem("OK", 5000);
  Serial.println("\nSMS accepted by modem.");
  return true;
}
  waitForModem("OK", 5000);
  Serial.println("\nSMS accepted by modem.");
  return true;
}

// ----- STEP 4 STARTS HERE -----
void pushLive() { 
  Snapshot s;
  xSemaphoreTake(snapMutex, portMAX_DELAY);
  s = snap;
  xSemaphoreGive(snapMutex);

  FirebaseJson j;
  j.set("ts/.sv", "timestamp");            // server-side time, no NTP needed
  j.set("status", s.status);
  j.set("sats", s.sats);
  if (s.hdopOk) j.set("hdop", s.hdop);
  if (s.hasFix) { j.set("lat", s.lat); j.set("lon", s.lon); }
  j.set("gpsSpeed", s.gpsSpeed);
  j.set("accelMag", s.accelMag);
  j.set("accelChange", s.accelChange);
  j.set("gyroMag", s.gyroMag);
  j.set("rssi", WiFi.RSSI());
  j.set("sim", (int)simMode);              // lets the dashboard show a SIMULATION badge
  Firebase.RTDB.setJSON(&fbdo, "/devices/sentinel01/live", &j);
  }
bool pushEvent(const StatusEvent& e) {
    FirebaseJson j;
  j.set("ts/.sv", "timestamp");
  j.set("from", e.from);
  j.set("to", e.to);
  j.set("sats", e.sats);
  j.set("hdop", e.hdop);
  j.set("gpsSpeed", e.spd);
  return Firebase.RTDB.pushJSON(&fbdo, "/devices/sentinel01/events", &j);
}
void readCommand() {
  if (Firebase.RTDB.getString(&fbdo, "/devices/sentinel01/commands/simMode")) {
    String m = fbdo.stringData();
    simMode = (m == "DEGRADED") ? 1 : (m == "SPOOF") ? 2 : (m == "BLACKOUT") ? 3 : 0;
  }
}
void firebaseTask(void*) {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  fbConfig.api_key = AIzaSyC3s0nonk22rvKT-mGNGpCGHE1KakSULN8;
  fbConfig.database_url = https://sentinel-bc1f1-default-rtdb.asia-southeast1.firebasedatabase.app;
  fbAuth.user.email = xyz678@gmail.com;
  fbAuth.user.password = xyz678;
  fbConfig.token_status_callback = tokenStatusCallback;
  Firebase.reconnectNetwork(true);
  Firebase.begin(&fbConfig, &fbAuth);

  unsigned long lastLive = 0, lastCmd = 0;
  for (;;) {
    if (WiFi.status() == WL_CONNECTED && Firebase.ready()) {
      StatusEvent ev;
      while (xQueueReceive(eventQueue, &ev, 0) == pdTRUE) {
        if (!pushEvent(ev)) {                       // failed: put it back, retry later
          xQueueSendToFront(eventQueue, &ev, 0);
          Serial.println(fbdo.errorReason());
          break;
        }
      }
      if (millis() - lastLive >= 1500) { lastLive = millis(); pushLive(); }
      if (millis() - lastCmd  >= 2000) { lastCmd  = millis(); readCommand(); }
    }
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}
void smsTask(void*) {
  char msg[160];
  for (;;) {
    if (xQueueReceive(smsQueue, msg, portMAX_DELAY) == pdTRUE) sendSMS(String(msg));
  }
}
void queueSMS(const String& m) {
  char buf[160];
  m.toCharArray(buf, sizeof(buf));
  xQueueSend(smsQueue, buf, 0);
}
// ----- STEP 4 ENDS HERE -----


// ---------- SETUP ----------
void setup() {
  Serial.begin(115200);

  Wire.begin(21, 22);

  if (!mpu.begin()) {
    Serial.println("MPU6050 NOT FOUND!");
    while (true) delay(100);
  }

  lcd.init();
  lcd.backlight();

  GPSserial.setRxBufferSize(1024);
  GPSserial.begin(9600, SERIAL_8N1, 16, 17);
  ModemSerial.begin(115200, SERIAL_8N1, 26, 27);

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("SENTINEL");
  lcd.setCursor(0, 1);
  lcd.print("GPS acquiring");

  Serial.println("SENTINEL STARTED");
  Serial.println("Waiting for first GPS fix...");
  snapMutex  = xSemaphoreCreateMutex();
  eventQueue = xQueueCreate(10, sizeof(StatusEvent));
  smsQueue   = xQueueCreate(4, 160);
  strlcpy(snap.status, "GPS_ACQUIRING", sizeof(snap.status));  // never upload an empty status
  xTaskCreatePinnedToCore(firebaseTask, "fb",  12288, NULL, 1, NULL, 0);
  xTaskCreatePinnedToCore(smsTask,      "sms",  4096, NULL, 1, NULL, 0);
  // ----- STEP 5 NEW BLOCK ENDS HERE ----
}

// ---------- MAIN LOOP ----------
void loop() {
  // Read all incoming GPS data
  while (GPSserial.available()) {
    gps.encode(GPSserial.read());
  }

  // Record only a valid, updated GPS location
  if (gps.location.isUpdated() &&
      gps.location.isValid()) {
    lastValidGPS = millis();
    gpsFixAcquired = true;
  }

  int satellites = gps.satellites.value();
  bool hdopAvailable = gps.hdop.isValid();
  bool locationValid = gps.location.isValid();
  bool speedValid = gps.speed.isValid();

  float gpsSpeed = speedValid ? gps.speed.kmph() : 0.0;

  // Read MPU6050
  sensors_event_t a, g, temp;
  mpu.getEvent(&a, &g, &temp);

  float accelMagnitude = sqrt(
    a.acceleration.x * a.acceleration.x +
    a.acceleration.y * a.acceleration.y +
    a.acceleration.z * a.acceleration.z
  );

  float accelerationChange = fabs(accelMagnitude - 9.81);

  float gyroMagnitude = sqrt(
    g.gyro.x * g.gyro.x +
    g.gyro.y * g.gyro.y +
    g.gyro.z * g.gyro.z
  );

  bool imuStationary =
    accelerationChange < 1.5 &&
    gyroMagnitude < 0.15;


  // NEW: simulation flags now come from the dashboard
  TEST_DEGRADED = (simMode == 1);
  TEST_SPOOFING = (simMode == 2);

// GNSS quality checks


bool poorGPS =
    TEST_DEGRADED ||
    satellites < 4 ||
    (hdopAvailable && gps.hdop.hdop() > 3.0);

  bool goodGPS =
    satellites >= 4 &&
    hdopAvailable &&
    gps.hdop.hdop() <= 3.0;

  if (gpsFixAcquired) {
    if (poorGPS) {
      poorGPSCount++;
      goodGPSCount = 0;

      if (poorGPSCount >= 3) {
        gnssDegraded = true;
      }
    } else if (goodGPS) {
      goodGPSCount++;
      poorGPSCount = 0;

      if (goodGPSCount >= 3) {
        gnssDegraded = false;
      }
    }
  }
  
  // Determine current status
  String currentStatus;

  if (!gpsFixAcquired) {
    currentStatus = "GPS ACQUIRING";
  } else if (simMode == 3 || millis() - lastValidGPS > GPS_TIMEOUT) {
    currentStatus = "GNSS BLACKOUT";
  } else if (gnssDegraded) {
    currentStatus = "GNSS DEGRADED";
  } else if (locationValid && speedValid &&
           (TEST_SPOOFING || gpsSpeed > 5.0) &&
           imuStationary) {
  currentStatus = "SUSPECTED SPOOFING";
  } else {
    currentStatus = "NORMAL";
  }
      currentStatus = "NORMAL";
  }

  xSemaphoreTake(snapMutex, portMAX_DELAY);
  strlcpy(snap.status, toCode(currentStatus), sizeof(snap.status));
  snap.sats = satellites;
  snap.hdopOk = hdopAvailable;
  snap.hdop = hdopAvailable ? gps.hdop.hdop() : 0;
  snap.hasFix = locationValid && gps.location.age() < 5000;  // stale position = no fix
  snap.lat = gps.location.lat();
  snap.lon = gps.location.lng();
  snap.gpsSpeed = gpsSpeed;
  snap.accelMag = accelMagnitude;
  snap.accelChange = accelerationChange;
  snap.gyroMag = gyroMagnitude;
  xSemaphoreGive(snapMutex);

  // Serial Monitor readings
  Serial.println("\n----------------");
  Serial.print("Satellites: ");
  Serial.println(satellites);
  Serial.print("HDOP: ");
if (hdopAvailable) {
  Serial.println(gps.hdop.hdop());
} else {
  Serial.println("Unavailable");
}

  if (locationValid) {
    Serial.print("Latitude: ");
    Serial.println(gps.location.lat(), 6);

    Serial.print("Longitude: ");
    Serial.println(gps.location.lng(), 6);
  } else {
    Serial.println("GPS location unavailable");
  }

  Serial.print("GPS Speed: ");
  Serial.println(gpsSpeed);

  Serial.print("Acceleration change: ");
  Serial.println(accelerationChange);

  Serial.print("Gyro magnitude: ");
  Serial.println(gyroMagnitude);

  Serial.print("SENTINEL STATUS: ");
  Serial.println(currentStatus);

  // LCD display
  // ---------- LCD DISPLAY ----------
static unsigned long lastScreenChange = 0;
static bool showLocation = true;

if (millis() - lastScreenChange >= 3000) {
  lastScreenChange = millis();
  showLocation = !showLocation;
}

lcd.setCursor(0, 0);
lcd.print("                ");
lcd.setCursor(0, 1);
lcd.print("                ");

if (!gpsFixAcquired) {
  lcd.setCursor(0, 0);
  lcd.print("SENTINEL");
  lcd.setCursor(0, 1);
  lcd.print("GPS acquiring");
}
else if (showLocation) {
  lcd.setCursor(0, 0);
  if (locationValid) {
    lcd.print("LAT:");
    lcd.print(gps.location.lat(), 4);
  } else {
    lcd.print("LAT: unavailable");
  }

  lcd.setCursor(0, 1);
  if (locationValid) {
    lcd.print("LON:");
    lcd.print(gps.location.lng(), 4);
  } else {
    lcd.print("LON: unavailable");
  }
}
else {
  lcd.setCursor(0, 0);
  lcd.print("SATS:");
  lcd.print(satellites);
  lcd.print(" HDOP:");
  if (hdopAvailable) {
    lcd.print(gps.hdop.hdop(), 1);
  } else {
    lcd.print("--");
  }

  lcd.setCursor(0, 1);
  lcd.print(currentStatus);
}
  // Send SMS only on status changes
  if (currentStatus != previousStatus) {
    StatusEvent ev;
    strlcpy(ev.from, toCode(previousStatus), sizeof(ev.from));
    strlcpy(ev.to,   toCode(currentStatus),  sizeof(ev.to));
    ev.sats = satellites;
    ev.hdop = hdopAvailable ? gps.hdop.hdop() : 0;
    ev.spd  = gpsSpeed;
    xQueueSend(eventQueue, &ev, 0);

    bool isAbnormal =
      currentStatus == "GNSS BLACKOUT" ||
      currentStatus == "GNSS DEGRADED" ||
      currentStatus == "SUSPECTED SPOOFING";

    bool wasAbnormal =
      previousStatus == "GNSS BLACKOUT" ||
      previousStatus == "GNSS DEGRADED" ||
      previousStatus == "SUSPECTED SPOOFING";

    // No startup alert before the first GPS fix
    if (isAbnormal) {
      String message = "SENTINEL ALERT: " + currentStatus;

      if (locationValid) {
        message += "\nLat: ";
        message += String(gps.location.lat(), 6);
        message += "\nLon: ";
        message += String(gps.location.lng(), 6);
      } else {
        message += "\nGPS location unavailable.";
      }

      queueSMS(message);
    }

    // Notify recovery only after a real alert
    if (currentStatus == "NORMAL" && wasAbnormal) {
      String message = "SENTINEL: GPS/status recovered.";

      if (locationValid) {
        message += "\nLat: ";
        message += String(gps.location.lat(), 6);
        message += "\nLon: ";
        message += String(gps.location.lng(), 6);
      }

      queueSMS(message);
    }

    previousStatus = currentStatus;
  }

  delay(1000);
}
