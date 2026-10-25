#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <esp_wifi.h>
#include <esp_netif.h>
#include <time.h>
#include <Firebase_ESP_Client.h>
#include <addons/TokenHelper.h>
#include <addons/RTDBHelper.h>
#include <LittleFS.h>
#include <DNSServer.h>
#include <HTTPClient.h>

// ============================================================
// NETWORK
// ============================================================
const char* INTERNET_SSID     = "Ajay";
const char* INTERNET_PASSWORD = "87654321";
const char* AP_SSID     = "CLASS_ATTENDANCE";
const char* AP_PASSWORD = "12345678";

IPAddress AP_IP     (192, 168, 4, 1);
IPAddress AP_GATEWAY(192, 168, 4, 1);
IPAddress AP_SUBNET (255, 255, 255, 0);

// ⚠️ IMPORTANT: CHANGE THIS TO YOUR LAPTOP'S ACTUAL IP ADDRESS
const char* LAPTOP_IP = "http://192.168.0.107:5000"; 

// ============================================================
// FIREBASE CONFIG
// ============================================================
#define FIREBASE_API_KEY       "AIzaSyDafpCJHgS_WsjEt3PDCWY3XC3g72L1GfE"
#define FIREBASE_DATABASE_URL  "https://authentication-303f8-default-rtdb.firebaseio.com/"

FirebaseData   fbdo;
FirebaseAuth   auth;
FirebaseConfig config;
bool firebaseReady = false;
unsigned long lastFirebaseCheck = 0;

WebServer   server(80);
DNSServer   dnsServer;
const byte  DNS_PORT = 53;
Preferences preferences;

const char* DEFAULT_HOD_PASSWORD     = "hod123";
const char* DEFAULT_TEACHER_PASSWORD = "teacher123";

#define MAX_STUDENTS  50
#define MAX_DEVICES   20
#define MAX_TEACHERS  15
#define MAX_SUBJECTS  10
#define MAX_RECORDS   20
#define MAX_TOKENS     8
#define MAX_PERIODS    5
#define TOKEN_TTL 1800000UL

struct Student { String id, name, mac; bool present; int8_t manual; unsigned long lastSeen; };
Student students[MAX_STUDENTS];
int studentCount = 0;

struct Device { String mac; bool registered; };
Device devices[MAX_DEVICES];
int deviceCount = 0;

struct Teacher { String id, name; };
Teacher teachers[MAX_TEACHERS];
int teacherCount = 0;

struct Subject { String code, name, teacherId, teacherName, days; int periodCount; String periodTimes[MAX_PERIODS]; };
Subject subjects[MAX_SUBJECTS];
int subjectCount = 0;

struct Record { String subject, subjectName, period, dateTime, presentIds; String photoFilename; int total, presentCount; };
Record records[MAX_RECORDS];
int recordCount = 0;

bool   attendanceRunning = false;
String currentSubject = "", currentSubjectName = "", currentPeriod = "";
String currentTimeStr = "--", currentDateStr = "--";

struct Token { String value; unsigned long expires; int role; String teacherId; };
Token tokens[MAX_TOKENS];

String hodPassword     = DEFAULT_HOD_PASSWORD;
String teacherPassword = DEFAULT_TEACHER_PASSWORD;

esp_netif_t* apNetif = nullptr;
bool natEnabled = false;

unsigned long lastScan = 0;
const unsigned long SCAN_INTERVAL = 2000;

// ============================================================
// HELPERS
// ============================================================
String B(bool v) { return v ? "true" : "false"; }
String normalizeMac(String mac) { mac.trim(); mac.toUpperCase(); mac.replace("-", ":"); mac.replace(" ", ""); return mac; }
String jsonEscape(String v) { v.replace("\\","\\\\"); v.replace("\"","\\\""); v.replace("\n","\\n"); v.replace("\r","\\r"); return v; }
String sanitize(String s) { s.trim(); s.replace("|",""); s.replace("\n"," "); s.replace("\r"," "); s.replace(","," "); s.replace(";",""); s.replace("\"",""); s.replace("\\",""); s.replace("<",""); s.replace(">",""); return s; }
String sanitizeDays(String s) { s.trim(); s.replace("|",""); s.replace("\n",""); s.replace("\r",""); s.replace(";",""); s.replace("\"",""); s.replace("\\",""); s.replace("<",""); s.replace(">",""); return s; }

void reply(bool ok, String msg) { server.send(ok ? 200 : 400, "application/json", "{\"ok\":" + String(ok?"true":"false") + ",\"message\":\"" + jsonEscape(msg) + "\"}"); }
String generateToken() { const char* hex = "0123456789abcdef"; String t = ""; for (int i = 0; i < 24; i++) t += hex[esp_random() % 16]; return t; }

int authRole() {
  if (!server.hasArg("token")) return 0;
  String t = server.arg("token"); unsigned long now = millis();
  for (int i = 0; i < MAX_TOKENS; i++) {
    if (tokens[i].value.length() > 0 && tokens[i].value == t) {
      if ((long)(tokens[i].expires - now) > 0) { tokens[i].expires = now + TOKEN_TTL; return tokens[i].role; }
    }
  }
  return 0;
}

bool isHOD() { return authRole() == 1; }
bool isTeacher() { return authRole() == 2; }

String authTeacherId() {
  if (!server.hasArg("token")) return "";
  String t = server.arg("token"); unsigned long now = millis();
  for (int i = 0; i < MAX_TOKENS; i++) {
    if (tokens[i].value.length() > 0 && tokens[i].value == t) {
      if ((long)(tokens[i].expires - now) > 0) { tokens[i].expires = now + TOKEN_TTL; return tokens[i].teacherId; }
    }
  }
  return "";
}

bool teacherOwnsSubject(int subjectIndex) {
  if (subjectIndex < 0 || subjectIndex >= subjectCount) return false;
  String tid = authTeacherId();
  return tid.length() > 0 && subjects[subjectIndex].teacherId.equalsIgnoreCase(tid);
}

