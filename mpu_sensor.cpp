#include "mpu_sensor.h"
#include "text_input.h"
#include "nav.h"
#include "screen_chrome.h"
#include <Wire.h>
#include <Preferences.h>
#include <math.h>
#include <lvgl.h>

#define MPU_SDA_PIN 15
#define MPU_SCL_PIN 7
#define MPU_ADDR    0x68

bool  g_mpuReady = false;
float g_mpuAx = 0, g_mpuAy = 0, g_mpuAz = 1.0f;
float g_mpuGx = 0, g_mpuGy = 0, g_mpuGz = 0;
float g_mpuTempC = 0;
float g_smoothRoll = 0, g_smoothPitch = 0;
bool  g_shakeEnabled = true;

static float s_offAx = 0, s_offAy = 0, s_offAz = 0;
static float s_offGx = 0, s_offGy = 0, s_offGz = 0;

static void saveMpuCal() {
  Preferences p; p.begin("mpucal", false);
  p.putFloat("ax", s_offAx); p.putFloat("ay", s_offAy); p.putFloat("az", s_offAz);
  p.putFloat("gx", s_offGx); p.putFloat("gy", s_offGy); p.putFloat("gz", s_offGz);
  p.putBool("done", true);
  p.end();
}
static void loadMpuCal() {
  Preferences p; p.begin("mpucal", true);
  s_offAx = p.getFloat("ax", 0); s_offAy = p.getFloat("ay", 0); s_offAz = p.getFloat("az", 0);
  s_offGx = p.getFloat("gx", 0); s_offGy = p.getFloat("gy", 0); s_offGz = p.getFloat("gz", 0);
  p.end();
}

static bool mpuWriteReg(uint8_t reg, uint8_t val) {
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(reg); Wire.write(val);
  return Wire.endTransmission() == 0;
}
static bool mpuReadBytes(uint8_t reg, uint8_t* buf, uint8_t len) {
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return false;
  uint8_t got = Wire.requestFrom((int)MPU_ADDR, (int)len);
  if (got != len) return false;
  for (uint8_t i = 0; i < len; i++) buf[i] = Wire.read();
  return true;
}

bool mpuInit() {
  loadMpuCal();
  Wire.begin(MPU_SDA_PIN, MPU_SCL_PIN);
  Wire.setClock(400000);
  uint8_t who = 0;
  if (!mpuReadBytes(0x75, &who, 1) || who != 0x68) {
    g_mpuReady = false;
    Serial.printf("[MPU6050] WHO_AM_I=0x%02X (harusnya 0x68) - cek wiring SDA=%d SCL=%d\n",
                  who, MPU_SDA_PIN, MPU_SCL_PIN);
    return false;
  }
  mpuWriteReg(0x6B, 0x00); // bangunkan dari sleep mode
  delay(10);
  mpuWriteReg(0x1C, 0x00); // rentang accel +-2g
  mpuWriteReg(0x1B, 0x00); // rentang gyro +-250 dps
  g_mpuReady = true;
  Serial.println("[MPU6050] OK, sensor siap.");
  return true;
}

static bool mpuReadRaw() {
  uint8_t buf[14];
  if (!mpuReadBytes(0x3B, buf, 14)) return false;
  int16_t rax = (buf[0] << 8) | buf[1], ray = (buf[2] << 8) | buf[3], raz = (buf[4] << 8) | buf[5];
  int16_t rtemp = (buf[6] << 8) | buf[7];
  int16_t rgx = (buf[8] << 8) | buf[9], rgy = (buf[10] << 8) | buf[11], rgz = (buf[12] << 8) | buf[13];
  g_mpuAx = rax / 16384.0f - s_offAx; g_mpuAy = ray / 16384.0f - s_offAy; g_mpuAz = raz / 16384.0f - s_offAz;
  g_mpuGx = rgx / 131.0f - s_offGx;   g_mpuGy = rgy / 131.0f - s_offGy;   g_mpuGz = rgz / 131.0f - s_offGz;
  g_mpuTempC = rtemp / 340.0f + 36.53f;
  return true;
}

static float s_shakeLastMag = 1.0f;
static unsigned long s_lastShakeMs = 0;
static void shakeCheck() {
  if (!g_shakeEnabled || !g_mpuReady) return;
  float mag = sqrtf(g_mpuAx * g_mpuAx + g_mpuAy * g_mpuAy + g_mpuAz * g_mpuAz);
  float delta = fabsf(mag - s_shakeLastMag);
  s_shakeLastMag = mag;
  if (delta > 0.7f && millis() - s_lastShakeMs > 1200) {
    s_lastShakeMs = millis();
    // Sengaja disederhanakan vs file lama: gating cuma cek keyboard lagi
    // kebuka (g_textInputActive) -- lock screen & app Canvas/Update belum
    // ada konsepnya di port ini, jadi belum ada yg perlu dikecualikan lagi.
    if (!g_textInputActive) {
      chromeShowToast("Goyangan terdeteksi -> Home");
      navGoHome();
    }
  }
}

void mpuUpdate() {
  if (!g_mpuReady) return;
  if (!mpuReadRaw()) return;
  float rollRaw = atan2f(g_mpuAy, g_mpuAz) * 180.0f / PI;
  float pitchRaw = atan2f(-g_mpuAx, sqrtf(g_mpuAy * g_mpuAy + g_mpuAz * g_mpuAz)) * 180.0f / PI;
  g_smoothRoll += (rollRaw - g_smoothRoll) * 0.25f;
  g_smoothPitch += (pitchRaw - g_smoothPitch) * 0.25f;
  shakeCheck();
}

void mpuCalibrate() {
  if (!g_mpuReady) return;
  chromeShowToast("Kalibrasi... taruh HP rata & diam!");
  lv_timer_handler(); // paksa toast di atas ke-render dulu SEBELUM nge-block
                       // (loop sampling di bawah gak yield ke LVGL sama sekali)
  delay(400); // beri waktu user meletakkan HP sblm sampling mulai

  const int N = 200;
  double sax = 0, say = 0, saz = 0, sgx = 0, sgy = 0, sgz = 0;
  int got = 0;
  for (int i = 0; i < N; i++) {
    if (mpuReadRaw()) {
      sax += g_mpuAx; say += g_mpuAy; saz += g_mpuAz;
      sgx += g_mpuGx; sgy += g_mpuGy; sgz += g_mpuGz;
      got++;
    }
    delay(5);
  }
  if (got < 10) { chromeShowToast("Kalibrasi gagal, coba lagi"); return; }

  s_offAx += sax / got;
  s_offAy += say / got;
  s_offAz += (saz / got) - 1.0f; // asumsi HP rata, layar ke atas -> Z seharusnya 1g
  s_offGx += sgx / got;
  s_offGy += sgy / got;
  s_offGz += sgz / got;

  saveMpuCal();
  chromeShowToast("Kalibrasi MPU6050 selesai!");
}
