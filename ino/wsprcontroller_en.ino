//WSPR controller by SP3VSS
//Not for commercial use
//Copyright by VSS 2k26


#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <WiFiUdp.h>
#include <NTPClient.h>
#include <LittleFS.h>
#include <ArduinoJson.h>

//------------------ KONFIGURACJA APLIKACJI ------------------
char SIGN[16] = "SP3VSS";
char LOC[16]  = "JO82";
int  POW      = 23;
long FREQ     = 14097100;

int timeOffsetHours = 1;

//------------------ OSTATNIO WYSLANY STRING ------------------
char lastSentBuffer[64]  = "--- fault ---";
char lastSentTime[6]     = "--:--";
unsigned long lastSentId = 0;

//------------------ OSTATNIO ODEBRANY UART ------------------
char lastUartBuffer[128] = "--- fault ---";
char lastUartTime[9]     = "--:--:--";
unsigned long lastUartId = 0;

//------------------ HARMONOGRAMY (5 sztuk) ------------------
#define NUM_SCHED 5

struct Schedule {
  bool enabled;
  bool days[7];
  int  sendH, sendM;
  long freq;
  bool sentThisMinute;
};

Schedule scheds[NUM_SCHED] = {
  {false, {true,true,true,true,true,true,true}, 20,0, 14097100, false},
  {false, {true,true,true,true,true,true,true}, 20,0, 14097100, false},
  {false, {true,true,true,true,true,true,true}, 20,0, 14097100, false},
  {false, {true,true,true,true,true,true,true}, 20,0, 14097100, false},
  {false, {true,true,true,true,true,true,true}, 20,0, 14097100, false},
};

//------------------ KONFIGURACJA WYSWIETLACZA ------------------
#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT  64
#define OLED_RESET     -1

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

//------------------ PRZYCISK / WYGASZANIE ------------------
#define BUTTON_PIN      15
#define DIM_TIMEOUT_MS  60000UL

bool          displayOn  = true;
unsigned long dimMillis  = 0;

//------------------ KONFIGURACJA WIFI ------------------
const char* WIFI_SSID     = "Xiaomi2G";
const char* WIFI_PASSWORD = "dupablada";

const bool USE_STATIC_IP = false;

IPAddress local_IP(192,168,1,50);
IPAddress gateway(192,168,1,1);
IPAddress subnet(255,255,255,0);
IPAddress dns(8,8,8,8);

//------------------ HTTP SERWER ------------------
ESP8266WebServer server(80);

//------------------ NTP ------------------
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "vega.cnk.poznan.pl", 0, 60 * 60 * 1000);

//------------------ LOGIKA APLIKACJI ------------------
bool alreadySentHW = false;

//------------------ PLIK USTAWIEN W LittleFS ------------------
#define SETTINGS_FILE "/settings.json"

//------------------ PROTOTYPY ------------------
void loadSettings();
void saveSettings();
void drawStartScreen();
void showIpOnOled();        // NOWA
void showConfigOnOled();
void dimDisplay();
void wakeDisplay();
void sendConfig();
void readUart();
void handleRoot();
void handleSave();
void handleSend();
void handleStatus();
void handleUartStatus();
void checkSchedules();
String makeSchedBlock(int idx);
String makeFreqSelect(const char* name, long current);
String getNtpDate();
String getNtpTime();
String getNtpTimeFull();
static void printCentered(const char* txt, int y);

//==================================================
// ZAPIS / ODCZYT USTAWIEN (LittleFS + JSON)
//==================================================
void saveSettings() {
  StaticJsonDocument<1024> doc;

  doc["sign"] = SIGN;
  doc["loc"]  = LOC;
  doc["pow"]  = POW;
  doc["freq"] = FREQ;
  doc["tz"]   = timeOffsetHours;

  JsonArray arr = doc.createNestedArray("scheds");
  for (int i = 0; i < NUM_SCHED; i++) {
    JsonObject o = arr.createNestedObject();
    o["en"]    = scheds[i].enabled;
    o["sendH"] = scheds[i].sendH;
    o["sendM"] = scheds[i].sendM;
    o["freq"]  = scheds[i].freq;
    JsonArray days = o.createNestedArray("days");
    for (int d = 0; d < 7; d++) days.add(scheds[i].days[d]);
  }

  File f = LittleFS.open(SETTINGS_FILE, "w");
  if (!f) return;
  serializeJson(doc, f);
  f.close();
}

