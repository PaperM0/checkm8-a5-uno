/*
 * USB host probe — detection only, no checkm8.
 * Prints every MAX3421E/USB state transition and the raw device descriptor,
 * so "does it even see the device?" can be answered independently of the exploit.
 */
#include "Usb.h"

USB Usb;
USB_DEVICE_DESCRIPTOR desc;
uint8_t last_state = 0xff;
bool reported = false;
uint32_t last_heartbeat = 0;

static const char *state_name(uint8_t s)
{
  switch (s) {
    case 0x10: return "DETACHED";
    case 0x11: return "DETACHED_INIT";
    case 0x12: return "DETACHED_WAIT_FOR_DEVICE";
    case 0x20: return "ATTACHED_SETTLE";
    case 0x30: return "ATTACHED_RESET_DEVICE";
    case 0x40: return "ATTACHED_WAIT_RESET_COMPLETE";
    case 0x50: return "ATTACHED_WAIT_SOF";
    case 0x51: return "ATTACHED_WAIT_RESET";
    case 0x60: return "GET_DEVICE_DESCRIPTOR_SIZE";
    case 0x70: return "ADDRESSING";
    case 0x80: return "CONFIGURING";
    case 0x90: return "RUNNING";
    case 0xa0: return "ERROR";
    default:   return "UNKNOWN";
  }
}

static void print_descriptor()
{
  const uint8_t *b = (const uint8_t *)&desc;
  Serial.print(F("[probe] raw: "));
  for (uint8_t i = 0; i < sizeof(desc); i++) {
    if (b[i] < 0x10) Serial.print('0');
    Serial.print(b[i], HEX);
    Serial.print(' ');
  }
  Serial.println();
  Serial.print(F("[probe] VID=0x")); Serial.print(desc.idVendor, HEX);
  Serial.print(F(" PID=0x")); Serial.print(desc.idProduct, HEX);
  Serial.print(F(" bcdUSB=0x")); Serial.print(desc.bcdUSB, HEX);
  Serial.print(F(" maxPkt0=")); Serial.print(desc.bMaxPacketSize0);
  Serial.print(F(" bcdDevice=0x")); Serial.print(desc.bcdDevice, HEX);
  Serial.print(F(" iSerial=")); Serial.println(desc.iSerialNumber);
  if (desc.idVendor == 0x5ac && desc.idProduct == 0x1227)
    Serial.println(F("[probe] => Apple DFU (0x5AC/0x1227) MATCH"));
  else
    Serial.println(F("[probe] => not the Apple DFU id the exploit expects"));
}

void setup()
{
  Serial.begin(115200);
  delay(300);
  Serial.println();
  Serial.println(F("[probe] usb host probe (no exploit)"));
  Serial.print(F("[probe] Usb.Init() = "));
  Serial.println(Usb.Init());
}

void loop()
{
  Usb.Task();

  uint8_t st = Usb.getUsbTaskState();
  if (st != last_state) {
    Serial.print(F("[probe] state 0x"));
    Serial.print(last_state, HEX);
    Serial.print(F(" -> 0x"));
    Serial.print(st, HEX);
    Serial.print(' ');
    Serial.println(state_name(st));
    last_state = st;
    /* Re-arm whenever the bus is not in RUNNING any more. The detach path can
       go 0x90 -> 0x12 (DETACHED_WAIT_FOR_DEVICE) and never visit 0x10, so
       testing for 0x10 alone misses re-plug events. */
    if (st != USB_STATE_RUNNING) reported = false;
  }

  if (st == USB_STATE_ERROR) {
    Serial.println(F("[probe] ERROR, re-arming"));
    Usb.setUsbTaskState(USB_ATTACHED_SUBSTATE_RESET_DEVICE);
    last_state = 0xff;
  }

  if (st == USB_STATE_RUNNING && !reported) {
    reported = true;
    uint8_t rcode = 0xff;
    for (uint8_t attempt = 1; attempt <= 3; attempt++) {
      rcode = Usb.getDevDescr(0, 0, sizeof(desc), (uint8_t *)&desc);
      Serial.print(F("[probe] getDevDescr try "));
      Serial.print(attempt);
      Serial.print(F(" rc=0x"));
      Serial.println(rcode, HEX);
      if (rcode == 0) break;
      delay(300);
    }
    if (rcode == 0)
      print_descriptor();
    else
      Serial.println(F("[probe] descriptor unreadable at address 0 "
                       "(device may have been moved by SET_ADDRESS)"));
  }

  if (millis() - last_heartbeat > 5000) {
    last_heartbeat = millis();
    Serial.print(F("[probe] alive, state=0x"));
    Serial.print(st, HEX);
    Serial.print(' ');
    Serial.println(state_name(st));
  }
}
