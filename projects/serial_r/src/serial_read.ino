/*
 * Reads the DFU device's string descriptors (manufacturer / product / serial).
 * The point is descriptor index 3: after a successful checkm8 the serial
 * becomes "PWND:[checkm8]" instead of the stock serial number.
 */
#include "Usb.h"

USB Usb;
USB_DEVICE_DESCRIPTOR d;
uint8_t last_state = 0xff;
uint8_t sbuf[128];
bool probed = false;

static void print_string(uint8_t index, const char *label)
{
  uint8_t rc = Usb.getStrDescr(0, 0, sizeof(sbuf), index, 0x0409, sbuf);
  Serial.print(label);
  Serial.print(F(" (idx "));
  Serial.print(index);
  Serial.print(F(") rc=0x"));
  Serial.print(rc, HEX);
  if (rc == 0) {
    uint8_t len = sbuf[0];
    if (len > sizeof(sbuf)) len = sizeof(sbuf);
    Serial.print(F("  \""));
    for (uint8_t i = 2; i < len; i += 2) {
      uint8_t c = sbuf[i];
      if (c == 0) break;
      Serial.print((c >= 0x20 && c < 0x7f) ? (char)c : '?');
    }
    Serial.print('"');
  }
  Serial.println();
}

static void probe()
{
  uint8_t rc = Usb.getDevDescr(0, 0, sizeof(d), (uint8_t *)&d);
  Serial.print(F("[ser] descriptor rc=0x"));
  Serial.println(rc, HEX);
  if (rc != 0) return;

  Serial.print(F("[ser] VID=0x")); Serial.print(d.idVendor, HEX);
  Serial.print(F(" PID=0x")); Serial.print(d.idProduct, HEX);
  Serial.print(F(" iSerial=")); Serial.println(d.iSerialNumber);

  print_string(d.iManufacturer, "[ser] mfg   ");
  print_string(d.iProduct,      "[ser] prod  ");
  print_string(d.iSerialNumber, "[ser] serial");

  /* The pwned serial is a UTF-16 string; look for 'PWND' in the low bytes. */
  if (Usb.getStrDescr(0, 0, sizeof(sbuf), d.iSerialNumber, 0x0409, sbuf) == 0) {
    bool pwned = (sbuf[2] == 'P' && sbuf[4] == 'W' && sbuf[6] == 'N' && sbuf[8] == 'D');
    Serial.println(pwned ? F("[ser] => PWND:[checkm8] CONFIRMED")
                         : F("[ser] => serial is NOT pwned"));
  }
}

void setup()
{
  Serial.begin(115200);
  delay(300);
  Serial.println();
  Serial.println(F("[ser] DFU string descriptor reader"));
  Serial.print(F("[ser] Usb.Init() = "));
  Serial.println(Usb.Init());
}

void loop()
{
  Usb.Task();
  uint8_t st = Usb.getUsbTaskState();

  if (st != last_state) {
    Serial.print(F("[ser] state 0x"));
    Serial.print(last_state, HEX);
    Serial.print(F(" -> 0x"));
    Serial.println(st, HEX);
    last_state = st;
    if (st != 0x90) probed = false;
  }

  if (st == 0x90 && !probed) {
    probed = true;
    probe();
  }

  static uint32_t beat = 0;
  if (millis() - beat > 10000) {
    beat = millis();
    Serial.print(F("[ser] alive, state=0x"));
    Serial.println(st, HEX);
  }
}
