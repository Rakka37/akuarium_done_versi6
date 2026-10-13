#include "esp_camera.h"
#include <WiFi.h>
#include <WiFiClient.h>
#include <HTTPClient.h>

#define CAMERA_MODEL_AI_THINKER
#include "camera_pins.h"
const char* ssid = "Togik";
const char* password = "togik123";
// const char* ssid = "HUAWEI-2.4G-bUU5";
// const char* password = "indonesia";
// const char* ssid = "Achillscndyfdla";
// const char* password = "cindy2005";

String uploadURL = "http://akuariumsmkpwr.my.id/upload.php";

String healthURL = "http://akuariumsmkpwr.my.id/health.php";

WiFiClient client;

bool cameraReady=false;
bool firstFrame=true;

uint32_t lastHash=0;

unsigned long lastCapture=0;
unsigned long lastHealth=0;

int failCount=0;



// ================= WIFI =================
void connectWiFi(){

  if(WiFi.status()==WL_CONNECTED) return;

  WiFi.begin(ssid,password);
  WiFi.setSleep(false);

  Serial.print("Connecting WiFi");

  int retry=0;

  while(WiFi.status()!=WL_CONNECTED){
    delay(500);
    Serial.print(".");
    retry++;

    if(retry>40){
      ESP.restart();
    }
  }

  Serial.println("\nWiFi Connected");
}



// ================= HEALTH =================
void sendHealth(String status){

  HTTPClient http;

  String url = healthURL + "?status=" + status;

  http.begin(client,url);
  http.GET();
  http.end();
}



// ================= CAMERA INIT =================
void initCamera(){

  camera_config_t config;

  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;

  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;

  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;

  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;

  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;

  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;

  // resolusi cukup tinggi tapi stabil
  config.frame_size = FRAMESIZE_SVGA;

  // kualitas jpeg tinggi
  config.jpeg_quality = 5;

  // double buffer
  config.fb_count = 2;

  esp_err_t err = esp_camera_init(&config);

  if(err != ESP_OK){
    Serial.println("Camera Init Failed");
    delay(3000);
    ESP.restart();
  }

  sensor_t * s = esp_camera_sensor_get();

  // tuning kualitas
  s->set_brightness(s,1);
  s->set_contrast(s,2);
  s->set_saturation(s,2);

  // kurangi noise
  s->set_gain_ctrl(s,0);
  s->set_agc_gain(s,4);

  // exposure stabil
  s->set_exposure_ctrl(s,1);
  s->set_aec2(s,1);

  // warna stabil
  s->set_whitebal(s,1);
  s->set_awb_gain(s,1);

  // detail
  s->set_denoise(s,1);
  s->set_sharpness(s,2);

  cameraReady=true;

  Serial.println("Camera Ready");

  sendHealth("READY");
}



// ================= HASH FRAME =================
uint32_t getHash(camera_fb_t * fb){

  uint32_t h=0;

  for(int i=0;i<fb->len;i+=80){
    h+=fb->buf[i];
  }

  return h;
}



// ================= UPLOAD FOTO =================
void upload(camera_fb_t * fb){

  HTTPClient http;

  http.begin(client,uploadURL);

  http.addHeader("Content-Type","image/jpeg");

  int code=http.POST(fb->buf,fb->len);

  Serial.print("Upload Code: ");
  Serial.println(code);

  http.end();
}



// ================= SETUP =================
void setup(){

  Serial.begin(115200);

  delay(3000);

  connectWiFi();

  initCamera();
}



// ================= LOOP =================
void loop(){

  connectWiFi();

  if(millis()-lastHealth>15000){

    sendHealth("ONLINE");

    lastHealth=millis();
  }


  // discard frame pertama
  camera_fb_t * fb = esp_camera_fb_get();
  esp_camera_fb_return(fb);

  delay(40);

  fb = esp_camera_fb_get();

  if(!fb){

    failCount++;

    if(failCount>=5) ESP.restart();

    return;
  }

  failCount=0;

  uint32_t nowHash = getHash(fb);

  if(firstFrame){

    lastHash=nowHash;
    firstFrame=false;

    esp_camera_fb_return(fb);

    return;
  }

  int diff = abs((int)nowHash-(int)lastHash);

  Serial.print("Frame Diff: ");
  Serial.println(diff);

  if(diff>2000 && millis()-lastCapture>5000){

    Serial.println("Ikan lewat → Foto");

    upload(fb);

    lastCapture=millis();
  }

  lastHash=nowHash;

  esp_camera_fb_return(fb);

  delay(1200);
}