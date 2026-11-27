/*
 * USB port tester / device lister.
 * Every attach is reported with the raw descriptor plus a decoded bus state,
 * and every 5 s it prints what the MAX3421E sees on D+/D- while idle.
 *
 *   bus=SE0  nothing is pulling D+ up  (no device, or no VBUS to it)
 *   bus=J    a full-speed device is attached
 *   bus=K    a low-speed device is attached
 *   bus=SE1  illegal / floating data lines (wiring or VBUS fault)
 */
#include "Usb.h"

USB Usb;
USB_DEVICE_DESCRIPTOR d;
uint8_t last_state = 0xff;
uint32_t last_beat = 0;
uint16_t attach_count = 0;

static const char *bus_state(uint8_t hirq_hrsl)
{
  switch (hirq_hrsl & 0xc0) {
    case 0x00: return "SE0 (no device)";
    case 0x80: return "J   (full-speed device)";
    case 0x40: return "K   (low-speed device)";
    default:   return "SE1 (illegal / floating)";
  }
}

static const char *class_name(uint8_t c)
{
  switch (c) {
    case 0x00: return "per-interface";
    case 0x02: return "CDC";
    case 0x03: return "HID";
    case 0x08: return "mass storage";
    case 0x09: return "hub";
    case 0xe0: return "wireless";
    case 0xff: return "vendor-specific";
    default:   return "other";
  }
}

static const char *known_device(uint16_t vid, uint16_t pid)
{
  if (vid != 0x05ac) return NULL;
  switch (pid) {
    case 0x1227: return "Apple DFU - the checkm8 target";
    case 0x1281: return "Apple DFU variant";
    case 0x12a8: return "Apple normal/recovery mode";
    default:     return "Apple device";
  }
}

static void report_device()
{
  uint8_t rc = 0xff;
  uint8_t used_addr = 0;

  for (uint8_t addr = 0; addr <= 1; addr++) {
    rc = Usb.getDevDescr(addr, 0, sizeof(d), (uint8_t *)&d);
    if (rc == 0) { used_addr = addr; break; }
  }

  attach_count++;

  if (rc != 0) {
    Serial.print(F("ATTACH #"));
    Serial.print(attach_count);
    Serial.print(F(" but descriptor read failed, rc=0x"));
    Serial.println(rc, HEX);
    return;
  }

  Serial.print(F("ATTACH #"));
  Serial.print(attach_count);
  Serial.print(F("  addr="));
  Serial.print(used_addr);
  Serial.print(F("  VID=0x"));
  Serial.print(d.idVendor, HEX);
  Serial.print(F("  PID=0x"));
  Serial.print(d.idProduct, HEX);
  Serial.print(F("  class="));
  Serial.print(class_name(d.bDeviceClass));
  Serial.print(F("  maxPkt0="));
  Serial.print(d.bMaxPacketSize0);
  Serial.print(F("  bcdUSB=0x"));
  Serial.println(d.bcdUSB, HEX);

  const char *name = known_device(d.idVendor, d.idProduct);
  if (name) {
    Serial.print(F("            => "));
    Serial.println(name);
  }
}

void setup()
{
  Serial.begin(115200);
  delay(300);
  Serial.println();
  Serial.println(F("[port] USB port tester - plug anything into the shield"));
  Serial.print(F("[port] Usb.Init() = "));
  Serial.println(Usb.Init());
}

void loop()
{
  Usb.Task();

  uint8_t st = Usb.getUsbTaskState();
  if (st != last_state) {
    Serial.print(F("[port] state 0x"));
    Serial.print(last_state, HEX);
    Serial.print(F(" -> 0x"));
    Serial.print(st, HEX);
    switch (st) {
      case 0x12: Serial.println(F("  no device")); break;
      case 0x20: Serial.println(F("  attach detected")); break;
      case 0x30: Serial.println(F("  resetting device")); break;
      case 0x60: Serial.println(F("  asking for descriptor size")); break;
      case 0x90: Serial.println(F("  RUNNING")); break;
      case 0xa0: Serial.println(F("  ERROR")); break;
      default:   Serial.println(); break;
    }
    last_state = st;
    if (st == 0x90) report_device();
  }

  if (millis() - last_beat > 5000) {
    last_beat = millis();
    uint8_t hrsl = Usb.regRd(rHRSL);
    Serial.print(F("[port] alive, state=0x"));
    Serial.print(st, HEX);
    Serial.print(F("  bus="));
    Serial.print(bus_state(hrsl));
    Serial.print(F("  HRSL=0x"));
    Serial.println(hrsl, HEX);
  }
}
