#include <WiFi.h>
#include <WiFiClientSecure.h>
#include "esp_camera.h"
#include <UniversalTelegramBot.h>
#include <ArduinoJson.h>

// ================= KİŞİSEL BİLGİLER (BURAYI DOLDUR) =================
//const char* ssid = "F**********"; 
const char* ssid = "***********"; 
const char* password = "*********";
//const char* password = "********";
String BOTtoken = "********:****************";
String CHAT_ID = "**********";
//String CHAT_ID = "**********";

// ================= TELEGRAM AYARLARI =================
WiFiClientSecure clientTCP;
UniversalTelegramBot bot(BOTtoken, clientTCP);

int botRequestDelay = 3000; // Isınmayı önlemek için 3 saniyede bir kontrol
unsigned long lastTimeBotRan;

// ================= KAMERA PİN AYARLARI (AI THINKER) =================
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27
#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

// ================= FOTOĞRAF ÇEKME VE GÖNDERME (Kukla Çekim Taktiği) =================
String sendPhotoTelegram(String caption) {
  const char* myDomain = "api.telegram.org";
  
  // 1. HAYAT KURTARAN HAMLE: Kameranın donanımsal hafızasındaki o bayat (eski) kareyi çek ve hemen çöpe at!
  camera_fb_t * dummy = esp_camera_fb_get();  
  if (dummy) {
    esp_camera_fb_return(dummy); // Sensörün hafızasını boşalttık
  }

  // 2. ŞİMDİ ASIL TAPTAZE FOTOĞRAFI ÇEK
  camera_fb_t * fb = esp_camera_fb_get();  
  if(!fb) {
    bot.sendMessage(CHAT_ID, "Kamera hatasi: Fotograf cekilemedi.", "");
    return "Hata";
  }
  
  // 3. Telegram'a gönder
  if (clientTCP.connect(myDomain, 443)) {
    String head = "--GozcuKulesi\r\nContent-Disposition: form-data; name=\"chat_id\"; \r\n\r\n" + CHAT_ID + "\r\n--GozcuKulesi\r\nContent-Disposition: form-data; name=\"photo\"; filename=\"esp32-cam.jpg\"\r\nContent-Type: image/jpeg\r\n\r\n";
    String tail = "\r\n--GozcuKulesi\r\nContent-Disposition: form-data; name=\"caption\"; \r\n\r\n" + caption + "\r\n--GozcuKulesi--\r\n";

    uint16_t imageLen = fb->len;
    uint16_t extraLen = head.length() + tail.length();
    uint16_t totalLen = imageLen + extraLen;
  
    clientTCP.println("POST /bot"+BOTtoken+"/sendPhoto HTTP/1.1");
    clientTCP.println("Host: " + String(myDomain));
    clientTCP.println("Content-Length: " + String(totalLen));
    clientTCP.println("Content-Type: multipart/form-data; boundary=GozcuKulesi");
    clientTCP.println();
    clientTCP.print(head);
  
    uint8_t *fbBuf = fb->buf;
    size_t fbLen = fb->len;
    for (size_t n=0;n<fbLen;n=n+1024) {
      if (n+1024<fbLen) {
        clientTCP.write(fbBuf, 1024);
        fbBuf += 1024;
      }
      else if (fbLen%1024>0) {
        size_t remainder = fbLen%1024;
        clientTCP.write(fbBuf, remainder);
      }
    }  
    clientTCP.print(tail);
    
    // 4. İŞ BİTTİ: Bu yeni fotoğrafı da RAM'den sil
    esp_camera_fb_return(fb);
    clientTCP.stop();
  }
  return "Basarili";
}

