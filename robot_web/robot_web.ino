/*
 * ESP32-WROOM-32U  +  4x goBILDA 1x15A sürücü + Yellow Jacket enkoderli motor  +  Adafruit BNO055 (I2C)
 *  +  WEB DRIVER STATION (WiFi + tarayıcıdan Xbox kol)
 *
 * ================= DRIVER STATION =================
 * Kart: normal "esp32" > "ESP32 Dev Module" (Bluepad32 kartı DEĞİL). Ek kütüphane gerekmez.
 * 1) Yükleyin. ESP32 kendi WiFi ağını açar: WIFI_SSID / WIFI_PASS (aşağıda)
 * 2) Bilgisayarı/telefonu bu ağa bağlayın, Chrome veya Edge ile  http://192.168.4.1  açın
 * 3) Xbox kolunu KABLOYLA bilgisayara (veya OTG ile telefona) takın, kolda bir tuşa basın
 *    -> Joystick sekmesinde "Connected" görünür. Kolu tarayıcı okur ve robota WiFi ile gönderir.
 * 4) Dashboard: Init -> Start (önce otonom, süre dolunca teleop) -> Stop
 *    EMERGENCY STOP: robot yeniden başlatılana kadar kilitli kalır (Reboot Robot düğmesi).
 *
 * Aynı anda tek cihaz kontrol eder (ilk açan). Sahip 5 sn sessiz kalırsa yer boşalır.
 * Kol verisi 500 ms gelmezse kol değerleri sıfırlanır; Driver Station 10 sn sessizse robot Stop'a geçer.
 *
 * TELEOP KOL TUŞLARI (teleopLoop içinde, değiştirebilirsiniz)
 *   Sol çubuk: ileri/geri + yana kayma   Sağ çubuk X: dönme
 *   RB basılı: turbo (%100)   LB basılı: yavaş (%25)   normal: %NORMAL_SPEED
 *   Y: saha-yönlü sürüş aç/kapa     X: heading + konum sıfırla
 *
 * KENDİ KODUNUZ: aşağıdaki "MAÇ FONKSİYONLARI" bölümü
 *   robotInit()  autonomousInit()  autonomousLoop()  teleopInit()  teleopLoop()  robotEnd()
 *   tlog("...")  -> Seri Monitör + Driver Station Telemetry paneli
 *
 * OTONOM: "OTONOM ROTA" bölümündeki AUTO_ROUTE dizisine adım yazın:
 *   Forward(60), TurnBy(90), StrafeRight(30), GoTo(0,0,0), Wait(500), Run(fonksiyon) ...
 *   Enkoder odometrisi + BNO055 açısıyla kapalı çevrim gider. Ayarlar: "OTONOM AYARLARI".
 *   Seri Monitörde "a" = Driver Station olmadan otonomu başlat, "s" = durdur.
 *
 * PATH PLANNER: Driver Station > "Path Planner" sekmesinde sahada yol çizin, "Send to robot" ile
 *   kaydedin (4 slot, LittleFS), Dashboard > "Auto Routine" ile seçin. Kod yüklemek gerekmez.
 *   Tools > Partition Scheme: "Default" olmalı (SPIFFS bölümü gerekir). Olaylar: pathEvent().
 *
 * Motor sırası:  0 = ON-SOL (FL)   1 = ON-SAG (FR)   2 = ARKA-SOL (RL)   3 = ARKA-SAG (RR)
 *
 * PIN TABLOSU
 *   Motor   PWM   ENC_A  ENC_B
 *   FL      32    27     26
 *   FR      23    17     16
 *   RL      13    25     33
 *   RR       4    18     19
 *   NOT: GPIO12 ve GPIO2'ye sürücü BAĞLAMAYIN (yükleme hatası). 15 açılışta sinyal verir.
 *   BNO055  VIN 3.3V, GND, SDA 21, SCL 22  (adres 0x28/0x29 otomatik bulunur)
 *   ESTOP_PIN (GPIO14) - GND arası buton: basınca motorlar durur + KİLİT
 *   BATT_PIN (GPIO34) - akü voltaj bölücü orta noktası (100k / 22k), bkz. "AKÜ VOLTAJI"
 *
 * Seri Monitör (115200 baud, "Newline") test komutları (maç çalışmıyorken):
 *   p <fl> <fr> <rl> <rr> | d <ileri> <sol> <don> | a | s | z | r | e
 *
 * İLK KURULUM: "p 20 20 20 20" yazın. Her tekerlek robotu İLERİ itmeli ve
 * her motorun sayımı ARTMALI. Değilse INVERT_MOTOR / INVERT_ENCODER dizilerini düzeltin.
 */

#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <Preferences.h>
#include <LittleFS.h>
#include <esp_system.h>
#include <esp_task_wdt.h>
#include <esp_wifi.h>
#include <esp_http_server.h>
#include <driver/pulse_cnt.h>
#include <driver/gpio.h>
#include "ds_page.h"
#include "auto_steps.h"
#include "planner.h"

// ================= AYARLAR =================
const int NUM_MOTORS = 4;
const char *MOTOR_NAME[NUM_MOTORS] = {"FL", "FR", "RL", "RR"};

const int PIN_PWM[NUM_MOTORS]   = {32, 23, 13, 4};
const int PIN_ENC_A[NUM_MOTORS] = {27, 17, 25, 18};
const int PIN_ENC_B[NUM_MOTORS] = {26, 16, 33, 19};

const bool INVERT_MOTOR[NUM_MOTORS]   = {false, true, false, true};
const bool INVERT_ENCODER[NUM_MOTORS] = {false, true, false, false};   // RR: kablo yeniden takılınca yönü değişti

// Teker başına bir turdaki enkoder sayımı. 740 rpm motor: 5 tur testinde ölçüldü (4 teker ort. 148.8 cm, 244.3 iken).
// Hassaslaştırmak için: robot takozdayken Hazırla, her tekeri tam 5 tur çevirin, okunan cm'yi R yapın:
//   yeni değer = bu değer * R / 150.8      (5 tur = 5 * 3.1416 * 9.6 = 150.8 cm olmalı)
// goBILDA: 312rpm=537.7 | 435rpm=384.5 | 223rpm=751.8 | 1150rpm=145.1
const float COUNTS_PER_REV = 241.1f;
const float WHEEL_DIAMETER_CM = 9.6f;
const float CM_PER_COUNT = PI * WHEEL_DIAMETER_CM / COUNTS_PER_REV;
// MESAFE KALİBRASYONU: Hazırla'ya basın, robotu elle tam 122 cm (2 karo) İLERİ itin, Panel'deki X'i okuyun.
//   ODOM_FWD_SCALE = 122 / okunan X.   Aynısını SOLA itip Y ile yapın: ODOM_STRAFE_SCALE = 122 / okunan Y.
//   Robot hedefi aşıyorsa okunan değer küçüktür -> katsayı 1'den büyük çıkar.
const float ODOM_FWD_SCALE    = 1.02f;   // kolla 122 cm sürüşte teker ortalaması 119.9 cm çıktı
const float ODOM_STRAFE_SCALE = 1.00f;
const bool DRIVE_MECANUM = true;

// ================= WIFI / DRIVER STATION =================
const char *WIFI_SSID = "MEB-ROBOT";
const char *WIFI_PASS = "robot1234";      // en az 8 karakter - DEĞİŞTİRİN
const int   WIFI_CHANNEL = 6;             // 1..13 (Logs sekmesinden kod yüklemeden değiştirilebilir)
const uint32_t JOY_TIMEOUT_MS   = 500;    // kol verisi gelmezse değerler sıfırlanır
const uint32_t OWNER_TIMEOUT_MS = 5000;   // sahip cihaz bu kadar sessizse başka cihaz alabilir
const uint32_t DS_SILENT_STOP_MS = 10000; // Driver Station bu kadar sessizse robot Stop
const uint32_t DEADLINE_MS      = 2000;   // loop turu bunu geçerse "Deadline Miss"

// ================= TELEOP AYARLARI =================
const float NORMAL_SPEED = 50;
const float TURBO_SPEED  = 100;
const float SLOW_SPEED   = 25;
const float TURN_SCALE   = 0.7f;
const float STICK_DEADZONE = 0.10f;

// ================= OTONOM AYARLARI =================
// 740 rpm motor hızlı (%100 güç ~ 370 cm/s), bu yüzden kazançlar düşük tutuldu.
const float AUTO_KP_DRIVE     = 0.6f;   // % güç / cm hata   (salınım yaparsa düşürün, hedefe yetişmiyorsa artırın)
const float AUTO_KP_TURN      = 0.5f;   // % güç / derece hata
const float AUTO_MIN_DRIVE    = 12;     // sürtünmeyi yenen en düşük güç (%8'de robot yerleşemiyordu)
const float AUTO_MIN_TURN     = 10;
const float AUTO_ACCEL        = 1.5f;   // her 20 ms'de en fazla güç artışı -> 0'dan %30'a 0.4 sn (patinaj olursa düşürün)
const float AUTO_HOLD_TURN    = 20;     // düz giderken açıyı tutmak için en fazla dönme gücü
const float AUTO_HOLD_SPEED   = 15;     // dönerken konumu tutmak için en fazla güç
const float AUTO_POS_TOL_CM   = 3.0f;   // hedefe bu kadar yakınsa "vardı"
const float AUTO_HEAD_TOL_DEG = 3.0f;
const uint32_t AUTO_SETTLE_MS = 150;    // hedefte bu kadar kalınca sonraki adım
const float AUTO_BOOST_RATE   = 12;     // hedefe yaklaşamadıkça en küçük güce saniyede eklenen %
const float AUTO_BOOST_MAX    = 12;     // en fazla ek (12 + 12 = %24, takılma alarmı %25'in altında)
float autoMinBoost = 0;                 // yerleşirken o anki ek güç

// ================= PID AYARLARI (Driver Station > "PID Ayarları" sekmesinden canlı değişir) =================
// Ki ve Kd varsayılan 0: eski davranış (yalnız P) korunur. Kalıcı kayıt: Preferences "pid".
struct Pid3 { float kp, ki, kd; };
Pid3 pidDrive    = {AUTO_KP_DRIVE, 0, 0};   // otonom konum
Pid3 pidTurn     = {AUTO_KP_TURN, 0, 0};    // otonom açı
Pid3 pidPath     = {0.8f, 0, 0};            // yol takibi: konum sapması (% güç / cm). Çizgiden sapıyorsa artırın, titriyorsa azaltın
Pid3 pidPathTurn = {0.7f, 0, 0};            // yol takibi: açı tutma
Pid3 pidVel      = {0.05f, 0.1f, 0};        // motor hız PID (% güç / rpm hata)
float velFF   = 1.0f;                       // ileri besleme: hedef güç % * velFF
float velMax  = 700;                        // %100 güç = kaç rpm (ölçüp ayarlayın)
float velOn   = 0;                          // 1 = motor başına hız PID açık (enkoder gerekir)
const float AUTO_I_MAX = 15;                // integral teriminin en fazla gücü (%)
const float AUTO_I_ZONE_CM = 20, AUTO_I_ZONE_DEG = 20;   // integral yalnız hedefe bu kadar yakınken birikir
const float PATH_I_MAX = 10;
const float VEL_I_MAX = 30;

struct PidParam { const char *key; float *ptr; float def, lo, hi; };
PidParam PID_PARAMS[] = {
  {"dKp", &pidDrive.kp, AUTO_KP_DRIVE, 0, 10},  {"dKi", &pidDrive.ki, 0, 0, 5},  {"dKd", &pidDrive.kd, 0, 0, 5},
  {"tKp", &pidTurn.kp, AUTO_KP_TURN, 0, 10},    {"tKi", &pidTurn.ki, 0, 0, 5},   {"tKd", &pidTurn.kd, 0, 0, 5},
  {"pKp", &pidPath.kp, 0.8f, 0, 10},            {"pKi", &pidPath.ki, 0, 0, 5},   {"pKd", &pidPath.kd, 0, 0, 5},
  {"qKp", &pidPathTurn.kp, 0.7f, 0, 10},        {"qKi", &pidPathTurn.ki, 0, 0, 5}, {"qKd", &pidPathTurn.kd, 0, 0, 5},
  {"vKp", &pidVel.kp, 0.05f, 0, 5},             {"vKi", &pidVel.ki, 0.1f, 0, 5}, {"vKd", &pidVel.kd, 0, 0, 5},
  {"vFF", &velFF, 1.0f, 0, 2},                  {"vMax", &velMax, 700, 100, 2000}, {"vOn", &velOn, 0, 0, 1},
};
const int PID_COUNT = sizeof(PID_PARAMS) / sizeof(PID_PARAMS[0]);

void pidDefaults() { for (int i = 0; i < PID_COUNT; i++) *PID_PARAMS[i].ptr = PID_PARAMS[i].def; }

void pidLoadPrefs() {
  Preferences p;
  p.begin("pid", true);
  for (int i = 0; i < PID_COUNT; i++) {
    float v = p.getFloat(PID_PARAMS[i].key, PID_PARAMS[i].def);
    *PID_PARAMS[i].ptr = constrain(v, PID_PARAMS[i].lo, PID_PARAMS[i].hi);
  }
  p.end();
}

void pidSavePrefs() {
  Preferences p;
  p.begin("pid", false);
  for (int i = 0; i < PID_COUNT; i++) p.putFloat(PID_PARAMS[i].key, *PID_PARAMS[i].ptr);
  p.end();
}

// Integral / türev durumları (her yeni hedefte sıfırlanır)
float apIntD = 0, apIntH = 0, apPrevDist = 0, apPrevH = 0;
bool apHave = false;
void autoPidReset() { apIntD = apIntH = apPrevDist = apPrevH = 0; apHave = false; }
float ppIx = 0, ppIy = 0, ppIh = 0, ppPrevEx = 0, ppPrevEy = 0, ppPrevHe = 0;
bool ppHave = false;
void pathPidReset() { ppIx = ppIy = ppIh = ppPrevEx = ppPrevEy = ppPrevHe = 0; ppHave = false; }
// Motor hız PID durumu
float velTarget[NUM_MOTORS] = {0, 0, 0, 0};   // hedef güç % (0 = PID dışı)
float velInt[NUM_MOTORS] = {0, 0, 0, 0}, velPrevErr[NUM_MOTORS] = {0, 0, 0, 0};
bool velHave[NUM_MOTORS] = {false, false, false, false};

