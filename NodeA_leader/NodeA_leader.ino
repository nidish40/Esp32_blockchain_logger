#include <WiFi.h>
#include <esp_now.h>
#include <vector>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "esp_wifi.h"   // <-- Added

// ---------- CONFIGURE ----------
uint8_t nodeBMac[] = {0xB0,0xA7,0x32,0x2B,0x53,0x40};  // <-- Node B STA MAC
const char* ssid = "Nidish’s iPhone";
const char* password = "helloman";
const char* serverURL = "http://172.20.10.12:5000/upload_block";
const int OFFLOAD_EVERY = 10;
const long interval = 10000;
// -------------------------------

struct Block {
  int index;
  float distance;
  String prevHash;
  String hash;
  unsigned long timestamp;
};

enum MsgType : uint8_t { NEW_BLOCK=0, SYNC_REQUEST, SYNC_BLOCK };
std::vector<Block> blockchain;

// ---------- Utilities ----------
String calcHash(int i,float d,String p,unsigned long t){
  return String(i)+"-"+String(d)+"-"+p+"-"+String(t);
}
Block createBlock(float distance){
  Block b;
  b.index=blockchain.size();
  b.distance=distance;
  b.timestamp=millis();
  b.prevHash=blockchain.back().hash;
  b.hash=calcHash(b.index,b.distance,b.prevHash,b.timestamp);
  return b;
}
String fmtTime(unsigned long ms){
  unsigned long s=ms/1000,h=s/3600,m=(s%3600)/60; s%=60;
  char buf[9]; sprintf(buf,"%02lu:%02lu:%02lu",h,m,s); return String(buf);
}
#define GREEN "\033[32m"
#define RESET "\033[0m"

// ---------- ESP-NOW ----------
void sendPacket(uint8_t type,String payload){
  String msg=String(type)+"|"+payload;
  esp_err_t r=esp_now_send(nodeBMac,(uint8_t*)msg.c_str(),msg.length()+1);
  if(r==ESP_OK) Serial.println(GREEN "📤 Packet sent successfully" RESET);
  else Serial.println("⚠️ Packet send failed");
}
void sendBlock(Block &b){
  String p=String(b.index)+","+String(b.distance)+","+b.prevHash+","+b.hash+","+String(b.timestamp);
  sendPacket(NEW_BLOCK,p);
}

// ---------- Wi-Fi ----------
void connectWiFi(){
  WiFi.mode(WIFI_STA);
  esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE);  // <-- Force channel 1
  WiFi.begin(ssid, password);
  Serial.print("Connecting to Wi-Fi");
  unsigned long start=millis();
  while(WiFi.status()!=WL_CONNECTED && millis()-start<10000){
    delay(500); Serial.print(".");
  }
  if(WiFi.status()==WL_CONNECTED){
    Serial.println(" ✅");
    Serial.print("IP: "); Serial.println(WiFi.localIP());
    wifi_second_chan_t sc;
uint8_t chan;
esp_wifi_get_channel(&chan, &sc);
Serial.print("📶 Node A Wi-Fi channel: ");
Serial.println(chan);

  } else Serial.println(" ❌ (continuing offline)");
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
void offloadOld(){
  if(blockchain.size()<=OFFLOAD_EVERY) return;
  int lim=blockchain.size()-OFFLOAD_EVERY;
  for(int i=0;i<lim;i++) offloadBlock(blockchain[i]);
  blockchain.erase(blockchain.begin(),blockchain.begin()+lim);
}

// ---------- Setup ----------
void setup() {
  Serial.begin(115200);
  Serial.println("\n🚀 Node A starting...");

  // 1️⃣ Connect to Wi-Fi first
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.print("Connecting to Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println(" ✅");
  Serial.print("IP: "); Serial.println(WiFi.localIP());

  // 2️⃣ Read actual channel used by Wi-Fi
  wifi_second_chan_t sc;
  uint8_t currentChan;
  esp_wifi_get_channel(&currentChan, &sc);
  Serial.print("📶 Node A Wi-Fi channel: ");
  Serial.println(currentChan);

  // 3️⃣ Init ESP-NOW and force same channel
  if (esp_now_init() != ESP_OK) {
    Serial.println("❌ ESP-NOW init failed!");
    return;
  }
  esp_wifi_set_channel(currentChan, WIFI_SECOND_CHAN_NONE);

  // 4️⃣ Add Node B peer on that channel
  esp_now_peer_info_t peer{};
  memcpy(peer.peer_addr, nodeBMac, 6);
  peer.channel = currentChan;
  peer.encrypt = false;
  if (esp_now_add_peer(&peer) == ESP_OK)
    Serial.println("✅ Peer added");
  else
    Serial.println("❌ Peer add failed");

  // 5️⃣ Create genesis block
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
    Serial.printf(GREEN "\n📦 Created Block #%d | Dist: %.2f | Time: %s\n" RESET,
                  b.index,b.distance,fmtTime(b.timestamp).c_str());
    sendBlock(b);
    offloadOld();
  }
}
