#include <WiFi.h>
#include <AsyncFsWebServer.h>
#include <FS.h>
#include <LittleFS.h>
#include <time.h>

// NTP 서버 설정
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 9 * 3600;  // 한국 시간 (GMT+9)
const int daylightOffset_sec = 0;


const char* ssid = "와이파이이름";
const char* password = "비밀번호";

AsyncFsWebServer server(80, LittleFS, "myServer");
#define LED1 5
#define LED2 18
#define LED3 19

struct LEDSchedule {
    String onTime;
    String offTime;
    bool enabled;
};

LEDSchedule schedules[4];  // 1~3번 LED 사용 (index 0은 미사용)

// FILESYSTEM INIT
bool startFilesystem(){
  if (LittleFS.begin()){
      File root = LittleFS.open("/", "r");
      File file = root.openNextFile();
      while (file){
          Serial.printf("FS File: %s, size: %d\n", file.name(), file.size());
          file = root.openNextFile();
      }
      return true;
  }
  else {
      Serial.println("ERROR on mounting filesystem. It will be reformatted!");
      LittleFS.format();
      ESP.restart();
  }
  return false;
}


// FS 정보 가져오기
void getFsInfo(fsInfo_t* fsInfo) {
    fsInfo->fsName = "LittleFS";
    fsInfo->totalBytes = LittleFS.totalBytes();
    fsInfo->usedBytes = LittleFS.usedBytes();
}


void handleLed(AsyncWebServerRequest *request) {
    if (!request->hasParam("led") || !request->hasParam("state")) {
        request->send(400, "text/plain", "Bad Request: Missing parameters");
        return;
    }

    int led = request->getParam("led")->value().toInt();
    String stateStr = request->getParam("state")->value();
    bool state = (stateStr == "on" || stateStr == "1"); 

    switch (led) {
        case 1: digitalWrite(LED1, state); break;
        case 2: digitalWrite(LED2, state); break;
        case 3: digitalWrite(LED3, state); break;
        default: 
            request->send(400, "text/plain", "Bad Request: Invalid LED number");
            return;
    }

    String response = "LED " + String(led) + " is now " + (state ? "ON" : "OFF");
    request->send(200, "text/plain", response);
}


// JSON 파일에서 스케줄 로드
void loadSchedule() {
    File file = LittleFS.open("/schedule.json", "r");
    if (!file) {
        Serial.println("스케줄 파일 없음, 기본값 사용.");
        return;
    }

    DynamicJsonDocument doc(512);
    deserializeJson(doc, file);
    file.close();

    for (int i = 1; i <= 3; i++) {
        schedules[i].onTime = doc[String(i)]["onTime"].as<String>();
        schedules[i].offTime = doc[String(i)]["offTime"].as<String>();
        schedules[i].enabled = doc[String(i)]["enabled"].as<bool>();
    }

    Serial.println("스케줄 로드 완료.");
}

// JSON 파일에 스케줄 저장
void saveSchedule() {
    DynamicJsonDocument doc(512);
    for (int i = 1; i <= 3; i++) {
        doc[String(i)]["onTime"] = schedules[i].onTime;
        doc[String(i)]["offTime"] = schedules[i].offTime;
        doc[String(i)]["enabled"] = schedules[i].enabled;
    }

    File file = LittleFS.open("/schedule.json", "w");
    serializeJson(doc, file);
    file.close();
}

// 현재 시간 가져오기
String getCurrentTime() {
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
        return "00:00";
    }
    char buffer[6];
    snprintf(buffer, sizeof(buffer), "%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min);
    return String(buffer);
}

// 자동 스케줄 실행
void checkSchedule() {
    String currentTime = getCurrentTime();
    Serial.println(currentTime);
    for (int i = 1; i <= 3; i++) {
        if (schedules[i].enabled) {
            if (schedules[i].onTime == currentTime) {
                digitalWrite(i == 1 ? LED1 : (i == 2 ? LED2 : LED3), HIGH);
            } else if (schedules[i].offTime == currentTime) {
                digitalWrite(i == 1 ? LED1 : (i == 2 ? LED2 : LED3), LOW);
            }
        }
    }
}

// 스케줄 설정 엔드포인트
void handleSetSchedule(AsyncWebServerRequest *request) {
    if (!request->hasParam("led") || !request->hasParam("onTime") || !request->hasParam("offTime") || !request->hasParam("enabled")) {
        request->send(400, "text/plain", "Bad Request: Missing parameters");
        return;
    }

    int led = request->getParam("led")->value().toInt();
    schedules[led].onTime = request->getParam("onTime")->value();
    schedules[led].offTime = request->getParam("offTime")->value();
    schedules[led].enabled = request->getParam("enabled")->value().toInt();

    saveSchedule();
    request->send(200, "text/plain", "Schedule updated");
}

// 스케줄 정보 불러오기
void handleGetSchedule(AsyncWebServerRequest *request) {
    DynamicJsonDocument doc(512);
    for (int i = 1; i <= 3; i++) {
        doc[String(i)]["onTime"] = schedules[i].onTime;
        doc[String(i)]["offTime"] = schedules[i].offTime;
        doc[String(i)]["enabled"] = schedules[i].enabled;
    }

    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
}


void setup() {
    Serial.begin(115200);
    pinMode(LED1, OUTPUT);
    pinMode(LED2, OUTPUT);
    pinMode(LED3, OUTPUT);
    
    if (!startFilesystem()) {
        Serial.println("LittleFS 오류!");
    }



    loadSchedule();  // 스케줄 불러오기


    // NTP 서버에서 시간 동기화
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    Serial.println("NTP 시간 동기화 완료!");


    IPAddress myIP = server.startWiFi(15000, ssid, password);
    WiFi.setSleep(WIFI_PS_NONE);

    // 웹 설정 페이지 및 파일 에디터 활성화
    server.enableFsCodeEditor();
    server.setFsInfoCallback(getFsInfo);

    // 파일 제공
    server.serveStatic("/", LittleFS, "/").setDefaultFile("index.html");
    
    // API 엔드포인트 추가
    server.on("/schedule", HTTP_GET, handleSetSchedule);
    server.on("/getSchedule", HTTP_GET, handleGetSchedule);

    // API 엔드포인트
    server.on("/led", HTTP_GET, handleLed);

    // 서버 시작
    server.init();
    Serial.print("ESP32 Web Server started at: ");
    Serial.println(myIP);
}

void loop() {
  checkSchedule();
    delay(1000);  // 1초마다 스케줄 체크
}
