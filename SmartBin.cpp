#define BLYNK_TEMPLATE_ID "TMPL3t-d1h7jz"
#define BLYNK_TEMPLATE_NAME "Quickstart Device"
#define BLYNK_AUTH_TOKEN "oJl2ujVC4flFo6CejDs232OB1MU8w0bG"
#define BLYNK_PRINT Serial

#include <ESP8266WiFi.h>
#include <BlynkSimpleEsp8266.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <DHT.h>

// -------- Pins --------
#define TRIG D5
#define ECHO D6
#define DHTPIN D7
#define DHTTYPE DHT11

DHT dht(DHTPIN, DHTTYPE);

// -------- WiFi --------
char ssid[] = "HAZELBLADE.";
char pass[] = "12345678";

// -------- Hotspot --------
const char* apName = "SmartBin";
const char* apPass = "12345678";

WiFiServer server(80);

// -------- OLED --------
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// -------- Settings --------
float binHeight = 30.0;
float temperature = 0;

unsigned long lastMeasure = 0;
int percent = 0;
bool alertSent = false;
bool fireAlertSent = false;

// =====================================================
float getDistance() {
  digitalWrite(TRIG, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG, LOW);

  long duration = pulseIn(ECHO, HIGH, 30000);
  return duration * 0.034 / 2;
}

// =====================================================
String getStatus(int p) {
  if (p < 30) return "EMPTY";
  if (p < 80) return "MID";
  return "FULL";
}

// =====================================================
void setup() {
  Serial.begin(9600);

  pinMode(TRIG, OUTPUT);
  pinMode(ECHO, INPUT);

  dht.begin();
  Wire.begin(D2, D1);

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("OLED failed");
    while (1);
  }

  display.setTextColor(WHITE);

  // Hotspot
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(apName, apPass);

  Serial.print("AP IP: ");
  Serial.println(WiFi.softAPIP());

  // WiFi for Blynk
  WiFi.begin(ssid, pass);
  Serial.print("Connecting WiFi");

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.print("Local IP: ");
  Serial.println(WiFi.localIP());

  Blynk.config(BLYNK_AUTH_TOKEN);
  Blynk.connect();

  server.begin();
  Serial.println("System Ready");
}

// =====================================================
void loop() {
  Blynk.run();

  if (millis() - lastMeasure > 3000) {
    lastMeasure = millis();

    // ---- Garbage ----
    float distance = getDistance();
    float filled = binHeight - distance;
    percent = constrain((filled / binHeight) * 100, 0, 100);

    // ---- Temperature ----
    temperature = dht.readTemperature();
    if (isnan(temperature)) temperature = 0;

    String status = getStatus(percent);

    // -------- OLED --------
    display.clearDisplay();

    display.setTextSize(1);
    display.setCursor(0, 0);
    display.println("Smart Garbage Bin");

    display.setTextSize(2);
    display.setCursor(0, 16);
    display.print(percent);
    display.print("%");

    display.setTextSize(1);
    display.setCursor(0, 40);
    display.print("Temp: ");
    display.print(temperature);
    display.print(" C");

    display.setCursor(0, 54);
    display.print("Status: ");
    display.print(status);

    display.display();

    // -------- Blynk --------
    Blynk.virtualWrite(V0, percent);
    Blynk.virtualWrite(V1, temperature);

    // 🔔 Bin full alert
    if (percent >= 90 && !alertSent) {
      Blynk.logEvent("bin_full", "⚠ Garbage Bin FULL!");
      alertSent = true;
    }
    if (percent < 80) alertSent = false;

    // 🔥 Fire alert
    if (temperature >= 50 && !fireAlertSent) {
      Blynk.logEvent("fire_alert", "High Temperature Detected!");
      fireAlertSent = true;
    }
    if (temperature < 45) fireAlertSent = false;

    Serial.print("Level: ");
    Serial.print(percent);
    Serial.print("  Temp: ");
    Serial.println(temperature);
  }

  // ================= WEB =================
  WiFiClient client = server.available();
  if (client) {
    client.println("HTTP/1.1 200 OK");
    client.println("Content-Type: text/html\n");

    client.println("<html><head>");
    client.println("<meta http-equiv='refresh' content='3'>");
    client.println("<title>Smart Bin</title></head><body>");
    client.println("<h1>Smart Garbage Monitor</h1>");
    client.println("<h2>Level: " + String(percent) + "%</h2>");
    client.println("<h2>Temp: " + String(temperature) + " C</h2>");
    client.println("<h3>Status: " + getStatus(percent) + "</h3>");
    client.println("</body></html>");

    client.stop();
  }
}