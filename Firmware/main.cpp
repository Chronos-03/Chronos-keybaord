#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <BleKeyboard.h>

enum HalfSide { SIDE_LEFT, SIDE_RIGHT };
constexpr HalfSide CONFIGURED_SIDE = SIDE_LEFT; 


BleKeyboard bleKeyboard("ESP32-S3 Split KB", "Custom", 100);

constexpr uint8_t NUM_ROWS = 4;
constexpr uint8_t NUM_COLS = 6;
constexpr uint8_t rowPins[NUM_ROWS] = {4, 5, 6, 7};
constexpr uint8_t colPins[NUM_COLS] = {15, 16, 17, 18, 21, 38};


constexpr char testKeymap[NUM_ROWS][NUM_COLS] = {
    {'1', '2', '3', '4', '5', '6'},
    {'q', 'w', 'e', 'r', 't', 'y'},
    {'a', 's', 'd', 'f', 'g', 'h'},
    {' ', ' ', ' ', 'z', 'x', 'c'}
};

struct Packet {
    uint8_t row;
    uint8_t col;
    bool pressed;
};

uint8_t broadcastAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
bool keyStates[NUM_ROWS][NUM_COLS] = {false};

void OnDataRecv(const uint8_t *mac, const uint8_t *incomingData, int len) {
    if (CONFIGURED_SIDE == SIDE_LEFT && len == sizeof(Packet)) {
        Packet pkt;
        memcpy(&pkt, incomingData, sizeof(pkt));
        
        char key = testKeymap[pkt.row][pkt.col];
        if (bleKeyboard.isConnected()) {
            if (pkt.pressed) bleKeyboard.press(key);
            else bleKeyboard.release(key);
        }
    }
}

void setup() {
    Serial.begin(115200);

    
    for (uint8_t r = 0; r < NUM_ROWS; r++) {
        pinMode(rowPins[r], OUTPUT);
        digitalWrite(rowPins[r], HIGH);
    }
    for (uint8_t c = 0; c < NUM_COLS; c++) {
        pinMode(colPins[c], INPUT_PULLUP);
    }

    
    WiFi.mode(WIFI_STA);
    if (esp_now_init() == ESP_OK) {
        esp_now_register_recv_cb(OnDataRecv);
        esp_now_peer_info_t peerInfo = {};
        memcpy(peerInfo.peer_addr, broadcastAddress, 6);
        peerInfo.channel = 0;
        peerInfo.encrypt = false;
        esp_now_add_peer(&peerInfo);
    }

    if (CONFIGURED_SIDE == SIDE_LEFT) {
        bleKeyboard.begin();
    }
}

void loop() {
    for (uint8_t r = 0; r < NUM_ROWS; r++) {
        digitalWrite(rowPins[r], LOW);
        delayMicroseconds(30);

        for (uint8_t c = 0; c < NUM_COLS; c++) {
            if (r == 3 && c < 3) continue;

            bool pressed = (digitalRead(colPins[c]) == LOW);
            if (pressed != keyStates[r][c]) {
                keyStates[r][c] = pressed;

                if (CONFIGURED_SIDE == SIDE_LEFT) {
                    char key = testKeymap[r][c];
                    if (bleKeyboard.isConnected()) {
                        if (pressed) bleKeyboard.press(key);
                        else bleKeyboard.release(key);
                    }
                } else {
                    Packet pkt = {r, c, pressed};
                    esp_now_send(broadcastAddress, (uint8_t *)&pkt, sizeof(pkt));
                }
            }
        }
        digitalWrite(rowPins[r], HIGH);
    }
    delay(10);
}