// ================= KORUMA AYARLARI =================
const uint32_t CMD_TIMEOUT_MS = 3000;   // seri p/d komutları için
const float    STALL_MIN_POWER = 25;   // otonomda yerleşirken düşük güçler (%8 + %7) yanlış "takıldı" alarmı veriyordu
const float    STALL_MAX_RPM   = 5;
const uint32_t STALL_MS        = 1000;
const int      ESTOP_PIN       = 14;    // -1 = kapalı

// ================= ŞİŞE SENSÖRÜ (limit switch) =================
// Switch: bir ucu GPIO 5, diğer ucu GND (normalde açık kontak). Şişe robottayken basılı olmalı.
// Ters çalışıyorsa (şişe yokken "VAR" diyorsa) BOTTLE_SW_ACTIVE_LOW = false yapın.
const int  BOTTLE_SW_PIN        = 5;    // -1 = sensör yok
const bool BOTTLE_SW_ACTIVE_LOW = true;
const int  BOTTLE_SW_DEBOUNCE   = 3;    // art arda bu kadar okuma (x 20 ms) aynıysa kabul
bool bottleIn = false;                  // şişe robotta mı (titreşimden arındırılmış)
bool bottleSawEmpty = false;            // şişe beklerken sensör en az bir kez boş göründü mü

// Şişe beklemeye başlarken çağrılır. Sensör zaten basılıysa (eski şişe hâlâ robotta) önce boşalması beklenir.
void bottleWaitBegin() {
  bottleSawEmpty = !bottleIn;
  if (bottleIn) tlog("Sensör zaten basılı - önce boşalması, sonra yeni şişe bekleniyor");
}

// Yeni şişe geldi mi: sensör boştan basılıya geçtiyse true
bool bottleArrived() {
  if (!bottleIn) bottleSawEmpty = true;
  return bottleSawEmpty && bottleIn;
}

void bottleUpdate() {
  static int same = 0;
  static bool last = false;
  if (BOTTLE_SW_PIN < 0) return;
  bool raw = digitalRead(BOTTLE_SW_PIN) == (BOTTLE_SW_ACTIVE_LOW ? LOW : HIGH);
  if (raw == last) { if (same < BOTTLE_SW_DEBOUNCE) same++; }
  else { last = raw; same = 1; }
  if (same >= BOTTLE_SW_DEBOUNCE) bottleIn = raw;
}

// ================= SERVO (PCA9685 -> goBILDA 6V Injector) =================
// Bağlantı: ESP32 SDA 21 / SCL 22 (BNO055 ile ortak I2C), PCA9685 adresi 0x40.
//   PCA9685 kanal 0..5  ->  Injector "Signal Input 1..6" (yalnızca S ve - pinleri)
//   PCA9685'in V+ klemensine BİR ŞEY BAĞLAMAYIN: servo gücü Injector'dan gelir.
//   ESP32, PCA9685 ve Injector toprakları ORTAK olmalı. Injector girişi (8-15 V) Ana Şalter'den sonra.
// Kullanım (loop içinden; örn. pathEvent veya teleopLoop):
//   servoAc(0, 90);          // 1. servoyu 90 dereceye götür
//   servoBirak(0);           // 1. servonun darbesini kes (servo serbest kalır)
//   servoHepsiniBirak();     // hepsini bırak
// Panel > "Servo Testi" kartı ve Seri Monitör "v <kanal> <derece>" ile elle denenir.
const uint8_t PCA_ADDR = 0x40;
const int SERVO_N = 6;   // kullanılan kanal sayısı (en fazla 16; kılavuz: robotta en fazla 12 servo)
// Servo modeline göre ayarlayın: darbe genişliği (µs) ve bu aralığın karşılık geldiği açı (derece).
// goBILDA servolarda genelde 500-2500 µs; açı aralığı modele göre 180 ya da 300 olabilir (ürün sayfasına bakın).
const float SERVO_MIN_US[SERVO_N]    = {500, 500, 500, 500, 500, 500};
const float SERVO_MAX_US[SERVO_N]    = {2500, 2500, 2500, 2500, 2500, 2500};
const float SERVO_RANGE_DEG[SERVO_N] = {180, 180, 180, 180, 180, 180};
// Açılışta ve "Hazırla"da servonun gideceği başlangıç açısı (-1 = darbe gönderme). Başlangıç konfigürasyonu için.
const float SERVO_START_DEG[SERVO_N] = {-1, -1, -1, -1, -1, -1};
bool pcaOk = false;

bool pcaWrite(uint8_t reg, const uint8_t *d, uint8_t n) {
  Wire.beginTransmission(PCA_ADDR);
  Wire.write(reg);
  for (uint8_t i = 0; i < n; i++) Wire.write(d[i]);
  return Wire.endTransmission() == 0;
}
bool pcaWrite1(uint8_t reg, uint8_t v) { return pcaWrite(reg, &v, 1); }

// Kanal darbesi: us > 0 ise o genişlikte darbe, us <= 0 ise çıkış tamamen kapalı
void pcaPulse(int ch, float us) {
  if (!pcaOk || ch < 0 || ch > 15) return;
  uint8_t b[4];
  if (us <= 0) {
    b[0] = 0; b[1] = 0; b[2] = 0; b[3] = 0x10;                 // LEDn_OFF_H bit 4 = tam kapalı
  } else {
    int off = (int)(us * 4096.0f * 50.0f / 1000000.0f + 0.5f);  // 50 Hz, 12 bit
    off = constrain(off, 1, 4095);
    b[0] = 0; b[1] = 0; b[2] = off & 0xFF; b[3] = off >> 8;
  }
  pcaWrite(0x06 + 4 * ch, b, 4);
}

void servoAc(int ch, float deg) {
  if (ch < 0 || ch >= SERVO_N) return;
  deg = constrain(deg, 0.0f, SERVO_RANGE_DEG[ch]);
  pcaPulse(ch, SERVO_MIN_US[ch] + (SERVO_MAX_US[ch] - SERVO_MIN_US[ch]) * deg / SERVO_RANGE_DEG[ch]);
}
void servoBirak(int ch) { if (ch >= 0 && ch < SERVO_N) pcaPulse(ch, 0); }
void servoHepsiniBirak() { for (int i = 0; i < SERVO_N; i++) servoBirak(i); }
// Panel / seri test komutu: derece >= 0 -> aç, < 0 -> bırak
void servoCmd(int ch, int deg) { if (deg < 0) servoBirak(ch); else servoAc(ch, deg); }
void servoBaslangic() { for (int i = 0; i < SERVO_N; i++) if (SERVO_START_DEG[i] >= 0) servoAc(i, SERVO_START_DEG[i]); }

void servoBegin() {
  Wire.beginTransmission(PCA_ADDR);
  pcaOk = Wire.endTransmission() == 0;
  if (!pcaOk) { tlog("PCA9685 bulunamadı (0x%02X) - servolar çalışmaz", PCA_ADDR); return; }
  pcaWrite1(0x00, 0x10);   // MODE1: uyku (frekans ancak uykuda değişir)
  pcaWrite1(0xFE, 121);    // PRE_SCALE = 25 MHz / (4096 x 50 Hz) - 1
  pcaWrite1(0x01, 0x04);   // MODE2: totem pole çıkış
  pcaWrite1(0x00, 0x20);   // MODE1: uyan, adres otomatik artsın
  delay(5);
  pcaWrite1(0x00, 0xA0);   // yeniden başlat
  for (int i = 0; i < 16; i++) pcaPulse(i, 0);
  servoBaslangic();
  tlog("PCA9685 hazır (0x%02X), %d servo kanalı", PCA_ADDR, SERVO_N);
}

const uint32_t WDT_TIMEOUT_S   = 3;

// ================= AKÜ VOLTAJI =================
// Akü + ---[ R1 100k ]---+---[ R2 22k ]--- GND      (orta nokta -> BATT_PIN)
// WiFi açıkken ADC2 çalışmaz; ADC1 pini kullanın (34, 35, 36, 39 boş). Pin girişte 3.1 V'u GEÇMEMELİ.
// KALİBRASYON: multimetreyle akü voltajını ölçün, Panel'deki değerle karşılaştırın:
//   yeni BATT_SCALE = BATT_SCALE * gerçek / okunan
const int   BATT_PIN       = 34;        // -1 = kapalı
const float BATT_R1        = 100000.0f;
const float BATT_R2        = 22000.0f;
const float BATT_SCALE     = 1.00f;
const float BATT_WARN_V    = 11.5f;     // Sürücü İstasyonunda sarı
const float BATT_LOW_V     = 11.0f;     // kırmızı + telemetri uyarısı
float battVolts = 0;

void batteryUpdate() {
  if (BATT_PIN < 0) return;
  static uint32_t lastMs = 0;
  static bool warned = false;
  uint32_t now = millis();
  if (lastMs && now - lastMs < 50) return;
  lastMs = now;
  float v = analogReadMilliVolts(BATT_PIN) / 1000.0f * (BATT_R1 + BATT_R2) / BATT_R2 * BATT_SCALE;
  battVolts = battVolts < 0.5f ? v : battVolts * 0.9f + v * 0.1f;   // motor çekişindeki dalgalanmayı yumuşat
  if (battVolts > 3 && battVolts < BATT_LOW_V && !warned) { warned = true; tlog("UYARI: Akü zayıf (%.1f V)", battVolts); }
  if (battVolts > BATT_LOW_V + 0.5f) warned = false;
}

// ================= SERVO PWM (goBILDA sürücü) =================
const int PWM_FREQ  = 50;
const int PWM_BITS  = 16;
const int PULSE_MID = 1500;
const int PULSE_MAX = 1950;

// ================= TELEMETRY (son 256 bayt) =================
const int TEL_SIZE = 256;
char telBuf[TEL_SIZE + 1] = "";
int telLen = 0;
volatile uint32_t telSeq = 1;
bool telDirty = true;
portMUX_TYPE dsMux = portMUX_INITIALIZER_UNLOCKED;

void tlog(const char *fmt, ...) {
  char line[128];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(line, sizeof(line), fmt, ap);
  va_end(ap);
  Serial.println(line);
  int n = strlen(line);
  if (n > TEL_SIZE - 1) n = TEL_SIZE - 1;
  portENTER_CRITICAL(&dsMux);
  int over = telLen + n + 1 - TEL_SIZE;
  if (over > 0) {   // eski satırlar düşer
    memmove(telBuf, telBuf + over, telLen - over);
    telLen -= over;
  }
  memcpy(telBuf + telLen, line, n);
  telLen += n;
  telBuf[telLen++] = '\n';
  telBuf[telLen] = 0;
  telSeq++;
  telDirty = true;
  portEXIT_CRITICAL(&dsMux);
}

// ================= ENKODER (donanım sayacı - PCNT) =================
// ESP32'nin PCNT birimi enkoder darbelerini donanımda sayar (x4 çözümleme + gürültü filtresi).
// Yazılım kesmesi motor gürültüsünde ve yüksek hızda darbe kaçırıyordu.
const uint32_t ENC_GLITCH_NS = 1000;   // bundan kısa darbeler gürültü sayılır (en fazla ~12000)
pcnt_unit_handle_t pcntUnit[NUM_MOTORS] = {};
int32_t encOffset[NUM_MOTORS] = {0, 0, 0, 0};
bool encOk = false;

bool encoderBegin(int i) {
  pinMode(PIN_ENC_A[i], INPUT_PULLUP);
  pinMode(PIN_ENC_B[i], INPUT_PULLUP);
  pcnt_unit_config_t uc = {};
  uc.low_limit = -32000;
  uc.high_limit = 32000;
  if (pcnt_new_unit(&uc, &pcntUnit[i]) != ESP_OK) return false;
  pcnt_glitch_filter_config_t fc = {};
  fc.max_glitch_ns = ENC_GLITCH_NS;
  pcnt_unit_set_glitch_filter(pcntUnit[i], &fc);
  pcnt_channel_handle_t cha = NULL, chb = NULL;
  pcnt_chan_config_t ca = {};
  ca.edge_gpio_num = PIN_ENC_A[i];
  ca.level_gpio_num = PIN_ENC_B[i];
  pcnt_chan_config_t cb = {};
  cb.edge_gpio_num = PIN_ENC_B[i];
  cb.level_gpio_num = PIN_ENC_A[i];
  if (pcnt_new_channel(pcntUnit[i], &ca, &cha) != ESP_OK) return false;
  if (pcnt_new_channel(pcntUnit[i], &cb, &chb) != ESP_OK) return false;
  // Önceki yazılım çözümlemesiyle aynı yön: A yükselirken B düşükse +1
  pcnt_channel_set_edge_action(cha, PCNT_CHANNEL_EDGE_ACTION_DECREASE, PCNT_CHANNEL_EDGE_ACTION_INCREASE);
  pcnt_channel_set_level_action(cha, PCNT_CHANNEL_LEVEL_ACTION_KEEP, PCNT_CHANNEL_LEVEL_ACTION_INVERSE);
  pcnt_channel_set_edge_action(chb, PCNT_CHANNEL_EDGE_ACTION_INCREASE, PCNT_CHANNEL_EDGE_ACTION_DECREASE);
  pcnt_channel_set_level_action(chb, PCNT_CHANNEL_LEVEL_ACTION_KEEP, PCNT_CHANNEL_LEVEL_ACTION_INVERSE);
  gpio_pullup_en((gpio_num_t)PIN_ENC_A[i]);
  gpio_pullup_en((gpio_num_t)PIN_ENC_B[i]);
  if (pcnt_unit_enable(pcntUnit[i]) != ESP_OK) return false;
  pcnt_unit_clear_count(pcntUnit[i]);
  return pcnt_unit_start(pcntUnit[i]) == ESP_OK;
}