void loadSettings() {
  if (!LittleFS.exists(SETTINGS_FILE)) return;

  File f = LittleFS.open(SETTINGS_FILE, "r");
  if (!f) return;

  StaticJsonDocument<1024> doc;
  DeserializationError err = deserializeJson(doc, f);
  f.close();

  if (err) return;

  if (doc.containsKey("sign")) {
    strncpy(SIGN, doc["sign"] | SIGN, sizeof(SIGN)-1); SIGN[sizeof(SIGN)-1] = '\0';
  }
  if (doc.containsKey("loc")) {
    strncpy(LOC,  doc["loc"]  | LOC,  sizeof(LOC)-1);  LOC[sizeof(LOC)-1]   = '\0';
  }
  if (doc.containsKey("pow"))  POW  = doc["pow"]  | POW;
  if (doc.containsKey("freq")) FREQ = doc["freq"] | FREQ;
  if (doc.containsKey("tz"))   timeOffsetHours = doc["tz"] | timeOffsetHours;

  JsonArray arr = doc["scheds"];
  for (int i = 0; i < NUM_SCHED && i < (int)arr.size(); i++) {
    JsonObject o      = arr[i];
    scheds[i].enabled = o["en"]    | false;
    scheds[i].sendH   = o["sendH"] | 20;
    scheds[i].sendM   = o["sendM"] | 0;
    scheds[i].freq    = o["freq"]  | 14097100L;
    JsonArray days    = o["days"];
    for (int d = 0; d < 7 && d < (int)days.size(); d++)
      scheds[i].days[d] = days[d] | true;
    scheds[i].sentThisMinute = false;
  }
}

//==================================================
// POMOCNICZE: data i czas z NTP
//==================================================
String getNtpDate() {
  unsigned long epoch = timeClient.getEpochTime();
  unsigned long days  = epoch / 86400UL;
  int y = 1970;
  while (true) {
    bool leap = (y % 4 == 0 && (y % 100 != 0 || y % 400 == 0));
    int diy = leap ? 366 : 365;
    if (days < (unsigned long)diy) break;
    days -= diy;
    y++;
  }
  int mdays[] = {31,28,31,30,31,30,31,31,30,31,30,31};
  bool leap = (y % 4 == 0 && (y % 100 != 0 || y % 400 == 0));
  if (leap) mdays[1] = 29;
  int m = 0;
  while (days >= (unsigned long)mdays[m]) { days -= mdays[m]; m++; }
  int day = (int)days + 1;
  int mon = m + 1;
  char buf[12];
  snprintf(buf, sizeof(buf), "%02d-%02d-%04d", day, mon, y);
  return String(buf);
}

String getNtpTime() {
  char buf[6];
  snprintf(buf, sizeof(buf), "%02d:%02d",
           timeClient.getHours(), timeClient.getMinutes());
  return String(buf);
}

String getNtpTimeFull() {
  char buf[9];
  snprintf(buf, sizeof(buf), "%02d:%02d:%02d",
           timeClient.getHours(),
           timeClient.getMinutes(),
           timeClient.getSeconds());
  return String(buf);
}

//==================================================
// POMOCNICZA: wycentrowanie tekstu na OLED
//==================================================
static void printCentered(const char* txt, int y) {
  int16_t x1, y1;
  uint16_t w, h;
  display.getTextBounds(txt, 0, 0, &x1, &y1, &w, &h);
  display.setCursor((SCREEN_WIDTH - w) / 2, y);
  display.print(txt);
}

