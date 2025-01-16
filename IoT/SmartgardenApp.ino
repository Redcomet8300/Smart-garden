#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <time.h>

// WiFi credentials
const char* ssid = "Your_SSID";
const char* password = "Your_PASSWORD";

// GPIO Pins
const int solenoidValvePin = 26; // Connected to relay

// Variables
bool isValveOpen = false;
bool isScheduledWatering = false; // To track if schedule is running

// Create an AsyncWebServer object on port 80
AsyncWebServer server(80);

// Function to open/close the valve
void setValve(bool state) {
  isValveOpen = state;
  digitalWrite(solenoidValvePin, state ? HIGH : LOW);
}

// Setup time using Network Time Protocol (NTP)
void setupTime() {
  configTime(0, 0, "pool.ntp.org"); // Use NTP server to sync time
  Serial.print("Syncing time");
  while (!time(nullptr)) {
    Serial.print(".");
    delay(1000);
  }
  Serial.println("\nTime synced!");
}

// Format time as HH:MM
String getCurrentTime() {
  time_t now = time(nullptr);
  struct tm* timeinfo = localtime(&now);
  char buffer[6];
  strftime(buffer, sizeof(buffer), "%H:%M", timeinfo);
  return String(buffer);
}

void setup() {
  // Initialize Serial Monitor
  Serial.begin(115200);

  // Initialize GPIOs
  pinMode(solenoidValvePin, OUTPUT);
  digitalWrite(solenoidValvePin, LOW); // Valve off by default

  // Connect to WiFi
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }
  Serial.println("\nConnected to WiFi!");
  Serial.println("IP Address: " + WiFi.localIP().toString());

  // Sync time
  setupTime();

  // Handle web server routes
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    String html = "<!DOCTYPE html><html><head><title>Smart Farm</title>";
    html += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
    html += "<style>body {font-family: Arial; text-align: center;}";
    html += ".btn {padding: 10px 20px; font-size: 18px; margin: 10px; border: none; cursor: pointer;}";
    html += ".on {background-color: green; color: white;}";
    html += ".off {background-color: red; color: white;}</style></head>";
    html += "<body><h1>Smart Farm Control</h1>";
    html += "<p>Current Time: " + getCurrentTime() + "</p>";
    html += "<p>Valve Status: " + String(isValveOpen ? "Open" : "Closed") + "</p>";
    html += "<button class='btn " + String(isValveOpen ? "off" : "on") + 
            "' onclick='toggleValve()'>" + String(isValveOpen ? "Close Valve" : "Open Valve") + "</button>";
    html += "<script>function toggleValve() {fetch('/toggle');location.reload();}</script>";
    html += "</body></html>";
    request->send(200, "text/html", html);
  });

  server.on("/toggle", HTTP_GET, [](AsyncWebServerRequest *request) {
    isScheduledWatering = false; // Disable scheduled watering if manually toggled
    setValve(!isValveOpen);
    request->send(200, "text/plain", "OK");
  });

  // Start server
  server.begin();
}

void loop() {
  // Get the current time
  String currentTime = getCurrentTime();

  // Check if it's 8:00 AM or 8:00 PM for scheduled watering
  if ((currentTime == "08:00" || currentTime == "20:00") && !isScheduledWatering) {
    isScheduledWatering = true;
    setValve(true); // Open valve
    Serial.println("Scheduled watering started.");
  }

  // Close the valve automatically after 10 minutes (600,000 ms)
  if (isScheduledWatering && millis() % 600000 == 0) {
    setValve(false); // Close valve
    isScheduledWatering = false;
    Serial.println("Scheduled watering ended.");
  }

  delay(1000); // Avoid excessive CPU usage
}
