#include <WiFi.h>
#include <esp_now.h>
#include <vector>
#include <mbedtls/sha256.h>

// Replace with your followers' MAC addresses
uint8_t follower1Mac[] = {0x24, 0x6F, 0x28, 0xAA, 0xBB, 0xCC};
uint8_t follower2Mac[] = {0x24, 0x6F, 0x28, 0xDD, 0xEE, 0xFF};

// --- Ultrasonic sensor pins ---
#define TRIG_PIN 5
#define ECHO_PIN 18

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
        mbedtls_sha256_free(&sha256_ctx);

        String result = "";
        for (int i = 0; i < 32; i++) {
            if (hash[i] < 16) result += "0";   // ensure 2-digit hex
            result += String(hash[i], HEX);
        }
        result.toUpperCase();
        return result;
    }
};

std::vector<Block> blockchain;

void createGenesisBlock() {
    blockchain.push_back(Block(0, 0, "0"));
}

Block createNewBlock(int newSensorData) {
    Block lastBlock = blockchain.back();
    Block newBlock(lastBlock.index + 1, newSensorData, lastBlock.currentHash);
    blockchain.push_back(newBlock);
    return newBlock;
}

// --- SENSOR FUNCTION ---
int readUltrasonicCM() {
    digitalWrite(TRIG_PIN, LOW);
    delayMicroseconds(2);
    digitalWrite(TRIG_PIN, HIGH);
    delayMicroseconds(10);
    digitalWrite(TRIG_PIN, LOW);

    long duration = pulseIn(ECHO_PIN, HIGH, 30000); // timeout 30ms
    int distance = duration * 0.034 / 2;  // convert to cm
    if (distance <= 0 || distance > 400) return -1; // invalid
    return distance;
}

// --- ESP-NOW CALLBACK FUNCTIONS ---
void OnDataSent(const wifi_tx_info_t *tx_info, esp_now_send_status_t status) {
    Serial.print("Send Status:\t");
    Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Success" : "Fail");

    if (tx_info != nullptr) {
        char macStr[18];
        snprintf(macStr, sizeof(macStr),
                 "%02X:%02X:%02X:%02X:%02X:%02X",
                 tx_info->peer_addr[0], tx_info->peer_addr[1], tx_info->peer_addr[2],
                 tx_info->peer_addr[3], tx_info->peer_addr[4], tx_info->peer_addr[5]);
        Serial.print("Sent to MAC: ");
        Serial.println(macStr);
    }
}

void OnDataRecv(const esp_now_recv_info_t *recvInfo, const uint8_t *incomingData, int len) {
    if (len <= 0) return;

    uint8_t messageType = incomingData[0];
    if (messageType == 1) {
        Serial.println("Leader received a vote reply from follower.");
    }
}

// --- SEND HELPERS ---
void sendToFollower(uint8_t *peerMac, const uint8_t *data, size_t len) {
    esp_err_t result = esp_now_send(peerMac, data, len);
    if (result != ESP_OK) {
        Serial.println("Error sending data");
    }
}

void broadcastToFollowers(const uint8_t *data, size_t len) {
    sendToFollower(follower1Mac, data, len);
    sendToFollower(follower2Mac, data, len);
}

// --- SETUP ---
void setup() {
    Serial.begin(115200);
    delay(1000);

    pinMode(TRIG_PIN, OUTPUT);
    pinMode(ECHO_PIN, INPUT);

    WiFi.mode(WIFI_STA);

    if (esp_now_init() != ESP_OK) {
        Serial.println("Error initializing ESP-NOW");
        return;
    }
    esp_now_register_send_cb(OnDataSent);
    esp_now_register_recv_cb(OnDataRecv);

    esp_now_peer_info_t peerInfo;
    memset(&peerInfo, 0, sizeof(peerInfo));

    memcpy(peerInfo.peer_addr, follower1Mac, 6);
    peerInfo.channel = 0;
    peerInfo.encrypt = false;
    esp_now_add_peer(&peerInfo);

    memcpy(peerInfo.peer_addr, follower2Mac, 6);
    peerInfo.channel = 0;
    peerInfo.encrypt = false;
    esp_now_add_peer(&peerInfo);

    createGenesisBlock();
    Serial.println("Leader ready. Genesis block created.");
}

// --- LOOP ---
void loop() {
    static unsigned long lastSend = 0;
    if (millis() - lastSend > 5000) {  // every 5 seconds
        lastSend = millis();

        // Read ultrasonic sensor
        int sensorData = readUltrasonicCM();
        if (sensorData == -1) {
            Serial.println("Invalid ultrasonic reading, skipping block creation.");
            return;
        }

        // Create new block
        Block newBlock = createNewBlock(sensorData);

        Serial.print("Leader created block #");
        Serial.print(newBlock.index);
        Serial.print(" Data=");
        Serial.println(newBlock.sensorData);

        // Step 1: Request votes
        uint8_t voteRequest[] = {1};
        broadcastToFollowers(voteRequest, sizeof(voteRequest));
        Serial.println("Vote request sent to followers.");

        delay(500); // small wait for replies

        // Step 2: Send new block message
        uint8_t newBlockMsg[] = {2};
        broadcastToFollowers(newBlockMsg, sizeof(newBlockMsg));
        Serial.println("New block broadcasted to followers.");
    }
}