//==================================================
// WYGASZANIE I BUDZENIE EKRANU
//==================================================
void dimDisplay() {
  display.ssd1306_command(SSD1306_DISPLAYOFF);
  displayOn = false;
}

void wakeDisplay() {
  display.ssd1306_command(SSD1306_DISPLAYON);
  displayOn = true;
  dimMillis = millis();
}

//==================================================
// EKRAN STARTOWY
//==================================================
void drawStartScreen() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  display.setTextSize(2);
  printCentered("WSPR", 0);

  display.setTextSize(1);
  printCentered("Controller", 20);

  display.setTextSize(2);
  printCentered("SP3VSS", 36);

  display.display();
  wakeDisplay();
}

//==================================================
// EKRAN IP PO POLACZENIU Z WIFI (5 sekund)
//==================================================
void showIpOnOled() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  IPAddress ip = WiFi.localIP();
  char ipBuf[24];
  snprintf(ipBuf, sizeof(ipBuf), "%u.%u.%u.%u",
           ip[0], ip[1], ip[2], ip[3]);

  display.setTextSize(1);
  printCentered("WiFi connected", 16);
  printCentered(ipBuf, 32);

  display.display();
  wakeDisplay();  // wlacz + reset timera

  delay(5000);    // trzymamy IP 5 sekund
}

//==================================================
// EKRAN PO WYSLANIU
//==================================================
void showConfigOnOled() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(2);

  printCentered(lastSentTime, 0);

  char freqBuf[12];
  long mhzInt  = FREQ / 1000000L;
  long mhzFrac = (FREQ % 1000000L) / 100L;
  snprintf(freqBuf, sizeof(freqBuf), "%ld.%04ld", mhzInt, mhzFrac);
  printCentered(freqBuf, 20);

  char locPowBuf[16];
  snprintf(locPowBuf, sizeof(locPowBuf), "%s %ddBm", LOC, POW);
  printCentered(locPowBuf, 42);

  display.display();
  wakeDisplay();
}

//==================================================
// WYSYLKA STRINGU CONFIG
//==================================================
void sendConfig() {
  snprintf(lastSentBuffer, sizeof(lastSentBuffer),
           "CONFIG:%s,%s,%d,%ld",
           SIGN, LOC, POW, FREQ);

  snprintf(lastSentTime, sizeof(lastSentTime), "%02d:%02d",
           timeClient.getHours(), timeClient.getMinutes());

  lastSentId++;
  Serial.println(lastSentBuffer);
  showConfigOnOled();
}

//==================================================
// ODCZYT UART (nieblokujacy)
//==================================================
void readUart() {
  static String uartLine = "";
  while (Serial.available()) {
    char c = (char)Serial.read();
    if (c == '\n' || c == '\r') {
      uartLine.trim();
      if (uartLine.length() > 0) {
        strncpy(lastUartBuffer, uartLine.c_str(), sizeof(lastUartBuffer)-1);
        lastUartBuffer[sizeof(lastUartBuffer)-1] = '\0';
        String t = getNtpTimeFull();
        strncpy(lastUartTime, t.c_str(), sizeof(lastUartTime)-1);
        lastUartTime[sizeof(lastUartTime)-1] = '\0';
        lastUartId++;
      }
      uartLine = "";
    } else {
      if (uartLine.length() < 126) uartLine += c;
    }
  }
}

//==================================================
// ENDPOINTY HTTP
//==================================================
void handleStatus() {
  String json = "{\"id\":";
  json += String(lastSentId);
  json += ",\"last\":\"";
  json += String(lastSentBuffer);
  json += "\",\"time\":\"";
  json += String(lastSentTime);
  json += "\"}";
  server.sendHeader("Cache-Control", "no-cache");
  server.send(200, "application/json", json);
}

