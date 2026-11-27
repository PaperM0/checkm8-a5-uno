/*
 * A/B control: UNPATCHED USB Host Shield library.
 * Stock enumeration runs Configuring() -> DefaultAddressing() -> SET_ADDRESS,
 * so the device ends up on address 1 and that is where board_qc reads it.
 * If this prints a real VID/PID, the hardware path is fine and the problem
 * is specific to the patched (address 0, no enumeration) path.
 */
#include "Usb.h"

USB Usb;
USB_DEVICE_DESCRIPTOR d;
uint8_t last_state = 0xff;
bool probed = false;

void setup()
{
  Serial.begin(115200);
  delay(300);
  Serial.println();
  Serial.println(F("[stock] unpatched-library descriptor reader"));
  Serial.print(F("[stock] Usb.Init() = "));
  Serial.println(Usb.Init());
}

void loop()
{
  Usb.Task();

  uint8_t st = Usb.getUsbTaskState();
  if (st != last_state) {
    Serial.print(F("[stock] state 0x"));
    Serial.print(last_state, HEX);
    Serial.print(F(" -> 0x"));
    Serial.println(st, HEX);
    last_state = st;
    if (st == 0x10) probed = false;
  }

  if (st == 0x90 && !probed) {
    probed = true;
    for (uint8_t addr = 0; addr <= 1; addr++) {
      uint8_t rc = Usb.getDevDescr(addr, 0, sizeof(d), (uint8_t *)&d);
      Serial.print(F("[stock] getDevDescr(addr="));
      Serial.print(addr);
      Serial.print(F(") rc=0x"));
      Serial.println(rc, HEX);
      if (rc == 0) {
        Serial.print(F("[stock] VID=0x")); Serial.print(d.idVendor, HEX);
        Serial.print(F(" PID=0x")); Serial.print(d.idProduct, HEX);
        Serial.print(F(" bcdDevice=0x")); Serial.print(d.bcdDevice, HEX);
        Serial.print(F(" maxPkt0=")); Serial.print(d.bMaxPacketSize0);
        Serial.print(F(" iSerial=")); Serial.println(d.iSerialNumber);
      }
    }
  }
}
