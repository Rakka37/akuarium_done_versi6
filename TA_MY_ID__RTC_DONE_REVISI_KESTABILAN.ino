#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <ESP32Servo.h>
#include <time.h>
#include <ThreeWire.h>
#include <RtcDS1302.h>

// ================= WIFI =================
const char* ssid = "HUAWEI-2.4G-bUU5";
const char* password = "indonesia";
// const char* ssid = "Rakka Levine Nathaniel ";
// const char* password = "rakka1922";
// ================= SERVER =================
const char* serverName = "https://akuariumrakka.my.id";

// ================= PIN =================
#define ONE_WIRE_BUS    5

// ===== TURBIDITY =====
#define TURBIDITY_A0    34

#define PH_PIN          35

#define RELAY_KIPAS     12
#define RELAY_HEATER    13
#define RELAY_KURAS     26
#define RELAY_ISI       25
#define RELAY_NTU       33

#define TRIG_PIN        18
#define ECHO_PIN        19

#define SERVO_PIN       2
#define SERVO_PH_PIN    21

#define RTC_CLK 4
#define RTC_DAT 22
#define RTC_RST 23

ThreeWire myWire(RTC_DAT, RTC_CLK, RTC_RST);
RtcDS1302<ThreeWire> rtc(myWire);

// ================= OBJECT =================
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature waterSensor(&oneWire);

Servo servo;
Servo servoPH;

WiFiClientSecure client;

// ================= PH =================
#define SAMPLES 10

float voltage;
float phValue;
float lastPH = 7.0;

int ntu = 0;
int lastNTU = 0;
String statusAir = "JERNIH";
String lastStatusAir = "JERNIH";

bool phAktif = false;

// ================= STATE =================
bool prosesKuras = false;
bool prosesIsi = false;

// ================= SERVO PH =================
int stopServoPH = 88;
int atasPH = 180;
int bawahPH = 0;

unsigned long lastServoTime = 0;
int stateServoPH = 0;

// ================= FEED =================
int jadwal[3][2] = {
  {7,00},
  {13,00},
  {18,00}
};

bool sudahMakan[3] = {false,false,false};


// =====================================================
// SERVO PAKAN (FULL POWER + DETACH)
// =====================================================
void putarServo(int putaran){
  servo.attach(SERVO_PIN, 500, 2500);
  delay(200);
  for(int i = 0; i < putaran; i++){
    servo.writeMicroseconds(2500);
    delay(400);
    servo.writeMicroseconds(1508);
    delay(300);
  }
  servo.detach();
}
// =====================================================
// SERVO PH
// =====================================================
void putarServoPH_Detik(int arah, int durasi_ms){

  servoPH.attach(SERVO_PH_PIN, 1000, 2000);

  servoPH.write(arah);

  delay(durasi_ms);

  servoPH.write(stopServoPH);

  delay(500);

  servoPH.detach();
}

void kontrolServoPH(){

  unsigned long now = millis();

  switch(stateServoPH){

   case 0:
      digitalWrite(RELAY_NTU, LOW);
      Serial.println("NTU OFF");
      Serial.println("PH TURUN 1 DETIK");
      putarServoPH_Detik(bawahPH, 500);
      lastServoTime = now;
      stateServoPH = 1;
      phAktif = true;
      break;

    case 1:
      if(now - lastServoTime >= 60000 ){ //120000
        Serial.println("SELESAI BACA PH");
        stateServoPH = 2;
        lastServoTime = now;
      }
      break;

    case 2:
      digitalWrite(RELAY_NTU, HIGH);
      delay(1000);
      Serial.println("NTU ON");
      Serial.println("PH NAIK 600 M DETIK");
      putarServoPH_Detik(atasPH, 600);
      lastServoTime = now;
      stateServoPH = 3;
      phAktif = false;
      break;

    case 3:

      if(now - lastServoTime >= 300000){ //300000

        Serial.println("SELESAI ISTIRAHAT");

        stateServoPH = 0;

        lastServoTime = now;
      }

      break;
  }
}

// =====================================================
// ULTRASONIC
// =====================================================
float bacaUltrasonik(){

  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);

  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);

  digitalWrite(TRIG_PIN, LOW);

  long durasi = pulseIn(ECHO_PIN, HIGH);

  float jarak = durasi * 0.034 / 2;

  return jarak;
}

// =====================================================
// PH SENSOR
// =====================================================
float readPH(){

  int buffer[SAMPLES];

  for (int i = 0; i < SAMPLES; i++){

    buffer[i] = analogRead(PH_PIN);

    delay(20);
  }

  for (int i = 0; i < SAMPLES - 1; i++){

    for (int j = i + 1; j < SAMPLES; j++){

      if (buffer[i] > buffer[j]){

        int temp = buffer[i];

        buffer[i] = buffer[j];

        buffer[j] = temp;
      }
    }
  }

  int median = buffer[SAMPLES / 2];

  voltage = median * (3.3 / 4095.0);

  float ph = 7 + ((2.5 - voltage) / 0.18);

  return ph;
}