void handleUartStatus() {
  String json = "{\"id\":";
  json += String(lastUartId);
  json += ",\"data\":\"";
  String data = String(lastUartBuffer);
  data.replace("\"", "&quot;");
  json += data;
  json += "\",\"time\":\"";
  json += String(lastUartTime);
  json += "\"}";
  server.sendHeader("Cache-Control", "no-cache");
  server.send(200, "application/json", json);
}

//==================================================
// SPRAWDZANIE HARMONOGRAMOW
//==================================================
void checkSchedules() {
  timeClient.update();

  int ntpDay   = timeClient.getDay();
  int ourDay   = (ntpDay == 0) ? 6 : (ntpDay - 1);
  int nowH     = timeClient.getHours();
  int nowM     = timeClient.getMinutes();

  for (int i = 0; i < NUM_SCHED; i++) {
    Schedule &s = scheds[i];
    if (!s.enabled) { s.sentThisMinute = false; continue; }
    if (!s.days[ourDay]) { s.sentThisMinute = false; continue; }

    bool isTime = (nowH == s.sendH && nowM == s.sendM);

    if (isTime && !s.sentThisMinute) {
      long savedFreq = FREQ;
      FREQ = s.freq;
      sendConfig();
      FREQ = savedFreq;
      s.sentThisMinute = true;
    }

    if (!isTime) s.sentThisMinute = false;
  }
}

//==================================================
// SETUP
//==================================================
void setup() {
  Serial.begin(9600);
  Wire.begin(4, 5);

  if (!LittleFS.begin()) {
    LittleFS.format();
    LittleFS.begin();
  }

  loadSettings();

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    for (;;);
  }

  pinMode(BUTTON_PIN, INPUT);
  drawStartScreen();

  WiFi.mode(WIFI_STA);
  if (USE_STATIC_IP) {
    WiFi.config(local_IP, gateway, subnet, dns);
  }

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }

  // PO POLACZENIU: pokaz IP na 5 sekund, potem znow ekran startowy
  showIpOnOled();
  drawStartScreen();

  timeClient.begin();
  timeClient.setTimeOffset(timeOffsetHours * 3600);
  timeClient.update();

  server.on("/",       handleRoot);
  server.on("/save",   handleSave);
  server.on("/send",   handleSend);
  server.on("/status", handleStatus);
  server.on("/uart",   handleUartStatus);
  server.begin();
}

//==================================================
// POMOCNICZA: blok harmonogramu
//==================================================
String makeSchedBlock(int idx) {
  Schedule &s = scheds[idx];
  char sendBuf[6];
  snprintf(sendBuf, sizeof(sendBuf), "%02d:%02d", s.sendH, s.sendM);

  String p  = "";
  String si = String(idx);

  p += "<div class='sched-block'>";
  p += "<div class='sched-enable'>";
  p += "<input type='checkbox' name='sched_en" + si + "' id='sched_en" + si + "'";
  if (s.enabled) p += " checked";
  p += "><label for='sched_en" + si + "' style='margin:0;cursor:pointer;'>";
  p += "Schedule " + String(idx + 1) + "</label></div>";

  const char* dayNames[] = {"Mon","Tue","Wed","Thu","Fri","Sat","Sun"};
  p += "<div class='days-row'>";
  for (int d = 0; d < 7; d++) {
    p += "<label class='day-label'>" + String(dayNames[d]);
    p += "<input type='checkbox' name='day" + si + "_" + String(d) + "'";
    if (s.days[d]) p += " checked";
    p += "></label>";
  }
  p += "</div>";

  p += "<div class='sched-row'>";
  p += "<div class='sched-time'>";
  p += "<label style='margin:0;font-size:11px;color:#aaa;'>Hour:</label>";
  p += "<input type='time' name='sched_time" + si + "' value='" + sendBuf + "'>";
  p += "</div>";

  p += "<div class='sched-freq'>";
  p += "<label style='margin:0;font-size:11px;color:#aaa;'>Frequency:</label>";
  p += "<select name='sched_freq" + si + "'>";
  long freqs[]        = {1838100,3570100,5288700,7040100,10140200,13555400,14097100,18106100,21096100,24926100,28126100,50294500};
  const char* bands[] = {"160m","80m","60m","40m","30m","22m","20m","17m","15m","12m","10m","6m"};
  for (int f = 0; f < 12; f++) {
    p += "<option value='" + String(freqs[f]) + "'";
    if (freqs[f] == s.freq) p += " selected";
    p += ">" + String(bands[f]) + " " + String(freqs[f]) + " Hz</option>";
  }
  p += "</select></div>";
  p += "</div>";
  p += "</div>";
  return p;
}

