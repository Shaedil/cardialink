/*
 * BLE ECG Receiver (Central/Client) for SparkFun ESP32 Thing Plus
 * Reconstructs and displays ECG signal sent over BLE.
 */

#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>

// --- Configuration ---
#define SERVICE_UUID           "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID_TX "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"
#define TARGET_DEVICE_NAME     "esp32-ecg-tx"

static BLEScan* pBLEScan;
static BLEAdvertisedDevice* myDevice = nullptr;
static BLEClient* pClient = nullptr;
static BLERemoteService* pRemoteService = nullptr;
static BLERemoteCharacteristic* pRemoteTxCharacteristic = nullptr;
static boolean doConnect = false;
static boolean connected = false;
static boolean isScanning = false;

static float lastEcgValue = 0.0f;
static unsigned long lastPrintTime = 0;

// --- Callback when new data arrives ---
static void notifyCallback(
  BLERemoteCharacteristic* pBLERemoteCharacteristic,
  uint8_t* pData,
  size_t length,
  bool isNotify) {
    if (length == 2) {  // Expect 2 bytes: uint16_t ADC value
        uint16_t rawValue = (pData[1] << 8) | pData[0];  // Little-endian reconstruction
        lastEcgValue = (float)rawValue;  // Store for plotting
    }
}

// --- BLE Client Callback ---
class MyClientCallback : public BLEClientCallbacks {
	void onConnect(BLEClient* pclient) {
    connected = true;
    if (isScanning) { pBLEScan->stop(); isScanning = false; }
	}
	void onDisconnect(BLEClient* pclient) {
    connected = false;
    doConnect = false;
    pClient = nullptr;
    pRemoteService = nullptr;
    pRemoteTxCharacteristic = nullptr;
    if (myDevice != nullptr) { delete myDevice; myDevice = nullptr; }
	}
};

// --- BLE Scan Callback ---
class MyAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice advertisedDevice) {
    if (!doConnect && advertisedDevice.getName() == TARGET_DEVICE_NAME) {
        myDevice = new BLEAdvertisedDevice(advertisedDevice);
        doConnect = true;
        if (isScanning) { pBLEScan->stop(); isScanning = false; }
    }
  }
};

// --- Connect to BLE server ---
bool connectToServer() {
    if (!myDevice) return false;
    if (pClient != nullptr) { delete pClient; pClient = nullptr; }

    pClient = BLEDevice::createClient();
    pClient->setClientCallbacks(new MyClientCallback());

    if (!pClient->connect(myDevice)) {
        delete pClient; pClient = nullptr; return false;
    }

    try { pRemoteService = pClient->getService(SERVICE_UUID); } catch (...) { pRemoteService = nullptr; }
    if (pRemoteService == nullptr) { pClient->disconnect(); return false; }

    pRemoteTxCharacteristic = pRemoteService->getCharacteristic(CHARACTERISTIC_UUID_TX);
    if (pRemoteTxCharacteristic == nullptr) { pClient->disconnect(); return false; }

    if(pRemoteTxCharacteristic->canNotify()) {
      pRemoteTxCharacteristic->registerForNotify(notifyCallback, true);
      BLERemoteDescriptor* pDesc = pRemoteTxCharacteristic->getDescriptor(BLEUUID((uint16_t)0x2902));
      if (pDesc != nullptr) {
          uint8_t notifyOn[] = {0x01, 0x00};
          pDesc->writeValue(notifyOn, 2, true);
      }
    }
    return true;
}

// --- Start BLE scan ---
void startScan() {
  if (!connected && !isScanning) {
    doConnect = false;
    pBLEScan->clearResults();
    pBLEScan->start(5, false);
    isScanning = true;
  }
}

void setup() {
  Serial.begin(115200);
  while (!Serial);

  Serial.println("Starting BLE ECG Receiver...");

  BLEDevice::init("");
  pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks(), true);
  pBLEScan->setActiveScan(true);
  pBLEScan->setInterval(100);
  pBLEScan->setWindow(99);

  startScan();
}

void loop() {
  if (doConnect) {
    if (!connectToServer()) {
      doConnect = false;
      if (myDevice) { delete myDevice; myDevice = nullptr; }
    }
    doConnect = false;
  }
  
  if (!connected && !isScanning && !doConnect) { 
    startScan(); 
  }

  if (connected) {
    unsigned long now = millis();
    if (now - lastPrintTime > 5) {   // Print every ~5ms (fast plot)
        Serial.println(lastEcgValue, 4); // Output for Arduino Serial Plotter
        lastPrintTime = now;
    }
  }

  delay(1);
}