#include <SoftwareSerial.h>
#include <DHT.h>

// ================= HABERLEŞME =================
SoftwareSerial espSerial(A1, A2);

// ================= PİN TANIMLAMALARI =================
#define MQ2_PIN A0
#define LIMIT_SW A4

#define ENC_R 3 
#define ENC_L 2 
#define DHT_PIN 4

#define ENB 5
#define IN4 6
#define IN3 7
#define IN2 8
#define IN1 9
#define ENA 10

#define TRIG_PIN 11
#define ECHO_PIN 12

// ================= SENSÖR KURULUMLARI =================
#define DHTTYPE DHT11
DHT dht(DHT_PIN, DHTTYPE);

// ================= ALARM EŞİKLERİ VE ZAMANLAYICILAR =================
const int GAZ_ESIK = 600;         // Temiz hava genelde 150-250 arasıdır. 600 ciddi bir gaz kaçağıdır.
const float SICAKLIK_ESIK = 45.0; // 45 Derece üzeri yangın şüphesidir.
unsigned long sonSensorOkuma = 0; 
unsigned long sonAlarmZamani = 0;

// ================= ENKODER VE MOTOR =================
// ================= ENKODER VE MOTOR (DİJİTAL FİLTRELİ) =================
volatile long tickL = 0;
volatile long tickR = 0;

volatile unsigned long sonZamanL = 0;
volatile unsigned long sonZamanR = 0;

void encL() { 
  // Sinyal 3 milisaniyeden (3000 mikrosaniye) hızlı geldiyse PARAZİTTİR, sayma!
  if (micros() - sonZamanL > 200) {
    tickL++; 
    sonZamanL = micros();
  }
}

void encR() { 
  if (micros() - sonZamanR > 200) {
    tickR++; 
    sonZamanR = micros();
  }
}

enum HareketTipi { ILERI, GERI, SAG, SOL, DONUS, RAPORLA };
struct Gorev { HareketTipi tip; long hedef; };

#define MAX 20 
Gorev q[MAX];
int b = 0;
int s = 0;

bool aktif = false;
HareketTipi aktifTip;
long hedef = 0;
long startL = 0;
long startR = 0;

// ================= MOTOR FONKSİYONLARI =================
void motor(int l, int r) {
  l = constrain(l, -255, 255);
  r = constrain(r, -255, 255);

  if (l >= 0) { digitalWrite(IN1, HIGH); digitalWrite(IN2, LOW); } 
  else { digitalWrite(IN1, LOW); digitalWrite(IN2, HIGH); l = -l; }

  if (r >= 0) { digitalWrite(IN3, HIGH); digitalWrite(IN4, LOW); } 
  else { digitalWrite(IN3, LOW); digitalWrite(IN4, HIGH); r = -r; }

  analogWrite(ENA, l);
  analogWrite(ENB, r);
}

void dur() {
  analogWrite(ENA, 0); analogWrite(ENB, 0);
  digitalWrite(IN1, LOW); digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW); digitalWrite(IN4, LOW);
}

// ================= KUYRUK (Hafıza) =================
void ekle(int t, long h) {
  int n = (s + 1) % MAX;
  if (n == b) return;
  q[s].tip = (HareketTipi)t;
  q[s].hedef = h;
  s = n;
}

bool bos() { return b == s; }

void next() {
  if (bos()) {
    aktif = false;
    dur();
    espSerial.println("BITTI");
    return;
  }
  Gorev g = q[b];
  b = (b + 1) % MAX;
  aktifTip = g.tip;
  hedef = g.hedef;
  startL = tickL;
  startR = tickR;
  aktif = true;
}

// ================= RADAR =================
long mesafeOlc() {
  digitalWrite(TRIG_PIN, LOW); delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH); delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  long sure = pulseIn(ECHO_PIN, HIGH, 30000); 
  return sure * 0.034 / 2;
}