void updateClock() {
  struct tm ti;
  if (getLocalTime(&ti, 5)) {
    char buf[32];
    strftime(buf, sizeof(buf), "%d-%m-%Y", &ti); currentDateStr = String(buf);
    strftime(buf, sizeof(buf), "%H:%M:%S", &ti); currentTimeStr = String(buf);
  } else {
    unsigned long s = millis() / 1000; char buf[40];
    snprintf(buf, sizeof(buf), "Uptime %02lu:%02lu:%02lu", s/3600, (s/60)%60, s%60);
    currentTimeStr = String(buf); currentDateStr = "--";
  }
}

String stampNow() { if (currentDateStr == "--") return currentTimeStr; return currentDateStr + " " + currentTimeStr; }
int findStudentByMAC(String mac) { mac = normalizeMac(mac); if (mac.length() == 0) return -1; for (int i = 0; i < studentCount; i++) if (students[i].mac.length() > 0 && students[i].mac == mac) return i; return -1; }
int findStudentByID(String id) { id.trim(); if (id.length() == 0) return -1; for (int i = 0; i < studentCount; i++) if (students[i].id.equalsIgnoreCase(id)) return i; return -1; }
int findSubjectByCode(String code) { code.trim(); for (int i = 0; i < subjectCount; i++) if (subjects[i].code.equalsIgnoreCase(code)) return i; return -1; }
int findTeacherByID(String id) { id.trim(); for (int i = 0; i < teacherCount; i++) if (teachers[i].id.equalsIgnoreCase(id)) return i; return -1; }
bool isDeviceOnline(String mac) { if (mac.length() == 0) return false; for (int i = 0; i < deviceCount; i++) if (devices[i].mac == mac) return true; return false; }

bool listContains(String list, String id) {
  int start = 0, len = list.length();
  while (start <= len) {
    int c = list.indexOf(',', start);
    String part = (c < 0) ? list.substring(start) : list.substring(start, c);
    part.trim(); if (part.length() > 0 && part.equalsIgnoreCase(id)) return true;
    if (c < 0) break; start = c + 1;
  }
  return false;
}

// ============================================================
// FIREBASE SYNC
// ============================================================
void syncStudentsToFirebase() {
  if (!Firebase.ready()) return;
  FirebaseJson json;
  for (int i = 0; i < studentCount; i++) {
    FirebaseJson s; s.set("id", students[i].id); s.set("name", students[i].name); s.set("mac", students[i].mac);
    json.set(students[i].id, s);
  }
  Firebase.RTDB.setJSON(&fbdo, "/students", &json);
}

void syncTeachersToFirebase() {
  if (!Firebase.ready()) return;
  FirebaseJson json;
  for (int i = 0; i < teacherCount; i++) {
    FirebaseJson t; t.set("id", teachers[i].id); t.set("name", teachers[i].name);
    json.set(teachers[i].id, t);
  }
  Firebase.RTDB.setJSON(&fbdo, "/teachers", &json);
}

void syncSubjectsToFirebase() {
  if (!Firebase.ready()) return;
  FirebaseJson json;
  for (int i = 0; i < subjectCount; i++) {
    FirebaseJson s; s.set("code", subjects[i].code); s.set("name", subjects[i].name); s.set("teacherId", subjects[i].teacherId); s.set("teacherName", subjects[i].teacherName); s.set("days", subjects[i].days); s.set("periodCount", subjects[i].periodCount);
    FirebaseJson periods;
    for (int k = 0; k < subjects[i].periodCount; k++) periods.set(String(k), subjects[i].periodTimes[k]);
    s.set("periodTimes", periods); json.set(subjects[i].code, s);
  }
  Firebase.RTDB.setJSON(&fbdo, "/subjects", &json);
}

void syncRecordsToFirebase() {
  if (!Firebase.ready()) return;
  FirebaseJson json;
  for (int i = 0; i < recordCount; i++) {
    FirebaseJson r; r.set("subject", records[i].subject); r.set("subjectName", records[i].subjectName); r.set("period", records[i].period); r.set("dateTime", records[i].dateTime); r.set("presentIds", records[i].presentIds); r.set("photoFilename", records[i].photoFilename); r.set("total", records[i].total); r.set("presentCount", records[i].presentCount);
    json.set(String(i), r);
  }
  Firebase.RTDB.setJSON(&fbdo, "/records", &json);
}

void pushLiveDeviceToFirebase(String mac, bool registered) {
  if (!Firebase.ready()) return;
  FirebaseJson d; d.set("mac", mac); d.set("registered", registered); d.set("lastSeen", stampNow());
  String path = "/liveDevices/" + mac; path.replace(":", "_");
  Firebase.RTDB.setJSON(&fbdo, path.c_str(), &d);
}

