// SEDS UNLV SRADS Liquids Engine Systems
// SPARK-2 Daedalus - Torch Igniter
//
// esp32-synnax-led.ino - bench test of the Synnax channels with just the on-board LED
//
//   test_data.h --> ESP32 --WiFi/TCP or USB--> synnax_oled_bridge.py --> Synnax  (pressure_1..5)
//   LED <-------- ESP32 <--WiFi/TCP or USB-- synnax_oled_bridge.py <-- Synnax  (*_cmd channels)
//
// Same as esp32-synnax-oled with the on-board LED in place of the screen, so it needs no
// screen, wiring or extra libraries. What this board does:
//   - While a bridge is connected, sends the next pressure row from test_data.h every
//     SAMPLE_INTERVAL_MS, starting over from the first row after the last one.
//   - Flashes the LED once for every command the bridge forwards from Synnax, and twice
//     at boot so you know the LED works.
//
// LINKS - both are always on; the bridge picks one.
//   WiFi: join the network in secrets.h, then run synnax_oled_bridge.py
//   USB:  run synnax_oled_bridge.py --usb. Rows only go out over USB after the bridge's
//         first heartbeat, so the Serial Monitor shows just status lines otherwise.
//
// PROTOCOL - same as esp32-synnax-oled, one message per '\n'-terminated line
//   ESP32 -> bridge:  D,<row number>,<p1>,<p2>,<p3>,<p4>,<p5>   one replayed row
//                     [ESP32] ...                               status, printed by the bridge
//   bridge -> ESP32:  C,<channel>,<value>                       a command from Synnax
//                     ~                                         heartbeat, every 500 ms
//   Typing "C,fuel_line_cmd,1" into the Serial Monitor (newline on) tests the LED alone.
//
// LED_PIN is GPIO2, the blue LED on most ESP32 DevKit boards. If yours has no LED there,
// wire an LED and a 220 ohm resistor from GPIO2 to GND.
//
// Build with board "ESP32 Dev Module", no extra libraries. The IDE only sees files in this
// folder, so it needs its own secrets.h (copy it from ../esp32-synnax-oled) and its own
// test_data.h (csv_to_header.py writes both sketches' copies).

#include <WiFi.h>
#include <lwip/sockets.h>
#include <errno.h>

#if __has_include("secrets.h")
#include "secrets.h"
#else
#error "Missing secrets.h - copy it from ../esp32-synnax-oled into this folder"
#endif

#include "test_data.h"

// =========================
// CONFIG
// =========================
const uint32_t SAMPLE_INTERVAL_MS = 10;    // 100 Hz, the rate the coldflow CSV was logged at
const uint32_t LINK_TIMEOUT_MS    = 2000;  // bridge heartbeat is every 500 ms

const int      AP_CHANNEL = 6;             // 1, 6 or 11 - whichever is quietest
const uint16_t TCP_PORT   = 8080;          // must match BRIDGE_PORT in synnax_oled_bridge.py
const uint32_t USB_BAUD   = 115200;        // must match BAUD in synnax_oled_bridge.py

const int      LED_PIN    = 2;             // on-board LED on most DevKits
const uint32_t FLASH_MS   = 100;           // LED on time, and the gap between flashes
const int      MAX_QUEUED = 10;            // a burst of commands flashes at most this many times

const size_t CMD_LINE_MAX     = 96;        // longest line accepted from the bridge
const size_t CMD_NAME_MAX     = 40;        // longest channel name accepted
const size_t CMD_VALUE_MAX    = 16;
const size_t WIFI_PENDING_MAX = 192;       // tail of one partly sent line; status lines are < 160

// Types come before the first function: the Arduino IDE inserts its generated function
// prototypes there, and they use these.
enum LinkId { LINK_USB, LINK_WIFI };
const char* const LINK_NAMES[] = {"USB", "WiFi"};

struct LineReader {
  char buf[CMD_LINE_MAX + 1];
  size_t len = 0;
  bool overflow = false;
};

constexpr bool sameString(const char* a, const char* b) {
  return *a == *b && (*a == '\0' || sameString(a + 1, b + 1));
}
static_assert(sizeof(AP_PASSWORD) - 1 >= 8 && sizeof(AP_PASSWORD) - 1 <= 63,
              "AP_PASSWORD must be 8-63 characters (WPA2)");
static_assert(!sameString(AP_PASSWORD, "CHANGE_ME"), "Set a real AP_PASSWORD in secrets.h");
static_assert(TEST_ROW_COUNT > 0, "test_data.h has no rows - rerun csv_to_header.py");