//==================================================
// POMOCNICZA: select czestotliwosci (quick-send)
//==================================================
String makeFreqSelect(const char* name, long current) {
  long freqs[]        = {1838100,3570100,5288700,7040100,10140200,13555400,14097100,18106100,21096100,24926100,28126100,50294500};
  const char* bands[] = {"160m","80m","60m","40m","30m","22m","20m","17m","15m","12m","10m","6m"};
  String s = "<select name='";
  s += name;
  s += "' style='flex:2;min-width:160px;'>";
  for (int f = 0; f < 12; f++) {
    s += "<option value='" + String(freqs[f]) + "'";
    if (freqs[f] == current) s += " selected";
    s += ">" + String(bands[f]) + " " + String(freqs[f]) + " Hz</option>";
  }
  s += "</select>";
  return s;
}

//==================================================
// OBSLUGA HTTP: STRONA GLOWNA
//==================================================
void handleRoot() {
  timeClient.update();
  String dateStr = getNtpDate();
  String timeStr = getNtpTime();

  String page;

  page += F("<!DOCTYPE html><html><head><meta charset='utf-8'>"
            "<meta name='viewport' content='width=device-width,initial-scale=1'>"
            "<title>WSPR Controller</title>"
            "<style>"
            "body{font-family:Arial,Helvetica,sans-serif;background:#111;color:#eee;margin:0;padding:20px;}"
            "h1{color:#0f0;text-align:center;font-size:32px;margin-bottom:2px;}"
            ".datetime{text-align:center;font-size:32px;font-weight:bold;color:#ff8800;margin-bottom:16px;}"
            "h3{color:#0f0;margin-bottom:6px;}"
            ".last-sent{text-align:center;font-size:13px;color:#ff0;margin-bottom:6px;"
              "border:1px solid #444;border-radius:4px;padding:6px 10px;background:#1a1a00;"
              "transition:background 0.4s;}"
            ".last-sent.flash{background:#5a5a00;}"
            ".uart-box{text-align:center;font-size:13px;color:#0ff;margin-bottom:8px;"
              "border:1px solid #444;border-radius:4px;padding:6px 10px;background:#001a1a;"
              "transition:background 0.4s;}"
            ".uart-box.flash{background:#005a5a;}"
            ".sent-time{color:#aaa;font-size:12px;margin-left:10px;}"
            ".uart-time{color:#aaa;font-size:12px;margin-left:10px;}"
            ".quick-send{display:flex;align-items:center;gap:8px;flex-wrap:wrap;"
              "border:1px solid #0a0;border-radius:4px;padding:8px 12px;"
              "background:#001a00;margin-bottom:18px;}"
            ".quick-send label{margin:0;font-size:13px;white-space:nowrap;color:#0f0;font-weight:bold;}"
            ".quick-send select,.quick-send input[type=number],.quick-send input[type=text]{"
              "padding:5px;background:#111;border:1px solid #555;color:#eee;"
              "border-radius:3px;font-size:13px;margin:0;}"
            ".quick-send input[type=number]{width:70px;flex:0 0 70px;}"
            ".quick-send input[type=text]{width:70px;flex:0 0 70px;}"
            ".quick-send button{padding:6px 14px;font-size:13px;background:#0f0;color:#000;"
              "border:none;border-radius:3px;cursor:pointer;margin:0;white-space:nowrap;}"
            ".quick-send button:hover{background:#3f3;}"
            ".wrapper{max-width:900px;margin:0 auto;}"
            ".container{display:flex;flex-direction:row;flex-wrap:nowrap;gap:0;width:900px;}"
            ".col{width:450px;flex:0 0 450px;box-sizing:border-box;padding:10px;}"
            ".col-left{border-right:1px solid #444;}"
            ".box{border:1px solid #444;border-radius:4px;padding:14px;background:#222;height:100%;box-sizing:border-box;}"
            "label{display:block;margin:6px 0;font-size:13px;cursor:pointer;}"
            "input[type=text],input[type=password],input[type=number],input[type=time],select{"
              "width:100%;padding:6px;margin:3px 0;box-sizing:border-box;"
              "background:#111;border:1px solid #555;color:#eee;"
              "border-radius:3px;font-size:13px;cursor:pointer;}"
            "select{appearance:none;-webkit-appearance:none;"
              "background-image:url(\"data:image/svg+xml;utf8,<svg fill='%230f0' height='20' viewBox='0 0 24 24' width='20' xmlns='http://www.w3.org/2000/svg'><path d='M7 10l5 5 5-5z'/></svg>\");"
              "background-repeat:no-repeat;background-position:right 8px center;padding-right:28px;}"
            "select:focus,input:focus{outline:1px solid #0f0;}"
            "select option{background:#222;color:#eee;}"
            "button{padding:10px 20px;font-size:16px;background:#0f0;color:#000;"
              "border:none;border-radius:4px;cursor:pointer;margin-top:10px;}"
            "button:hover{background:#3f3;}"
            "hr{border-color:#444;margin:10px 0;}"
            ".sched-block{border:1px solid #333;border-radius:4px;padding:10px;"
              "margin-bottom:10px;background:#1a1a1a;}"
            ".sched-enable{display:flex;align-items:center;gap:10px;margin-bottom:6px;"
              "font-size:15px;font-weight:bold;color:#0f0;}"
            ".sched-enable input[type=checkbox]{width:20px;height:20px;accent-color:#0f0;"
              "cursor:pointer;margin:0;}"
            ".days-row{display:flex;gap:4px;flex-wrap:wrap;margin:4px 0 8px;}"
            ".day-label{display:flex;flex-direction:column;align-items:center;"
              "font-size:11px;gap:3px;cursor:pointer;min-width:30px;}"
            ".day-label input[type=checkbox]{width:18px;height:18px;accent-color:#0f0;"
              "cursor:pointer;margin:0;}"
            ".sched-row{display:flex;gap:8px;align-items:flex-end;}"
            ".sched-time{flex:0 0 110px;}"
            ".sched-freq{flex:1;}"
            "</style>"
            "<script>"
            "var knownSentId=");
  page += String(lastSentId);
  page += F(";var knownUartId=");
  page += String(lastUartId);
  page += F(";"
            "function pollStatus(){"
              "var xhr=new XMLHttpRequest();"
              "xhr.open('GET','/status',true);"
              "xhr.onload=function(){"
                "if(xhr.status===200){"
                  "var d=JSON.parse(xhr.responseText);"
                  "if(d.id!==knownSentId){"
                    "knownSentId=d.id;"
                    "document.getElementById('lastStr').textContent=d.last;"
                    "document.getElementById('lastTime').textContent='@ '+d.time;"
                    "var box=document.getElementById('lastBox');"
                    "box.classList.add('flash');"
                    "setTimeout(function(){box.classList.remove('flash');},800);"
                  "}"
                "}"
              "};"
              "xhr.send();"
            "}"
            "function pollUart(){"
              "var xhr=new XMLHttpRequest();"
              "xhr.open('GET','/uart',true);"
              "xhr.onload=function(){"
                "if(xhr.status===200){"
                  "var d=JSON.parse(xhr.responseText);"
                  "if(d.id!==knownUartId){"
                    "knownUartId=d.id;"
                  "}"
                  "document.getElementById('uartStr').textContent=d.data;"
                  "document.getElementById('uartTime').textContent=d.time?'@ '+d.time:'';"
                  "var box=document.getElementById('uartBox');"
                  "box.classList.add('flash');"
                  "setTimeout(function(){box.classList.remove('flash');},800);"
                "}"
              "};"
              "xhr.send();"
            "}"
            "function tickClock(){"
              "var el=document.getElementById('clockEl');"
              "if(!el)return;"
              "var now=new Date();"
              "var hh=String(now.getHours()).padStart(2,'0');"
              "var mm=String(now.getMinutes()).padStart(2,'0');"
              "var dd=String(now.getDate()).padStart(2,'0');"
              "var mo=String(now.getMonth()+1).padStart(2,'0');"
              "var yy=now.getFullYear();"
              "el.textContent=dd+'-'+mo+'-'+yy+'   '+hh+':'+mm;"
            "}"
            "setInterval(pollStatus,5000);"
            "setInterval(pollUart,30000);"
            "setInterval(tickClock,1000);"
            "tickClock();"
            "pollUart();"
            "</script>"
            "</head><body>"
            "<div class='wrapper'>"
            "<h1>WSPR Controller</h1>");

  page += F("<p class='datetime' id='clockEl'>");
  page += dateStr + " &nbsp; " + timeStr;
  page += F("</p>");

  page += F("<div class='last-sent' id='lastBox'>"
            "&#128225; Last sent: <b id='lastStr'>");
  page += String(lastSentBuffer);
  page += F("</b><span class='sent-time' id='lastTime'>");
  if (lastSentId > 0) { page += "@ "; page += String(lastSentTime); }
  page += F("</span></div>");

  page += F("<div class='uart-box' id='uartBox'>"
            "&#128281; Last received: <b id='uartStr'>");
  page += String(lastUartBuffer);
  page += F("</b><span class='uart-time' id='uartTime'>");
  if (lastUartId > 0) { page += "@ "; page += String(lastUartTime); }
  page += F("</span></div>");

  page += F("<form action='/send' method='GET'>"
            "<div class='quick-send'>"
            "<label>&#9889; Send now:</label>");
  page += makeFreqSelect("q_freq", FREQ);
  page += F("<input type='number' name='q_pow' min='1' max='23' value='");
  page += String(POW);
  page += F("' title='Power 1-23 dBm'>"
            "<input type='text' name='q_loc' maxlength='8' value='");
  page += String(LOC);
  page += F("' placeholder='LOCATOR'>"
            "<button type='submit'>&#9654; Send</button></div></form>");

  page += F("<form action='/save' method='GET'><div class='container'>");

  page += F("<div class='col col-left'><div class='box'>"
            "<h3>Sending Schedules</h3>");
  for (int i = 0; i < NUM_SCHED; i++) page += makeSchedBlock(i);
  page += F("</div></div>");

  page += F("<div class='col'><div class='box'>"
            "<h3>String parameters</h3>"
            "<label>Amateur Callsign:"
            "<input type='text' name='sign' maxlength='14' placeholder='np. SP3VSS'></label>"
            "<label>Power [dBm — range 1&#8211;23]:"
            "<input type='number' name='pow' min='1' max='23' value='");
  page += String(POW);
  page += F("'></label>"
            "<label>Locator Maidenhead (LOC):"
            "<input type='text' name='loc' maxlength='8' placeholder='np. JO82' value='");
  page += String(LOC);
  page += F("'></label><hr>"
            "<h3>WiFi Settings</h3>"
            "<label>SSID:<input type='text' name='ssid' placeholder='SSID'></label>"
            "<label>Password:<input type='password' name='pass' placeholder='password'></label>"
            "<hr><h3>NTP Settings</h3>"
            "<label>Time zone (UTC offset in hour):"
            "<input type='number' name='tz' value='");
  page += String(timeOffsetHours);
  page += F("' min='-12' max='14' step='0.5'></label>"
            "</div></div></div>"
            "<p style='text-align:center;margin-top:15px;'>"
            "<button type='submit'>Save settings</button></p>"
            "</form></div></body></html>");

  server.send(200, "text/html", page);
}

