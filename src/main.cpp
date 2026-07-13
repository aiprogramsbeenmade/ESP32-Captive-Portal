#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include "CaptiveDNS.h"

// ---------------------------------------------------------------------------
// Configurazione Access Point
// ---------------------------------------------------------------------------
static const char* AP_SSID     = "ESP32 - Test";
static const char* AP_PASSWORD = nullptr; // AP aperto: più affidabile per il captive portal detection
static IPAddress    AP_IP(192, 168, 4, 1);
static IPAddress    AP_GATEWAY(192, 168, 4, 1);
static IPAddress    AP_SUBNET(255, 255, 255, 0);

static const char* CONFIG_PATH = "/config.json";

WebServer   server(80);
CaptiveDNS  captiveDNS;

// ---------------------------------------------------------------------------
// Helpers per servire file statici da LittleFS
// ---------------------------------------------------------------------------
static bool serveFile(const String& path, const char* contentType) {
    if (!LittleFS.exists(path)) return false;
    File f = LittleFS.open(path, "r");
    server.streamFile(f, contentType);
    f.close();
    return true;
}

static void handleRoot() {
    if (!serveFile("/index.html", "text/html")) {
        server.send(500, "text/plain", "index.html mancante in LittleFS");
    }
}

static void handleStyle() {
    if (!serveFile("/style.css", "text/css")) {
        server.send(404, "text/plain", "not found");
    }
}

static void handleAppJs() {
    if (!serveFile("/app.js", "application/javascript")) {
        server.send(404, "text/plain", "not found");
    }
}

// ---------------------------------------------------------------------------
// Step 5: ricezione del payload JSON dal form e scrittura permanente su Flash
// ---------------------------------------------------------------------------
static void handleSubmit() {
    if (server.method() != HTTP_POST) {
        server.send(405, "application/json", "{\"error\":\"method not allowed\"}");
        return;
    }

    if (!server.hasArg("plain")) {
        server.send(400, "application/json", "{\"error\":\"body mancante\"}");
        return;
    }

    const String body = server.arg("plain");

    JsonDocument doc; // ArduinoJson v7: documento a dimensione dinamica
    DeserializationError err = deserializeJson(doc, body);
    if (err) {
        server.send(400, "application/json", "{\"error\":\"JSON non valido\"}");
        return;
    }

    // Validazione minima dei campi attesi dal form (vedi data/index.html)
    if (!doc["email"].is<const char*>() || !doc["password"].is<const char*>()) {
        server.send(422, "application/json", "{\"error\":\"Campi obbligatori mancanti\"}");
        return;
    }

    // Scrittura permanente su LittleFS
    File f = LittleFS.open(CONFIG_PATH, "w");
    if (!f) {
        server.send(500, "application/json", "{\"error\":\"impossibile scrivere su flash\"}");
        return;
    }
    serializeJson(doc, f);
    f.close();

    Serial.println("[SUBMIT] Configurazione salvata in " + String(CONFIG_PATH));
    server.send(200, "application/json", "{\"status\":\"ok\"}");
}

// ---------------------------------------------------------------------------
// Step 4: riconoscimento delle richieste "non-locali" generate dai sistemi
// operativi per rilevare la presenza di un captive portal, e redirect 302
// verso la pagina di login sull'IP dell'AP.
// ---------------------------------------------------------------------------
static bool isCaptivePortalProbe(const String& host, const String& uri) {
    // Endpoint noti usati da Android / iOS / Windows per il probe di connettività
    static const char* knownProbes[] = {
        "/generate_204",        // Android
        "/gen_204",              // Android (varianti Chrome)
        "/hotspot-detect.html",  // iOS / macOS
        "/library/test/success.html", // iOS (variante)
        "/ncsi.txt",             // Windows
        "/connecttest.txt",      // Windows
        "/success.txt"           // Firefox
    };

    for (const char* probe : knownProbes) {
        if (uri.equalsIgnoreCase(probe)) return true;
    }

    // Se l'Host richiesto non è l'IP del nostro AP, la richiesta è "non-locale"
    if (host.length() > 0 && host != AP_IP.toString()) {
        return true;
    }

    return false;
}

