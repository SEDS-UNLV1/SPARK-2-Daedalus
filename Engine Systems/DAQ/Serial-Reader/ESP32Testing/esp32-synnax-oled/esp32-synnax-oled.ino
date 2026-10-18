// SEDS UNLV SRADS Liquids Engine Systems
// SPARK-2 Daedalus - Torch Igniter
//
// esp32-synnax-oled.ino - bench test of the Synnax channels: no Mega, no valves
//
//   test_data.h --> ESP32 --WiFi/TCP or USB--> synnax_oled_bridge.py --> Synnax  (pressure_1..5)
//   OLED <------- ESP32 <--WiFi/TCP or USB-- synnax_oled_bridge.py <-- Synnax  (*_cmd channels)
//
// The ESP32 never talks to Synnax itself: Synnax's client libraries need a full computer.
// synnax_oled_bridge.py on the laptop does that half, the same way esp32-bridge.py does
// for the Mega.
//
// What this board does:
//   - While a bridge is connected, sends the next pressure row from test_data.h every
//     SAMPLE_INTERVAL_MS, starting over from the first row after the last one.
//   - Shows each command the bridge forwards on the OLED as
//     "Command <n>: <channel> received", n counting commands since boot.
//
// LINKS - both are always on; the bridge picks one.
//   WiFi: this board runs its own network (name and password in secrets.h). Join it from
//         the laptop, then run synnax_oled_bridge.py (it connects to 192.168.4.1:8080).
//   USB:  run synnax_oled_bridge.py --usb. Rows only go out over USB after the bridge's
//         first heartbeat, so the Serial Monitor shows just status lines otherwise.
//
// PROTOCOL - one message per '\n'-terminated line
//   ESP32 -> bridge:  D,<row number>,<p1>,<p2>,<p3>,<p4>,<p5>   one replayed row
//                     [ESP32] ...                               status, printed by the bridge
//   bridge -> ESP32:  C,<channel>,<value>                       a command from Synnax
//                     ~                                         heartbeat, every 500 ms
//   Typing "C,fuel_line_cmd,1" into the Serial Monitor (newline on) tests the OLED alone.
//
// WIRING - SSD1306 0.96" 128x64 I2C OLED
//   VCC -> 3V3   GND -> GND   SDA -> GPIO21   SCL -> GPIO22
//
// Build with board "ESP32 Dev Module" and the Adafruit SSD1306 and Adafruit GFX libraries.
// Copy secrets.h.example to secrets.h first. To replay a different CSV, regenerate
// test_data.h with csv_to_header.py and re-flash.

#include <WiFi.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <lwip/sockets.h>
#include <errno.h>

#if __has_include("secrets.h")
#include "secrets.h"
#else
#error "Missing secrets.h - copy secrets.h.example to secrets.h and set AP_PASSWORD"
#endif

#include "test_data.h"

// =========================
// CONFIG
// =========================
const uint32_t SAMPLE_INTERVAL_MS = 10;    // 100 Hz, the rate the coldflow CSV was logged at
const uint32_t LINK_TIMEOUT_MS    = 2000;  // bridge heartbeat is every 500 ms
const uint32_t SCREEN_REFRESH_MS  = 500;   // row counter refresh; commands redraw at once

const int      AP_CHANNEL = 6;             // 1, 6 or 11 - whichever is quietest
const uint16_t TCP_PORT   = 8080;          // must match BRIDGE_PORT in synnax_oled_bridge.py
const uint32_t USB_BAUD   = 115200;        // must match BAUD in synnax_oled_bridge.py

const int     OLED_SDA      = 21;
const int     OLED_SCL      = 22;
const uint8_t OLED_ADDR     = 0x3C;
const int     SCREEN_WIDTH  = 128;
const int     SCREEN_HEIGHT = 64;

const size_t CMD_LINE_MAX     = 96;        // longest line accepted from the bridge
const size_t CMD_NAME_MAX     = 40;        // longest channel name shown
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
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
bool oledOk = false;

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
char lastName[CMD_NAME_MAX + 1] = "";
char lastValue[CMD_VALUE_MAX + 1] = "";
LinkId lastVia = LINK_USB;

bool screenDirty = true;
uint32_t lastDraw = 0;

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
// LINKS
// =========================
void dropWifi(const char* why) {
  client.stop();
  wifiConnected = false;
  wifiPendingLen = 0;
  wifiReader.len = 0;
  wifiReader.overflow = false;
  screenDirty = true;
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
    screenDirty = true;
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
    strlcpy(lastName, name, sizeof(lastName));
    strlcpy(lastValue, value, sizeof(lastValue));
    lastVia = from;
    screenDirty = true;
    logMsg("Command %lu: %s received (value %s, via %s)",
           (unsigned long)commandCount, lastName, lastValue, LINK_NAMES[from]);
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
    screenDirty = true;
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
    screenDirty = true;
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
// SCREEN
// =========================
void drawScreen() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  display.setCursor(0, 0);
  display.printf("WiFi %s  USB %s", wifiConnected ? "ok" : "--", usbLive ? "ok" : "--");
  display.setCursor(0, 9);
  display.printf("rows sent %lu", (unsigned long)rowsSent);
  display.drawFastHLine(0, 19, SCREEN_WIDTH, SSD1306_WHITE);

  display.setCursor(0, 24);
  if (commandCount > 0) {
    display.printf("Command %lu:\n", (unsigned long)commandCount);
    display.println(lastName);   // wraps onto a second line past 21 characters
    display.println("received");
    display.setCursor(0, 56);
    display.printf("value %s via %s", lastValue, LINK_NAMES[lastVia]);
  } else if (wifiConnected || usbLive) {
    display.println("Waiting for commands");
  } else {
    display.println("Join WiFi network:");
    display.println(AP_SSID);
    display.println("or run bridge --usb");
  }
  display.display();
}

void serviceScreen() {
  if (!oledOk) return;
  uint32_t now = millis();
  if (!screenDirty && now - lastDraw < SCREEN_REFRESH_MS) return;
  drawScreen();
  screenDirty = false;
  lastDraw = now;
}

// =========================
// SETUP / LOOP
// =========================
void setup() {
  Serial.setTxBufferSize(2048);   // rows over USB must never stall the loop
  Serial.begin(USB_BAUD);
  delay(200);
  logMsg("esp32-synnax-oled (built %s %s)", __DATE__, __TIME__);
  logMsg("%d test rows from \"%s\", one every %lu ms", TEST_ROW_COUNT, TEST_DATA_SOURCE,
         (unsigned long)SAMPLE_INTERVAL_MS);

  Wire.begin(OLED_SDA, OLED_SCL);
  oledOk = display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR);
  if (!oledOk) {
    logMsg("OLED not found at 0x%02X (SDA %d, SCL %d) - running without it", OLED_ADDR,
           OLED_SDA, OLED_SCL);
  }

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
  logMsg("waiting for synnax_oled_bridge.py...");
}

void loop() {
  serviceUsb();
  serviceWifi();
  serviceRows();
  serviceScreen();
  delay(1);   // let the WiFi stack run
}