//==================================================
// OBSLUGA HTTP: ZAPIS USTAWIEN
//==================================================
void handleSave() {
  if (server.hasArg("sign") && server.arg("sign").length() > 0) {
    String s = server.arg("sign"); s.trim(); s.toUpperCase();
    strncpy(SIGN, s.c_str(), sizeof(SIGN)-1); SIGN[sizeof(SIGN)-1] = '\0';
  }
  if (server.hasArg("pow")) {
    int p = server.arg("pow").toInt();
    if (p >= 1 && p <= 23) POW = p;
  }
  if (server.hasArg("loc") && server.arg("loc").length() > 0) {
    String l = server.arg("loc"); l.trim(); l.toUpperCase();
    strncpy(LOC, l.c_str(), sizeof(LOC)-1); LOC[sizeof(LOC)-1] = '\0';
  }
  if (server.hasArg("tz")) {
    float tzH = server.arg("tz").toFloat();
    timeOffsetHours = (int)tzH;
    timeClient.setTimeOffset((long)(tzH * 3600));
  }
  for (int i = 0; i < NUM_SCHED; i++) {
    String si = String(i);
    Schedule &s = scheds[i];
    s.enabled = server.hasArg("sched_en" + si);
    for (int d = 0; d < 7; d++)
      s.days[d] = server.hasArg("day" + si + "_" + String(d));
    if (server.hasArg("sched_time" + si)) {
      String t = server.arg("sched_time" + si);
      if (t.length() == 5) {
        s.sendH = t.substring(0,2).toInt();
        s.sendM = t.substring(3,5).toInt();
      }
    }
    if (server.hasArg("sched_freq" + si)) {
      long f = server.arg("sched_freq" + si).toInt();
      if (f > 0) s.freq = f;
    }
    s.sentThisMinute = false;
  }

  saveSettings();

  server.sendHeader("Location", "/");
  server.send(303);
}