void loadAllFromFirebase() {
  // Load Students (with safety check)
  if (Firebase.RTDB.getJSON(&fbdo, "/students")) {
    FirebaseJson* j = fbdo.jsonObjectPtr();
    if (j) { studentCount = 0; size_t count = j->iteratorBegin();
      for (size_t i = 0; i < count && studentCount < MAX_STUDENTS; i++) {
        int type; String key, val; j->iteratorGet(i, type, key, val);
        if (type == FirebaseJson::JSON_OBJECT) {
          FirebaseJson sub; sub.setJsonData(val); FirebaseJsonData d1, d2, d3;
          sub.get(d1, "id"); sub.get(d2, "name"); sub.get(d3, "mac");
          if (d1.stringValue.length() > 0 && d2.stringValue.length() > 0) {
            students[studentCount].id = d1.stringValue; 
            students[studentCount].name = d2.stringValue; 
            students[studentCount].mac = normalizeMac(d3.stringValue);
            students[studentCount].present = false; 
            students[studentCount].manual = 0; 
            students[studentCount].lastSeen = 0;
            studentCount++;
          }
        }
      } j->iteratorEnd();
    }
  }

  // Load Teachers
  if (Firebase.RTDB.getJSON(&fbdo, "/teachers")) {
    FirebaseJson* j = fbdo.jsonObjectPtr();
    if (j) { teacherCount = 0; size_t count = j->iteratorBegin();
      for (size_t i = 0; i < count && teacherCount < MAX_TEACHERS; i++) {
        int type; String key, val; j->iteratorGet(i, type, key, val);
        if (type == FirebaseJson::JSON_OBJECT) {
          FirebaseJson sub; sub.setJsonData(val); FirebaseJsonData d1, d2;
          sub.get(d1, "id"); sub.get(d2, "name");
          if (d1.stringValue.length() > 0) {
            teachers[teacherCount].id = d1.stringValue; 
            teachers[teacherCount].name = d2.stringValue;
            teacherCount++;
          }
        }
      } j->iteratorEnd();
    }
  }

  // Load Subjects
  if (Firebase.RTDB.getJSON(&fbdo, "/subjects")) {
    FirebaseJson* j = fbdo.jsonObjectPtr();
    if (j) { subjectCount = 0; size_t count = j->iteratorBegin();
      for (size_t i = 0; i < count && subjectCount < MAX_SUBJECTS; i++) {
        int type; String key, val; j->iteratorGet(i, type, key, val);
        if (type == FirebaseJson::JSON_OBJECT) {
          FirebaseJson sub; sub.setJsonData(val); FirebaseJsonData d;
          sub.get(d, "code"); subjects[subjectCount].code = d.stringValue;
          sub.get(d, "name"); subjects[subjectCount].name = d.stringValue;
          if (subjects[subjectCount].code.length() > 0) {
            sub.get(d, "teacherId"); subjects[subjectCount].teacherId = d.stringValue;
            sub.get(d, "teacherName"); subjects[subjectCount].teacherName = d.stringValue;
            sub.get(d, "days"); subjects[subjectCount].days = d.stringValue;
            sub.get(d, "periodCount"); subjects[subjectCount].periodCount = d.intValue;
            for (int k = 0; k < subjects[subjectCount].periodCount; k++) {
              FirebaseJsonData pd; sub.get(pd, "periodTimes[" + String(k) + "]");
              subjects[subjectCount].periodTimes[k] = pd.stringValue;
            }
            subjectCount++;
          }
        }
      } j->iteratorEnd();
    }
  }

  // Load Records
  if (Firebase.RTDB.getJSON(&fbdo, "/records")) {
    FirebaseJson* j = fbdo.jsonObjectPtr();
    if (j) { recordCount = 0; size_t count = j->iteratorBegin();
      for (size_t i = 0; i < count && recordCount < MAX_RECORDS; i++) {
        int type; String key, val; j->iteratorGet(i, type, key, val);
        if (type == FirebaseJson::JSON_OBJECT) {
          FirebaseJson sub; sub.setJsonData(val); FirebaseJsonData d;
          sub.get(d, "subject");      records[recordCount].subject      = d.stringValue;
          if (records[recordCount].subject.length() > 0) {
            sub.get(d, "subjectName");  records[recordCount].subjectName  = d.stringValue;
            sub.get(d, "period");       records[recordCount].period       = d.stringValue;
            sub.get(d, "dateTime");     records[recordCount].dateTime     = d.stringValue;
            sub.get(d, "presentIds");   records[recordCount].presentIds   = d.stringValue;
            sub.get(d, "photoFilename");records[recordCount].photoFilename= d.stringValue;
            sub.get(d, "total");        records[recordCount].total        = d.intValue;
            sub.get(d, "presentCount"); records[recordCount].presentCount = d.intValue;
            recordCount++;
          }
        }
      } j->iteratorEnd();
    }
  }
}

// ============================================================
// ATTENDANCE LOGIC
// ============================================================
void clearAttendance() { for (int i = 0; i < studentCount; i++) { students[i].present = false; students[i].manual = 0; students[i].lastSeen = 0; } }

void saveCurrentRecord() {
  String presentIds = ""; int pc = 0;
  for (int i = 0; i < studentCount; i++) { if (students[i].present) { if (presentIds.length() > 0) presentIds += ","; presentIds += students[i].id; pc++; } }
  if (recordCount >= MAX_RECORDS) { for (int i = 0; i < MAX_RECORDS - 1; i++) records[i] = records[i + 1]; recordCount = MAX_RECORDS - 1; }
  Record &r = records[recordCount]; 
  r.subject = currentSubject; 
  r.subjectName = currentSubjectName; 
  r.period = currentPeriod; 
  r.dateTime = stampNow(); 
  r.presentIds = presentIds; 
  r.photoFilename = ""; 
  r.total = studentCount; 
  r.presentCount = pc; 
  recordCount++;
  syncRecordsToFirebase();
}

void scanDevices() {
  deviceCount = 0; 
  wifi_sta_list_t list; 
  memset(&list, 0, sizeof(list));
  
  if (esp_wifi_ap_get_sta_list(&list) != ESP_OK) return;
  
  for (int i = 0; i < list.num && deviceCount < MAX_DEVICES; i++) {
    char buf[18]; 
    snprintf(buf, sizeof(buf), "%02X:%02X:%02X:%02X:%02X:%02X", list.sta[i].mac[0], list.sta[i].mac[1], list.sta[i].mac[2], list.sta[i].mac[3], list.sta[i].mac[4], list.sta[i].mac[5]);
    String mac = normalizeMac(String(buf)); 
    
    // Check if we already know this device to prevent Firebase spam
    bool isNew = true;
    for (int d = 0; d < deviceCount; d++) {
      if (devices[d].mac == mac) {
        isNew = false;
        break;
      }
    }
    
    devices[deviceCount].mac = mac;
    int si = findStudentByMAC(mac); 
    devices[deviceCount].registered = (si >= 0);
    
    if (attendanceRunning && si >= 0 && students[si].manual != -1) { 
      students[si].present = true; 
      students[si].lastSeen = millis(); 
    }
    
    // ONLY push to Firebase if it's a brand new device
    if (isNew) {
      pushLiveDeviceToFirebase(mac, devices[deviceCount].registered);
    }
    
    deviceCount++;
  }
}

bool enableNAT() { 
  apNetif = esp_netif_get_handle_from_ifkey("WIFI_AP_DEF"); 
  if (!apNetif) return false; 
  natEnabled = false; 
  return false; 
}

// ============================================================
// LITTLEFS & CAPTIVE PORTAL
// ============================================================
void handleRoot() { File file = LittleFS.open("/index.html", "r"); if (!file) { server.send(404, "text/plain", "index.html not found"); return; } server.streamFile(file, "text/html"); file.close(); }
void handleCSS()  { File file = LittleFS.open("/style.css", "r");  if (!file) { server.send(404, "text/plain", "style.css not found"); return; }  server.streamFile(file, "text/css"); file.close(); }
void handleJS()   { File file = LittleFS.open("/app.js", "r");     if (!file) { server.send(404, "text/plain", "app.js not found"); return; }     server.streamFile(file, "application/javascript"); file.close(); }

