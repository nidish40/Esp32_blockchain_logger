#include <WiFi.h>
#include <esp_now.h>
#include <vector>
#include "esp_wifi.h"

struct Block{
  int index; float distance; String prevHash; String hash; unsigned long timestamp;
};
std::vector<Block> blockchain;
const int MAX_BLOCKS_IN_RAM = 10;

uint8_t nodeAMac[] = {0xF4,0x65,0x0B,0xE8,0x46,0x5C}; // Node A STA MAC

// -------------- Display --------------
void printBlockchain(){
  Serial.println("\n--- Node B Blockchain (in RAM) ---");
  for(Block &b:blockchain){
    Serial.printf("#%d | %.2f cm | %lu ms\n",b.index,b.distance,b.timestamp);
  }
  Serial.printf("(Total: %d blocks)\n", (int)blockchain.size());
}

// -------------- ESP-NOW --------------
void onReceive(const esp_now_recv_info_t *info,const uint8_t *data,int len){
  String msg; for(int i=0;i<len;i++) msg+=(char)data[i];
  int sep=msg.indexOf('|'); if(sep==-1)return;
  String payload=msg.substring(sep+1);
  int a=payload.indexOf(','),b=payload.indexOf(',',a+1);
  int c=payload.indexOf(',',b+1),d=payload.indexOf(',',c+1);
  Block blk;
  blk.index=payload.substring(0,a).toInt();
  blk.distance=payload.substring(a+1,b).toFloat();
  blk.prevHash=payload.substring(b+1,c);
  blk.hash=payload.substring(c+1,d);
  blk.timestamp=payload.substring(d+1).toInt();

  blockchain.push_back(blk);
  if(blockchain.size()>MAX_BLOCKS_IN_RAM)
    blockchain.erase(blockchain.begin());

  Serial.printf("📥 Received Block #%d\n",blk.index);
  printBlockchain();
}

void setup(){
  Serial.begin(115200);
  Serial.println("\n🚀 Node B starting...");
  WiFi.mode(WIFI_STA);

  // Use same channel as Node A
  uint8_t targetChannel=6;
  esp_wifi_set_channel(targetChannel,WIFI_SECOND_CHAN_NONE);

  if(esp_now_init()!=ESP_OK){ Serial.println("❌ ESP-NOW init failed!"); return; }
  esp_now_register_recv_cb(onReceive);

  esp_now_peer_info_t peer{};
  memcpy(peer.peer_addr,nodeAMac,6);
  peer.channel=targetChannel; peer.encrypt=false;
  if(esp_now_add_peer(&peer)==ESP_OK) Serial.println("✅ Peer added");

  Serial.println("📡 Node B ready and listening...");
}

void loop(){}