//==================================================
// OBSLUGA HTTP: WYSLANIE POJEDYNCZEGO STRINGU
//==================================================
void handleSend() {
  if (server.hasArg("q_freq")) {
    long f = server.arg("q_freq").toInt();
    if (f > 0) FREQ = f;
  }
  if (server.hasArg("q_pow")) {
    int p = server.arg("q_pow").toInt();
    if (p >= 1 && p <= 23) POW = p;
  }
  if (server.hasArg("q_loc") && server.arg("q_loc").length() > 0) {
    String l = server.arg("q_loc"); l.trim(); l.toUpperCase();
    strncpy(LOC, l.c_str(), sizeof(LOC)-1); LOC[sizeof(LOC)-1] = '\0';
  }
  sendConfig();
  server.sendHeader("Location", "/");
  server.send(303);
}

//==================================================
// LOOP
//==================================================
void loop() {
  server.handleClient();
  timeClient.update();

  readUart();

  if (displayOn && (millis() - dimMillis >= DIM_TIMEOUT_MS)) {
    dimDisplay();
  }

  static bool buttonWasHigh = false;
  int btnState = digitalRead(BUTTON_PIN);

  if (btnState == HIGH) {
    if (!buttonWasHigh) {
      buttonWasHigh = true;
      if (!displayOn) {
        showConfigOnOled();
      } else if (!alreadySentHW) {
        sendConfig();
        alreadySentHW = true;
      }
    }
  } else {
    buttonWasHigh = false;
    alreadySentHW = false;
  }

  static unsigned long lastSchedCheck = 0;
  if (millis() - lastSchedCheck >= 10000) {
    lastSchedCheck = millis();
    checkSchedules();
  }
}