// =========================
// STATE
// =========================
WiFiServer server(TCP_PORT);
WiFiClient client;
bool wifiOk = false;           // network is up
bool wifiConnected = false;    // a bridge is connected over TCP
uint32_t wifiLastHeard = 0;
char wifiPending[WIFI_PENDING_MAX];
size_t wifiPendingLen = 0;

bool usbLive = false;          // a bridge has sent a heartbeat over USB recently
uint32_t usbLastHeard = 0;

LineReader usbReader, wifiReader;

int nextRow = 0;
uint32_t rowsSent = 0;         // also the row number; keeps counting across replays
uint32_t lastRowAt = 0;

uint32_t commandCount = 0;
int flashesQueued = 0;
bool ledLit = false;
uint32_t ledChangedAt = 0;

// =========================
// OUTPUT
// =========================
void dropWifi(const char* why);

// Pushes the rest of a partly sent line. Returns true once nothing is left.
bool flushWifiPending() {
  if (wifiPendingLen == 0) return true;
  int sent = send(client.fd(), wifiPending, wifiPendingLen, MSG_DONTWAIT);
  if (sent < 0) {
    if (errno != EAGAIN && errno != EWOULDBLOCK) dropWifi("send failed");
    return false;
  }
  memmove(wifiPending, wifiPending + sent, wifiPendingLen - sent);
  wifiPendingLen -= sent;
  return wifiPendingLen == 0;
}

// Never blocks: WiFiClient::write() can stall the loop for up to 10 s on a dying link.
// A line that doesn't fit is dropped whole, never cut, so the bridge never sees half a
// row. Lost rows show up as gaps in the bridge's row count.
void wifiSend(const char* data, size_t n) {
  if (!wifiConnected || n > WIFI_PENDING_MAX) return;
  if (!flushWifiPending()) return;

  int sent = send(client.fd(), data, n, MSG_DONTWAIT);
  if (sent < 0) {
    if (errno != EAGAIN && errno != EWOULDBLOCK) dropWifi("send failed");
    return;
  }
  wifiPendingLen = n - sent;
  memcpy(wifiPending, data + sent, wifiPendingLen);
}

// Status lines go to the Serial Monitor and to a WiFi bridge, which prints them.
void logMsg(const char* fmt, ...) {
  char buf[160];
  int prefix = snprintf(buf, sizeof(buf), "[ESP32] ");
  va_list args;
  va_start(args, fmt);
  vsnprintf(buf + prefix, sizeof(buf) - prefix - 1, fmt, args);  // leave room for '\n'
  va_end(args);
  size_t len = strlen(buf);
  buf[len++] = '\n';
  Serial.write((const uint8_t*)buf, len);
  wifiSend(buf, len);
}

// =========================
// LED
// =========================
// Each queued flash is FLASH_MS on, then FLASH_MS off, so back-to-back commands still
// show up as separate flashes.
void serviceLed() {
  uint32_t now = millis();
  if (now - ledChangedAt < FLASH_MS) return;
  if (ledLit) {
    digitalWrite(LED_PIN, LOW);
    ledLit = false;
    ledChangedAt = now;
  } else if (flashesQueued > 0) {
    flashesQueued--;
    digitalWrite(LED_PIN, HIGH);
    ledLit = true;
    ledChangedAt = now;
  }
}

// =========================
// LINKS
// =========================
void dropWifi(const char* why) {
  client.stop();
  wifiConnected = false;
  wifiPendingLen = 0;
  wifiReader.len = 0;
  wifiReader.overflow = false;
  logMsg("WiFi bridge dropped: %s", why);
}

void markHeard(LinkId from) {
  if (from == LINK_WIFI) {
    wifiLastHeard = millis();
    return;
  }
  usbLastHeard = millis();
  if (!usbLive) {
    usbLive = true;
    logMsg("USB bridge connected");
  }
}

bool validName(const char* s) {
  size_t n = strlen(s);
  if (n == 0 || n > CMD_NAME_MAX) return false;
  for (size_t i = 0; i < n; i++) {
    if (!isalnum((unsigned char)s[i]) && s[i] != '_' && s[i] != '-' && s[i] != '.') return false;
  }
  return true;
}

void handleLine(char* line, LinkId from) {
  size_t n = strlen(line);
  while (n > 0 && (line[n - 1] == '\r' || line[n - 1] == ' ')) line[--n] = '\0';
  if (n == 0) return;

  if (strcmp(line, "~") == 0) {
    markHeard(from);
    return;
  }

  if (strncmp(line, "C,", 2) == 0) {
    char* name = line + 2;
    char* comma = strchr(name, ',');
    const char* value = "";
    if (comma) {
      *comma = '\0';
      value = comma + 1;
    }
    if (!validName(name) || strlen(value) == 0 || strlen(value) > CMD_VALUE_MAX) {
      logMsg("bad command from %s: name \"%.40s\", value \"%.16s\"", LINK_NAMES[from], name, value);
      return;
    }
    markHeard(from);
    commandCount++;
    if (flashesQueued < MAX_QUEUED) flashesQueued++;
    logMsg("Command %lu: %s received (value %s, via %s) - LED flash",
           (unsigned long)commandCount, name, value, LINK_NAMES[from]);
    return;
  }

  logMsg("ignored line from %s: %.40s", LINK_NAMES[from], line);
}

