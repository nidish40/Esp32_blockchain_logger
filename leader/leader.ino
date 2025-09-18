#include <WiFi.h>
#include <esp_now.h>
#include <vector>
#include <mbedtls/sha256.h>

// Sensor pin definitions
#define TRIG_PIN 5
#define ECHO_PIN 18

// Variables for timing and distance
long duration;
int distanceCm;

// MAC addresses of the two follower nodes
uint8_t follower1MacAddress[] = {0xF4, 0x65, 0x0B, 0xE8, 0x31, 0x60};
uint8_t follower2MacAddress[] = {0xB0, 0xA7, 0x32, 0x2B, 0x53, 0x40};

// A boolean to track if a vote has been received
bool voteReceived = false;

// --- BLOCKCHAIN DATA STRUCTURES ---
struct Block {
    int index;
    unsigned long timestamp;
    int sensorData;
    String previousHash;
    String currentHash;

    Block(int idx, int data, String prevHash) {
        index = idx;
        sensorData = data;
        previousHash = prevHash;
        timestamp = millis();
        currentHash = calculateHash();
    }

    String calculateHash() {
        String dataToHash = String(index) + String(timestamp) + String(sensorData) + previousHash;
        unsigned char hash[32];
        mbedtls_sha256_context sha256_ctx;
        mbedtls_sha256_init(&sha256_ctx);
        mbedtls_sha256_starts(&sha256_ctx, 0);
        mbedtls_sha256_update(&sha256_ctx, (const unsigned char*)dataToHash.c_str(), dataToHash.length());
        mbedtls_sha256_finish(&sha256_ctx, hash);

        String result = "";
        for (int i = 0; i < 32; i++) {
            result += String(hash[i], HEX);
        }
        return result;
    }
};

std::vector<Block> blockchain;

void createGenesisBlock() {
    blockchain.push_back(Block(0, 0, "0"));
}

void addBlock(int newSensorData) {
    Block lastBlock = blockchain.back();
    Block newBlock = Block(lastBlock.index + 1, newSensorData, lastBlock.currentHash);
    blockchain.push_back(newBlock);
}

// --- CORRECTED ESP-NOW CALLBACK FUNCTIONS ---
void OnDataSent(const wifi_tx_info_t *tx_info, esp_now_send_status_t status) {
  Serial.print("Last Packet Send Status:\t");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
}

void OnDataRecv(const esp_now_recv_info_t *recv_info, const uint8_t *incomingData, int len) {
  voteReceived = true;
  Serial.println("Received a vote from a follower.");
}

void setup() {
    Serial.begin(115200);
    pinMode(TRIG_PIN, OUTPUT);
    pinMode(ECHO_PIN, INPUT);
    
    WiFi.mode(WIFI_STA);
    // Explicitly set the Wi-Fi channel for consistent ESP-NOW communication
    WiFi.softAP("any_ssid", "any_password", 1); 

    if (esp_now_init() != ESP_OK) {
        Serial.println("Error initializing ESP-NOW");
        return;
    }
    esp_now_register_send_cb(OnDataSent);
    esp_now_register_recv_cb(OnDataRecv);

    esp_now_peer_info_t peerInfo;
    peerInfo.channel = 0;
    peerInfo.encrypt = false;
    
    memcpy(peerInfo.peer_addr, follower1MacAddress, 6);
    if (esp_now_add_peer(&peerInfo) != ESP_OK) {
        Serial.println("Failed to add follower 1");
    }
    memcpy(peerInfo.peer_addr, follower2MacAddress, 6);
    if (esp_now_add_peer(&peerInfo) != ESP_OK) {
        Serial.println("Failed to add follower 2");
    }

    createGenesisBlock();
}

void loop() {
    digitalWrite(TRIG_PIN, LOW);
    delayMicroseconds(2);
    digitalWrite(TRIG_PIN, HIGH);
    delayMicroseconds(10);
    digitalWrite(TRIG_PIN, LOW);
    duration = pulseIn(ECHO_PIN, HIGH);
    distanceCm = duration * 0.034 / 2;

    Serial.print("Sensor reading: ");
    Serial.println(distanceCm);
    
    voteReceived = false;
    
    uint8_t requestMessage[] = {1};
    esp_now_send(follower1MacAddress, requestMessage, sizeof(requestMessage));
    esp_now_send(follower2MacAddress, requestMessage, sizeof(requestMessage));
    Serial.println("Broadcasting request for votes...");

    unsigned long timeout = millis() + 2000;
    while (!voteReceived && millis() < timeout) {
    }

    if (voteReceived) {
        Serial.println("Vote received. Creating new block...");
        addBlock(distanceCm);
        
        Block latestBlock = blockchain.back();
        Serial.print("New Block Added: ");
        Serial.println(latestBlock.index);
        Serial.print("  Sensor Data: ");
        Serial.print(latestBlock.sensorData);
        Serial.println(" cm");
        
        uint8_t blockMessage[] = {2};
        esp_now_send(follower1MacAddress, blockMessage, sizeof(blockMessage));
        esp_now_send(follower2MacAddress, blockMessage, sizeof(blockMessage));
        
        Serial.println("New block broadcasted to followers.");
        Serial.println("------------------------------------");
    } else {
        Serial.println("No vote received. Skipping block creation.");
        Serial.println("------------------------------------");
    }
    
    delay(5000);
}