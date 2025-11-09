#include <WiFi.h>
#include <esp_now.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "esp_wifi.h"
#include <vector>

uint8_t nodeBMac[] = {0xB0,0xA7,0x32,0x2B,0x53,0x40}; // Node B MAC
const int MAX_BLOCKS_IN_RAM = 10;
const long interval = 5000;

enum MsgType : uint8_t { NEW_BLOCK = 0, CHANNEL_SYNC = 99, SYNC_ACK = 100 };

struct Block {
  int index;
  float distance;
  String prevHash;
  String hash;
  unsigned long timestamp;
};
std::vector<Block> blockchain;

String ssid, password, serverURL;
#define GREEN "\033[32m"
#define RESET "\033[0m"

bool ackReceived = false;

// ---------- Utility ----------
String calcHash(int i,float d,String p,unsigned long t){
  return String(i)+"-"+String(d)+"-"+p+"-"+String(t);
}
Block createBlock(float d){
  Block b;
  b.index = blockchain.empty()?0:blockchain.back().index+1;
  b.distance=d; b.timestamp=millis();
  b.prevHash = blockchain.empty()?"GENESIS_HASH":blockchain.back().hash;
  b.hash = calcHash(b.index,b.distance,b.prevHash,b.timestamp);
  return b;
}
float getReading(){ return random(50,150)/1.0; }
String fmt(unsigned long ms){ unsigned long s=ms/1000; char buf[9];
  sprintf(buf,"%02lu:%02lu:%02lu",s/3600,(s%3600)/60,s%60); return String(buf);
}

// ---------- Input ----------
String readLine(const char *prompt){
  Serial.println(prompt);
  while(!Serial.available());
  String s=Serial.readStringUntil('\n'); s.trim(); return s;
}

// ---------- Offload ----------
void offloadBlock(const Block &b,String url){
  if(WiFi.status()!=WL_CONNECTED) return;
  HTTPClient http; http.begin(url); http.addHeader("Content-Type","application/json");
  StaticJsonDocument<256> d;
  d["index"]=b.index; d["distance"]=b.distance; d["prevHash"]=b.prevHash;
  d["hash"]=b.hash; d["timestamp"]=b.timestamp;
  String payload; serializeJson(d,payload);
  int code=http.POST(payload);
  Serial.printf("🌐 Offloaded #%d → %d\n",b.index,code);
  http.end();
}
void trim(){
  while(blockchain.size()>MAX_BLOCKS_IN_RAM){
    offloadBlock(blockchain.front(),serverURL);
    blockchain.erase(blockchain.begin());
  }
}

// ---------- ESP-NOW ----------
void sendPacket(uint8_t t,String p){
  String m=String(t)+"|"+p;
  esp_now_send(nodeBMac,(uint8_t*)m.c_str(),m.length()+1);
}

void onReceive(const esp_now_recv_info_t *info,const uint8_t *data,int len){
  String msg;
  for(int i=0;i<len;i++) msg+=(char)data[i];
  int sep=msg.indexOf('|'); if(sep==-1)return;
  uint8_t type=msg.substring(0,sep).toInt();
  if(type==SYNC_ACK){
    ackReceived=true;
    Serial.println(GREEN "🤝 Node B acknowledged channel sync!" RESET);
  }
}

// ---------- Setup ----------
void setup(){
  Serial.begin(115200);
  Serial.println("\n🚀 Node A starting...");

  String ip = readLine("Enter Flask server IP (e.g. 192.168.0.202):");
  serverURL = "http://" + ip + ":5000/upload_block";
  ssid = readLine("Enter Wi-Fi SSID:");
  password = readLine("Enter Wi-Fi Password:");

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid.c_str(),password.c_str());
  Serial.print("Connecting");
  int t=0;
  while(WiFi.status()!=WL_CONNECTED && t<30){ delay(500); Serial.print("."); t++; }
  Serial.println();
  if(WiFi.status()==WL_CONNECTED){
    Serial.println("✅ Wi-Fi connected");
    Serial.print("IP: "); Serial.println(WiFi.localIP());
  } else Serial.println("❌ Wi-Fi failed");

  wifi_second_chan_t sc; uint8_t ch;
  esp_wifi_get_channel(&ch,&sc);
  Serial.printf("📶 Wi-Fi channel detected: %d\n",ch);
  esp_wifi_set_promiscuous(true);
  esp_wifi_set_channel(ch,WIFI_SECOND_CHAN_NONE);
  esp_wifi_set_promiscuous(false);

  if(esp_now_init()!=ESP_OK){ Serial.println("❌ ESP-NOW init failed"); return; }
  esp_now_register_recv_cb(onReceive);
  esp_now_peer_info_t p{}; memcpy(p.peer_addr,nodeBMac,6); p.channel=ch; p.encrypt=false;
  esp_now_add_peer(&p);
  Serial.println("✅ Peer added");

  delay(1000);  // allow Node B radio to wake up

  unsigned long start = millis();
  while(!ackReceived && millis()-start < 8000){
    sendPacket(CHANNEL_SYNC,String(ch));
    Serial.printf("📡 Sent channel sync → %d\n",ch);
    delay(1000);
  }

  blockchain.push_back({0,0,"0","GENESIS_HASH",0});
  Serial.println(GREEN "✅ Blockchain initialized" RESET);
}

// ---------- Loop ----------
unsigned long last=0;
void loop(){
  if(millis()-last>=interval){
    last=millis();
    Block b=createBlock(getReading());
    blockchain.push_back(b);
    trim();
    Serial.printf(GREEN "\n📦 Block #%d | %.2f cm | %s\n" RESET,
                  b.index,b.distance,fmt(b.timestamp).c_str());
    sendPacket(NEW_BLOCK,
               String(b.index)+","+String(b.distance)+","+
               b.prevHash+","+b.hash+","+String(b.timestamp));
  }
}
