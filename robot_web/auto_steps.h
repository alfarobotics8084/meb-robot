// Otonom rota adımları. Rotanın kendisi robot_web.ino içindeki AUTO_ROUTE dizisindedir.
// (Bu yapılar ayrı dosyada çünkü Arduino .ino ön işlemcisi struct döndüren fonksiyonlarda hata verir.)
#pragma once

enum AutoStepType { AS_FWD, AS_STRAFE, AS_TURN_TO, AS_TURN_BY, AS_GOTO, AS_WAIT, AS_CALL, AS_WAIT_SW };

struct AutoStep {
  AutoStepType type;
  float a, b, c;        // tipe göre: cm / derece / x,y,açı / ms
  float speed;          // % güç (0..100)
  uint32_t timeoutMs;   // 0 = otomatik hesapla
  void (*fn)();         // Run() için
  const char *name;     // Run() için (telemetry)
};

// ---- Rota yazarken kullanılacak adımlar ----
// Koordinatlar: Init anında robot (0,0)'da, 0° bakıyor. X = ileri, Y = sol, açı = sola (CCW) +.
// Göreli hareketler (Forward/Strafe/TurnBy) bir önceki adımın HEDEFİNE göre hesaplanır, hata birikmez.

// cm ileri (eksi = geri)
inline AutoStep Forward(float cm, float speed = 50, uint32_t timeoutMs = 0) {
  return {AS_FWD, cm, 0, 0, speed, timeoutMs, nullptr, nullptr};
}
inline AutoStep Backward(float cm, float speed = 50, uint32_t timeoutMs = 0) { return Forward(-cm, speed, timeoutMs); }

// cm yana kay (mecanum)
inline AutoStep StrafeLeft(float cm, float speed = 40, uint32_t timeoutMs = 0) {
  return {AS_STRAFE, cm, 0, 0, speed, timeoutMs, nullptr, nullptr};
}
inline AutoStep StrafeRight(float cm, float speed = 40, uint32_t timeoutMs = 0) { return StrafeLeft(-cm, speed, timeoutMs); }

// Sahaya göre mutlak açıya dön (0 = başlangıç yönü, 90 = sol, -90 = sağ). En kısa yoldan döner.
inline AutoStep TurnTo(float deg, float speed = 40, uint32_t timeoutMs = 0) {
  return {AS_TURN_TO, deg, 0, 0, speed, timeoutMs, nullptr, nullptr};
}
// Şu anki hedef açıya göre dön (+ = sola, - = sağa). 180'den büyük değer kısa yoldan döner.
inline AutoStep TurnBy(float deg, float speed = 40, uint32_t timeoutMs = 0) {
  return {AS_TURN_BY, deg, 0, 0, speed, timeoutMs, nullptr, nullptr};
}

// Sahada (x, y) noktasına git ve deg açısına dön - aynı anda (mecanum)
inline AutoStep GoTo(float x, float y, float deg, float speed = 50, uint32_t timeoutMs = 0) {
  return {AS_GOTO, x, y, deg, speed, timeoutMs, nullptr, nullptr};
}

// Bekle (motorlar durur)
inline AutoStep Wait(uint32_t ms) {
  return {AS_WAIT, (float)ms, 0, 0, 0, 0, nullptr, nullptr};
}

// Şişe sensörü (limit switch) basılana kadar bekle; en fazla maxMs sonra yine devam eder
inline AutoStep WaitBottle(uint32_t maxMs = 8000) {
  return {AS_WAIT_SW, (float)maxMs, 0, 0, 0, 0, nullptr, nullptr};
}

// Kendi fonksiyonunu bir kez çalıştır (mekanizma, servo vb.) ve hemen sonraki adıma geç
inline AutoStep Run(void (*fn)(), const char *name = "action") {
  return {AS_CALL, 0, 0, 0, 0, 0, fn, name};
}