void handleCaptivePortal() { server.sendHeader("Location", "http://192.168.4.1/", true); server.send(302, "text/plain", ""); }
void handleAndroid204() { server.sendHeader("Location", "http://192.168.4.1/", true); server.send(302, "text/plain", ""); }
void handleAppleDetect() { server.send_P(200, "text/html", "<HTML><HEAD><TITLE>Success</TITLE></HEAD><BODY>Success</BODY></HTML>"); }

// ============================================================
// PROXY TO LAPTOP (Face Recognition & Selfies)
// ============================================================
void handleProxyRecognize() {
  HTTPClient http; http.begin(String(LAPTOP_IP) + "/api/recognize");
  http.addHeader("Content-Type", "text/plain");
  String body = server.arg("plain");
  int code = http.POST(body);
  if (code > 0) { server.send(200, "application/json", http.getString()); } 
  else { server.send(500, "application/json", "{\"ok\":false,\"msg\":\"ESP32 to Laptop Proxy Failed\"}"); }
  http.end();
}

void handleProxyMark() {
  HTTPClient http; http.begin(String(LAPTOP_IP) + "/api/mark");
  http.addHeader("Content-Type", "application/json");
  String body = server.arg("plain");
  int code = http.POST(body);
  if (code > 0) { server.send(200, "application/json", http.getString()); } 
  else { server.send(500, "application/json", "{\"ok\":false,\"msg\":\"ESP32 to Laptop Proxy Failed\"}"); }
  http.end();
}

void handleProxyPhoto() {
  if (!server.hasArg("filename")) { server.send(400, "text/plain", "Missing filename"); return; }
  String filename = server.arg("filename");
  HTTPClient http; 
  http.begin(String(LAPTOP_IP) + "/captures/" + filename);
  int code = http.GET();
  
  if (code == 200) {
    server.sendHeader("Content-Type", "image/jpeg");
    server.setContentLength(http.getSize());
    server.send(200, "image/jpeg", "");
    WiFiClient* stream = http.getStreamPtr();
    uint8_t buff[512] = { 0 };
    int len = http.getSize();
    while (http.connected() && (len > 0 || len == -1)) {
      size_t size = stream->available();
      if (size) {
        int c = stream->readBytes(buff, ((size > sizeof(buff)) ? sizeof(buff) : size));
        server.client().write(buff, c);
        if (len > 0) len -= c;
      }
      delay(1);
    }
  } else { 
    server.send(404, "text/plain", "Image not found"); 
  }
  http.end();
}

// ============================================================
// FAST SYNC TRIGGER (Called by Python Server)
// ============================================================
void handleSyncTrigger() {
  server.send(200, "application/json", "{\"ok\":true}");
  loadAllFromFirebase();
}

// ============================================================
// API (Login & Data)
// ============================================================
void handleLogin() {
  if (!server.hasArg("pass") || !server.hasArg("role")) { server.send(400, "application/json", "{\"ok\":false,\"message\":\"Password and role required\"}"); return; }
  String role = server.arg("role"), pass = server.arg("pass"); int roleId = 0; String expected = ""; String loginTeacherId = "";
  if (role == "hod") { roleId = 1; expected = hodPassword; }
  else if (role == "teacher") {
    roleId = 2; expected = teacherPassword;
    if (!server.hasArg("teacherId")) { server.send(400, "application/json", "{\"ok\":false,\"message\":\"Teacher ID required\"}"); return; }
    loginTeacherId = sanitize(server.arg("teacherId"));
    if (findTeacherByID(loginTeacherId) < 0) { server.send(401, "application/json", "{\"ok\":false,\"message\":\"Teacher ID not found.\"}"); return; }
  } else { server.send(400, "application/json", "{\"ok\":false,\"message\":\"Invalid role\"}"); return; }
  if (pass != expected) { server.send(401, "application/json", "{\"ok\":false,\"message\":\"Incorrect password\"}"); return; }
  
  String t = generateToken(); int slot = 0; unsigned long oldest = 0xFFFFFFFF; unsigned long now = millis();
  for (int i = 0; i < MAX_TOKENS; i++) {
    if (tokens[i].value.length() == 0 || (long)(tokens[i].expires - now) <= 0) { slot = i; break; }
    if (tokens[i].expires < oldest) { oldest = tokens[i].expires; slot = i; }
  }
  tokens[slot].value = t; tokens[slot].expires = now + TOKEN_TTL; tokens[slot].role = roleId; tokens[slot].teacherId = loginTeacherId;
  server.send(200, "application/json", "{\"ok\":true,\"token\":\"" + t + "\",\"role\":\"" + role + "\"}");
}

