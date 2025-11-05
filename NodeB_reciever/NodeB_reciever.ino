#include <WiFi.h>
#include <esp_now.h>
#include <vector>
#include "esp_wifi.h"

// ---------------- Blockchain structures ----------------
struct Block {
  int index;
  float distance;
  String prevHash;
  String hash;
  unsigned long timestamp;
};

enum MsgType : uint8_t { NEW_BLOCK = 0, SYNC_REQUEST, SYNC_BLOCK };
std::vector<Block> blockchain;

// ---------------- Timestamp formatting ----------------
String formatTime(unsigned long ms) {
  unsigned long totalSeconds = ms / 1000;
  unsigned long h = totalSeconds / 3600;
  unsigned long m = (totalSeconds % 3600) / 60;
  unsigned long s = totalSeconds % 60;
  char buf[9];
  sprintf(buf, "%02lu:%02lu:%02lu", h, m, s);
  return String(buf);
}

// ---------------- Serial colors ----------------
#define CYAN "\033[36m"
#define RED  "\033[31m"
#define RESET "\033[0m"

// ---------------- Display blockchain ----------------
void printBlockchain() {
  Serial.println("\n--- Node B Blockchain ---");
  Serial.println("Idx | Distance | Time     | Prev Hash | Hash");
  Serial.println("-----------------------------------------------");
  for (Block &b : blockchain) {
    Serial.printf("%d   | %.2f     | %s | %s | %s\n",
                  b.index, b.distance,
                  formatTime(b.timestamp).c_str(),
                  b.prevHash.substring(0,8).c_str(),
                  b.hash.substring(0,8).c_str());
  }
}

// ---------------- Validation ----------------
bool validateBlock(Block &b) {
  if (b.index == 0) return true;
  if (blockchain.empty()) return false;
  return b.prevHash == blockchain.back().hash;
}

// ---------------- ESP-NOW send helpers ----------------
uint8_t nodeAMac[] = {0xF4,0x65,0x0B,0xE8,0x46,0x5C};   // <-- replace with Node A STA MAC

void sendPacket(uint8_t type, String payload) {
  String message = String(type) + "|" + payload;
  esp_err_t result = esp_now_send(nodeAMac, (uint8_t*)message.c_str(), message.length() + 1);
  if (result == ESP_OK) Serial.println(CYAN "📤 Packet sent successfully" RESET);
  else Serial.println(RED "⚠️ Packet send failed" RESET);
}

void requestSync(int fromIndex) {
  Serial.println(RED "⚠️ Node B requesting sync..." RESET);
  sendPacket(SYNC_REQUEST, String(fromIndex));
}

// ---------------- ESP-NOW receive callback ----------------
void onReceive(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
  // Debug: show raw packet
  Serial.println("📡 Packet received!");
  Serial.print("Raw data: ");
  for (int i = 0; i < len; i++) Serial.print((char)data[i]);
  Serial.println();

  String msg;
  for (int i = 0; i < len; i++) msg += (char)data[i];
  int sep = msg.indexOf('|');
  if (sep == -1) return;

  uint8_t type = msg.substring(0, sep).toInt();
  String payload = msg.substring(sep + 1);

  if (type == NEW_BLOCK || type == SYNC_BLOCK) {
    int a = payload.indexOf(','), b = payload.indexOf(',', a+1);
    int c = payload.indexOf(',', b+1), d = payload.indexOf(',', c+1);

    Block blk;
    blk.index     = payload.substring(0, a).toInt();
    blk.distance  = payload.substring(a+1, b).toFloat();
    blk.prevHash  = payload.substring(b+1, c);
    blk.hash      = payload.substring(c+1, d);
    blk.timestamp = payload.substring(d+1).toInt();

    if (validateBlock(blk)) {
      blockchain.push_back(blk);
      Serial.printf(CYAN "\n📥 Received Block #%d ✅ Appended\n" RESET, blk.index);
    } else {
      Serial.printf(RED "\n📥 Block #%d ❌ Rejected (Prev hash mismatch)\n" RESET, blk.index);
      requestSync(blockchain.size());
    }

    printBlockchain();
  }
}

// ---------------- Setup ----------------
void setup() {
  Serial.begin(115200);
  Serial.println("\n🚀 Node B starting...");
  WiFi.mode(WIFI_STA);

  // --- NEW: Auto-sync channel with Node A ---
  wifi_second_chan_t sc;
  uint8_t currentChan;
  esp_wifi_get_channel(&currentChan, &sc);
  Serial.print("📶 Current channel before sync: ");
  Serial.println(currentChan);

  // If you know Node A’s Wi-Fi channel, set it here manually for now (most routers use 6 or 11)
  uint8_t targetChannel = 6;   // << try 1, 6, or 11 depending on your router
  esp_wifi_set_channel(targetChannel, WIFI_SECOND_CHAN_NONE);
  Serial.print("🔧 Forced to channel: ");
  Serial.println(targetChannel);
  // -----------------------------------------

  // Create genesis block
  Block g{0, 0, "0", "GENESIS_HASH", 0};
  blockchain.push_back(g);

  // Init ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println(RED "❌ ESP-NOW init failed!" RESET);
    return;
  }
  esp_now_register_recv_cb(onReceive);

  // Add Node A as peer
  esp_now_peer_info_t peer{};
  memcpy(peer.peer_addr, nodeAMac, 6);
  peer.channel = targetChannel;  // must match Node A
  peer.encrypt = false;

  if (esp_now_add_peer(&peer) == ESP_OK)
    Serial.println(CYAN "✅ Peer added" RESET);
  else
    Serial.println(RED "❌ Failed to add peer" RESET);

  Serial.println(CYAN "📡 Node B ready and listening..." RESET);
}

// ---------------- Loop ----------------
void loop() {
  // Everything handled via callback
}
