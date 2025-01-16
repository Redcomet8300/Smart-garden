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
    String html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <title>Smart Farm Control</title>
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <style>
    body {
      font-family: 'Arial', sans-serif;
      margin: 0;
      padding: 0;
      background-color: #f4f4f9;
      color: #333;
    }
    header {
      background-color: #4CAF50;
      color: white;
      padding: 15px;
      text-align: center;
      font-size: 24px;
      box-shadow: 0 4px 6px rgba(0, 0, 0, 0.1);
    }
    .container {
      max-width: 800px;
      margin: 20px auto;
      padding: 20px;
      background: white;
      border-radius: 8px;
      box-shadow: 0 4px 6px rgba(0, 0, 0, 0.1);
    }
    .card {
      background: #f9f9f9;
      padding: 15px;
      border-radius: 8px;
      margin-bottom: 20px;
      box-shadow: 0 2px 4px rgba(0, 0, 0, 0.1);
    }
    button {
      padding: 10px 20px;
      font-size: 16px;
      border: none;
      border-radius: 5px;
      cursor: pointer;
      margin-top: 10px;
    }
    .btn-on {
      background-color: #4CAF50;
      color: white;
    }
    .btn-off {
      background-color: #f44336;
      color: white;
    }
    input[type="time"], input[type="number"] {
      padding: 10px;
      font-size: 16px;
      margin: 10px 0;
      border: 1px solid #ccc;
      border-radius: 5px;
      width: 100%;
    }
    input[type="submit"] {
      background-color: #4CAF50;
      color: white;
      padding: 10px 20px;
      border: none;
      border-radius: 5px;
      cursor: pointer;
      font-size: 16px;
    }
    footer {
      text-align: center;
      padding: 10px;
      background-color: #333;
      color: white;
      margin-top: 20px;
      font-size: 14px;
    }
  </style>
</head>
<body>
  <header>
    Smart Farm Control
  </header>
  <div class="container">
    <div class="card">
      <h2>Current Status</h2>
      <p><strong>Current Time:</strong> %TIME%</p>
      <p><strong>Valve Status:</strong> <span>%VALVE_STATUS%</span></p>
      <button class="btn %VALVE_BUTTON_CLASS%" onclick="toggleValve()">
        %VALVE_BUTTON_TEXT%
      </button>
    </div>
    <div class="card">
      <h2>Set Watering Schedule</h2>
      <form method="GET" action="/set">
        <label for="morning">Morning Time:</label>
        <input type="time" id="morning" name="morning" value="%MORNING_TIME%">
        <label for="evening">Evening Time:</label>
        <input type="time" id="evening" name="evening" value="%EVENING_TIME%">
        <label for="duration">Watering Duration (minutes):</label>
        <input type="number" id="duration" name="duration" value="%DURATION%">
        <input type="submit" value="Save Settings">
      </form>
    </div>
  </div>
  <footer>
    &copy; 2025 Smart Farm System
  </footer>
  <script>
    function toggleValve() {
      fetch('/toggle').then(() => location.reload());
    }
  </script>
</body>
</html>
)rawliteral";

    // Replace placeholders with dynamic values
    html.replace("%TIME%", getCurrentTime());
    html.replace("%VALVE_STATUS%", isValveOpen ? "Open" : "Closed");
    html.replace("%VALVE_BUTTON_CLASS%", isValveOpen ? "btn-off" : "btn-on");
    html.replace("%VALVE_BUTTON_TEXT%", isValveOpen ? "Close Valve" : "Open Valve");
    html.replace("%MORNING_TIME%", morningTime);
    html.replace("%EVENING_TIME%", eveningTime);
    html.replace("%DURATION%", String(wateringDuration));
    
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