void handleData() {
  int presentCount = 0; 
  for (int i = 0; i < studentCount; i++) if (students[i].present) presentCount++;
  int absentCount = studentCount - presentCount; 
  float pct = studentCount > 0 ? ((float)presentCount / (float)studentCount) * 100.0 : 0;
  int r = authRole(); 
  bool showMAC = (r == 1); 
  bool showDevices = (r == 1);
  
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, "application/json", "");

  server.sendContent("{");
  server.sendContent("\"total\":" + String(studentCount) + ","); 
  server.sendContent("\"present\":" + String(presentCount) + ","); 
  server.sendContent("\"absent\":" + String(absentCount) + ",");
  server.sendContent("\"percentage\":" + String(pct, 1) + ","); 
  server.sendContent("\"running\":" + B(attendanceRunning) + ",");
  server.sendContent("\"subject\":\"" + jsonEscape(currentSubject) + "\","); 
  server.sendContent("\"period\":\"" + jsonEscape(currentPeriod) + "\",");
  server.sendContent("\"internet\":" + B(WiFi.status() == WL_CONNECTED) + ","); 
  server.sendContent("\"nat\":" + B(natEnabled) + ",");
  server.sendContent("\"time\":\"" + jsonEscape(currentTimeStr) + "\","); 
  server.sendContent("\"date\":\"" + jsonEscape(currentDateStr) + "\",");
  
  server.sendContent("\"students\":[");
  for (int i = 0; i < studentCount; i++) {
    if (i > 0) server.sendContent(","); 
    server.sendContent("{");
    server.sendContent("\"id\":\"" + jsonEscape(students[i].id) + "\","); 
    server.sendContent("\"name\":\"" + jsonEscape(students[i].name) + "\",");
    server.sendContent("\"mac\":\""); 
    if (showMAC) server.sendContent(students[i].mac); 
    server.sendContent("\",");
    server.sendContent("\"present\":" + B(students[i].present) + ","); 
    server.sendContent("\"manual\":" + String(students[i].manual) + ",");
    server.sendContent("\"online\":" + B(isDeviceOnline(students[i].mac))); 
    server.sendContent("}");
  }
  server.sendContent("],");
  
  server.sendContent("\"devices\":["); 
  if (showDevices) { 
    for (int i = 0; i < deviceCount; i++) { 
      if (i > 0) server.sendContent(","); 
      server.sendContent("{\"mac\":\"" + devices[i].mac + "\","); 
      server.sendContent("\"registered\":" + B(devices[i].registered) + "}"); 
    } 
  } 
  server.sendContent("],");
  
  server.sendContent("\"teachers\":["); 
  for (int i = 0; i < teacherCount; i++) { 
    if (i > 0) server.sendContent(","); 
    server.sendContent("{\"id\":\"" + jsonEscape(teachers[i].id) + "\","); 
    server.sendContent("\"name\":\"" + jsonEscape(teachers[i].name) + "\"}"); 
  } 
  server.sendContent("],");
  
  server.sendContent("\"subjects\":["); 
  bool firstSubject = true;
  for (int i = 0; i < subjectCount; i++) {
    if (r == 2 && !teacherOwnsSubject(i)) continue; 
    if (!firstSubject) server.sendContent(","); 
    firstSubject = false; 
    server.sendContent("{");
    server.sendContent("\"code\":\"" + jsonEscape(subjects[i].code) + "\","); 
    server.sendContent("\"name\":\"" + jsonEscape(subjects[i].name) + "\",");
    server.sendContent("\"teacherId\":\"" + jsonEscape(subjects[i].teacherId) + "\","); 
    server.sendContent("\"teacherName\":\"" + jsonEscape(subjects[i].teacherName) + "\",");
    server.sendContent("\"days\":\"" + jsonEscape(subjects[i].days) + "\","); 
    server.sendContent("\"periodTimes\":[");
    for (int k = 0; k < subjects[i].periodCount; k++) { 
      if (k > 0) server.sendContent(","); 
      server.sendContent("\"" + jsonEscape(subjects[i].periodTimes[k]) + "\""); 
    } 
    server.sendContent("]}");
  } 
  server.sendContent("],");
  
  server.sendContent("\"records\":["); 
  bool firstRecord = true;
  for (int i = 0; i < recordCount; i++) {
    if (r == 2) { 
      int si = findSubjectByCode(records[i].subject); 
      if (!teacherOwnsSubject(si)) continue; 
    } 
    if (!firstRecord) server.sendContent(","); 
    firstRecord = false; 
    server.sendContent("{");
    server.sendContent("\"subject\":\"" + jsonEscape(records[i].subject) + "\","); 
    server.sendContent("\"subjectName\":\"" + jsonEscape(records[i].subjectName) + "\",");
    server.sendContent("\"period\":\"" + jsonEscape(records[i].period) + "\","); 
    server.sendContent("\"dateTime\":\"" + jsonEscape(records[i].dateTime) + "\",");
    server.sendContent("\"presentIds\":\"" + jsonEscape(records[i].presentIds) + "\","); 
    server.sendContent("\"photoFilename\":\"" + jsonEscape(records[i].photoFilename) + "\","); 
    server.sendContent("\"total\":" + String(records[i].total) + ","); 
    server.sendContent("\"presentCount\":" + String(records[i].presentCount)); 
    server.sendContent("}");
  } 
  server.sendContent("]"); 
  server.sendContent("}");
}

void handleStudentLookup() {
  if (!server.hasArg("id")) { server.send(400, "application/json", "{\"found\":false}"); return; }
  String id = server.arg("id"); id.trim(); int idx = findStudentByID(id);
  if (idx < 0) { server.send(200, "application/json", "{\"found\":false}"); return; }
  String subCode[MAX_RECORDS], subName[MAX_RECORDS]; int held[MAX_RECORDS], attended[MAX_RECORDS]; int n = 0;
  for (int r = 0; r < recordCount; r++) {
    String code = records[r].subject; int found = -1;
    for (int k = 0; k < n; k++) if (subCode[k].equalsIgnoreCase(code)) { found = k; break; }
    if (found < 0) { if (n >= MAX_RECORDS) continue; found = n; subCode[n] = code; subName[n] = records[r].subjectName; held[n] = 0; attended[n] = 0; n++; }
    held[found]++; if (listContains(records[r].presentIds, students[idx].id)) attended[found]++;
  }
  int totHeld = 0, totAtt = 0; for (int k = 0; k < n; k++) { totHeld += held[k]; totAtt += attended[k]; }
  float overallPct = totHeld > 0 ? (totAtt * 100.0) / totHeld : 0;
  String json; json.reserve(2000);
  json = "{\"found\":true,"; json += "\"id\":\"" + jsonEscape(students[idx].id) + "\","; json += "\"name\":\"" + jsonEscape(students[idx].name) + "\","; json += "\"present\":" + B(students[idx].present) + ",";
  json += "\"overall\":{"; json += "\"held\":" + String(totHeld) + ","; json += "\"attended\":" + String(totAtt) + ","; json += "\"percent\":" + String(overallPct, 1); json += "},\"summary\":[";
  for (int k = 0; k < n; k++) { if (k > 0) json += ","; float pct = held[k] > 0 ? (attended[k] * 100.0) / held[k] : 0; json += "{"; json += "\"code\":\"" + jsonEscape(subCode[k]) + "\","; json += "\"name\":\"" + jsonEscape(subName[k]) + "\","; json += "\"held\":" + String(held[k]) + ","; json += "\"attended\":" + String(attended[k]) + ","; json += "\"percent\":" + String(pct, 1); json += "}"; }
  json += "]}"; server.send(200, "application/json", json);
}

