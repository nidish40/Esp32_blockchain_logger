#include <WiFi.h>
#include <esp_now.h>
#include <vector>
#include <mbedtls/sha256.h>

// MAC address of the leader node
uint8_t leaderMacAddress[] = {0xF4, 0x65, 0x0B, 0xE8, 0x46, 0x5C};

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
    uint8_t messageType = incomingData[0];
    
    if (messageType == 1) {
        Serial.println("Received request for vote from leader.");
        uint8_t reply[] = {1};
        esp_now_send(leaderMacAddress, reply, sizeof(reply));
    } else if (messageType == 2) {
        Serial.println("Received new block from leader.");
        addBlock(0);
        Serial.print("New block added to my ledger. My ledger size is: ");
        Serial.println(blockchain.size());
    }
}

void setup() {
    Serial.begin(115200);
    
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
    memcpy(peerInfo.peer_addr, leaderMacAddress, 6);
    if (esp_now_add_peer(&peerInfo) != ESP_OK) {
        Serial.println("Failed to add leader peer");
    }

    createGenesisBlock();
    Serial.println("Ready to receive blocks from leader.");
}

void loop() {
    delay(100);
}