// ================= BEYİN VE KONTROL (Sola Çekme Düzeltildi) =================
void kontrol() {
  if (!aktif) return;

  if (digitalRead(LIMIT_SW) == HIGH) { 
    dur(); aktif = false; b = s = 0; 
    espSerial.println("CARPISMA"); 
    return;
  }

  if (aktifTip == RAPORLA) {
    dur();
    int gaz = analogRead(MQ2_PIN);
    float sicaklik = dht.readTemperature();
    espSerial.print("RAPOR|Sicaklik: "); espSerial.print(sicaklik);
    espSerial.print("C, Gaz: "); espSerial.print(gaz);
    delay(5000); next(); return;
  }

  if (aktifTip == ILERI) {
    long mesafe = mesafeOlc();
    if (mesafe > 0 && mesafe < 15) { dur(); return; }
  }

  long atilanAdimL = abs(tickL - startL);
  long atilanAdimR = abs(tickR - startR);
  long ortalamaAdim = min(atilanAdimL, atilanAdimR);

  if (ortalamaAdim >= hedef) {
    dur(); 
    delay(500);      // 🛑 Gövdenin parke üzerindeki kayması fiziksel olarak tamamen bitsin
   // 🔍 ZİHNİ SIFIRLAMADAN HEMEN ÖNCE SAYILARI ESP32'YE GÖNDER!
    Serial.print("RAPOR|Sol Tick: "); Serial.print(tickL);
    Serial.print(" - Sag Tick: "); Serial.println(tickR);
    noInterrupts();  
    tickL = 0;       // 🧠 Hafızayı güvenle temizle
    tickR = 0;
    interrupts();   
    next();          // Diğer komuta tertemiz başla
    return;
  }

  // 🛠️ MOTOR KALİBRASYONU VE ENKODER DENGELEYİCİ
  int solHiz = 155; 
  int sagHiz = 155; 
  
  // DİNAMİK DENGE (PID Mantığı Düzeltildi)
  int hata = atilanAdimL - atilanAdimR; 
  int duzeltme = constrain(hata / 3, -30, 30);

  // 🚨 EĞER SOL FAZLA GİTTİYSE (hata pozitifse) SOLU YAVAŞLAT, SAĞI HIZLANDIR!
  solHiz -= duzeltme; 
  sagHiz += duzeltme;

  // Hızların L298N sınırlarının dışına veya durma noktasına düşmesini engelle
  solHiz = constrain(solHiz, 100, 255);
  sagHiz = constrain(sagHiz, 100, 255);

  if (aktifTip == ILERI) {
    motor(solHiz, sagHiz);
  }
  else if (aktifTip == GERI) {
    motor(-solHiz, -sagHiz);
  }
  else if (aktifTip == SAG) {
    motor(-solHiz, sagHiz); // Dönüşlerde PID'ye gerek yok, kendi ana hızlarında dönsün
  }
  else if (aktifTip == SOL) {
    motor(solHiz, -sagHiz);
  }
  else if (aktifTip == DONUS) {
    motor(200, -200);
  }
}
// ================= ROTA VE KOMUTLAR (1 cm = 4 Adım Kuralına Göre Ayarlandı) =================
void komut(String k) {
  b = s = 0; aktif = false; dur();

  if (k == "MUTFAK") {
    ekle(ILERI, 200);  // 130 cm koridor 
    ekle(SAG, 26);     // 92-93 derece mutfağa dön
    ekle(ILERI, 100);  // 65 cm ocağa yanaş 
    ekle(RAPORLA, 0);  
    
    // DÖNÜŞ (U Dönüşlü)
    ekle(SAG, 60);    // 180 derece U Dönüşü
    ekle(ILERI, 100);  // Mutfaktan çık
    ekle(SOL, 27);     // Koridora dön
    ekle(ILERI, 100);  // Başlangıç öncesine git
    ekle(SAG, 56);    // yanaşmak için u dönüşü yap
    ekle(GERI, 110);   // Başlangıç konumu için yuvaya gir
    next();
  }
  else if (k == "ODA") {
    ekle(ILERI, 100); 
    ekle(SAG, 25);    
    ekle(ILERI, 300); 
    ekle(RAPORLA, 0);  
    
    ekle(SAG, 56);   
    ekle(ILERI, 300); 
    ekle(SOL, 25);    
    ekle(ILERI, 55);
    ekle(SAG, 56);   
    ekle(GERI, 55);   
    next();
  }
  

  else if (k == "SALON") {
    ekle(ILERI, 100); 
    ekle(SOL, 25);
    ekle(ILERI, 240);
    ekle(SOL, 25);
    ekle(ILERI, 200);
    ekle(SAG, 13);
    ekle(ILERI, 200);
    ekle(RAPORLA, 0);  
    
    ekle(SAG, 56);   
    ekle(ILERI, 200);
    ekle(SOL, 13);
    ekle(ILERI, 200);
    ekle(SAG, 25);
    ekle(ILERI, 240);  
    ekle(SOL, 25);
    ekle(GERI, 110);   // Başlangıç konumu için yuvaya gir
    next();
  }

  // 🛑 MANUEL KONTROLLER (Hassas Değerler)
  else if (k == "ILERI") { ekle(ILERI, 200); next(); } // Tam 130 cm
  else if (k == "GERI")  { ekle(GERI, 200); next(); }  // Tam 130 cm
  else if (k == "SAG")   { ekle(SAG, 25); next(); }   // Tam 90 Derece
  else if (k == "SOL")   { ekle(SOL, 25); next(); }   // Tam 90 Derece
  else if (k == "DUR")   { dur(); }
}

