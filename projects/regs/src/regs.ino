/*
 * MAX3421E register probe.
 * Answers, straight from the chip: is the die alive, is VBUS present,
 * and is there a D+ pull-up (J state) on the bus right now.
 * This isolates physical-layer problems from enumeration/exploit problems.
 */
#include "Usb.h"

USB Usb;
uint32_t last_print = 0;

static const char *hrsl_name(uint8_t hr)
{
  switch (hr & 0x0f) {
    case 0x00: return "SE0  no pull-up => nothing on the bus (or no VBUS)";
    case 0x01: return "J    full-speed device attached";
    case 0x02: return "K    low-speed device / SE1 signalling";
    case 0x03: return "SE1  illegal bus state";
    default:   return "error/invalid bus state";
  }
}

static void print_flags(uint8_t usbirq)
{
  Serial.print(F(" OSCOK="));
  Serial.print((usbirq & bmOSCOKIRQ) ? 1 : 0);
  Serial.print(F(" VBUS="));
  Serial.print((usbirq & bmVBUSIRQ) ? 1 : 0);
  Serial.print(F(" NOVBUS="));
  Serial.print((usbirq & bmNOVBUSIRQ) ? 1 : 0);
}

void setup()
{
  Serial.begin(115200);
  delay(300);
  Serial.println();
  Serial.println(F("[regs] MAX3421E register probe"));

  int rc = Usb.Init();
  Serial.print(F("[regs] Usb.Init() = "));
  Serial.println(rc);

  uint8_t rev = Usb.regRd(rREVISION);
  Serial.print(F("[regs] die revision = 0x"));
  Serial.println(rev, HEX);
  Serial.println(F("[regs] expect 0x12 or 0x13 for a working MAX3421E"));
}

void loop()
{
  Usb.Task();

  if (millis() - last_print < 500)
    return;
  last_print = millis();

  uint8_t usbirq = Usb.regRd(rUSBIRQ);
  uint8_t hirq   = Usb.regRd(rHIRQ);
  uint8_t hrsl   = Usb.regRd(rHRSL);
  uint8_t st     = Usb.getUsbTaskState();

  Serial.print(F("[regs] t="));
  Serial.print(millis() / 1000);
  Serial.print(F("s state=0x"));
  Serial.print(st, HEX);

  Serial.print(F(" usbirq=0x"));
  if (usbirq < 0x10) Serial.print('0');
  Serial.print(usbirq, HEX);
  print_flags(usbirq);

  Serial.print(F(" hirq=0x"));
  if (hirq < 0x10) Serial.print('0');
  Serial.print(hirq, HEX);

  Serial.print(F(" hrsl=0x"));
  if (hrsl < 0x10) Serial.print('0');
  Serial.print(hrsl, HEX);
  Serial.print(' ');
  Serial.println(hrsl_name(hrsl));
}