// ============================================================
// COMMAND HANDLER
// ============================================================
void handleCommand() {
  int role = authRole(); if (role == 0) { server.send(401, "application/json", "{\"ok\":false,\"message\":\"Please log in again\"}"); return; }
  bool hod = (role == 1); bool teacher = (role == 2); String cmd = server.arg("cmd");

  if (cmd == "register") {
    if (!hod) { reply(false, "HOD only"); return; }
    if (!server.hasArg("id") || !server.hasArg("name") || !server.hasArg("mac")) { reply(false, "ID, name and MAC required"); return; }
    String id = sanitize(server.arg("id")), name = sanitize(server.arg("name")), mac = normalizeMac(server.arg("mac"));
    if (id.length() == 0 || name.length() == 0 || mac.length() == 0) { reply(false, "All fields required"); return; }
    if (findStudentByID(id) >= 0) { reply(false, "ID already exists"); return; }
    if (findStudentByMAC(mac) >= 0) { reply(false, "MAC already registered"); return; }
    if (studentCount >= MAX_STUDENTS) { reply(false, "Max students reached"); return; }
    students[studentCount].id = id; students[studentCount].name = name; students[studentCount].mac = mac; students[studentCount].present = false; students[studentCount].manual = 0; students[studentCount].lastSeen = 0; studentCount++;
    syncStudentsToFirebase(); reply(true, "Student registered"); return;
  }

  if (cmd == "edit") {
    if (!hod) { reply(false, "HOD only"); return; }
    if (!server.hasArg("index") || !server.hasArg("id") || !server.hasArg("name")) { reply(false, "Missing fields"); return; }
    int index = server.arg("index").toInt(); if (index < 0 || index >= studentCount){ reply(false, "Invalid index"); return; }
    String newID = sanitize(server.arg("id")), newName = sanitize(server.arg("name")), newMAC = normalizeMac(server.arg("mac"));
    if (newID.length() == 0 || newName.length() == 0) { reply(false, "ID and name required"); return; }
    int dupID = findStudentByID(newID); if (dupID >= 0 && dupID != index){ reply(false, "ID already exists"); return; }
    if (newMAC.length() > 0) { int dupMAC = findStudentByMAC(newMAC); if (dupMAC >= 0 && dupMAC != index){ reply(false, "MAC already used"); return; } }
    students[index].id = newID; students[index].name = newName; students[index].mac = newMAC;
    syncStudentsToFirebase(); reply(true, "Student updated"); return;
  }

  if (cmd == "delete") {
    if (!hod) { reply(false, "HOD only"); return; }
    if (!server.hasArg("index")){ reply(false, "Missing index"); return; }
    int index = server.arg("index").toInt(); if (index < 0 || index >= studentCount){ reply(false, "Invalid index"); return; }
    for (int i = index; i < studentCount - 1; i++) students[i] = students[i + 1];
    studentCount--; syncStudentsToFirebase(); reply(true, "Student deleted"); return;
  }

  if (cmd == "clearall") {
    if (!hod) { reply(false, "HOD only"); return; }
    studentCount = 0; syncStudentsToFirebase(); reply(true, "All students deleted"); return;
  }

  if (cmd == "tradd") {
    if (!hod) { reply(false, "HOD only"); return; }
    if (!server.hasArg("id") || !server.hasArg("name")){ reply(false, "Missing"); return; }
    String id = sanitize(server.arg("id")), name = sanitize(server.arg("name"));
    if (id.length() == 0 || name.length() == 0){ reply(false, "ID and name required"); return; }
    if (findTeacherByID(id) >= 0){ reply(false, "Teacher ID exists"); return; }
    if (teacherCount >= MAX_TEACHERS){ reply(false, "Max teachers reached"); return; }
    teachers[teacherCount].id = id; teachers[teacherCount].name = name; teacherCount++;
    syncTeachersToFirebase(); reply(true, "Teacher added"); return;
  }

  if (cmd == "trdel") {
    if (!hod) { reply(false, "HOD only"); return; }
    if (!server.hasArg("index")){ reply(false, "Missing index"); return; }
    int index = server.arg("index").toInt(); if (index < 0 || index >= teacherCount){ reply(false, "Invalid index"); return; }
    for (int i = index; i < teacherCount - 1; i++) teachers[i] = teachers[i + 1];
    teacherCount--; syncTeachersToFirebase(); reply(true, "Teacher deleted"); return;
  }

  if (cmd == "subadd") {
    if (!hod && !teacher) { reply(false, "HOD or Teacher only"); return; }
    if (!server.hasArg("code") || !server.hasArg("name")) { reply(false, "Missing fields"); return; }
    String code = sanitize(server.arg("code")), name = sanitize(server.arg("name"));
    if (code.length() == 0 || name.length() == 0){ reply(false, "Required"); return; }
    if (findSubjectByCode(code) >= 0){ reply(false, "Code exists"); return; }
    if (subjectCount >= MAX_SUBJECTS){ reply(false, "Max subjects reached"); return; }
    String teacherId = "", teacherName = "";
    if (hod) {
      if (!server.hasArg("teacherId")){ reply(false, "Pick a teacher"); return; }
      teacherId = sanitize(server.arg("teacherId"));
      int ti = findTeacherByID(teacherId); if (ti < 0){ reply(false, "Teacher not found"); return; }
      teacherName = teachers[ti].name;
    } else {
      teacherId = authTeacherId(); int ti = findTeacherByID(teacherId);
      if (ti < 0){ reply(false, "Teacher account not found"); return; }
      teacherName = teachers[ti].name;
    }
    subjects[subjectCount].code = code; subjects[subjectCount].name = name; subjects[subjectCount].teacherId = teacherId; subjects[subjectCount].teacherName = teacherName; subjects[subjectCount].days = ""; subjects[subjectCount].periodCount = 0; subjectCount++;
    syncSubjectsToFirebase(); reply(true, "Subject created"); return;
  }

  if (cmd == "subedit") {
    if (!hod && !teacher) { reply(false, "HOD or Teacher only"); return; }
    if (!server.hasArg("old") || !server.hasArg("code") || !server.hasArg("name")) { reply(false, "Missing"); return; }
    int idx = findSubjectByCode(server.arg("old")); if (idx < 0){ reply(false, "Subject not found"); return; }
    if (teacher && !teacherOwnsSubject(idx)) { reply(false, "You can edit only your assigned subject"); return; }
    String code = sanitize(server.arg("code")), name = sanitize(server.arg("name"));
    if (code.length() == 0 || name.length() == 0){ reply(false, "Required"); return; }
    int other = findSubjectByCode(code); if (other >= 0 && other != idx){ reply(false, "Code already used"); return; }
    subjects[idx].code = code; subjects[idx].name = name;
    if (hod) { if (server.hasArg("teacherId")) { String tId = sanitize(server.arg("teacherId")); int ti = findTeacherByID(tId); if (ti < 0){ reply(false, "Teacher not found"); return; } subjects[idx].teacherId = tId; subjects[idx].teacherName = teachers[ti].name; } }
    syncSubjectsToFirebase(); reply(true, "Subject updated"); return;
  }

  if (cmd == "subdel") {
    if (!hod && !teacher) { reply(false, "HOD or Teacher only"); return; }
    if (!server.hasArg("code")){ reply(false, "Missing code"); return; }
    int idx = findSubjectByCode(server.arg("code")); if (idx < 0){ reply(false, "Subject not found"); return; }
    if (teacher && !teacherOwnsSubject(idx)) { reply(false, "You can delete only your assigned subject"); return; }
    for (int i = idx; i < subjectCount - 1; i++) subjects[i] = subjects[i + 1]; subjectCount--;
    syncSubjectsToFirebase(); reply(true, "Subject deleted"); return;
  }

  if (cmd == "setschedule") {
    if (!hod && !teacher) { reply(false, "HOD or Teacher only"); return; }
    if (!server.hasArg("subject") || !server.hasArg("days") || !server.hasArg("periods")) { reply(false, "Missing"); return; }
    int idx = findSubjectByCode(server.arg("subject")); if (idx < 0){ reply(false, "Subject not found"); return; }
    if (teacher && !teacherOwnsSubject(idx)) { reply(false, "You can edit only your assigned subject schedule"); return; }
    subjects[idx].days = sanitizeDays(server.arg("days"));
    String periodsStr = server.arg("periods"); subjects[idx].periodCount = 0; int start = 0;
    while (start <= (int)periodsStr.length() && subjects[idx].periodCount < MAX_PERIODS) {
      int c = periodsStr.indexOf(',', start);
      String p = (c < 0) ? periodsStr.substring(start) : periodsStr.substring(start, c);
      p = sanitize(p); if (p.length() > 0) subjects[idx].periodTimes[subjects[idx].periodCount++] = p;
      if (c < 0) break; start = c + 1;
    }
    syncSubjectsToFirebase(); reply(true, "Schedule saved"); return;
  }

  if (cmd == "start") {
    if (!teacher && !hod) { reply(false, "Not allowed"); return; }
    if (!server.hasArg("subject") || !server.hasArg("period")){ reply(false, "Select subject and period"); return; }
    int si = findSubjectByCode(server.arg("subject")); if (si < 0){ reply(false, "Unknown subject"); return; }
    if (teacher && !teacherOwnsSubject(si)) { reply(false, "You can start attendance only for your assigned subject"); return; }
    if (attendanceRunning && studentCount > 0) saveCurrentRecord();
    currentSubject = subjects[si].code; currentSubjectName = subjects[si].name; currentPeriod = server.arg("period");
    clearAttendance(); attendanceRunning = true; scanDevices(); reply(true, "Attendance started"); return;
  }

  if (cmd == "stop") {
    if (!teacher && !hod) { reply(false, "Not allowed"); return; }
    if (!attendanceRunning){ reply(false, "No session"); return; }
    attendanceRunning = false; saveCurrentRecord(); reply(true, "Attendance saved"); return;
  }

  if (cmd == "reset") {
    if (!teacher && !hod) { reply(false, "Not allowed"); return; }
    clearAttendance(); reply(true, "Attendance reset"); return;
  }

  if (cmd == "mark") {
    if (!teacher && !hod) { reply(false, "Not allowed"); return; }
    if (!server.hasArg("index") || !server.hasArg("state")){ reply(false, "Missing"); return; }
    int index = server.arg("index").toInt(); int state = server.arg("state").toInt();
    if (index < 0 || index >= studentCount){ reply(false, "Invalid index"); return; }
    if (state != -1 && state != 0 && state != 1) state = 0;
    students[index].manual = (int8_t)state;
    if (state == 1) { students[index].present = true; students[index].lastSeen = millis(); }
    else if (state == -1) { students[index].present = false; }
    else { students[index].present = isDeviceOnline(students[index].mac); }
    reply(true, "Marked"); return;
  }

  if (cmd == "recdel") {
    if (!hod && !teacher) { reply(false, "HOD or Teacher only"); return; }
    if (!server.hasArg("index")){ reply(false, "Missing"); return; }
    int index = server.arg("index").toInt(); if (index < 0 || index >= recordCount){ reply(false, "Invalid"); return; }
    if (teacher) { int si = findSubjectByCode(records[index].subject); if (si < 0 || !teacherOwnsSubject(si)) { reply(false, "You can delete only records for your subject"); return; } }
    for (int i = index; i < recordCount - 1; i++) records[i] = records[i + 1]; recordCount--;
    syncRecordsToFirebase(); reply(true, "Record deleted"); return;
  }

  if (cmd == "recclear") {
    if (!hod) { reply(false, "HOD only"); return; }
    recordCount = 0; syncRecordsToFirebase(); reply(true, "All records cleared"); return;
  }

  if (cmd == "sethodpass") {
    if (!hod) { reply(false, "HOD only"); return; }
    if (!server.hasArg("old") || !server.hasArg("new")){ reply(false, "Missing"); return; }
    if (server.arg("old") != hodPassword){ reply(false, "Wrong password"); return; }
    String np = sanitize(server.arg("new")); if (np.length() < 4){ reply(false, "Min 4 chars"); return; }
    hodPassword = np; preferences.putString("hpass", hodPassword);
    for (int i = 0; i < MAX_TOKENS; i++) { tokens[i].value = ""; tokens[i].teacherId = ""; }
    reply(true, "Password updated"); return;
  }

  if (cmd == "setteacherpass") {
    if (!teacher && !hod) { reply(false, "Not allowed"); return; }
    if (!server.hasArg("old") || !server.hasArg("new")){ reply(false, "Missing"); return; }
    if (server.arg("old") != teacherPassword){ reply(false, "Wrong password"); return; }
    String np = sanitize(server.arg("new")); if (np.length() < 4){ reply(false, "Min 4 chars"); return; }
    teacherPassword = np; preferences.putString("tpass", teacherPassword);
    for (int i = 0; i < MAX_TOKENS; i++) { tokens[i].value = ""; tokens[i].teacherId = ""; }
    reply(true, "Password updated"); return;
  }

  reply(false, "Unknown command");
}