// ================= TELEGRAM KOMUT YÖNETİMİ =================
void handleNewMessages(int numNewMessages) {
  for (int i = 0; i < numNewMessages; i++) {
    String chat_id = String(bot.messages[i].chat_id);
    if (chat_id != CHAT_ID) continue; // Yabancıları engelle
    
    String text = bot.messages[i].text;

    if (text == "/start") {
      String welcome = "OKB Asistani Aktif! \n";
      welcome += "Gorev emirlerinizi bekliyorum.\n";
      bot.sendMessage(chat_id, welcome, "");
    }
    
    // OTONOM GÖREVLER
    else if (text == "/mutfak") {
      bot.sendMessage(chat_id, "🔍 Mutfak devriyesi basladi. Ocaga gidiliyor...", "");
      Serial.println("MUTFAK");
    }
    else if (text == "/oda") {
      bot.sendMessage(chat_id, "🔍 Oda devriyesi basladi. Gidiliyor...", "");
      Serial.println("ODA");
    }
    else if (text == "/salon") {
      bot.sendMessage(chat_id, "🔍 Salon devriyesi basladi. Gidiliyor...", "");
      Serial.println("SALON");
    }
    
    // ANLIK RAPOR VE MANUEL KONTROL
    else if (text == "/foto") {
      bot.sendMessage(chat_id, "Fotograf cekiliyor...", "");
      sendPhotoTelegram("Anlik Durum Goruntusu");
    }
    else if (text == "/ileri") { Serial.println("ILERI"); }
    else if (text == "/geri")  { Serial.println("GERI"); }
    else if (text == "/sag")   { Serial.println("SAG"); }
    else if (text == "/sol")   { Serial.println("SOL"); }
    else if (text == "/dur")   { 
      Serial.println("DUR"); 
      bot.sendMessage(chat_id, "Acil fren yapildi!", "");
    }
  }
}

// ================= KURULUM =================
void setup() {
  // Haberleşme hızı Arduino SoftwareSerial'e uygun olarak KESİNLİKLE 9600 olmalı
  Serial.begin(9600); 

  // Wi-Fi Bağlantısı
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  clientTCP.setCACert(TELEGRAM_CERTIFICATE_ROOT);
  while (WiFi.status() != WL_CONNECTED) { delay(500); }

  // Kamera Kurulumu
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

  if(psramFound()){
    config.frame_size = FRAMESIZE_XGA;
    config.jpeg_quality = 8;
    config.fb_count = 1;
  } else {
    config.frame_size = FRAMESIZE_SVGA;
    config.jpeg_quality = 10;
    config.fb_count = 1;
  }
  
  esp_camera_init(&config);
}

// ================= ANA DÖNGÜ =================
void loop() {
  // 1. TELEGRAM MESAJ KONTROLÜ
  if (millis() > lastTimeBotRan + botRequestDelay)  {
    int numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    while(numNewMessages) {
      handleNewMessages(numNewMessages);
      numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    }
    lastTimeBotRan = millis();
  }

  // 2. ARDUINO'DAN GELEN VERİLERİ (OMURİLİĞİ) DİNLE
  if (Serial.available()) {
    String gelenVeri = Serial.readStringUntil('\n');
    gelenVeri.trim();

    // Normal Rapor
    if (gelenVeri.startsWith("RAPOR|")) {
      String sensorRaporu = gelenVeri.substring(6); 
      sendPhotoTelegram("📸 Hedef Konum Raporu:\n\n" + sensorRaporu); 
    }
    // Limit Switch Çarpışması
    else if (gelenVeri == "CARPISMA") {
      bot.sendMessage(CHAT_ID, "⚠️ ACIL DURUM: Asistan engelle karsilasti (Limit Switch). Motorlar kilitlendi!", "");
    }
    // Otonom Devriye Bitişi
    else if (gelenVeri == "BITTI") {
      bot.sendMessage(CHAT_ID, "✅ Gorev tamamlandi. Asistan yuvaya dondu ve beklemede.", "");
    }
    
    // 🚨 YENİ EKLENEN: 7/24 GAZ VE YANGIN ALARMI KONTROLÜ
    else if (gelenVeri.startsWith("ACIL|")) {
      if (gelenVeri.indexOf("GAZ") != -1) {
        String gazSeviyesi = gelenVeri.substring(9);
        String mesaj = "🚨 DİKKAT! TEHLİKELİ GAZ KAÇAĞI! 🚨\nSeviye: " + gazSeviyesi + "\nDerhal ortamı havalandırın!";
        bot.sendMessage(CHAT_ID, mesaj, "");
        sendPhotoTelegram("🚨 Anlık Gaz Kaçağı Görüntüsü!"); // Kanıt için fotoğraf da atsın
      } 
      else if (gelenVeri.indexOf("YANGIN") != -1) {
        String sicaklikSeviyesi = gelenVeri.substring(12);
        String mesaj = "🔥 DİKKAT! YANGIN ŞÜPHESİ! 🔥\nSıcaklık: " + sicaklikSeviyesi + "°C\nAcil kontrol ediniz!";
        bot.sendMessage(CHAT_ID, mesaj, "");
        sendPhotoTelegram("🔥 Anlık Ortam Görüntüsü!");
      }
    }
  }
}  