// Yalnızca loop'tan çağrılır. Donanım sayacı 16 bit; taşmadan önce yazılım toplamına aktarılır.
void readAllEncoders(int32_t out[NUM_MOTORS]) {
  for (int i = 0; i < NUM_MOTORS; i++) {
    int c = 0;
    if (pcntUnit[i]) {
      pcnt_unit_get_count(pcntUnit[i], &c);
      if (c > 16000 || c < -16000) {
        pcnt_unit_clear_count(pcntUnit[i]);
        encOffset[i] += c;
        c = 0;
      }
    }
    int32_t v = encOffset[i] + c;
    out[i] = INVERT_ENCODER[i] ? -v : v;
  }
}

void resetAllEncoders() {
  for (int i = 0; i < NUM_MOTORS; i++) {
    if (pcntUnit[i]) pcnt_unit_clear_count(pcntUnit[i]);
    encOffset[i] = 0;
  }
}

// ================= MOTOR =================
void writePulseUs(int m, int us) {
  uint32_t duty = (uint32_t)((uint64_t)us * ((1UL << PWM_BITS) - 1) * PWM_FREQ / 1000000UL);
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcWrite(PIN_PWM[m], duty);
#else
  ledcWrite(m, duty);
#endif
}

float motorPower[NUM_MOTORS] = {0, 0, 0, 0};

void setMotorPower(int m, float power) {
  power = constrain(power, -100.0f, 100.0f);
  motorPower[m] = power;
  if (INVERT_MOTOR[m]) power = -power;
  writePulseUs(m, PULSE_MID + (int)(power * (PULSE_MAX - PULSE_MID) / 100.0f));
}

void velReset(int m) { velTarget[m] = 0; velInt[m] = 0; velPrevErr[m] = 0; velHave[m] = false; }

void stopAll() { for (int i = 0; i < NUM_MOTORS; i++) { velReset(i); setMotorPower(i, 0); } }

// Sürüş komutu: hız PID kapalıysa doğrudan güç, açıksa hedef olarak saklanır (velControlStep uygular)
void commandMotor(int m, float power) {
  power = constrain(power, -100.0f, 100.0f);
  if (velOn < 0.5f || power == 0) { velReset(m); setMotorPower(m, power); return; }
  velTarget[m] = power;
}

// ileri, sol (yan), don (CCW +) : -100..100
void drive(float fwd, float left, float turn) {
  if (!DRIVE_MECANUM) left = 0;
  float p[NUM_MOTORS] = {
    fwd - left - turn,   // FL
    fwd + left + turn,   // FR
    fwd + left - turn,   // RL
    fwd - left + turn    // RR
  };
  float mx = 100;
  for (int i = 0; i < NUM_MOTORS; i++) mx = max(mx, fabsf(p[i]));
  for (int i = 0; i < NUM_MOTORS; i++) commandMotor(i, p[i] * 100.0f / mx);
}

float wrap180(float a) {
  while (a > 180) a -= 360;
  while (a < -180) a += 360;
  return a;
}

// ================= BNO055 =================
uint8_t BNO_ADDR = 0x28;
bool bnoReady = false;
uint8_t bnoFailCount = 0;
const uint8_t BNO_CHIP_ID    = 0x00;
const uint8_t BNO_EUL_H_L    = 0x1A;
const uint8_t BNO_CALIB_STAT = 0x35;
const uint8_t BNO_SYS_STATUS = 0x39;
const uint8_t BNO_OPR_MODE   = 0x3D;
const uint8_t BNO_PWR_MODE   = 0x3E;
const uint8_t BNO_SYS_TRIG   = 0x3F;
const uint8_t BNO_PAGE_ID    = 0x07;
const uint8_t BNO_SYS_ERR    = 0x3A;
const uint8_t BNO_MODE = 0x08;            // IMU (pusulasız)
const bool BNO_USE_EXT_CRYSTAL = false;

bool imuOk = false;
float imuRawHeading = 0;
float imuZero = 0;
uint8_t imuCalib = 0, imuSysStatus = 0;

bool bnoRead(uint8_t reg, uint8_t *buf, uint8_t n) {
  Wire.beginTransmission(BNO_ADDR);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom(BNO_ADDR, n) != n) return false;
  for (uint8_t i = 0; i < n; i++) buf[i] = Wire.read();
  return true;
}

bool bnoWrite(uint8_t reg, uint8_t val) {
  Wire.beginTransmission(BNO_ADDR);
  Wire.write(reg);
  Wire.write(val);
  return Wire.endTransmission() == 0;
}

void i2cScan() {
  Serial.println("I2C taraniyor (SDA 21, SCL 22)...");
  int found = 0;
  for (uint8_t a = 1; a < 127; a++) {
    Wire.beginTransmission(a);
    if (Wire.endTransmission() == 0) {
      Serial.printf("  Cihaz: 0x%02X%s\n", a, (a == 0x28 || a == 0x29) ? "  <-- BNO055 adresi" : "");
      found++;
    }
  }
  if (!found) Serial.println("  HICBIR CIHAZ YOK -> SDA/SCL, VIN/GND kontrol edin");
}

bool bnoProbe(uint8_t addr, int tries) {
  BNO_ADDR = addr;
  uint8_t id = 0;
  for (int i = 0; i < tries; i++) {
    if (bnoRead(BNO_CHIP_ID, &id, 1) && id == 0xA0) return true;
    if (tries > 1) delay(100);
  }
  return false;
}

bool bnoBegin(int tries, bool verbose) {
  if (!bnoProbe(0x28, tries) && !bnoProbe(0x29, tries > 1 ? 3 : 1)) {
    if (verbose) tlog("BNO055 bulunamadı (0x28/0x29)");
    return false;
  }
  bnoWrite(BNO_OPR_MODE, 0x00);  delay(25);
  bnoWrite(BNO_PAGE_ID, 0x00);
  bnoWrite(BNO_PWR_MODE, 0x00);  delay(10);
  bnoWrite(BNO_SYS_TRIG, BNO_USE_EXT_CRYSTAL ? 0x80 : 0x00);  delay(10);
  bnoWrite(BNO_OPR_MODE, BNO_MODE); delay(30);
  delay(100);
  uint8_t mode = 0xFF, st = 0xFF, err = 0xFF;
  bnoRead(BNO_OPR_MODE, &mode, 1);
  bnoRead(BNO_SYS_STATUS, &st, 1);
  bnoRead(BNO_SYS_ERR, &err, 1);
  Serial.printf(">> BNO055 adres 0x%02X  mod:0x%02X  durum:%d  hata:%d\n", BNO_ADDR, mode & 0x0F, st, err);
  tlog("BNO055 hazır, adres 0x%02X", BNO_ADDR);
  bnoReady = true;
  bnoFailCount = 0;
  return true;
}

void imuUpdate() {
  uint8_t d[2];
  if (bnoReady && bnoRead(BNO_EUL_H_L, d, 2)) {
    imuRawHeading = (int16_t)((d[1] << 8) | d[0]) / 16.0f;
    imuOk = true;
    bnoFailCount = 0;
    bnoRead(BNO_CALIB_STAT, &imuCalib, 1);
    bnoRead(BNO_SYS_STATUS, &imuSysStatus, 1);
    if (imuSysStatus != 5 && imuSysStatus != 0) {
      static uint8_t badCount = 0;
      if (++badCount > 50) { badCount = 0; bnoReady = false; tlog("BNO055 füzyonu kayboldu, yeniden başlatılıyor"); }
    }
  } else {
    imuOk = false;
    if (bnoReady && ++bnoFailCount > 25) {
      bnoReady = false;
      tlog("BNO055 bağlantısı koptu");
    }
  }
}

float imuHeading() { return wrap180(-(imuRawHeading - imuZero)); }

// ================= ODOMETRİ =================
const uint32_t CONTROL_PERIOD_MS = 20;   // 50 Hz
int32_t counts[NUM_MOTORS], prevCounts[NUM_MOTORS];
float rpm[NUM_MOTORS] = {0, 0, 0, 0};
float posX = 0, posY = 0;
float headingDeg = 0, prevHeadingDeg = 0;

void resetOdometry() {
  readAllEncoders(prevCounts);
  posX = posY = 0;
  prevHeadingDeg = headingDeg;
}

void odometryStep() {
  const float dt = CONTROL_PERIOD_MS / 1000.0f;
  readAllEncoders(counts);
  imuUpdate();
  if (imuOk) {
    float nh = imuHeading();
    // Teşhis: 20 ms'de 40°'den büyük açı değişimi fiziksel olarak imkansız -> sensör yön kaybetmiş demektir
    static float lastH = 0;
    static uint32_t lastJumpLog = 0;
    if (fabsf(wrap180(nh - lastH)) > 40 && millis() - lastJumpLog > 500) {
      lastJumpLog = millis();
      tlog("BNO055 yön sıçraması: %.0f° -> %.0f° (sensör beslemesini/kabloyu kontrol edin)", lastH, nh);
    }
    lastH = nh;
    headingDeg = nh;
  }

  float d[NUM_MOTORS];
  for (int i = 0; i < NUM_MOTORS; i++) {
    int32_t dc = counts[i] - prevCounts[i];
    prevCounts[i] = counts[i];
    d[i] = dc * CM_PER_COUNT;
    float r = dc / COUNTS_PER_REV / dt * 60.0f;
    rpm[i] = 0.7f * rpm[i] + 0.3f * r;
  }
  float dFwd  = ODOM_FWD_SCALE * (d[0] + d[1] + d[2] + d[3]) / 4.0f;
  float dLeft = DRIVE_MECANUM ? ODOM_STRAFE_SCALE * (-d[0] + d[1] + d[2] - d[3]) / 4.0f : 0.0f;
  float dHead = wrap180(headingDeg - prevHeadingDeg);
  float mid = (prevHeadingDeg + dHead / 2.0f) * DEG_TO_RAD;
  prevHeadingDeg = headingDeg;
  posX += dFwd * cosf(mid) - dLeft * sinf(mid);
  posY += dFwd * sinf(mid) + dLeft * cosf(mid);
}

// Motor başına hız PID: 50 Hz, odometryStep'ten sonra çağrılır. Hedef % güç -> rpm, çıkış % güç.
void velControlStep() {
  if (velOn < 0.5f) return;
  const float dt = CONTROL_PERIOD_MS / 1000.0f;
  for (int i = 0; i < NUM_MOTORS; i++) {
    if (velTarget[i] == 0) continue;
    float tr = velTarget[i] / 100.0f * velMax;
    float err = tr - rpm[i];
    float imax = VEL_I_MAX / max(pidVel.ki, 0.0001f);
    velInt[i] = constrain(velInt[i] + err * dt, -imax, imax);
    float d = velHave[i] ? (err - velPrevErr[i]) / dt : 0;
    velPrevErr[i] = err;
    velHave[i] = true;
    float out = velFF * velTarget[i] + pidVel.kp * err + pidVel.ki * velInt[i] + pidVel.kd * d;
    if (velTarget[i] > 0) out = max(out, 0.0f); else out = min(out, 0.0f);   // PID yönü ters çeviremez
    setMotorPower(i, out);
  }
}

void resetHeading() {
  imuUpdate();
  imuZero = imuRawHeading;
  headingDeg = 0;
  resetOdometry();
  tlog(imuOk ? "Açı + konum sıfırlandı" : "Konum sıfırlandı (BNO055 yok)");
}

// ================= KORUMA =================
bool safetyLock = false;
bool encTestMode = false;
uint8_t encEdges[NUM_MOTORS][2];
uint8_t encLast[NUM_MOTORS][2];
uint32_t lastCmdMs = 0;
uint32_t stallTime[NUM_MOTORS] = {0, 0, 0, 0};

bool anyMotorOn() {
  for (int i = 0; i < NUM_MOTORS; i++) if (motorPower[i] != 0) return true;
  return false;
}

void lockout(const char *reason) {
  stopAll();
  if (!safetyLock) tlog("GÜVENLİK: %s - motorlar durdu, KİLİTLİ", reason);
  safetyLock = true;
}

bool unlockSafety() {
  stopAll();
  if (ESTOP_PIN >= 0 && digitalRead(ESTOP_PIN) == LOW) {
    tlog("Acil durdurma butonu hâlâ basılı - kilit sürüyor");
    return false;
  }
  safetyLock = false;
  return true;
}

void safetyCheck() {
  const uint32_t dt = CONTROL_PERIOD_MS;
  if (ESTOP_PIN >= 0 && digitalRead(ESTOP_PIN) == LOW) {
    lockout("Acil durdurma butonu");
    return;
  }
  for (int i = 0; i < NUM_MOTORS; i++) {
    if (fabsf(motorPower[i]) >= STALL_MIN_POWER && fabsf(rpm[i]) < STALL_MAX_RPM) {
      stallTime[i] += dt;
      if (stallTime[i] >= STALL_MS) {
        char msg[48];
        snprintf(msg, sizeof(msg), "%s motoru takıldı / enkoder yok", MOTOR_NAME[i]);
        lockout(msg);
        stallTime[i] = 0;
      }
    } else {
      stallTime[i] = 0;
    }
  }
  if (CMD_TIMEOUT_MS > 0 && anyMotorOn() && millis() - lastCmdMs >= CMD_TIMEOUT_MS) {
    stopAll();
    tlog("Komut zaman aşımı - motorlar durdu");
  }
}

const char *resetReasonText(esp_reset_reason_t r) {
  switch (r) {
    case ESP_RST_POWERON:  return "Güç verildi";
    case ESP_RST_SW:       return "Yazılım reseti";
    case ESP_RST_PANIC:    return "PANIC (kod çöktü)";
    case ESP_RST_INT_WDT:
    case ESP_RST_TASK_WDT:
    case ESP_RST_WDT:      return "WATCHDOG (kod takıldı)";
    case ESP_RST_BROWNOUT: return "BROWNOUT (voltaj düştü)";
    case ESP_RST_EXT:      return "Reset tuşu";
    default:               return "Diğer";
  }
}

