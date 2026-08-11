/*
 * ===========================================================================================
 *  PROJECT: SECURE LIGHTWEIGHT UART COMMUNICATION
 *  NODE: BOB (Receiver & Decryption Node)
 *  
 *  DESCRIPTION:
 *  This node listens for Alice's public key, responds with its own, and derives
 *   the same shared secret. It then waits for encrypted sensor data, decrypts 
 *   it using Speck 32/64, and displays the result on a 16x2 LCD.
 * 
 *  HARDWARE SETUP:
 *  - ATmega328P (Arduino Uno/Nano)
 *  - LCD 16x2 (Hitachi HD44780 compatible)
 *    Pins: RS=12, EN=11, D4=5, D5=4, D6=3, D7=2
 *  - Entropy Source: Analog Pin A0 (Floating)
 * ===========================================================================================
 */

#include <LiquidCrystal.h>

// Initialize LCD in 4-bit mode
LiquidCrystal lcd(12, 11, 5, 4, 3, 2);

// ── DH-LITE KEY EXCHANGE PARAMETERS ────────────────────────────────────────────────────────
const uint8_t DH_P = 251; // Prime modulus
const uint8_t DH_G = 6;   // Primitive root generator

uint8_t myPrivate;    // Bob's secret private key (b)
uint8_t myPublic;     // Bob's public key (B = g^b mod p)
uint8_t peerPublic;   // Alice's public key (A)
uint8_t sharedSecret; // Final shared key (K = A^b mod p)

/**
 * Fast Modular Exponentiation: (base^exp) % mod
 */
uint8_t dh_lite_modpow(uint8_t base, uint8_t exp, uint8_t mod) {
  uint16_t result = 1;
  uint16_t b = base % mod;
  while (exp > 0) {
    if (exp & 1) result = (result * b) % mod;
    exp >>= 1;
    b = (b * b) % mod;
  }
  return (uint8_t)result;
}

// ── SPECK 32/64 (DECRYPTION LOGIC) ─────────────────────────────────────────────────────────
#define SPECK_ROUNDS 22
uint16_t roundKeys[SPECK_ROUNDS];

#define ROR16(x, r) (((x) >> (r)) | ((x) << (16 - (r))))
#define ROL16(x, r) (((x) << (r)) | ((x) >> (16 - (r))))

/**
 * Key Schedule: Same as Alice. Expands the 64-bit key into 22 rounds.
 */
void speck_key_schedule(const uint16_t K[4]) {
  uint16_t l[3] = { K[1], K[2], K[3] };
  roundKeys[0] = K[0];
  for (uint8_t i = 0; i < SPECK_ROUNDS - 1; i++) {
    l[i % 3] = (ROR16(l[i % 3], 7) + roundKeys[i]) ^ i;
    roundKeys[i + 1] = ROL16(roundKeys[i], 2) ^ l[i % 3];
  }
}

/**
 * Decrypt a single 32-bit block.
 * Notice: This performs the operations in reverse order (ROR -> Subtraction -> ROR).
 */
void speck_decrypt(uint16_t* x, uint16_t* y) {
  for (int8_t i = SPECK_ROUNDS - 1; i >= 0; i--) {
    // Reverse the y round function
    *y = ROR16(*y ^ *x, 2);
    // Reverse the x round function
    *x = ROL16((*x ^ roundKeys[i]) - *y, 7);
  }
}

/**
 * Build the 64-bit key from the 8-bit shared secret.
 */
void buildSpeckKey(uint8_t secret) {
  uint16_t K[4];
  K[0] = (uint16_t)secret | ((uint16_t)secret << 8);
  K[1] = K[0] ^ 0xAAAA;
  K[2] = K[0] ^ 0x5555;
  K[3] = K[0] ^ 0x0F0F;
  speck_key_schedule(K);
}

/**
 * Helper to update both lines of the LCD.
 */
void lcdShow(const char* l1, const char* l2) {
  lcd.clear(); 
  lcd.print(l1); 
  lcd.setCursor(0, 1); 
  lcd.print(l2);
}

