/*
 * ===========================================================================================
 *  PROJECT: SECURE LIGHTWEIGHT UART COMMUNICATION
 *  NODE: ATTACKER (Dual-Line Sniffer)
 *  
 *  DESCRIPTION:
 *  This node acts as a passive eavesdropper. It uses Hardware Serial to listen 
 *  to Alice's line and SoftwareSerial to listen to Bob's line. It intercepts 
 *  the public keys and encrypted data, but cannot derive the secret or read 
 *  the sensor data.
 * 
 *  HARDWARE SETUP:
 *  - ATmega328P
 *  - LCD 16x2 (RS=12, EN=11, D4=5, D5=4, D6=3, D7=2)
 *  - RX Pin (0): Connected to Alice's TX line
 *  - Pin 6 (SoftRX): Connected to Bob's TX line
 * ===========================================================================================
 */

#include <LiquidCrystal.h>
#include <SoftwareSerial.h>

// Use Pin 6 for Software RX (to listen to Bob)
SoftwareSerial bobSniffer(6, 7); 
LiquidCrystal lcd(12, 11, 5, 4, 3, 2);

uint8_t alicePK = 0, bobPK = 0;
bool aliceCaught = false, bobCaught = false;

// Buffer for intercepted ciphertext
uint8_t cipherBuf[8];
uint8_t cipherIdx = 0;
bool inFrame = false;

void setup() {
  Serial.begin(9600);    // Alice sniffer
  bobSniffer.begin(9600); // Bob sniffer
  
  lcd.begin(16, 2);
  lcd.print("ATTACKER ACTIVE");
  lcd.setCursor(0, 1);
  lcd.print("Sniffing lines..");
  delay(1000);
}

void loop() {
  // ── LISTEN TO ALICE (Hardware RX) ──────────────────────────────────
  if (Serial.available()) {
    uint8_t b = Serial.read();
    
    // Handshake packet from Alice (0xD1)
    if (b == 0xD1) {
      while (!Serial.available());
      alicePK = Serial.read();
      aliceCaught = true;
      updateDisplay();
    }
    // Encrypted data packet marker
    else if (!inFrame && b == 0xAA) {
      inFrame = true;
      cipherIdx = 0;
    }
    else if (inFrame) {
      if (cipherIdx < 8) {
        cipherBuf[cipherIdx++] = b;
      } else {
        if (b == 0x55) {
          showInterceptedData();
        }
        inFrame = false;
      }
    }
  }

  // ── LISTEN TO BOB (Software RX) ────────────────────────────────────
  if (bobSniffer.available()) {
    uint8_t b = bobSniffer.read();
    
    // Handshake packet from Bob (0xD2)
    if (b == 0xD2) {
      while (!bobSniffer.available());
      bobPK = bobSniffer.read();
      bobCaught = true;
      updateDisplay();
    }
  }
}

/**
 * Display the raw encrypted bytes in Hex format.
 */
void showInterceptedData() {
  lcd.clear();
  lcd.print("Alice Enc Data:");
  lcd.setCursor(0, 1);
  
  // Format each byte as two hex digits
  for (uint8_t i = 0; i < 8; i++) {
    if (cipherBuf[i] < 0x10) lcd.print('0'); // Leading zero
    lcd.print(cipherBuf[i], HEX);
  }
}

/**
 * Display the intercepted public keys.
 */
void updateDisplay() {
  lcd.clear();
  lcd.print("A_PK:"); 
  if (aliceCaught) lcd.print(alicePK); else lcd.print("??");
  
  lcd.setCursor(8, 0);
  lcd.print("B_PK:"); 
  if (bobCaught) lcd.print(bobPK); else lcd.print("??");
  
  lcd.setCursor(0, 1);
  if (aliceCaught && bobCaught) {
    lcd.print("KEY EXCH CAUGHT!");
  } else {
    lcd.print("Waiting for DH..");
  }
}