void watchdogBegin() {
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  esp_task_wdt_config_t cfg = {};
  cfg.timeout_ms = WDT_TIMEOUT_S * 1000;
  cfg.idle_core_mask = 0;
  cfg.trigger_panic = true;
  if (esp_task_wdt_reconfigure(&cfg) != ESP_OK) esp_task_wdt_init(&cfg);
#else
  esp_task_wdt_init(WDT_TIMEOUT_S, true);
#endif
  esp_task_wdt_add(NULL);
}

// ================= KOL (tarayıcıdan gelir) =================
// Standart tarayıcı düğme sırası
enum {
  BTN_A = 0, BTN_B, BTN_X, BTN_Y, BTN_LB, BTN_RB, BTN_LT, BTN_RT,
  BTN_BACK, BTN_START, BTN_L3, BTN_R3, BTN_UP, BTN_DOWN, BTN_LEFT, BTN_RIGHT, BTN_HOME
};

struct Joystick {
  float leftX, leftY, rightX, rightY;   // -1..1  (Y: İLERİ = +, X: SAĞ = +)
  float lt, rt;                         // 0..1
  uint32_t buttons, prevButtons;
  bool connected;
  bool btn(int i) const { return buttons & (1UL << i); }
  bool pressed(int i) const { return (buttons & ~prevButtons) & (1UL << i); }   // basıldığı an
};
Joystick js = {};

// WebSocket görevinden gelen ham veri (dsMux ile korunur)
struct RawJoy { float ax[4], lt, rt; uint32_t buttons; bool connected; } rawJoy = {};
uint32_t joyLastMs = 0;

void updateJoystick() {
  RawJoy r;
  uint32_t age;
  portENTER_CRITICAL(&dsMux);
  r = rawJoy;
  age = millis() - joyLastMs;
  portEXIT_CRITICAL(&dsMux);
  static bool joyTimedOut = false;
  if (joyLastMs == 0 || age >= JOY_TIMEOUT_MS) {
    r = RawJoy{};   // veri yok -> sıfırla
    if (!joyTimedOut && joyLastMs != 0) tlog("Kol verisi gelmiyor (%lu ms) - değerler sıfırlandı", (unsigned long)age);
    joyTimedOut = true;
  } else {
    if (joyTimedOut && joyLastMs != 0) tlog("Kol verisi geri geldi");
    joyTimedOut = false;
  }
  if (!r.connected && js.connected) tlog("Kol bağlı değil (tarayıcı)");
  js.prevButtons = js.buttons;
  js.leftX  =  r.ax[0];
  js.leftY  = -r.ax[1];   // tarayıcı ileriyi eksi verir
  js.rightX =  r.ax[2];
  js.rightY = -r.ax[3];
  js.lt = r.lt;
  js.rt = r.rt;
  js.buttons = r.buttons;
  js.connected = r.connected;
}

float deadzone(float v) {
  v = constrain(v, -1.0f, 1.0f);
  if (fabsf(v) < STICK_DEADZONE) return 0;
  return (v > 0 ? 1 : -1) * (fabsf(v) - STICK_DEADZONE) / (1.0f - STICK_DEADZONE);
}

// ================= MAÇ FONKSİYONLARI (kendi kodunuz) =================
bool fieldOriented = false;

// Init'e basınca bir kez. Robot hazırlanır, HAREKET ETMEZ.
void robotInit() {
  stopAll();
  resetAllEncoders();
  resetHeading();
  servoBaslangic();
}

// ================= OTONOM ROTA (BURAYI DÜZENLEYİN) =================
// Init anında robot (0,0)'da ve 0° bakıyor.  X = ileri, Y = sol, açı = sola (CCW) +
// Adımlar (son iki parametre isteğe bağlı: hız %, zaman aşımı ms):
//   Forward(cm)  Backward(cm)  StrafeLeft(cm)  StrafeRight(cm)
//   TurnTo(derece)  TurnBy(derece)  GoTo(x, y, derece)  Wait(ms)  Run(fonksiyon, "isim")
//   WaitBottle(en fazla ms)  -> şişe sensörü (limit switch) basılana kadar bekle
// Bir adım zaman aşımına uğrarsa Telemetry'ye yazılır ve sonraki adıma geçilir.

void autoExampleAction() {          // mekanizma için örnek (servo, kol, vb.)
  tlog("Örnek işlem");
}

AutoStep AUTO_ROUTE[] = {
  Forward(60),                  // 60 cm ileri (%50)
  TurnBy(90),                   // 90° sola dön
  StrafeRight(30),              // 30 cm sağa kay (%40)
  Wait(500),                    // yarım saniye bekle
  Run(autoExampleAction, "example"),
  GoTo(0, 0, 0),                // başlangıç noktasına ve yönüne dön
};
const int AUTO_STEPS = sizeof(AUTO_ROUTE) / sizeof(AUTO_ROUTE[0]);

// ---- PATH PLANNER ----
// Driver Station > Path Planner sekmesinde çizilen yol robota kaydedilir. Dashboard'daki
// "Auto Routine" seçimi "Path N" ise otonom o yolu izler, "Code route" ise yukarıdaki AUTO_ROUTE'u.
// Planner'da bir noktaya "Event #" verirseniz robot o noktaya gelince bu fonksiyon BİR KEZ çağrılır.
// BEKLEME (delay) KOYMAYIN - beklemek için planner'da "Stop here" + "Wait" kullanın.
// Hazır rotalarda (GERİ DÖNÜŞTÜR): Olay 1 = robot GDT penceresinin önünde, şişeyi bırak
//                                  Olay 2 = robot rafın önünde ya da insan oyuncunun P'sinde, şişe almaya hazırlan
// "Şişe bekle" noktasında robot, Olay 2'den sonra şişe sensörü (GPIO 5) basılana kadar bekler.
void pathEvent(int id) {
  switch (id) {
    case 1:
      // şişeyi GDT penceresine bırak (pencere yerden 31-56 cm yükseklikte)
      // örnek: servoAc(0, 90);
      break;
    case 2:
      // raftan şişe al
      // örnek: servoAc(0, 0);
      break;
  }
}

// ---- Otonom motoru (genelde değiştirmeniz gerekmez) ----
uint32_t autoElapsedMs = 0;   // otonom başlayalı geçen süre
int autoStep = -1;
uint32_t autoStepStartMs = 0, autoSettleStartMs = 0, autoStepTimeoutMs = 0;
float autoTX = 0, autoTY = 0, autoTH = 0;   // hedef poz (saha)
float autoSpeedNow = 0, autoTurnNow = 0;   // rampa için

void autoDescribe(int i, char *b, size_t n) {
  const AutoStep &s = AUTO_ROUTE[i];
  switch (s.type) {
    case AS_FWD:     snprintf(b, n, "%s %.0f cm @%%%.0f", s.a >= 0 ? "İleri" : "Geri", fabsf(s.a), s.speed); break;
    case AS_STRAFE:  snprintf(b, n, "Yana %s %.0f cm @%%%.0f", s.a >= 0 ? "sola" : "sağa", fabsf(s.a), s.speed); break;
    case AS_TURN_TO: snprintf(b, n, "Açıya dön %.0f°", s.a); break;
    case AS_TURN_BY: snprintf(b, n, "Dön %+.0f°", s.a); break;
    case AS_GOTO:    snprintf(b, n, "Git (%.0f,%.0f) %.0f°", s.a, s.b, s.c); break;
    case AS_WAIT:    snprintf(b, n, "Bekle %.0f ms", s.a); break;
    case AS_CALL:    snprintf(b, n, "Çalıştır %s", s.name ? s.name : "işlem"); break;
    case AS_WAIT_SW: snprintf(b, n, "Şişe bekle (en fazla %.0f ms)", s.a); break;
  }
}

void autoBeginStep(int i) {
  autoPidReset();
  autoStep = i;
  autoStepStartMs = millis();
  autoSettleStartMs = 0;
  if (i >= AUTO_STEPS) {
    stopAll();
    tlog("Otonom rota bitti (%.1f sn)", autoElapsedMs / 1000.0f);
    return;
  }
  const AutoStep &s = AUTO_ROUTE[i];
  float th = autoTH * DEG_TO_RAD;
  float est = 0;   // %50 güçte tahmini süre (ms)
  switch (s.type) {
    case AS_FWD:
      autoTX += s.a * cosf(th);
      autoTY += s.a * sinf(th);
      est = fabsf(s.a) * 30;
      break;
    case AS_STRAFE:
      autoTX -= s.a * sinf(th);
      autoTY += s.a * cosf(th);
      est = fabsf(s.a) * 45;
      break;
    case AS_TURN_TO:
      est = fabsf(wrap180(s.a - autoTH)) * 20;
      autoTH = wrap180(s.a);
      break;
    case AS_TURN_BY:
      est = fabsf(wrap180(s.a)) * 20;
      autoTH = wrap180(autoTH + s.a);
      break;
    case AS_GOTO:
      est = hypotf(s.a - autoTX, s.b - autoTY) * 40 + fabsf(wrap180(s.c - autoTH)) * 20;
      autoTX = s.a;
      autoTY = s.b;
      autoTH = wrap180(s.c);
      break;
    case AS_WAIT_SW:
      bottleWaitBegin();
      break;
    case AS_WAIT:
    case AS_CALL:
      break;
  }
  float spd = s.speed > 10 ? s.speed : 10;
  autoStepTimeoutMs = s.timeoutMs ? s.timeoutMs : 2000 + (uint32_t)(est * 50.0f / spd);

  char d[48];
  autoDescribe(i, d, sizeof(d));
  tlog("Otonom %d/%d: %s", i + 1, AUTO_STEPS, d);
}

// Robotu hedef poza sürer. Hedefteyse true döner.
bool autoDriveToTarget(float maxSpeed, bool turnStep) {
  float ex = autoTX - posX, ey = autoTY - posY;
  float dist = hypotf(ex, ey);
  float herr = wrap180(autoTH - headingDeg);

  // Hedefe yakın (20 cm) ama varamıyorsa en küçük gücü yavaş yavaş artır (robot sürtünmede takılmasın)
  static uint32_t nearSinceMs = 0;
  uint32_t nowMs = millis();
  bool notThere = dist > AUTO_POS_TOL_CM || (imuOk && fabsf(herr) > AUTO_HEAD_TOL_DEG);
  if (notThere && dist < 20) {
    if (nearSinceMs == 0) nearSinceMs = nowMs;
    autoMinBoost = min(AUTO_BOOST_MAX, (nowMs - nearSinceMs) / 1000.0f * AUTO_BOOST_RATE);
  } else {
    nearSinceMs = 0;
    autoMinBoost = 0;
  }

  // Konum: hedefe yaklaştıkça yavaşla, ama sürtünmeyi yenecek kadar it
  // PID durumu: türev (hata değişim hızı) ve integral (yalnız hedefe yakınken, sınırlı)
  const float pdt = CONTROL_PERIOD_MS / 1000.0f;
  float aherr = fabsf(herr);
  float dDist = apHave ? (dist - apPrevDist) / pdt : 0;
  float dHerr = apHave ? (aherr - apPrevH) / pdt : 0;
  apPrevDist = dist; apPrevH = aherr; apHave = true;
  if (dist > AUTO_POS_TOL_CM && dist < AUTO_I_ZONE_CM)
    apIntD = min(apIntD + dist * pdt, AUTO_I_MAX / max(pidDrive.ki, 0.0001f));
  if (aherr > AUTO_HEAD_TOL_DEG && aherr < AUTO_I_ZONE_DEG)
    apIntH = min(apIntH + aherr * pdt, AUTO_I_MAX / max(pidTurn.ki, 0.0001f));

  float vMax = turnStep ? AUTO_HOLD_SPEED : maxSpeed;
  float v = 0;
  float minD = AUTO_MIN_DRIVE + autoMinBoost;
  if (dist > AUTO_POS_TOL_CM)
    v = constrain(pidDrive.kp * dist + pidDrive.ki * apIntD + pidDrive.kd * dDist, minD, max(vMax, minD));
  autoSpeedNow = min(v, autoSpeedNow + AUTO_ACCEL);   // hızlanma sınırlı, yavaşlama anında
  float fwd = 0, left = 0;
  if (dist > 0.01f) {
    float ux = ex / dist, uy = ey / dist;   // saha yönü -> robot yönü
    float h = headingDeg * DEG_TO_RAD;
    fwd  = autoSpeedNow * ( ux * cosf(h) + uy * sinf(h));
    left = autoSpeedNow * (-ux * sinf(h) + uy * cosf(h));
  }

  // Açı
  float tMax = turnStep ? maxSpeed : AUTO_HOLD_TURN;
  float w = 0;
  if (imuOk && fabsf(herr) > AUTO_HEAD_TOL_DEG)
    w = constrain(pidTurn.kp * aherr + pidTurn.ki * apIntH + pidTurn.kd * dHerr, AUTO_MIN_TURN + autoMinBoost * 0.6f, max(tMax, AUTO_MIN_TURN + autoMinBoost * 0.6f));
  autoTurnNow = min(w, autoTurnNow + AUTO_ACCEL * 2);
  float turn = herr >= 0 ? autoTurnNow : -autoTurnNow;

  drive(fwd, left, turn);
  return dist <= AUTO_POS_TOL_CM && (!imuOk || fabsf(herr) <= AUTO_HEAD_TOL_DEG);
}