// ================= SETUP VE LOOP =================
void setup() {
  Serial.begin(115200); 
  espSerial.begin(9600); 
  dht.begin();

  pinMode(LIMIT_SW, INPUT_PULLUP);
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  pinMode(ENA, OUTPUT); pinMode(ENB, OUTPUT);
  pinMode(IN1, OUTPUT); pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT); pinMode(IN4, OUTPUT);

  pinMode(ENC_L, INPUT_PULLUP); pinMode(ENC_R, INPUT_PULLUP);

  attachInterrupt(digitalPinToInterrupt(ENC_L), encL, CHANGE);
  attachInterrupt(digitalPinToInterrupt(ENC_R), encR, CHANGE);

  dur();
}

// ================= SETUP VE LOOP =================
// ... (Setup kısmı aynı kalacak) ...

void loop() {
  // 1. 🚨 ARKA PLAN SENSÖR RADARI (SADECE ROBOT DURURKEN ÇALIŞIR)
  // "!aktif" demek, robot şu an hiçbir göreve gitmiyor, beklemede demektir.
  if (!aktif) {
    if (millis() - sonSensorOkuma > 2000) {
      sonSensorOkuma = millis();
      int anlikGaz = analogRead(MQ2_PIN);
      float anlikSicaklik = dht.readTemperature();

      // Eğer eşikler aşılırsa VE son alarmın üzerinden en az 30 saniye geçmişse (Spam engelleme)
      if ((anlikGaz > GAZ_ESIK || anlikSicaklik > SICAKLIK_ESIK) && (millis() - sonAlarmZamani > 30000)) {
        sonAlarmZamani = millis(); // Kronometreyi sıfırla
        
        if (anlikGaz > GAZ_ESIK) {
          espSerial.println("ACIL|GAZ:" + String(anlikGaz));
        } 
        else if (anlikSicaklik > SICAKLIK_ESIK) {
          espSerial.println("ACIL|YANGIN:" + String(anlikSicaklik));
        }
      }
    }
  }

  // 2. HABERLEŞMEYİ DİNLE (Kesintisiz)
  if (espSerial.available()) {
    String k = espSerial.readStringUntil('\n');
    k.trim();
    komut(k);
  }

  // 3. MOTOR VE ENKODER KONTROLÜ (Kesintisiz)
  kontrol();
}