void handlePing() { server.send(200, "application/json", "{\"status\":\"ok\"}"); }
void handleNotFound() { server.send(404, "text/plain", "Not found: " + server.uri()); }

bool connectToInternet() {
  Serial.println(); Serial.println("Connecting to Internet Wi-Fi...");
  WiFi.begin(INTERNET_SSID, INTERNET_PASSWORD);
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 20000) { delay(500); Serial.print("."); }
  Serial.println();
  if (WiFi.status() == WL_CONNECTED) { Serial.print("Internet IP: "); Serial.println(WiFi.localIP()); return true; }
  Serial.println("Internet Wi-Fi failed."); return false;
}

bool startAttendanceAP() {
  if (!WiFi.softAPConfig(AP_IP, AP_GATEWAY, AP_SUBNET)) return false;
  bool ok = WiFi.softAP(AP_SSID, AP_PASSWORD, 1, false, 4); // Channel 1
  if (!ok) return false; delay(500); Serial.print("AP IP: "); Serial.println(WiFi.softAPIP()); return true;
}

void WiFiEvent(WiFiEvent_t event) {
  switch (event) {
    case ARDUINO_EVENT_WIFI_STA_GOT_IP: Serial.print("[WiFi] STA IP: "); Serial.println(WiFi.localIP()); break;
    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED: natEnabled = false; break;
    case ARDUINO_EVENT_WIFI_AP_STACONNECTED: Serial.println("[WiFi] Device joined AP"); break;
    case ARDUINO_EVENT_WIFI_AP_STADISCONNECTED: Serial.println("[WiFi] Device left AP"); break;
    default: break;
  }
}