// ---- Path Planner yolu: depolama + izleme ----
const int   PATH_SLOTS = 4;
const int   PATH_MAX_PTS = 400;
// Robot, planlayıcının hesapladığı zamanda olması gereken noktayı takip eder:
//   güç = yoldaki hız (ileri besleme) + PATH_KP_POS * konum hatası (düzeltme)
// (konum kazancı artık pidPath, açı kazancı pidPathTurn: PID sekmesinden ayarlanır)
const float PATH_CORR_MAX = 15;       // düzeltmenin en fazla gücü
const float PATH_KS = 5;              // hareket ederken eklenen sabit güç (robot planlanandan yavaşsa artırın)
const float PATH_MAX_LAG_CM = 25;     // robot hedeften bu kadar gerideyse zaman bekler (kaçan hedefi kovalamaz)
const float PATH_TURN_MAX = 30;
const float PATH_END_SPEED = 20;      // durma noktasına yerleşirken en fazla güç
const uint32_t PATH_SETTLE_TIMEOUT_MS = 2000;
const float    PATH_ABORT_ERR_CM = 60;     // hedeften bu kadar uzaklaşırsa...
const uint32_t PATH_ABORT_MS     = 700;    // ...ve bu kadar sürerse yol iptal, motorlar durur
const float    PATH_SETTLE_ABORT_CM = 40;  // durma aşamasında hedefe bundan uzak kalırsa da iptal

struct PathPt { float x, y, h, v, t; uint16_t w; uint8_t e, c; };   // cm, derece, % güç, sn (son kalkıştan), bekleme ms, olay, 1 = şişe sensörünü bekle
PathPt pathPts[PATH_MAX_PTS];
int pathN = 0;
int pathActive = 0;                   // 0 = koddaki AUTO_ROUTE, 1..4 = planner yolu
char pathName[33] = "";
bool fsOk = false;
volatile bool pathReloadPending = false;
volatile int pathActRequest = -1;     // web'den gelen seçim, loop uygular

enum PathPhase { PP_FOLLOW, PP_SETTLE, PP_WAIT, PP_DONE };
bool pathMode = false;
PathPhase pathPhase = PP_DONE;
int pathIdx = 0, pathSegStart = 0, pathSegEnd = 0, pathFired = -1;
uint32_t pathPhaseStartMs = 0, pathSegTimeoutMs = 0, pathSettleMs = 0, pathLastMs = 0;
float pathTau = 0;       // segment içindeki yörünge zamanı (sn)
float pathMaxErr = 0;    // en büyük sapma (telemetri için)
uint32_t pathBigErrMs = 0;  // büyük sapmanın başladığı an
float pathStillX = 0, pathStillY = 0;   // engele dayanma algılaması: son hareket edilen konum
uint32_t pathStillMs = 0;               // ...ve o anın zamanı
const float PATH_PUSH_DONE_CM = 12;     // en yüksek güçte kıpırdamıyor ve hedefe bundan yakınsa "engele dayandı"

void pathFileName(int slot, char *fn, size_t n) { snprintf(fn, n, "/p%d.txt", slot); }

// Dosya biçimi: "H,<nokta sayısı>,<isim>" satırı, sonra "x,y,h,v,w,e" satırları (mm, derece*10, %*10, ms, olay), sonra "J<planner json>"
void pathLoad() {
  pathN = 0;
  pathName[0] = 0;
  if (pathActive < 1) return;
  if (!fsOk) { tlog("Dosya sistemi yok - kod rotası kullanılıyor"); return; }
  char fn[12];
  pathFileName(pathActive, fn, sizeof(fn));
  File f = LittleFS.open(fn, "r");
  if (!f) { tlog("Yol %d boş - kod rotası kullanılıyor", pathActive); return; }
  char line[96];
  bool oldFormat = false;
  while (f.available()) {
    int n = 0;
    while (f.available()) {
      int c = f.read();
      if (c == '\n') break;
      if (n < (int)sizeof(line) - 1) line[n++] = (char)c;
    }
    line[n] = 0;
    if (line[0] == 'H') {
      const char *p = strchr(line + 2, ',');
      if (p) strlcpy(pathName, p + 1, sizeof(pathName));
      continue;
    }
    int x, y, h, v, w, e, c = 0;
    long t = 0;
    int got = sscanf(line, "%d,%d,%d,%d,%d,%d,%ld,%d", &x, &y, &h, &v, &w, &e, &t, &c);
    if (got == 6) oldFormat = true;
    if (pathN < PATH_MAX_PTS && got >= 7) {
      PathPt &p = pathPts[pathN++];
      p.x = x / 10.0f;
      p.y = y / 10.0f;
      p.h = h / 10.0f;
      p.v = constrain(v / 10.0f, 0.0f, 100.0f);
      p.t = t / 1000.0f;
      p.w = (uint16_t)constrain(w, 0, 60000);
      p.e = (uint8_t)constrain(e, 0, 255);
      p.c = c ? 1 : 0;
    }
  }
  f.close();
  if (oldFormat) {
    pathN = 0;
    tlog("Yol %d eski biçimde - Yol Planlayıcı'dan tekrar 'Robota gönder'", pathActive);
    return;
  }
  tlog("Otonom = yol %d '%s' (%d nokta)", pathActive, pathName, pathN);
}

float pathDist(int i) { return hypotf(pathPts[i].x - posX, pathPts[i].y - posY); }

int pathNextStop(int from) {
  for (int i = from + 1; i < pathN; i++) if (pathPts[i].w > 0) return i;
  return pathN - 1;
}

void pathFireEvents(int upto) {
  for (int i = pathFired + 1; i <= upto && i < pathN; i++) {
    if (pathPts[i].e) { tlog("Olay %d", pathPts[i].e); pathEvent(pathPts[i].e); }
  }
  if (upto > pathFired) pathFired = upto;
}

// Segment içinde i. noktanın zamanı (segmentin ilk noktası = 0)
float pathT(int i) { return i == pathSegStart ? 0.0f : pathPts[i].t; }

void pathStartSegment(int from) {
  pathPidReset();
  pathSegStart = from;
  pathIdx = from;
  pathSegEnd = pathNextStop(from);
  pathTau = 0;
  pathLastMs = millis();
  pathSegTimeoutMs = 3000 + (uint32_t)(pathT(pathSegEnd) * 2000);   // planlanan sürenin 2 katı + 3 sn
  pathPhaseStartMs = millis();
  pathPhase = PP_FOLLOW;
}

void pathNextOrDone() {
  if (pathSegEnd >= pathN - 1) {
    pathPhase = PP_DONE;
    stopAll();
    tlog("Yol bitti (%.1f sn, en büyük sapma %.0f cm, son açı hatası %.0f°, açı %.0f°)",
         autoElapsedMs / 1000.0f, pathMaxErr, wrap180(autoTH - headingDeg), headingDeg);
  } else {
    pathStartSegment(pathSegEnd);
  }
}

void pathArrived() {
  pathFireEvents(pathSegEnd);
  if (pathPts[pathSegEnd].w > 0 && pathSegEnd < pathN - 1) {
    stopAll();
    pathPhase = PP_WAIT;
    pathPhaseStartMs = millis();
    if (pathPts[pathSegEnd].c) {
      tlog("Şişe bekleniyor (en fazla %.1f sn)", pathPts[pathSegEnd].w / 1000.0f);
      bottleWaitBegin();
    }
    return;
  }
  pathNextOrDone();
}

void pathBeginSettle() {
  autoPidReset();
  const PathPt &t = pathPts[pathSegEnd];
  autoTX = t.x;
  autoTY = t.y;
  autoTH = t.h;
  pathPhase = PP_SETTLE;
  pathPhaseStartMs = millis();
  pathSettleMs = 0;
  pathBigErrMs = 0;
  pathStillX = posX;
  pathStillY = posY;
  pathStillMs = millis();
}

void pathInit() {
  pathMode = true;
  pathFired = -1;
  pathMaxErr = 0;
  pathBigErrMs = 0;
  pathSegStart = 0;
  autoSpeedNow = autoTurnNow = 0;
  if (!imuOk) tlog("UYARI: BNO055 yok - açı kontrol edilmiyor");
  tlog("Yol '%s' başladı", pathName);
  pathFireEvents(0);
  if (pathPts[0].w > 0 && pathN > 1) {   // başlangıçta bekle
    pathSegEnd = 0;
    pathPhase = PP_WAIT;
    pathPhaseStartMs = millis();
    return;
  }
  pathStartSegment(0);
}

void pathTick() {
  uint32_t now = millis();
  switch (pathPhase) {
    case PP_FOLLOW: {
      float dt = (now - pathLastMs) / 1000.0f;
      pathLastMs = now;

      // zamana göre hedef noktayı bul
      while (pathIdx < pathSegEnd && pathT(pathIdx + 1) <= pathTau) pathIdx++;
      if (pathIdx < pathSegEnd) pathFireEvents(pathIdx);
      if (pathIdx >= pathSegEnd) { pathBeginSettle(); break; }

      const PathPt &a = pathPts[pathIdx], &b = pathPts[pathIdx + 1];
      float ta = pathT(pathIdx), tb = pathT(pathIdx + 1);
      float f = tb > ta ? constrain((pathTau - ta) / (tb - ta), 0.0f, 1.0f) : 1.0f;
      float tx = a.x + (b.x - a.x) * f, ty = a.y + (b.y - a.y) * f;
      float th = a.h + wrap180(b.h - a.h) * f;
      float tv = a.v + (b.v - a.v) * f;

      // ileri besleme: yol yönünde, planlanan hızla
      float sx = b.x - a.x, sy = b.y - a.y, sl = hypotf(sx, sy);
      float vx = 0, vy = 0;
      float ff = tv > 0.5f ? tv + PATH_KS : 0;   // sürtünmeyi yenmek için sabit güç ekle
      if (sl > 0.01f) { vx = ff * sx / sl; vy = ff * sy / sl; }

      // düzeltme: hedef noktaya olan sapma
      float ex = tx - posX, ey = ty - posY, err = hypotf(ex, ey);
      if (err > pathMaxErr) pathMaxErr = err;
      // kaçma koruması: robot yoldan çok uzaklaştıysa dur
      if (err > PATH_ABORT_ERR_CM) {
        if (pathBigErrMs == 0) pathBigErrMs = now;
        else if (now - pathBigErrMs >= PATH_ABORT_MS) {
          stopAll();
          pathPhase = PP_DONE;
          tlog("YOL İPTAL: hedeften %.0f cm saptı - mesafe kalibrasyonunu kontrol edin", err);
          break;
        }
      } else {
        pathBigErrMs = 0;
      }
      // PID: orantılı + integral (sınırlı) + türev
      float dex = 0, dey = 0;
      if (ppHave && dt > 0.005f) { dex = (ex - ppPrevEx) / dt; dey = (ey - ppPrevEy) / dt; }
      ppPrevEx = ex; ppPrevEy = ey;
      float pimax = PATH_I_MAX / max(pidPath.ki, 0.0001f);
      ppIx = constrain(ppIx + ex * dt, -pimax, pimax);
      ppIy = constrain(ppIy + ey * dt, -pimax, pimax);
      float cx = pidPath.kp * ex + pidPath.ki * ppIx + pidPath.kd * dex;
      float cy = pidPath.kp * ey + pidPath.ki * ppIy + pidPath.kd * dey;
      float cm = hypotf(cx, cy);
      if (cm > PATH_CORR_MAX) { cx *= PATH_CORR_MAX / cm; cy *= PATH_CORR_MAX / cm; }
      vx += cx;
      vy += cy;

      // robot çok geride kaldıysa zaman ilerlemesin
      if (err < PATH_MAX_LAG_CM) pathTau += dt;

      float h = headingDeg * DEG_TO_RAD;
      float fwd  =  vx * cosf(h) + vy * sinf(h);
      float left = -vx * sinf(h) + vy * cosf(h);
      float herr = wrap180(th - headingDeg);
      float dhe = (ppHave && dt > 0.005f) ? wrap180(herr - ppPrevHe) / dt : 0;
      ppPrevHe = herr;
      ppHave = true;
      float himax = PATH_I_MAX / max(pidPathTurn.ki, 0.0001f);
      ppIh = constrain(ppIh + herr * dt, -himax, himax);
      float turn = imuOk ? constrain(pidPathTurn.kp * herr + pidPathTurn.ki * ppIh + pidPathTurn.kd * dhe,
                                     -PATH_TURN_MAX, PATH_TURN_MAX) : 0;
      autoSpeedNow = hypotf(vx, vy);
      autoTurnNow = fabsf(turn);
      drive(fwd, left, turn);

      if (now - pathPhaseStartMs >= pathSegTimeoutMs) {
        tlog("Yol ZAMAN AŞIMI, nokta %d/%d civarı", pathIdx + 1, pathN);
        pathBeginSettle();
      }
      break;
    }
    case PP_SETTLE: {
      // kaçma koruması: buraya hedefe yakınken girilir; uzak kalıyorsa enkoder robotu takip etmiyordur
      float se = pathDist(pathSegEnd);
      if (se > PATH_SETTLE_ABORT_CM) {
        if (pathBigErrMs == 0) pathBigErrMs = now;
        else if (now - pathBigErrMs >= PATH_ABORT_MS) {
          stopAll();
          pathPhase = PP_DONE;
          tlog("YOL İPTAL: durma noktasına %.0f cm kaldı ama yaklaşılamıyor - enkoderleri kontrol edin", se);
          break;
        }
      } else {
        pathBigErrMs = 0;
      }
      // engele dayanma: en yüksek düzeltme gücünde 0.3 sn kıpırdamıyorsa (ör. GDT duvarı) vardı say
      if (hypotf(posX - pathStillX, posY - pathStillY) > 0.5f) {
        pathStillX = posX;
        pathStillY = posY;
        pathStillMs = now;
      } else if (autoMinBoost >= AUTO_BOOST_MAX - 0.01f && now - pathStillMs >= 300 && se < PATH_PUSH_DONE_CM) {
        tlog("Durma noktası %d: engele dayandı (%.1f cm kala), devam", pathSegEnd + 1, se);
        pathArrived();
        break;
      }
      if (autoDriveToTarget(PATH_END_SPEED, false)) {
        if (pathSettleMs == 0) pathSettleMs = now;
        else if (now - pathSettleMs >= AUTO_SETTLE_MS) { pathArrived(); break; }
      } else {
        pathSettleMs = 0;
      }
      if (now - pathPhaseStartMs >= PATH_SETTLE_TIMEOUT_MS) {
        tlog("Yol durma noktası %d'ye varılamadı (hata %.1f cm)", pathSegEnd + 1, pathDist(pathSegEnd));
        pathArrived();
      }
      break;
    }
    case PP_WAIT: {
      stopAll();
      autoSpeedNow = autoTurnNow = 0;
      const PathPt &wp = pathPts[pathSegEnd];
      uint32_t waited = now - pathPhaseStartMs;
      if (wp.c) {
        // şişe bekle: sensör basılınca hemen devam, w = en fazla bekleme
        if (bottleArrived()) {
          tlog("Şişe alındı (%.1f sn bekledi)", waited / 1000.0f);
          pathNextOrDone();
        } else if (waited >= wp.w) {
          tlog("Şişe gelmedi - %.1f sn sonra devam", waited / 1000.0f);
          pathNextOrDone();
        }
      } else if (waited >= wp.w) {
        pathNextOrDone();
      }
      break;
    }
    case PP_DONE:
      stopAll();
      break;
  }
}

