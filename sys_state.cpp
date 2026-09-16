#include "sys_state.h"
#include <Preferences.h>

bool  g_ntpSynced      = false;
bool  g_wifiConnected  = false;
int   g_battPercent    = 100;
float g_battVoltage    = 3.7f;
bool  g_autoRotate     = true; // default sama spt file lama (autoRotateEnabled=true)

void sysLoadAutoRotatePref() {
  Preferences p; p.begin("ui", true);
  g_autoRotate = p.getBool("autorot", true);
  p.end();
}
void sysSaveAutoRotatePref() {
  Preferences p; p.begin("ui", false);
  p.putBool("autorot", g_autoRotate);
  p.end();
}

void sysUpdateBattery() {
  // TODO: analogRead(BATT_ADC_PIN) + hitung persen, spt battUpdate() lama.
  // Sengaja dibiarkan kosong dulu -- app Baterai tetap bisa dites tampilannya
  // pakai nilai default di atas sampai bagian ADC/voltage-divider di-port.
}