// =====================================================
// HTTPS GET
// =====================================================
void sendGET(String url){

  if(WiFi.status() != WL_CONNECTED){

    Serial.println("WiFi tidak terhubung");

    return;
  }

  HTTPClient https;

  client.setInsecure();

  Serial.println("================================");

  Serial.print("REQUEST: ");
  Serial.println(url);

  https.begin(client, url);

  int httpCode = https.GET();

  Serial.print("HTTP CODE: ");
  Serial.println(httpCode);

  https.end();
}

// =====================================================
// SETUP
// =====================================================
void setup() {
  Serial.begin(115200);
  waterSensor.begin();
  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);
  analogSetPinAttenuation(TURBIDITY_A0, ADC_11db);
  analogSetPinAttenuation(PH_PIN, ADC_11db);

  pinMode(TURBIDITY_A0, INPUT);
  pinMode(PH_PIN, INPUT);
  pinMode(RELAY_KIPAS, OUTPUT);
  pinMode(RELAY_HEATER, OUTPUT);
  pinMode(RELAY_KURAS, OUTPUT);
  pinMode(RELAY_ISI, OUTPUT);
  pinMode(RELAY_NTU, OUTPUT);
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  digitalWrite(RELAY_KIPAS, LOW);
  digitalWrite(RELAY_HEATER, LOW);
  digitalWrite(RELAY_KURAS, LOW);
  digitalWrite(RELAY_ISI, LOW);
  digitalWrite(RELAY_NTU, HIGH);
  
  servo.setPeriodHertz(50);
  servoPH.setPeriodHertz(50);
  servoPH.attach(SERVO_PH_PIN, 500, 2400);
  servoPH.write(stopServoPH);
  rtc.Begin();
  if (!rtc.IsDateTimeValid()) {
    Serial.println("RTC tidak valid, set waktu awal");
    rtc.SetDateTime(RtcDateTime(__DATE__, __TIME__));
  }

  // ===== WIFI =====
  WiFi.begin(ssid, password);
  Serial.print("Connecting WiFi");
  unsigned long startAttemptTime = millis();
  while (WiFi.status() != WL_CONNECTED &&
         millis() - startAttemptTime < 10000){
    Serial.print(".");
    delay(500);
  }

  if(WiFi.status() == WL_CONNECTED){
    Serial.println("CONNECTED");
  }
  else{
    Serial.println("WIFI GAGAL → MODE OFFLINE");
  }

  configTime(7*3600, 0, "pool.ntp.org");
  
  // ===== SYNC NTP → RTC =====
  struct tm timeinfo;

  if (getLocalTime(&timeinfo)) {

    Serial.println("SYNC NTP → RTC");

    rtc.SetDateTime(RtcDateTime(
      timeinfo.tm_year + 1900,
      timeinfo.tm_mon + 1,
      timeinfo.tm_mday,
      timeinfo.tm_hour,
      timeinfo.tm_min,
      timeinfo.tm_sec
    ));
  }

  Serial.println("SYSTEM START");
}

