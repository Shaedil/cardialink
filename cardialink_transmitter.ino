#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

// --- BLE service/characteristic UUIDs (Nordic UART) ---
#define SERVICE_UUID           "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID_TX "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"
#define SAMPLE_INTERVAL_MS 20
#define OVERSAMPLE_COUNT 16

// --- ECG input pin ---
#define ECG_PIN 14

BLECharacteristic* pTxCharacteristic;
bool deviceConnected = false;

// Server callbacks to track connection state
class MyServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) {
    deviceConnected = true;
    Serial.println("Client connected");
  }
  void onDisconnect(BLEServer* pServer) {
    deviceConnected = false;
    Serial.println("Client disconnected");
    // restart advertising so we can reconnect
    pServer->getAdvertising()->start();
  }
};

void setup() {
  Serial.begin(115200);
  while (!Serial);

  // initialize ADC pin
  pinMode(ECG_PIN, INPUT);

  // --- BLE setup ---
  BLEDevice::init("esp32-ecg-tx");
  BLEServer* pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  BLEService* pService = pServer->createService(SERVICE_UUID);

  // TX characteristic (notify)
  pTxCharacteristic = pService->createCharacteristic(
                        CHARACTERISTIC_UUID_TX,
                        BLECharacteristic::PROPERTY_NOTIFY
                      );
  // add descriptor so client can enable notifications
  pTxCharacteristic->addDescriptor(new BLE2902());

  pService->start();

  // advertise
  BLEAdvertising* pAdvertising = pServer->getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  pAdvertising->start();

  Serial.println("BLE ECG Transmitter is now advertising");
}

void loop() {
  static unsigned long lastSend = 0;
  unsigned long now = millis();

  // only send when enough time has elapsed
  if (deviceConnected && (now - lastSend >= SAMPLE_INTERVAL_MS)) {
    lastSend = now;
    uint32_t acc = 0;
    for (int i = 0; i < OVERSAMPLE_COUNT; ++i) {
      acc += analogRead(ECG_PIN);
    }
    uint16_t raw = acc / OVERSAMPLE_COUNT;
    uint8_t buf[2] = { uint8_t(raw & 0xFF), uint8_t(raw >> 8) };
    pTxCharacteristic->setValue(buf, 2);
    pTxCharacteristic->notify();

    Serial.println(raw);
  }

  // tiny yield
  delay(1);
}