static void handleNotFound() {
    const String host = server.hostHeader();
    const String uri  = server.uri();

    if (isCaptivePortalProbe(host, uri)) {
        // Redirect 302 verso la pagina di login: questo è ciò che fa scattare
        // l'apertura automatica della webview di captive portal sullo smartphone.
        server.sendHeader("Location", String("http://") + AP_IP.toString() + "/", true);
        server.send(302, "text/plain", "");
        return;
    }

    server.send(404, "text/plain", "404: risorsa non trovata");
}

// ---------------------------------------------------------------------------
// Pannello di debug: GET /admin (protetto da Basic Auth) per ispezionare
// rapidamente l'ultima configurazione salvata su LittleFS.
// ---------------------------------------------------------------------------
static const char* ADMIN_USER = "admin";
static const char* ADMIN_PASS = "admin"; // TODO: cambiare prima di un uso reale

static void handleAdmin() {
    if (!server.authenticate(ADMIN_USER, ADMIN_PASS)) {
        return server.requestAuthentication(); // fa comparire il prompt user/password del browser
    }

    String configContent = "(nessun file /config.json presente)";
    if (LittleFS.exists(CONFIG_PATH)) {
        File f = LittleFS.open(CONFIG_PATH, "r");
        configContent = f.readString();
        f.close();
    }

    // Pretty-print del JSON per leggibilità
    JsonDocument doc;
    String pretty = configContent;
    if (deserializeJson(doc, configContent) == DeserializationError::Ok) {
        pretty = "";
        serializeJsonPretty(doc, pretty);
    }

    String html = "<!DOCTYPE html><html lang='it'><head><meta charset='UTF-8'>";
    html += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
    html += "<title>Admin - Configurazione salvata</title>";
    html += "<style>body{font-family:monospace;background:#111;color:#e5e5e5;padding:24px;}";
    html += "h1{color:#fff;font-size:1.1rem;} pre{background:#1c1c1c;padding:16px;border-radius:8px;";
    html += "overflow-x:auto;border:1px solid #333;} .path{color:#6b7280;font-size:0.8rem;}</style></head><body>";
    html += "<h1>Ultima configurazione salvata</h1>";
    html += "<p class='path'>" + String(CONFIG_PATH) + "</p>";
    html += "<pre>" + pretty + "</pre>";
    html += "</body></html>";

    server.send(200, "text/html", html);
}


void setup() {
    Serial.begin(115200);
    delay(300);
    Serial.println("\n[BOOT] Avvio servizi ESP32-S3 captive portal...");

    // 1. Mount LittleFS (formatta automaticamente se non è mai stato inizializzato)
    if (!LittleFS.begin(true)) {
        Serial.println("[FATAL] Impossibile montare LittleFS");
        return;
    }
    Serial.println("[FS] LittleFS montato correttamente");

    // 1. Access Point
    WiFi.mode(WIFI_AP);
    WiFi.softAPConfig(AP_IP, AP_GATEWAY, AP_SUBNET);
    WiFi.softAP(AP_SSID, AP_PASSWORD);
    Serial.printf("[AP] SSID='%s' IP=%s\n", AP_SSID, WiFi.softAPIP().toString().c_str());

    // 1. DNS captive su porta 53
    if (captiveDNS.start(AP_IP)) {
        Serial.println("[DNS] CaptiveDNS in ascolto sulla porta 53");
    } else {
        Serial.println("[DNS] Errore nell'avvio di CaptiveDNS");
    }

    // Routing HTTP
    server.on("/", HTTP_GET, handleRoot);
    server.on("/style.css", HTTP_GET, handleStyle);
    server.on("/app.js", HTTP_GET, handleAppJs);
    server.on("/api/submit", HTTP_POST, handleSubmit);
    server.on("/admin", HTTP_GET, handleAdmin);
    server.onNotFound(handleNotFound);

    server.begin();
    Serial.println("[HTTP] Web server in ascolto sulla porta 80");
}

// ---------------------------------------------------------------------------
// Loop
// ---------------------------------------------------------------------------
void loop() {
    captiveDNS.handle();
    server.handleClient();
}