// =====================================================
// LOOP
// =====================================================
void loop() {

  kontrolServoPH();

  // =====================================================
  // SUHU
  // =====================================================
  waterSensor.requestTemperatures();

  float suhuAir = waterSensor.getTempCByIndex(0);

  // =====================================================
  // NTU FINAL VALID
  // =====================================================
  int adc = 0;
  float voltageNTU = 0;

  if(!phAktif){

    long total = 0;

    for(int i = 0; i < 500; i++){

      total += analogRead(TURBIDITY_A0);

      delay(2);
    }

    adc = total / 500;

    voltageNTU = adc * (3.3 / 4095.0);

    if(adc > 3500){

      ntu = map(adc, 4095, 3500, 0, 19);
    }
    else if(adc > 2200){

      ntu = map(adc, 3500, 2200, 20, 49);
    }
    else{

      ntu = map(adc, 2200, 1500, 50, 100);
    }

    if(ntu < 0) ntu = 0;
    if(ntu > 100) ntu = 100;

    if(ntu <= 19){

      statusAir = "JERNIH";
    }
    else if(ntu <= 49){

      statusAir = "SEDANG";
    }
    else{

      statusAir = "KERUH";
    }

    lastNTU = ntu;
    lastStatusAir = statusAir;
  }
  else{

    ntu = lastNTU;
    statusAir = lastStatusAir;
  }
  // =====================================================
  // PH
  // =====================================================
  if(phAktif){

    phValue = readPH();

    lastPH = phValue;
  }
  else{

    phValue = lastPH;
  }

  // =====================================================
  // ULTRASONIC
  // =====================================================
  float jarakAir = bacaUltrasonik();

  // =====================================================
  // SERIAL
  // =====================================================
  Serial.println("========== SENSOR ==========");

  Serial.print("Suhu Air : ");
  Serial.println(suhuAir);

  Serial.print("ADC NTU : ");
  Serial.println(adc);

  Serial.print("Voltage NTU : ");
  Serial.println(voltageNTU, 3);

  Serial.print("NTU : ");
  Serial.println(ntu);

  Serial.print("Status Air : ");
  Serial.println(statusAir);

  Serial.print("pH : ");
  Serial.println(phValue);

  Serial.print("Jarak Air : ");
  Serial.println(jarakAir);

  // =====================================================
  // SUHU CONTROL
  // =====================================================
  if (suhuAir >= 30) {

    digitalWrite(RELAY_KIPAS, HIGH);
    digitalWrite(RELAY_HEATER, LOW);
  } 
  else if (suhuAir <= 25) {

    digitalWrite(RELAY_KIPAS, LOW);
    digitalWrite(RELAY_HEATER, HIGH);
  } 
  else {

    digitalWrite(RELAY_KIPAS, LOW);
    digitalWrite(RELAY_HEATER, LOW);
  }

  // =====================================================
  // KURAS
  // =====================================================
  if(ntu >= 50 && !prosesKuras && !prosesIsi){

    sendGET(String(serverName)+"/aktuator.php?pesan=Air%20keruh,%20pompa%20kuras%20aktif&jenis=ALERT");

    prosesKuras = true;

    digitalWrite(RELAY_KURAS, HIGH);
  }

  // =====================================================
  // KURAS SELESAI → ISI
  // =====================================================
  if(prosesKuras && jarakAir >= 17){

    Serial.println("KURAS SELESAI → ISI");

    digitalWrite(RELAY_KURAS, LOW);

    prosesKuras = false;

    prosesIsi = true;

    digitalWrite(RELAY_ISI, HIGH);

    sendGET(String(serverName)+"/aktuator.php?pesan=Air%20habis%20dikuras,%20pompa%20isi%20aktif&jenis=INFO");
  }

  // =====================================================
  // ISI SELESAI
  // =====================================================
  if(prosesIsi && jarakAir <= 7.80){

    Serial.println("AIR PENUH → NORMAL");

    digitalWrite(RELAY_ISI, LOW);

    prosesIsi = false;

    sendGET(String(serverName)+"/aktuator.php?pesan=Air%20sudah%20penuh,%20sistem%20normal&jenis=INFO");
  }

  // =====================================================
  // KIRIM DATA
  // =====================================================
  String statusPH = phAktif ? "TURUN" : "NAIK";

  String urlUpdate = String(serverName) +
    "/update.php?air=" + String(suhuAir,1) +
    "&ntu=" + String(ntu) +
    "&ph=" + String(phValue,2) +
    "&jarak=" + String(jarakAir,1) +
    "&ph_status=" + statusPH;

  sendGET(urlUpdate);

  // =====================================================
  // MANUAL FEED
  // =====================================================
  HTTPClient https;

  client.setInsecure();

  https.begin(client,String(serverName)+"/feed.php");

  int code = https.GET();

  String res = https.getString();

  https.end();

  if(res.indexOf("1") >= 0){

    Serial.println("PAKAN MANUAL");

    putarServo(1);

    sendGET(String(serverName)+"/feed.php?reset=1");
  }

  // =====================================================
  // JADWAL (NTP + RTC BACKUP)
  // =====================================================
  int jam, menit;

  struct tm timeinfo;

  if (getLocalTime(&timeinfo)) {

    jam = timeinfo.tm_hour;
    menit = timeinfo.tm_min;
  } 
  else {

    RtcDateTime now = rtc.GetDateTime();

    jam = now.Hour();
    menit = now.Minute();

    Serial.println("MODE OFFLINE - RTC AKTIF");
  }

  // =====================================================
  // EKSEKUSI JADWAL
  // =====================================================
  for(int i=0;i<3;i++){

    if(jam == jadwal[i][0] &&
       menit == jadwal[i][1] &&
       !sudahMakan[i]){

      Serial.println("PAKAN OTOMATIS");

      putarServo(1);

      sendGET(String(serverName)+"/auto_feed.php");

      sudahMakan[i] = true;
    }
  }

  // =====================================================
  // RESET HARIAN
  // =====================================================
  if(jam == 0 && menit == 0){

    for(int i=0;i<3;i++){

      sudahMakan[i] = false;
    }
  }

  delay(2000);
}