void feedByte(LineReader& r, char c, LinkId from) {
  if (c == '\n') {
    if (r.overflow) {
      logMsg("dropped over-long line from %s", LINK_NAMES[from]);
    } else {
      r.buf[r.len] = '\0';
      handleLine(r.buf, from);
    }
    r.len = 0;
    r.overflow = false;
    return;
  }
  if (r.overflow) return;
  if (r.len < CMD_LINE_MAX) r.buf[r.len++] = c;
  else r.overflow = true;
}

void serviceUsb() {
  for (int budget = 256; budget > 0 && Serial.available() > 0; budget--) {
    feedByte(usbReader, (char)Serial.read(), LINK_USB);
  }
  if (usbLive && millis() - usbLastHeard > LINK_TIMEOUT_MS) {
    usbLive = false;
    logMsg("USB bridge lost: no heartbeat for %lu ms", (unsigned long)LINK_TIMEOUT_MS);
  }
}

void serviceWifi() {
  if (!wifiOk) return;

  WiFiClient incoming = server.accept();
  if (incoming) {
    // A new connection replaces the old one, so a restarted bridge never waits for the
    // old connection to time out.
    if (wifiConnected) dropWifi("replaced by a new connection");
    client = incoming;
    client.setNoDelay(true);
    wifiConnected = true;
    wifiLastHeard = millis();
    logMsg("WiFi bridge connected from %s", client.remoteIP().toString().c_str());
  }
  if (!wifiConnected) return;

  if (!client.connected()) {
    dropWifi("closed by laptop");
    return;
  }
  for (int budget = 512; budget > 0 && client.available() > 0; budget--) {
    int c = client.read();
    if (c < 0) break;
    feedByte(wifiReader, (char)c, LINK_WIFI);
  }
  if (millis() - wifiLastHeard > LINK_TIMEOUT_MS) {
    dropWifi("no heartbeat for 2 s");
    return;
  }
  flushWifiPending();
}

// =========================
// TEST DATA
// =========================
void sendRow() {
  const float* p = TEST_ROWS[nextRow];
  char line[96];
  int n = snprintf(line, sizeof(line), "D,%lu,%g,%g,%g,%g,%g\n", (unsigned long)rowsSent,
                   p[0], p[1], p[2], p[3], p[4]);
  if (usbLive) Serial.write((const uint8_t*)line, n);
  wifiSend(line, n);
  rowsSent++;
  nextRow = (nextRow + 1) % TEST_ROW_COUNT;
}

void serviceRows() {
  uint32_t now = millis();
  if (!wifiConnected && !usbLive) {
    lastRowAt = now;   // nobody listening: hold the replay where it is
    return;
  }
  if (now - lastRowAt < SAMPLE_INTERVAL_MS) return;
  lastRowAt += SAMPLE_INTERVAL_MS;
  if (now - lastRowAt > 100) lastRowAt = now;  // long stall: skip ahead instead of bursting
  sendRow();
}

// =========================
// SETUP / LOOP
// =========================
void setup() {
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  Serial.setTxBufferSize(2048);   // rows over USB must never stall the loop
  Serial.begin(USB_BAUD);
  delay(200);
  logMsg("esp32-synnax-led (built %s %s)", __DATE__, __TIME__);
  logMsg("%d test rows from \"%s\", one every %lu ms", TEST_ROW_COUNT, TEST_DATA_SOURCE,
         (unsigned long)SAMPLE_INTERVAL_MS);

  WiFi.persistent(false);
  WiFi.mode(WIFI_AP);
  wifiOk = WiFi.softAP(AP_SSID, AP_PASSWORD, AP_CHANNEL);
  if (wifiOk) {
    server.begin();
    server.setNoDelay(true);
    logMsg("WiFi network \"%s\" at %s, TCP port %u", AP_SSID,
           WiFi.softAPIP().toString().c_str(), TCP_PORT);
  } else {
    logMsg("could not start the WiFi network - USB only");
  }

  flashesQueued = 2;   // boot check: two flashes
  logMsg("waiting for synnax_oled_bridge.py...");
}

void loop() {
  serviceUsb();
  serviceWifi();
  serviceRows();
  serviceLed();
  delay(1);   // let the WiFi stack run
}
