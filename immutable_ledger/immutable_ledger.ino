#include <WiFi.h> // This library is needed for the ESP32 to work properly, even without a network.
#include <vector>
#include <mbedtls/sha256.h> // The built-in SHA-256 library for ESP32

// Sensor pin definitions
#define TRIG_PIN 5
#define ECHO_PIN 18

// Variables for timing and distance
long duration;
int distanceCm;

// --- BLOCKCHAIN DATA STRUCTURES ---

// A struct to represent a single block in the blockchain
struct Block {
    int index;
    unsigned long timestamp;
    int sensorData;
    String previousHash;
    String currentHash;

    // Constructor to create a new Block object
    Block(int idx, int data, String prevHash) {
        index = idx;
        sensorData = data;
        previousHash = prevHash;
        timestamp = millis();
        currentHash = calculateHash();
    }

    // Function to compute the block's cryptographic hash
    String calculateHash() {
        // Concatenate all block data into a single string to be hashed
        String dataToHash = String(index) + String(timestamp) + String(sensorData) + previousHash;
        
        // Use mbedtls to perform the SHA-256 hash
        unsigned char hash[32]; // SHA-256 produces a 32-byte hash
        mbedtls_sha256_context sha256_ctx;
        mbedtls_sha256_init(&sha256_ctx);
        mbedtls_sha256_starts(&sha256_ctx, 0); // 0 for SHA-256
        mbedtls_sha256_update(&sha256_ctx, (const unsigned char*)dataToHash.c_str(), dataToHash.length());
        mbedtls_sha256_finish(&sha256_ctx, hash);

        // Convert the byte hash to a printable hexadecimal string
        String result = "";
        for (int i = 0; i < 32; i++) {
            result += String(hash[i], HEX);
        }
        return result;
    }
};

// A vector to hold all the blocks, forming the blockchain
std::vector<Block> blockchain;

// --- BLOCKCHAIN FUNCTIONS ---

// Function to create the first block (the "genesis block")
void createGenesisBlock() {
    blockchain.push_back(Block(0, 0, "0"));
}

// Function to add a new block to the blockchain
void addBlock(int newSensorData) {
    Block lastBlock = blockchain.back();
    Block newBlock = Block(lastBlock.index + 1, newSensorData, lastBlock.currentHash);
    blockchain.push_back(newBlock);
}

// --- MAIN PROGRAM ---

void setup() {
    Serial.begin(115200);

    // Sensor pin setup
    pinMode(TRIG_PIN, OUTPUT);
    pinMode(ECHO_PIN, INPUT);

    Serial.println("Creating the Genesis Block...");
    createGenesisBlock();
    Serial.println("Genesis Block created.");
    Serial.println();
}

void loop() {
    // --- SENSOR DATA COLLECTION ---
    digitalWrite(TRIG_PIN, LOW);
    delayMicroseconds(2);
    digitalWrite(TRIG_PIN, HIGH);
    delayMicroseconds(10);
    digitalWrite(TRIG_PIN, LOW);

    duration = pulseIn(ECHO_PIN, HIGH);
    distanceCm = duration * 0.034 / 2;

    // --- ADDING DATA TO THE BLOCKCHAIN ---
    addBlock(distanceCm);

    // --- PRINTING THE LATEST BLOCK INFO ---
    Block latestBlock = blockchain.back();
    Serial.print("New Block Added: ");
    Serial.println(latestBlock.index);
    Serial.print("  Timestamp: ");
    Serial.println(latestBlock.timestamp);
    Serial.print("  Sensor Data: ");
    Serial.print(latestBlock.sensorData);
    Serial.println(" cm");
    Serial.print("  Hash: ");
    Serial.println(latestBlock.currentHash);
    Serial.print("  Previous Hash: ");
    Serial.println(latestBlock.previousHash);
    Serial.println();

    delay(5000); // Wait 5 seconds before the next reading and block
}