void setup() {
  Serial.begin(115200); delay(1000);
  Serial.println("\n========================================");
  Serial.println(" ESP32 SMART ATTENDANCE v5.8 - Fast");
  Serial.println("========================================");

  if (!LittleFS.begin(true)) { Serial.println("LittleFS mount failed!"); } else { Serial.println("LittleFS mounted."); }

  preferences.begin("attendance", false);
  hodPassword = preferences.getString("hpass", DEFAULT_HOD_PASSWORD); teacherPassword = preferences.getString("tpass", DEFAULT_TEACHER_PASSWORD);
  for (int i = 0; i < MAX_TOKENS; i++) { tokens[i].value = ""; tokens[i].teacherId = ""; }

  WiFi.onEvent(WiFiEvent);
  WiFi.mode(WIFI_AP_STA);
  WiFi.setSleep(false);

  if (!startAttendanceAP()) { Serial.println("FATAL: AP failed."); while (true) delay(1000); }
  
  dnsServer.start(DNS_PORT, "*", AP_IP);

  bool ok = connectToInternet();
  if (ok) {
    configTime(19800, 0, "pool.ntp.org", "time.nist.gov");
    time_t now = time(nullptr); unsigned long ntpStart = millis();
    while (now < 24 * 3600 && millis() - ntpStart < 10000) { delay(500); now = time(nullptr); }

    enableNAT();

    config.api_key = FIREBASE_API_KEY; config.database_url = FIREBASE_DATABASE_URL; config.token_status_callback = tokenStatusCallback;
    Firebase.signUp(&config, &auth, "", "");
    Firebase.begin(&config, &auth); fbdo.setBSSLBufferSize(4096, 1024); Firebase.reconnectWiFi(true); firebaseReady = true;

    unsigned long t0 = millis();
    while (!Firebase.ready() && millis() - t0 < 10000) { delay(300); }
    if (Firebase.ready()) { loadAllFromFirebase(); }
  }

  updateClock();

  server.on("/",             HTTP_GET, handleRoot);
  server.on("/style.css",    HTTP_GET, handleCSS);
  server.on("/app.js",       HTTP_GET, handleJS);
  
  server.on("/api/recognize", HTTP_POST, handleProxyRecognize);
  server.on("/api/mark",      HTTP_POST, handleProxyMark);
  server.on("/api/photo",     HTTP_GET,  handleProxyPhoto);
  server.on("/api/sync",      HTTP_GET,  handleSyncTrigger);

  server.on("/generate_204", HTTP_GET, handleAndroid204);
  server.on("/hotspot-detect.html", HTTP_GET, handleAppleDetect);
  server.onNotFound(handleCaptivePortal);

  server.on("/api/data",     HTTP_GET, handleData);
  server.on("/api/login",    HTTP_GET, handleLogin);
  server.on("/api/student",  HTTP_GET, handleStudentLookup);
  server.on("/api/cmd",      HTTP_GET, handleCommand);
  server.on("/api/ping",     HTTP_GET, handlePing);
  server.begin();

  Serial.println("========================================");
  Serial.println(" WEB SERVER READY");
  Serial.print(" Portal: http://"); Serial.println(WiFi.softAPIP());
  Serial.println("========================================");
}

void loop() {
  dnsServer.processNextRequest();
  server.handleClient();
  updateClock();

  if (millis() - lastFirebaseCheck >= 10000) {
    lastFirebaseCheck = millis();
    if (!Firebase.ready()) { Firebase.reconnectWiFi(true); }
  }

  static unsigned long lastWiFiCheck = 0;
  if (millis() - lastWiFiCheck >= 5000) {
    lastWiFiCheck = millis();
    if (WiFi.status() != WL_CONNECTED) { 
      WiFi.disconnect(); 
      WiFi.begin(INTERNET_SSID, INTERNET_PASSWORD); 
    }
  }

  if (millis() - lastScan >= SCAN_INTERVAL) {
    lastScan = millis(); 
    scanDevices();
  }
}