// ── STATE MACHINE ──────────────────────────────────────────────────────────────────────────
enum Phase {
  PH_BOOT,       // Initial delay
  PH_KEYGEN,     // Generate public key
  PH_WAIT_ALICE, // Listen for Alice's 0xD1 packet
  PH_SEND_PK,    // Send own 0xD2 packet
  PH_DERIVE,     // Calculate secret and setup Speck
  PH_SUCCESS,    // Visual confirmation
  PH_IDLE        // Wait for encrypted sensor frames
};

Phase phase = PH_BOOT;
unsigned long timer = 0;
uint8_t msgNum = 0;

// Buffer for the incoming 8-byte ciphertext payload
uint8_t frameBuf[8];
uint8_t frameIdx = 0;
bool inFrame = false;

void setup() {
  Serial.begin(9600);
  lcd.begin(16, 2);
  lcdShow("  SECURE UART", " Speck-Light 32");
  
  randomSeed(analogRead(A0));
  myPrivate = random(2, 250); // Bob's secret 'b'
  timer = millis();
}

void loop() {
  switch (phase) {
    case PH_BOOT:
      if (millis() - timer >= 1000) phase = PH_KEYGEN;
      break;

    case PH_KEYGEN:
      // B = g^b mod p
      myPublic = dh_lite_modpow(DH_G, myPrivate, DH_P);
      lcd.clear();
      lcd.print("My PK: "); lcd.print(myPublic);
      lcd.setCursor(0, 1); lcd.print("Waiting Alice..");
      phase = PH_WAIT_ALICE;
      break;

    case PH_WAIT_ALICE:
      // Scan for Alice's handshake
      if (Serial.available() >= 2) {
        if (Serial.peek() == 0xD1) { // Alice's PK header
          Serial.read();
          peerPublic = Serial.read();
          phase = PH_SEND_PK;
          timer = millis();
        } else {
          Serial.read(); // Skip irrelevant bytes
        }
      }
      break;

    case PH_SEND_PK:
      // Reply with Bob's PK
      if (millis() - timer >= 1000) {
        Serial.write(0xD2);      // Header for Bob's Public Key
        Serial.write(myPublic);
        phase = PH_DERIVE;
        timer = millis();
      }
      break;

    case PH_DERIVE:
      // K = A^b mod p
      sharedSecret = dh_lite_modpow(peerPublic, myPrivate, DH_P);
      buildSpeckKey(sharedSecret);
      lcdShow("Shared Secret:", String(sharedSecret).c_str());
      timer = millis();
      phase = PH_SUCCESS;
      break;

    case PH_SUCCESS:
      if (millis() - timer >= 1500) {
        lcdShow("SECURE LINK", "Ready...");
        phase = PH_IDLE;
      }
      break;

    case PH_IDLE:
      // FRAME PARSER
      while (Serial.available()) {
        uint8_t b = Serial.read();
        
        // Look for start marker
        if (!inFrame && b == 0xAA) {
          inFrame = true;
          frameIdx = 0;
          continue;
        }
        
        if (inFrame) {
          if (frameIdx < 8) {
            frameBuf[frameIdx++] = b; // Store ciphertext bytes
          } else {
            // Check for end marker
            if (b == 0x55) {
              decryptAndDisplay();
            }
            inFrame = false; // Reset for next frame
          }
        }
      }
      break;
  }
  delay(5);
}

/**
 * Decrypt the two blocks and show on LCD.
 */
void decryptAndDisplay() {
  char plain[9];
  
  for (uint8_t b = 0; b < 2; b++) {
    uint16_t x, y;
    // Extract words from buffer
    memcpy(&x, frameBuf + (b*4), 2);
    memcpy(&y, frameBuf + (b*4) + 2, 2);
    
    speck_decrypt(&x, &y);
    
    // Copy plaintext back
    memcpy(plain + (b*4), &x, 2);
    memcpy(plain + (b*4) + 2, &y, 2);
  }
  
  plain[8] = '\0'; // Null-terminate
  msgNum++;
  
  lcd.clear();
  lcd.print("Msg #"); lcd.print(msgNum);
  lcd.setCursor(0, 1); lcd.print(plain);
}
