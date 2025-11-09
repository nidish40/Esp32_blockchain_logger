#include <WiFi.h>
#include <esp_now.h>
#include "esp_wifi.h"
#include <vector>

enum MsgType : uint8_t { NEW_BLOCK = 0, CHANNEL_SYNC = 99, SYNC_ACK = 100 };

struct Block { int index; float distance; String prevHash; String hash; unsigned long timestamp; };
std::vector<Block> blockchain;
const int MAX_BLOCKS_IN_RAM = 10;
bool syncComplete=false;

uint8_t nodeAMac[] = {0xF4,0x65,0x0B,0xE8,0x46,0x5C};   // Node A MAC
uint8_t currentChannel = 1;

void printBlocks(){
  Serial.println("\n--- Node B Blocks (in RAM) ---");
  for(Block &b:blockchain)
    Serial.printf("#%d | %.2f cm | %lu ms\n",b.index,b.distance,b.timestamp);
  Serial.printf("(Total %d)\n",(int)blockchain.size());
}

void sendAck(){
  String m=String(SYNC_ACK)+"|ACK";
  esp_now_send(nodeAMac,(uint8_t*)m.c_str(),m.length()+1);
}

void onReceive(const esp_now_recv_info_t *info,const uint8_t *data,int len){
  String msg; for(int i=0;i<len;i++) msg+=(char)data[i];
  int sep=msg.indexOf('|'); if(sep==-1)return;
  uint8_t type=msg.substring(0,sep).toInt();
  String payload=msg.substring(sep+1);

  if(type==CHANNEL_SYNC){
    uint8_t newCh=payload.toInt();
    if(newCh!=currentChannel){
      currentChannel=newCh;
      esp_wifi_set_channel(currentChannel,WIFI_SECOND_CHAN_NONE);
      Serial.printf("🔄 Synced to Node A channel → %d\n",currentChannel);
      sendAck();
      Serial.println("🤝 Handshake sent – ready for blocks");
    }
    return;
  }

  if(type==NEW_BLOCK){
    int a=payload.indexOf(','),b=payload.indexOf(',',a+1);
    int c=payload.indexOf(',',b+1),d=payload.indexOf(',',c+1);
    Block blk;
    blk.index=payload.substring(0,a).toInt();
    blk.distance=payload.substring(a+1,b).toFloat();
    blk.prevHash=payload.substring(b+1,c);
    blk.hash=payload.substring(c+1,d);
    blk.timestamp=payload.substring(d+1).toInt();
    blockchain.push_back(blk);
    if(blockchain.size()>MAX_BLOCKS_IN_RAM) blockchain.erase(blockchain.begin());
    if(!syncComplete){ syncComplete=true; Serial.println("✅ Sync complete – receiving blocks"); }
    Serial.printf("📥 Received Block #%d\n",blk.index);
    printBlocks();
  }
}

void setup(){
  Serial.begin(115200);
  Serial.println("\n🚀 Node B starting...");
  WiFi.mode(WIFI_STA);
  esp_wifi_set_channel(currentChannel,WIFI_SECOND_CHAN_NONE);

  wifi_second_chan_t sc; uint8_t ch;
  esp_wifi_get_channel(&ch,&sc);
  Serial.printf("📶 Starting on channel %d\n",ch);

  delay(1000);  // allow radio to wake up

  if(esp_now_init()!=ESP_OK){ Serial.println("❌ ESP-NOW init failed"); return; }
  esp_now_register_recv_cb(onReceive);

  esp_now_peer_info_t p{};
  memcpy(p.peer_addr,nodeAMac,6);
  p.channel=currentChannel;
  p.encrypt=false;
  esp_now_add_peer(&p);

  Serial.println("📡 Listening for channel sync from Node A...");
}

void loop(){}
