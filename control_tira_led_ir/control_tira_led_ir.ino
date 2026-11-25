#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <IRremoteESP8266.h>
#include <IRsend.h>

const char* WIFI_SSID = "TU_RED";
const char* WIFI_PASS = "TU_CLAVE";
const char* API_TOKEN = "cambia-este-token-por-uno-largo";
const char* HOSTNAME = "tira-led";
const uint16_t IR_PIN = 4;
const unsigned long MIN_GAP_MS = 120;

struct Command {
  const char* name;
  uint32_t code;
};

// Códigos NEC del mando de 24 botones, fila por fila.
const Command COMMANDS[] = {
  {"brightness_up", 0x00F700FF}, {"brightness_down", 0x00F7807F}, {"off", 0x00F740BF}, {"on", 0x00F7C03F},
  {"red", 0x00F720DF},     {"green", 0x00F7A05F},    {"blue", 0x00F7609F},     {"white", 0x00F7E01F},
  {"color_1", 0x00F710EF}, {"color_2", 0x00F7906F},  {"color_3", 0x00F750AF},  {"flash", 0x00F7D02F},
  {"color_4", 0x00F730CF}, {"color_5", 0x00F7B04F},  {"color_6", 0x00F7708F},  {"strobe", 0x00F7F00F},
  {"color_7", 0x00F708F7}, {"color_8", 0x00F78877},  {"color_9", 0x00F748B7},  {"fade", 0x00F7C837},
  {"color_10", 0x00F728D7}, {"color_11", 0x00F7A857}, {"color_12", 0x00F76897}, {"smooth", 0x00F7E817},
};

IRsend ir(IR_PIN);
WebServer server(80);
String lastCommand = "";
unsigned long lastSentAt = 0;

bool authorized() {
  return server.header("Authorization") == String("Bearer ") + API_TOKEN;
}

void reply(int status, const String& json) {
  server.sendHeader("Cache-Control", "no-store");
  server.send(status, "application/json", json);
}

const Command* findCommand(const String& name) {
  for (const Command& c : COMMANDS) {
    if (name == c.name) return &c;
  }
  return nullptr;
}

void handleStatus() {
  if (!authorized()) return reply(401, "{\"error\":\"unauthorized\"}");
  String last = lastCommand.length() ? "\"" + lastCommand + "\"" : String("null");
  reply(200, String("{\"device\":\"") + HOSTNAME + "\",\"ip\":\"" + WiFi.localIP().toString() +
                 "\",\"rssi\":" + WiFi.RSSI() + ",\"uptime_ms\":" + millis() + ",\"last_command\":" + last + "}");
}

void handleCommand() {
  if (!authorized()) return reply(401, "{\"error\":\"unauthorized\"}");
  const Command* c = findCommand(server.arg("command"));
  if (!c) return reply(400, "{\"error\":\"unknown_command\"}");
  if (millis() - lastSentAt < MIN_GAP_MS) return reply(429, "{\"error\":\"rate_limited\"}");

  ir.sendNEC(c->code, 32);
  lastSentAt = millis();
  lastCommand = c->name;
  reply(200, String("{\"sent\":true,\"command\":\"") + c->name + "\"}");
}

void connectWifi() {
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.setHostname(HOSTNAME);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.println(WiFi.localIP());
}

void setup() {
  Serial.begin(115200);
  ir.begin();
  connectWifi();
  MDNS.begin(HOSTNAME);

  const char* headers[] = {"Authorization"};
  server.collectHeaders(headers, 1);
  server.on("/api/v1/status", HTTP_GET, handleStatus);
  server.on("/api/v1/command", HTTP_POST, handleCommand);
  server.onNotFound([] { reply(404, "{\"error\":\"not_found\"}"); });
  server.begin();
}

void loop() {
  server.handleClient();

  static unsigned long lastCheck = 0;
  if (millis() - lastCheck > 10000) {
    lastCheck = millis();
    if (WiFi.status() != WL_CONNECTED) WiFi.reconnect();
  }
}