// Otonom başlarken bir kez
void autonomousInit() {
  pathMode = false;
  if (pathActive > 0 && pathN > 0) { pathInit(); return; }
  autoTX = posX;
  autoTY = posY;
  autoTH = headingDeg;
  autoSpeedNow = autoTurnNow = 0;
  if (!imuOk) tlog("UYARI: BNO055 yok - dönüşler atlanır");
  autoBeginStep(0);
}

// Otonom boyunca 50 Hz (her 20 ms)
void autonomousLoop() {
  if (pathMode) { pathTick(); return; }
  // Run() adımları aynı turda biter
  while (autoStep >= 0 && autoStep < AUTO_STEPS && AUTO_ROUTE[autoStep].type == AS_CALL) {
    if (AUTO_ROUTE[autoStep].fn) AUTO_ROUTE[autoStep].fn();
    autoBeginStep(autoStep + 1);
  }
  if (autoStep < 0 || autoStep >= AUTO_STEPS) { stopAll(); return; }

  const AutoStep &s = AUTO_ROUTE[autoStep];
  uint32_t now = millis();
  uint32_t stepMs = now - autoStepStartMs;

  if (s.type == AS_WAIT) {
    stopAll();
    autoSpeedNow = autoTurnNow = 0;
    if (stepMs >= (uint32_t)s.a) autoBeginStep(autoStep + 1);
    return;
  }
  if (s.type == AS_WAIT_SW) {
    stopAll();
    autoSpeedNow = autoTurnNow = 0;
    if (bottleArrived()) { tlog("Şişe alındı (%.1f sn)", stepMs / 1000.0f); autoBeginStep(autoStep + 1); }
    else if (stepMs >= (uint32_t)s.a) { tlog("Şişe gelmedi - devam"); autoBeginStep(autoStep + 1); }
    return;
  }

  bool turnStep = (s.type == AS_TURN_TO || s.type == AS_TURN_BY);
  if (autoDriveToTarget(s.speed, turnStep)) {
    if (autoSettleStartMs == 0) autoSettleStartMs = now;
    else if (now - autoSettleStartMs >= AUTO_SETTLE_MS) { autoBeginStep(autoStep + 1); return; }
  } else {
    autoSettleStartMs = 0;
  }

  if (stepMs >= autoStepTimeoutMs) {
    tlog("Adım %d ZAMAN AŞIMI (hata %.1f cm, %.1f°)", autoStep + 1,
         hypotf(autoTX - posX, autoTY - posY), wrap180(autoTH - headingDeg));
    autoBeginStep(autoStep + 1);
  }
}

// Teleop başlarken bir kez
void teleopInit() {
  stopAll();
}

// Teleop boyunca 50 Hz
void teleopLoop() {
  if (js.pressed(BTN_Y)) {
    fieldOriented = !fieldOriented;
    tlog("Saha-yönlü sürüş: %s", fieldOriented ? "AÇIK" : "KAPALI");
  }
  if (js.pressed(BTN_X)) resetHeading();

  float fwd  =  deadzone(js.leftY);
  float left = -deadzone(js.leftX);
  float turn = -deadzone(js.rightX) * TURN_SCALE;

  if (fieldOriented) {
    float h = headingDeg * DEG_TO_RAD;
    float rf =  fwd * cosf(h) + left * sinf(h);
    float rl = -fwd * sinf(h) + left * cosf(h);
    fwd = rf; left = rl;
  }

  float speed = NORMAL_SPEED;
  if (js.btn(BTN_RB)) speed = TURBO_SPEED;
  else if (js.btn(BTN_LB)) speed = SLOW_SPEED;

  drive(fwd * speed, left * speed, turn * speed);
}

// Stop / EMERGENCY STOP / bağlantı kopunca bir kez
void robotEnd() {
  stopAll();
  servoHepsiniBirak();   // Stop / Acil durdur / bağlantı kopması: servo darbeleri de kesilir
}

// ================= MAÇ AKIŞI =================
enum MatchState { ST_STANDBY = 0, ST_INIT, ST_AUTO, ST_TELEOP, ST_STOPPED };
MatchState matchState = ST_STANDBY;
bool estopped = false;
bool autoEnabled = true;
uint32_t autoDurationMs = 30000;
uint32_t autoStartMs = 0;
bool deadlineMiss = false;

void startTeleop() {
  matchState = ST_TELEOP;
  tlog("Sürücü kontrolü (teleop)");
  teleopInit();
}

void dsInit() {
  deadlineMiss = false;
  if (!unlockSafety()) return;
  robotInit();
  matchState = ST_INIT;
  tlog("Hazırlandı - sistemler hazır");
}

void dsStart() {
  if (safetyLock) { tlog("KİLİTLİ - tekrar Hazırla'ya basın"); return; }
  if (autoEnabled) {
    matchState = ST_AUTO;
    autoStartMs = millis();
    autoElapsedMs = 0;
    tlog("Otonom %.0f sn", autoDurationMs / 1000.0f);
    autonomousInit();
  } else {
    startTeleop();
  }
}

void dsStop(const char *why) {
  robotEnd();
  stopAll();
  matchState = ST_STOPPED;
  tlog("Durduruldu (%s)", why);
}

void dsEstop() {
  stopAll();
  robotEnd();
  stopAll();
  estopped = true;
  safetyLock = true;
  matchState = ST_STOPPED;
  tlog("ACİL DURDURULDU");
}

void matchTick() {
  if (estopped) { stopAll(); return; }
  if (matchState == ST_AUTO) {
    autoElapsedMs = millis() - autoStartMs;
    if (!autoEnabled || autoElapsedMs >= autoDurationMs) {
      stopAll();
      startTeleop();
    } else if (!safetyLock) {
      autonomousLoop();
    }
    lastCmdMs = millis();
  }
  if (matchState == ST_TELEOP) {
    if (!safetyLock) teleopLoop();
    lastCmdMs = millis();
  }
}

// ================= WIFI =================
int wifiChannel = WIFI_CHANNEL;
const char *chanSource = "macro";
char chanResult[48] = "";
volatile int lastDisconnectReason = -1;
int staRssi = 0;
int clientCount = 0;
uint32_t sketchSize = 0, freeSketch = 0;

void wifiBegin() {
  Preferences p;
  p.begin("ds", true);
  int ch = p.getUChar("ch", 0);
  p.end();
  if (ch >= 1 && ch <= 13) { wifiChannel = ch; chanSource = "nvs"; }

  WiFi.onEvent([](arduino_event_id_t e, arduino_event_info_t info) {
    if (e == ARDUINO_EVENT_WIFI_AP_STADISCONNECTED) lastDisconnectReason = info.wifi_ap_stadisconnected.reason;
  });
  WiFi.mode(WIFI_AP);
  WiFi.softAP(WIFI_SSID, WIFI_PASS, wifiChannel);
  WiFi.setSleep(false);
  tlog("WiFi %s kanal %d -> http://%s", WIFI_SSID, wifiChannel, WiFi.softAPIP().toString().c_str());
}

void applyChannel(int ch) {
  Preferences p;
  p.begin("ds", false);
  if (ch >= 1 && ch <= 13) {
    p.putUChar("ch", ch);
    p.end();
    wifiChannel = ch;
    chanSource = "nvs";
    snprintf(chanResult, sizeof(chanResult), "Kanal %d uygulandı ve kaydedildi", ch);
    tlog("%s", chanResult);
    WiFi.softAP(WIFI_SSID, WIFI_PASS, wifiChannel);   // bağlı cihazlar yeniden bağlanır
  } else {
    p.remove("ch");
    p.end();
    snprintf(chanResult, sizeof(chanResult), "Kayıtlı kanal silindi - açılışta kanal %d", WIFI_CHANNEL);
    tlog("%s", chanResult);
  }
}

void networkTick() {
  wifi_sta_list_t list;
  if (esp_wifi_ap_get_sta_list(&list) == ESP_OK) {
    clientCount = list.num;
    staRssi = list.num > 0 ? list.sta[0].rssi : 0;
  }
}

// ================= WEB SUNUCU (ESP-IDF httpd + WebSocket) =================
httpd_handle_t httpd = NULL;
int ownerFd = -1;
uint32_t ownerLastMs = 0;
uint32_t dsLastMs = 0;          // sahipten son mesaj
bool ownerChanged = false;

struct PendingCmd { char name[12]; int a, b; };
PendingCmd cmdQueue[8];
int cmdHead = 0, cmdTail = 0;

// loop'un hazırladığı durum (dsMux ile korunur)
char statusCore[680] = "\"st\":0";
char telJson[TEL_SIZE * 2 + 8] = "\"\"";
uint32_t telJsonSeq = 0;

static esp_err_t sendText(httpd_req_t *req, const char *s) {
  httpd_ws_frame_t out;
  memset(&out, 0, sizeof(out));
  out.type = HTTPD_WS_TYPE_TEXT;
  out.payload = (uint8_t *)s;
  out.len = strlen(s);
  return httpd_ws_send_frame(req, &out);
}

static esp_err_t wsHandler(httpd_req_t *req) {
  if (req->method == HTTP_GET) return ESP_OK;   // el sıkışma

  httpd_ws_frame_t f;
  memset(&f, 0, sizeof(f));
  if (httpd_ws_recv_frame(req, &f, 0) != ESP_OK) return ESP_FAIL;
  if (f.len == 0) return ESP_OK;
  if (f.len > 200) return ESP_FAIL;
  char in[201];
  f.payload = (uint8_t *)in;
  if (httpd_ws_recv_frame(req, &f, f.len) != ESP_OK) return ESP_FAIL;
  in[f.len] = 0;
  if (f.type != HTTPD_WS_TYPE_TEXT) return ESP_OK;

  int fd = httpd_req_to_sockfd(req);
  uint32_t now = millis();

  // Tek sahip
  bool owner = false;
  portENTER_CRITICAL(&dsMux);
  if (ownerFd < 0 || ownerFd == fd || now - ownerLastMs >= OWNER_TIMEOUT_MS) {
    if (ownerFd != fd) ownerChanged = true;
    ownerFd = fd;
    ownerLastMs = now;
    dsLastMs = now;
    owner = true;
  }
  portEXIT_CRITICAL(&dsMux);
  if (!owner) return sendText(req, "{\"busy\":1}");

  double t = 0;
  unsigned clientTel = 0;
  if (in[0] == 'J') {
    // J,<t>,<telSeq>,<ax0>,<ax1>,<ax2>,<ax3>,<buttons>,<lt>,<rt>,<connected>
    RawJoy r = {};
    unsigned long btn = 0;
    int con = 0;
    if (sscanf(in + 2, "%lf,%u,%f,%f,%f,%f,%lu,%f,%f,%d", &t, &clientTel,
               &r.ax[0], &r.ax[1], &r.ax[2], &r.ax[3], &btn, &r.lt, &r.rt, &con) == 10) {
      r.buttons = btn;
      r.connected = con;
      portENTER_CRITICAL(&dsMux);
      rawJoy = r;
      joyLastMs = now;
      portEXIT_CRITICAL(&dsMux);
    }
  } else if (in[0] == 'C') {
    // C,<t>,<telSeq>,<komut>,<a>,<b>
    PendingCmd c = {};
    if (sscanf(in + 2, "%lf,%u,%11[^,],%d,%d", &t, &clientTel, c.name, &c.a, &c.b) >= 3) {
      portENTER_CRITICAL(&dsMux);
      int next = (cmdHead + 1) % 8;
      if (next != cmdTail) { cmdQueue[cmdHead] = c; cmdHead = next; }
      portEXIT_CRITICAL(&dsMux);
    }
  }

  static char resp[sizeof(statusCore) + sizeof(telJson) + 64];
  portENTER_CRITICAL(&dsMux);
  int n = snprintf(resp, sizeof(resp), "{%s,\"t\":%.1f,\"ts\":%lu", statusCore, t, (unsigned long)telJsonSeq);
  if (clientTel != telJsonSeq && n < (int)sizeof(resp))
    n += snprintf(resp + n, sizeof(resp) - n, ",\"tel\":%s", telJson);
  portEXIT_CRITICAL(&dsMux);
  if (n < (int)sizeof(resp) - 1) { resp[n++] = '}'; resp[n] = 0; }
  return sendText(req, resp);
}

