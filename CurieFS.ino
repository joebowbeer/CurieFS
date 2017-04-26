// BLE MIDI foot switch for Android 101

#include <CurieBLE.h>

#define LED_PIN LED_BUILTIN
#define LED_ACTIVE HIGH

// TRS jack breakout connected to pins 7..2
#define TIP_PIN 7
#define RNG_PIN 5
#define SLV_PIN 3

#define CHANNEL 10

#define CONTROLCHANGE 0xB0
#define RNG_CONTROLLER 81
#define TIP_CONTROLLER 80

#define BLE_PACKET_SIZE 20

// BLE MIDI
BLEService midiSvc("03B80E5A-EDE8-4B33-A751-6CE34EC4C700");
BLECharacteristic midiChar("7772E5DB-3868-4112-A1A9-F2669D106BF3",
    BLEWrite | BLEWriteWithoutResponse | BLENotify | BLERead, BLE_PACKET_SIZE);

bool connected;
int rngInitState;
int rngLastState;
int tipInitState;
int tipLastState;

uint8_t midiData[BLE_PACKET_SIZE];
int byteOffset = 0;
uint8_t lastStatus;
uint32_t lastTime;

void setup() {
  // Configure tip and ring pins as digital input with pull-up resistor
  pinMode(RNG_PIN, INPUT_PULLUP);
  pinMode(TIP_PIN, INPUT_PULLUP);

  // Configure sleave pin as ground
  pinMode(SLV_PIN, OUTPUT);
  digitalWrite(SLV_PIN, LOW);

  connected = false;

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LED_ACTIVE);

  setupBle();
}

void loop() {
  BLE.poll();
  // connection
  if (connected != !!BLE.central()) {
    connected = !connected;
    if (connected) {
      // Read initial pin states
      rngInitState = digitalRead(RNG_PIN);
      rngLastState = rngInitState;
      tipInitState = digitalRead(TIP_PIN);
      tipLastState = tipInitState;
      digitalWrite(LED_PIN, !LED_ACTIVE);
    } else {
      digitalWrite(LED_PIN, LED_ACTIVE);
    }
  }
  if (!connected) {
    return;
  }
  // ring
  int rngState = digitalRead(RNG_PIN);
  if (rngState != rngLastState) {
    rngLastState = rngState;
    if (rngState != rngInitState) {
      digitalWrite(LED_PIN, LED_ACTIVE);
      sendMessage(CONTROLCHANGE + CHANNEL, RNG_CONTROLLER, 127);
    } else {
      digitalWrite(LED_PIN, !LED_ACTIVE);
      sendMessage(CONTROLCHANGE + CHANNEL, RNG_CONTROLLER, 0);
    }
    delay(5); // debounce
  }
  // tip
  int tipState = digitalRead(TIP_PIN);
  if (tipState != tipLastState) {
    tipLastState = tipState;
    if (tipState != tipInitState) {
      digitalWrite(LED_PIN, LED_ACTIVE);
      sendMessage(CONTROLCHANGE + CHANNEL, TIP_CONTROLLER, 127);
    } else {
      digitalWrite(LED_PIN, !LED_ACTIVE);
      sendMessage(CONTROLCHANGE + CHANNEL, TIP_CONTROLLER, 0);
    }
    delay(5); // debounce
  }
}

void setupBle() {
  BLE.begin();

  BLE.setConnectionInterval(6, 12); // 7.5 to 15 millis

  // set the local name peripheral advertises
  BLE.setLocalName("FS101");

  // set the UUID for the service this peripheral advertises
  BLE.setAdvertisedServiceUuid(midiSvc.uuid());

  // add service and characteristic
  BLE.addService(midiSvc);
  midiSvc.addCharacteristic(midiChar);

  // set an initial value for the characteristic
  sendMessage(0, 0, 0);

  BLE.advertise();
}

boolean sendMessage(uint8_t status, uint8_t byte1, uint8_t byte2) {
  return loadMessage(status, byte1, byte2) && sendMessages();
}

boolean loadMessage(uint8_t status, uint8_t byte1, uint8_t byte2) {
  // Assert BLE_PACKET_SIZE > 4
  if (byteOffset > BLE_PACKET_SIZE - 4) return false;
  uint32_t timestamp = millis();
  boolean empty = byteOffset == 0;
  if (empty) {
    uint8_t headTs = timestamp >> 7;
    headTs |= 1 << 7;  // set the 7th bit
    headTs &= ~(1 << 6);  // clear the 6th bit
    midiData[byteOffset++] = headTs;
  }
  if (empty || lastStatus != status || lastTime != timestamp) {
    uint8_t msgTs = timestamp;
    msgTs |= 1 << 7;  // set the 7th bit
    midiData[byteOffset++] = msgTs;
    midiData[byteOffset++] = status;
    midiData[byteOffset++] = byte1;
    midiData[byteOffset++] = byte2;
    lastStatus = status;
    lastTime = timestamp;
  } else {
    midiData[byteOffset++] = byte1;
    midiData[byteOffset++] = byte2;
  }
  return true;
}

boolean sendMessages() {
  if (byteOffset != 0) {
    midiChar.setValue(midiData, byteOffset);
    byteOffset = 0;
    return true;
  }
  return false;
}
