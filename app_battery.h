// app_battery.h
// Port dari battEnter()/drawBatteryApp()/battTouch() di file lama --
// ikon baterai besar + detail teknis. Nilai voltase/persen masih dari
// sys_state.h (stub 3.7V/100% sampai kode ADC asli di-port).
#pragma once

void batteryScreenShow();
