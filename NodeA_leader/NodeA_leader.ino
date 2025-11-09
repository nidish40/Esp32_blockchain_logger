#include <WiFi.h>
#include <esp_now.h>
#include <vector>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "esp_wifi.h"

// ---------- CONFIG ----------
uint8_t nodeBMac[] = {0xB0,0xA7,0x32,0x2B,0x53,0x40}; // Node B STA MAC
const char* serverURL = "http://172.20.10.12:5000/upload_block"; // Flask server
const int MAX_BLOCKS_IN_RAM = 10;
const long interval = 10000; // 10 s
// ----------------------------

struct Block {
  int index;
  float distance;
  String prevHash;
  String hash;
  unsigned long timestamp;
};

std::vector<Block> blockchain;

String calcHash(int i,float d,String p,unsigned long t){
  return String(i)+"-"+String(d)+"-"+p+"-"+String(t);
}
Block createBlock(float distance){
  Block b;
  b.index=blockchain.empty()?0:blockchain.back().index+1;
  b.distance=distance;
  b.timestamp=millis();
  b.prevHash=blockchain.empty()?"GENESIS_HASH":blockchain.back().hash;
  b.hash=calcHash(b.index,b.distance,b.prevHash,b.timestamp);
  return b;
}
String fmtTime(unsigned long ms){
  unsigned long s=ms/1000,h=s/3600,m=(s%3600)/60; s%=60;
  char buf[9]; sprintf(buf,"%02lu:%02lu:%02lu",h,m,s); return String(buf);
}
#define GREEN "\033[32m"
#define RESET "\033[0m"

// ---------- Wi-Fi ----------
String ssid, password;
void connectWiFiDynamic(){
  Serial.println("\nEnter Wi-Fi SSID:");
  while (Serial.available()==0);
  ssid = Serial.readStringUntil('\n');
  ssid.trim();

  Serial.println("Enter Wi-Fi Password:");
  while (Serial.available()==0);
  password = Serial.readStringUntil('\n');
  password.trim();

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid.c_str(), password.c_str());
  Serial.print("Connecting to "); Serial.println(ssid);
  int t=0;
  while (WiFi.status()!=WL_CONNECTED && t<30){ delay(500); Serial.print("."); t++; }
  if (WiFi.status()==WL_CONNECTED){
    Serial.println("\n✅ Wi-Fi connected!");
    Serial.print("IP: "); Serial.println(WiFi.localIP());
  } else Serial.println("\n⚠️ Wi-Fi connect failed, continuing offline.");
}

// ---------- ESP-NOW ----------
void sendBlock(Block &b){
  String payload=String(b.index)+","+String(b.distance)+","+b.prevHash+","+b.hash+","+String(b.timestamp);
  String msg="0|"+payload;
  esp_err_t r=esp_now_send(nodeBMac,(uint8_t*)msg.c_str(),msg.length()+1);
  if(r==ESP_OK) Serial.println(GREEN "📤 Packet sent successfully" RESET);
  else Serial.println("⚠️ Packet send failed");
}

// ---------- Offload ----------
void offloadBlock(Block &b){
  if(WiFi.status()!=WL_CONNECTED) return;
  HTTPClient http; http.begin(serverURL); http.addHeader("Content-Type","application/json");
  StaticJsonDocument<256> doc;
  doc["index"]=b.index; doc["distance"]=b.distance;
  doc["prevHash"]=b.prevHash; doc["hash"]=b.hash; doc["timestamp"]=b.timestamp;
  String payload; serializeJson(doc,payload);
  int code=http.POST(payload);
  Serial.printf("🌐 Offload Block #%d → %d\n",b.index,code);
  http.end();
}
void trimBlockchain(){
  while(blockchain.size()>MAX_BLOCKS_IN_RAM){
    offloadBlock(blockchain.front());
    blockchain.erase(blockchain.begin());
  }
}

// ---------- Setup ----------
void setup() {
  Serial.begin(115200);
  Serial.println("\n🚀 Node A starting...");

  // --- 1️⃣ Ask for Wi-Fi credentials dynamically ---
  Serial.println("Enter Wi-Fi SSID:");
  while (Serial.available() == 0);
  String ssid = Serial.readStringUntil('\n'); ssid.trim();

  Serial.println("Enter Wi-Fi Password:");
  while (Serial.available() == 0);
  String password = Serial.readStringUntil('\n'); password.trim();

  // --- 2️⃣ Connect to Wi-Fi ---
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid.c_str(), password.c_str());
  Serial.print("Connecting to "); Serial.println(ssid);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\n✅ Wi-Fi connected!");
  Serial.print("IP: "); Serial.println(WiFi.localIP());

  // --- 3️⃣ Get and print actual Wi-Fi channel ---
  wifi_second_chan_t sc;
  uint8_t currentChannel;
  esp_wifi_get_channel(&currentChannel, &sc);
  Serial.print("📶 Wi-Fi channel in use: ");
  Serial.println(currentChannel);

  // --- 4️⃣ Init ESP-NOW fresh on the same channel ---
  if (esp_now_init() != ESP_OK) {
    Serial.println("❌ ESP-NOW init failed!");
    return;
  }
  esp_wifi_set_channel(currentChannel, WIFI_SECOND_CHAN_NONE);

  // --- 5️⃣ Add Node B as peer on this channel ---
  esp_now_peer_info_t peer{};
  memcpy(peer.peer_addr, nodeBMac, 6);
  peer.channel = currentChannel;    // <-- key line
  peer.encrypt = false;
  if (esp_now_add_peer(&peer) == ESP_OK)
    Serial.println("✅ Peer added");
  else
    Serial.println("❌ Peer add failed");

  // --- 6️⃣ Genesis block ---
  Block g{0, 0, "0", "GENESIS_HASH", 0};
  blockchain.push_back(g);
  Serial.println(GREEN "✅ Blockchain initialized" RESET);
}


// ---------- Loop ----------
unsigned long lastBlock=0;
float getReading(){ return random(50,150)/1.0; }
void loop(){
  if(millis()-lastBlock>=interval){
    lastBlock=millis();
    Block b=createBlock(getReading());
    blockchain.push_back(b);
    trimBlockchain();

    Serial.printf(GREEN "\n📦 Block #%d | Dist: %.2f | Time: %s\n" RESET,
                  b.index,b.distance,fmtTime(b.timestamp).c_str());
    sendBlock(b);
  }
}