static esp_err_t rootHandler(httpd_req_t *req) {
  httpd_resp_set_type(req, "text/html");
  httpd_resp_set_hdr(req, "Cache-Control", "no-store");
  return httpd_resp_send(req, DS_PAGE, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t infoHandler(httpd_req_t *req) {
  char buf[600];
  snprintf(buf, sizeof(buf),
           "{\"ssid\":\"%s\",\"ch\":%d,\"chs\":\"%s\",\"ip\":\"%s\",\"chr\":\"%s\","
           "\"chip\":\"%s rev %d\",\"cpu\":%lu,\"sdk\":\"%s\",\"up\":%lu,"
           "\"heap\":%lu,\"heapT\":%lu,\"psram\":%lu,\"sketch\":%lu,\"flash\":%lu,\"free\":%lu}",
           WIFI_SSID, wifiChannel, chanSource, WiFi.softAPIP().toString().c_str(), chanResult,
           ESP.getChipModel(), ESP.getChipRevision(), (unsigned long)ESP.getCpuFreqMHz(), ESP.getSdkVersion(),
           (unsigned long)(millis() / 1000), (unsigned long)ESP.getFreeHeap(), (unsigned long)ESP.getHeapSize(),
           (unsigned long)ESP.getPsramSize(), (unsigned long)sketchSize, (unsigned long)ESP.getFlashChipSize(),
           (unsigned long)freeSketch);
  httpd_resp_set_type(req, "application/json");
  httpd_resp_set_hdr(req, "Cache-Control", "no-store");
  return httpd_resp_send(req, buf, HTTPD_RESP_USE_STRLEN);
}

// ---- Path Planner uçları ----
static int queryInt(httpd_req_t *req, const char *key, int def) {
  char q[64], v[12];
  if (httpd_req_get_url_query_str(req, q, sizeof(q)) != ESP_OK) return def;
  if (httpd_query_key_value(q, key, v, sizeof(v)) != ESP_OK) return def;
  return atoi(v);
}

static esp_err_t sendJsonStatus(httpd_req_t *req, const char *status, const char *json) {
  if (status) httpd_resp_set_status(req, status);
  httpd_resp_set_type(req, "application/json");
  httpd_resp_set_hdr(req, "Cache-Control", "no-store");
  return httpd_resp_send(req, json, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t sendErr(httpd_req_t *req, const char *status, const char *msg) {
  char buf[96];
  snprintf(buf, sizeof(buf), "{\"err\":\"%s\"}", msg);
  return sendJsonStatus(req, status, buf);
}

static esp_err_t plannerJsHandler(httpd_req_t *req) {
  httpd_resp_set_type(req, "application/javascript; charset=utf-8");
  httpd_resp_set_hdr(req, "Cache-Control", "no-store");
  return httpd_resp_send(req, PLANNER_JS, HTTPD_RESP_USE_STRLEN);
}

// GET /paths -> slot listesi
static esp_err_t pathsHandler(httpd_req_t *req) {
  char buf[640];
  int n = snprintf(buf, sizeof(buf), "{\"fs\":%d,\"act\":%d,\"s\":[", fsOk, pathActive);
  for (int s = 1; s <= PATH_SLOTS; s++) {
    char name[33] = "", line[48] = "";
    int cnt = 0;
    char fn[12];
    pathFileName(s, fn, sizeof(fn));
    if (fsOk && LittleFS.exists(fn)) {
      File f = LittleFS.open(fn, "r");
      if (f) {
        int k = f.readBytesUntil('\n', line, sizeof(line) - 1);
        line[k] = 0;
        f.close();
        if (line[0] == 'H') {
          cnt = atoi(line + 2);
          const char *p = strchr(line + 2, ',');
          if (p) strlcpy(name, p + 1, sizeof(name));
        }
      }
    }
    char esc[72];
    jsonEscape(esc, sizeof(esc), name);
    n += snprintf(buf + n, sizeof(buf) - n, "%s{\"n\":%d,\"name\":%s}", s > 1 ? "," : "", cnt, esc);
  }
  snprintf(buf + n, sizeof(buf) - n, "]}");
  return sendJsonStatus(req, NULL, buf);
}

// GET /path?s=N -> kayıtlı dosya
static esp_err_t pathGetHandler(httpd_req_t *req) {
  int s = queryInt(req, "s", 0);
  if (s < 1 || s > PATH_SLOTS || !fsOk) return sendErr(req, "400 Bad Request", "geçersiz slot");
  char fn[12];
  pathFileName(s, fn, sizeof(fn));
  if (!LittleFS.exists(fn)) return sendErr(req, "404 Not Found", "boş slot");
  File f = LittleFS.open(fn, "r");
  if (!f) return sendErr(req, "404 Not Found", "boş slot");
  httpd_resp_set_type(req, "text/plain");
  httpd_resp_set_hdr(req, "Cache-Control", "no-store");
  char chunk[512];
  while (f.available()) {
    int k = f.read((uint8_t *)chunk, sizeof(chunk));
    if (k <= 0) break;
    if (httpd_resp_send_chunk(req, chunk, k) != ESP_OK) { f.close(); return ESP_FAIL; }
  }
  f.close();
  return httpd_resp_send_chunk(req, NULL, 0);
}

// POST /path?s=N -> yolu kaydet
static esp_err_t pathPostHandler(httpd_req_t *req) {
  int s = queryInt(req, "s", 0);
  if (s < 1 || s > PATH_SLOTS) return sendErr(req, "400 Bad Request", "geçersiz slot");
  if (!fsOk) return sendErr(req, "500 Internal Server Error", "robotta dosya sistemi yok");
  if (matchState == ST_AUTO) return sendErr(req, "409 Conflict", "otonom çalışıyor");
  size_t len = req->content_len;
  if (len < 4 || len > 32000) return sendErr(req, "400 Bad Request", "geçersiz boyut");
  char *body = (char *)malloc(len + 1);
  if (!body) return sendErr(req, "500 Internal Server Error", "bellek yetersiz");
  size_t got = 0;
  while (got < len) {
    int r = httpd_req_recv(req, body + got, len - got);
    if (r == HTTPD_SOCK_ERR_TIMEOUT) continue;
    if (r <= 0) { free(body); return ESP_FAIL; }
    got += r;
  }
  body[len] = 0;
  int cnt = 0;
  if (strncmp(body, "H,", 2) != 0 || sscanf(body + 2, "%d", &cnt) != 1 || cnt < 1 || cnt > PATH_MAX_PTS) {
    free(body);
    return sendErr(req, "400 Bad Request", "geçersiz yol verisi");
  }
  char fn[12];
  pathFileName(s, fn, sizeof(fn));
  File f = LittleFS.open(fn, "w");
  if (!f) { free(body); return sendErr(req, "500 Internal Server Error", "dosya açılamadı"); }
  size_t w = f.write((const uint8_t *)body, len);
  f.close();
  free(body);
  if (w != len) return sendErr(req, "500 Internal Server Error", "yazılamadı (flash dolu mu?)");
  if (s == pathActive) pathReloadPending = true;
  tlog("Yol kaydedildi: slot %d (%d nokta)", s, cnt);
  char ok[48];
  snprintf(ok, sizeof(ok), "{\"ok\":1,\"n\":%d}", cnt);
  return sendJsonStatus(req, NULL, ok);
}

// GET /pathact?s=N -> otonomda kullanılacak yol (0 = koddaki rota)
static esp_err_t pathActHandler(httpd_req_t *req) {
  int s = queryInt(req, "s", -1);
  if (s < 0 || s > PATH_SLOTS) return sendErr(req, "400 Bad Request", "geçersiz slot");
  if (matchState == ST_AUTO) return sendErr(req, "409 Conflict", "otonom çalışıyor");
  pathActRequest = s;
  return sendJsonStatus(req, NULL, "{\"ok\":1}");
}

// GET /pathdel?s=N
static esp_err_t pathDelHandler(httpd_req_t *req) {
  int s = queryInt(req, "s", 0);
  if (s < 1 || s > PATH_SLOTS || !fsOk) return sendErr(req, "400 Bad Request", "geçersiz slot");
  if (matchState == ST_AUTO) return sendErr(req, "409 Conflict", "otonom çalışıyor");
  char fn[12];
  pathFileName(s, fn, sizeof(fn));
  LittleFS.remove(fn);
  if (s == pathActive) pathReloadPending = true;
  return sendJsonStatus(req, NULL, "{\"ok\":1}");
}

// ---- PID ayar uçları ----
// GET /pid -> tüm değerler (JSON)
static esp_err_t pidGetHandler(httpd_req_t *req) {
  char buf[640];
  int n = snprintf(buf, sizeof(buf), "{");
  for (int i = 0; i < PID_COUNT && n < (int)sizeof(buf) - 32; i++)
    n += snprintf(buf + n, sizeof(buf) - n, "%s\"%s\":%.4g", i ? "," : "", PID_PARAMS[i].key, *PID_PARAMS[i].ptr);
  snprintf(buf + n, sizeof(buf) - n, "}");
  return sendJsonStatus(req, NULL, buf);
}

// GET /pidset?n=<anahtar>&v=<değer> -> tek değeri canlı değiştir (kaydetmez)
static esp_err_t pidSetHandler(httpd_req_t *req) {
  char q[64], key[8], val[16];
  if (httpd_req_get_url_query_str(req, q, sizeof(q)) != ESP_OK ||
      httpd_query_key_value(q, "n", key, sizeof(key)) != ESP_OK ||
      httpd_query_key_value(q, "v", val, sizeof(val)) != ESP_OK)
    return sendErr(req, "400 Bad Request", "n ve v gerekli");
  float v = atof(val);
  for (int i = 0; i < PID_COUNT; i++) {
    if (strcmp(PID_PARAMS[i].key, key) == 0) {
      v = constrain(v, PID_PARAMS[i].lo, PID_PARAMS[i].hi);
      *PID_PARAMS[i].ptr = v;
      tlog("PID %s = %.4g", key, v);
      char ok[48];
      snprintf(ok, sizeof(ok), "{\"ok\":1,\"v\":%.4g}", v);
      return sendJsonStatus(req, NULL, ok);
    }
  }
  return sendErr(req, "400 Bad Request", "bilinmeyen anahtar");
}

static esp_err_t pidSaveHandler(httpd_req_t *req) {
  pidSavePrefs();
  tlog("PID değerleri kalıcı kaydedildi");
  return sendJsonStatus(req, NULL, "{\"ok\":1}");
}

static esp_err_t pidResetHandler(httpd_req_t *req) {
  pidDefaults();
  pidSavePrefs();
  tlog("PID değerleri varsayılana döndü");
  return sendJsonStatus(req, NULL, "{\"ok\":1}");
}

void webBegin() {
  httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
  cfg.server_port = 80;
  cfg.stack_size = 8192;
  cfg.lru_purge_enable = true;
  cfg.max_open_sockets = 6;
  cfg.max_uri_handlers = 16;
  if (httpd_start(&httpd, &cfg) != ESP_OK) { tlog("Web sunucusu başlatılamadı"); return; }

  httpd_uri_t root = {};
  root.uri = "/";  root.method = HTTP_GET;  root.handler = rootHandler;
  httpd_register_uri_handler(httpd, &root);

  httpd_uri_t info = {};
  info.uri = "/info";  info.method = HTTP_GET;  info.handler = infoHandler;
  httpd_register_uri_handler(httpd, &info);

  httpd_uri_t ws = {};
  ws.uri = "/ws";  ws.method = HTTP_GET;  ws.handler = wsHandler;  ws.is_websocket = true;
  httpd_register_uri_handler(httpd, &ws);

  struct { const char *uri; httpd_method_t m; esp_err_t (*h)(httpd_req_t *); } routes[] = {
    {"/planner.js", HTTP_GET, plannerJsHandler},
    {"/paths", HTTP_GET, pathsHandler},
    {"/path", HTTP_GET, pathGetHandler},
    {"/path", HTTP_POST, pathPostHandler},
    {"/pathact", HTTP_GET, pathActHandler},
    {"/pathdel", HTTP_GET, pathDelHandler},
    {"/pid", HTTP_GET, pidGetHandler},
    {"/pidset", HTTP_GET, pidSetHandler},
    {"/pidsave", HTTP_GET, pidSaveHandler},
    {"/pidreset", HTTP_GET, pidResetHandler},
  };
  for (auto &r : routes) {
    httpd_uri_t u = {};
    u.uri = r.uri;  u.method = r.m;  u.handler = r.h;
    httpd_register_uri_handler(httpd, &u);
  }
}

// JSON için metni kaçışla
int jsonEscape(char *out, int cap, const char *s) {
  int n = 0;
  if (n < cap - 1) out[n++] = '"';
  for (; *s && n < cap - 3; s++) {
    char c = *s;
    if (c == '"' || c == '\\') { out[n++] = '\\'; out[n++] = c; }
    else if (c == '\n') { out[n++] = '\\'; out[n++] = 'n'; }
    else if ((uint8_t)c >= 0x20) out[n++] = c;
  }
  out[n++] = '"';
  out[n] = 0;
  return n;
}

// loop içinde: komutları işle, zaman aşımlarını uygula, durumu hazırla
void dsTick() {
  uint32_t now = millis();

  // Komutlar
  while (true) {
    PendingCmd c;
    bool have = false;
    portENTER_CRITICAL(&dsMux);
    if (cmdTail != cmdHead) { c = cmdQueue[cmdTail]; cmdTail = (cmdTail + 1) % 8; have = true; }
    portEXIT_CRITICAL(&dsMux);
    if (!have) break;

    if (!strcmp(c.name, "estop")) dsEstop();
    else if (!strcmp(c.name, "reboot")) { tlog("Yeniden başlatılıyor..."); stopAll(); delay(200); ESP.restart(); }
    else if (!strcmp(c.name, "chan")) applyChannel(c.a);
    else if (estopped) tlog("ACİL DURDURULDU - yeniden başlatma gerekli");
    else if (!strcmp(c.name, "servo")) { if (matchState != ST_AUTO) servoCmd(c.a, c.b); }   // Panel servo testi (otonomda kapalı)
    else if (!strcmp(c.name, "init")) { if (matchState == ST_STANDBY || matchState == ST_STOPPED) dsInit(); }
    else if (!strcmp(c.name, "start")) {
      if (matchState == ST_INIT) {
        autoDurationMs = (uint32_t)constrain(c.a, 1, 600) * 1000UL;
        autoEnabled = c.b != 0;
        dsStart();
      }
    }
    else if (!strcmp(c.name, "stop")) { if (matchState != ST_STANDBY && matchState != ST_STOPPED) dsStop("Durdur düğmesi"); }
    else if (!strcmp(c.name, "auto")) {
      if (matchState != ST_TELEOP) {
        autoEnabled = c.a != 0;
        if (matchState == ST_AUTO && !autoEnabled) tlog("Otonom kapatıldı");
      }
    }
  }

  // Path Planner: seçim / yeniden yükleme (otonom sırasında değil)
  if (matchState != ST_AUTO) {
    int req = pathActRequest;
    if (req >= 0) {
      pathActRequest = -1;
      pathActive = req;
      Preferences p;
      p.begin("ds", false);
      p.putUChar("act", (uint8_t)req);
      p.end();
      if (req == 0) tlog("Otonom = kod rotası (AUTO_ROUTE)");
      pathReloadPending = true;
    }
    if (pathReloadPending) { pathReloadPending = false; pathLoad(); }
  }

  // Sahip değişti / sessiz kaldı
  bool changed;
  uint32_t lastMsg;
  portENTER_CRITICAL(&dsMux);
  changed = ownerChanged;
  ownerChanged = false;
  lastMsg = dsLastMs;
  portEXIT_CRITICAL(&dsMux);
  if (changed) tlog("Sürücü İstasyonu bağlandı");
  static bool silentHandled = true;
  if (lastMsg != 0 && now - lastMsg < DS_SILENT_STOP_MS) silentHandled = false;
  if (!silentHandled && lastMsg != 0 && now - lastMsg >= DS_SILENT_STOP_MS) {
    silentHandled = true;
    if (matchState == ST_AUTO || matchState == ST_TELEOP) dsStop("Sürücü İstasyonu 10 sn sessiz");
  }

  // Telemetry JSON (değiştiyse)
  static char telCopy[TEL_SIZE + 1];
  static char telEsc[sizeof(telJson)];
  bool dirty;
  uint32_t seq;
  portENTER_CRITICAL(&dsMux);
  dirty = telDirty;
  telDirty = false;
  seq = telSeq;
  if (dirty) memcpy(telCopy, telBuf, telLen + 1);
  portEXIT_CRITICAL(&dsMux);
  if (dirty) jsonEscape(telEsc, sizeof(telEsc), telCopy);

  // Durum
  static char core[sizeof(statusCore)];
  float autoLeft = 0;
  if (matchState == ST_AUTO) autoLeft = max(0.0f, (autoDurationMs - (now - autoStartMs)) / 1000.0f);
  uint32_t joyAge = joyLastMs ? now - joyLastMs : 0;
  batteryUpdate();
  snprintf(core, sizeof(core),
           "\"bv\":%.2f,\"bw\":%.1f,\"bl\":%.1f,"
           "\"st\":%d,\"ar\":%.1f,\"ad\":%lu,\"ae\":%d,\"es\":%d,\"lk\":%d,\"fo\":%d,"
           "\"heap\":%lu,\"heapT\":%lu,\"rssi\":%d,\"dm\":%d,\"ja\":%lu,\"cl\":%d,\"ld\":%d,"
           "\"x\":%.1f,\"y\":%.1f,\"h\":%.1f,\"bno\":%d,"
           "\"rpm\":[%.0f,%.0f,%.0f,%.0f],\"p\":[%.0f,%.0f,%.0f,%.0f],"
           "\"pa\":%d,\"pn\":%d,\"pi\":%d,\"d\":[%.1f,%.1f,%.1f,%.1f],\"sw\":%d,\"pc\":%d",
           BATT_PIN >= 0 ? battVolts : -1.0f, BATT_WARN_V, BATT_LOW_V,
           (int)matchState, autoLeft, (unsigned long)(autoDurationMs / 1000), autoEnabled, estopped, safetyLock, fieldOriented,
           (unsigned long)ESP.getFreeHeap(), (unsigned long)ESP.getHeapSize(), staRssi, deadlineMiss,
           (unsigned long)joyAge, clientCount, (int)lastDisconnectReason,
           posX, posY, headingDeg, imuOk,
           rpm[0], rpm[1], rpm[2], rpm[3], motorPower[0], motorPower[1], motorPower[2], motorPower[3],
           pathActive, pathN, (pathMode && matchState == ST_AUTO) ? pathIdx : -1,
           counts[0] * CM_PER_COUNT, counts[1] * CM_PER_COUNT, counts[2] * CM_PER_COUNT, counts[3] * CM_PER_COUNT,
           BOTTLE_SW_PIN < 0 ? -1 : (int)bottleIn, pcaOk ? 1 : 0);
  portENTER_CRITICAL(&dsMux);
  memcpy(statusCore, core, sizeof(core));
  if (dirty) { memcpy(telJson, telEsc, sizeof(telEsc)); telJsonSeq = seq; }
  portEXIT_CRITICAL(&dsMux);
}

// ================= SERİ KOMUTLAR (test) =================
void printHelp() {
  Serial.println("Komutlar: p <fl> <fr> <rl> <rr> | d <ileri> <sol> <don> | v <kanal> <derece> (servo) | a (otonom test) | s | z | r | e");
}

void handleSerial() {
  if (!Serial.available()) return;
  String line = Serial.readStringUntil('\n');
  line.trim();
  if (line.length() == 0) return;
  char cmd = tolower(line.charAt(0));
  const char *arg = line.c_str() + 1;

  if ((cmd == 'p' || cmd == 'd') && (matchState == ST_AUTO || matchState == ST_TELEOP)) {
    Serial.println(">> Mac calisiyor - once Driver Station'dan Stop");
    return;
  }
  if ((cmd == 'p' || cmd == 'd') && safetyLock) {
    Serial.println(">> KILIT aktif - once 's' yazin");
    return;
  }
  if (cmd == 'p' || cmd == 'd') lastCmdMs = millis();

  switch (cmd) {
    case 'p': {
      float a[4];
      if (sscanf(arg, "%f %f %f %f", &a[0], &a[1], &a[2], &a[3]) == 4) {
        for (int i = 0; i < NUM_MOTORS; i++) setMotorPower(i, a[i]);
        Serial.printf(">> Guc FL:%.0f FR:%.0f RL:%.0f RR:%.0f\n", a[0], a[1], a[2], a[3]);
      } else Serial.println("Kullanim: p <fl> <fr> <rl> <rr>");
      break;
    }
    case 'd': {
      float f, l, t;
      if (sscanf(arg, "%f %f %f", &f, &l, &t) == 3) {
        drive(f, l, t);
        Serial.printf(">> Surus ileri:%.0f sol:%.0f don:%.0f\n", f, l, t);
      } else Serial.println("Kullanim: d <ileri> <sol> <don>");
      break;
    }
    case 'v': {   // servo testi: v <kanal 0-5> <derece, -1 = bırak>
      int ch, dg;
      if (sscanf(arg, "%d %d", &ch, &dg) == 2 && ch >= 0 && ch < SERVO_N) {
        if (matchState == ST_AUTO) { Serial.println(">> Otonom calisiyor - once durdurun"); break; }
        servoCmd(ch, dg);
        Serial.printf(">> Servo %d (PCA kanal %d): %s%s\n", ch + 1, ch, dg < 0 ? "birakildi" : "derece ayarlandi", pcaOk ? "" : "  (PCA9685 YOK!)");
      } else Serial.println("Kullanim: v <kanal 0-5> <derece, -1 = birak>");
      break;
    }
    case 'a':   // Driver Station olmadan otonom testi (30 sn, sonra teleop'ta bekler)
      if (estopped) { Serial.println(">> EMERGENCY STOP - reboot gerekli"); break; }
      if (matchState == ST_AUTO || matchState == ST_TELEOP) { Serial.println(">> Mac zaten calisiyor - 's' ile durdurun"); break; }
      dsInit();
      if (matchState == ST_INIT) { autoDurationMs = 30000; autoEnabled = true; dsStart(); }
      break;
    case 's':
      if (matchState == ST_AUTO || matchState == ST_TELEOP) dsStop("Seri s");
      if (estopped) { stopAll(); Serial.println(">> EMERGENCY STOP - reboot gerekli"); break; }
      if (unlockSafety()) Serial.println(">> Dur (kilit acik)");
      break;
    case 'z':
      resetAllEncoders();
      resetOdometry();
      Serial.println(">> Enkoderler ve konum sifirlandi");
      break;
    case 'r':
      resetHeading();
      break;
    case 'e':
      encTestMode = !encTestMode;
      if (encTestMode) {
        stopAll();
        for (int i = 0; i < NUM_MOTORS; i++) {
          encLast[i][0] = digitalRead(PIN_ENC_A[i]);
          encLast[i][1] = digitalRead(PIN_ENC_B[i]);
          encEdges[i][0] = encEdges[i][1] = 0;
        }
        Serial.println(">> ENKODER TESTI: tekerlekleri elle cevirin. Tekrar 'e' = kapat");
      } else {
        Serial.println(">> Enkoder testi kapandi");
      }
      break;
    default:
      printHelp();
  }
}

// ================= SETUP / LOOP =================
void setup() {
  Serial.begin(115200);
  delay(200);
  tlog("Açılma nedeni: %s", resetReasonText(esp_reset_reason()));
  pidLoadPrefs();
  if (ESTOP_PIN >= 0) pinMode(ESTOP_PIN, INPUT_PULLUP);
  if (BOTTLE_SW_PIN >= 0) pinMode(BOTTLE_SW_PIN, INPUT_PULLUP);
  if (BATT_PIN >= 0) { pinMode(BATT_PIN, INPUT); analogSetPinAttenuation(BATT_PIN, ADC_11db); }

  // 1) Motor PWM - sürücüler açılışta nötr görmeli
  for (int i = 0; i < NUM_MOTORS; i++) {
#if ESP_ARDUINO_VERSION_MAJOR >= 3
    ledcAttach(PIN_PWM[i], PWM_FREQ, PWM_BITS);
#else
    ledcSetup(i, PWM_FREQ, PWM_BITS);
    ledcAttachPin(PIN_PWM[i], i);
#endif
  }
  stopAll();

  // 2) WiFi + web (sürücüler arm olurken başlasın)
  sketchSize = ESP.getSketchSize();
  freeSketch = ESP.getFreeSketchSpace();
  wifiBegin();
  webBegin();

  Serial.println("Notr sinyal - suruculer arm ediliyor (3 sn)...");
  delay(3000);

  // 3) Enkoderler
  encOk = true;
  for (int i = 0; i < NUM_MOTORS; i++) {
    if (!encoderBegin(i)) {
      encOk = false;
      tlog("%s enkoder sayacı başlatılamadı!", MOTOR_NAME[i]);
    }
  }
  if (encOk) tlog("Enkoderler donanım sayacında (PCNT)");

  // 4) BNO055
  Wire.begin(21, 22);
  Wire.setClock(100000);
  i2cScan();
  bnoBegin(10, true);
  servoBegin();
  delay(100);
  imuUpdate();
  imuZero = imuRawHeading;
  headingDeg = 0;
  resetOdometry();

  // 5) Path Planner yolları (LittleFS, ilk açılışta biçimlendirir)
  fsOk = LittleFS.begin(true);
  if (!fsOk) tlog("LittleFS başlamadı - Yol Planlayıcı kapalı (Tools > Partition Scheme: Default)");
  {
    Preferences p;
    p.begin("ds", true);
    pathActive = p.getUChar("act", 0);
    p.end();
    if (pathActive > PATH_SLOTS) pathActive = 0;
  }
  pathLoad();

  tlog("Hazır - Bekleme");
  printHelp();
  watchdogBegin();
}

void loop() {
  static uint32_t lastLoopMs = millis();
  uint32_t now = millis();
  if (now - lastLoopMs >= DEADLINE_MS && !deadlineMiss) {
    deadlineMiss = true;
    stopAll();
    tlog("GECİKME AŞIMI: döngü %lu ms sürdü", (unsigned long)(now - lastLoopMs));
  }
  lastLoopMs = now;

  esp_task_wdt_reset();
  handleSerial();

  static uint32_t lastControl = 0, lastPrint = 0, lastDs = 0, lastNet = 0;

  if (now - lastControl >= CONTROL_PERIOD_MS) {
    lastControl = now;
    odometryStep();
    updateJoystick();
    safetyCheck();
    bottleUpdate();
    if (!encTestMode) { matchTick(); if (!safetyLock && !estopped) velControlStep(); }
  }
  if (now - lastDs >= 50) { lastDs = now; dsTick(); }
  if (now - lastNet >= 500) { lastNet = now; networkTick(); }

  // BNO055 yoksa / koptuysa 2 sn'de bir tekrar dene
  static uint32_t lastBnoTry = 0;
  if (!bnoReady && now - lastBnoTry >= 2000) {
    lastBnoTry = now;
    if (bnoBegin(1, false)) {
      delay(50);
      imuUpdate();
      imuZero = imuRawHeading + headingDeg;
    }
  }

  if (encTestMode) {
    for (int i = 0; i < NUM_MOTORS; i++) {
      uint8_t a = digitalRead(PIN_ENC_A[i]), b = digitalRead(PIN_ENC_B[i]);
      if (a != encLast[i][0] && encEdges[i][0] < 255) encEdges[i][0]++;
      if (b != encLast[i][1] && encEdges[i][1] < 255) encEdges[i][1]++;
      encLast[i][0] = a;
      encLast[i][1] = b;
    }
    if (now - lastPrint >= 300) {
      lastPrint = now;
      for (int i = 0; i < NUM_MOTORS; i++) {
        Serial.printf("%s[A%d:%d(%d) B%d:%d(%d) s:%ld] ", MOTOR_NAME[i],
                      PIN_ENC_A[i], encLast[i][0], encEdges[i][0],
                      PIN_ENC_B[i], encLast[i][1], encEdges[i][1], (long)counts[i]);
      }
      Serial.println();
    }
  } else if (now - lastPrint >= 500) {
    lastPrint = now;
    static const char *ST_NAME[] = {"Standby", "Init", "Auto", "Teleop", "Stopped"};
    Serial.printf("%s%s | X:%.1f Y:%.1f H:%.1f | rpm %.0f %.0f %.0f %.0f | kol:%s%s\n",
                  ST_NAME[matchState], estopped ? " [ESTOP]" : (safetyLock ? " [KILIT]" : ""),
                  posX, posY, headingDeg, rpm[0], rpm[1], rpm[2], rpm[3],
                  js.connected ? "VAR" : "YOK", fieldOriented ? " [SAHA]" : "");
  }
  delay(1);
}
