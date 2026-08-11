/*
 * ===========================================================================================
 *  PROJECT: SECURE LIGHTWEIGHT UART COMMUNICATION
 *  NODE: ALICE (Sensor & Encryption Node)
 *  
 *  DESCRIPTION:
 *  This node performs a DH-Lite key exchange with Bob, derives a shared secret,
 *  and then uses the Speck 32/64 block cipher to encrypt temperature data from 
 *  an LM35 sensor. The encrypted data is sent over UART to Bob.
 * 
 *  HARDWARE SETUP:
 *  - ATmega328P (Arduino Uno/Nano)
 *  - LM35 Temperature Sensor: Connected to Analog Pin A0
 *  - Trigger Button: Pin 8 (Uses internal pull-up, active LOW)
 *  - Entropy Source: Analog Pin A1 (Left floating to pick up atmospheric noise)
 * ===========================================================================================
 */

// ── DH-LITE KEY EXCHANGE PARAMETERS ────────────────────────────────────────────────────────
// Using a small 8-bit prime (251) for the Diffie-Hellman handshake.
// p = 251 (Prime), g = 6 (Generator / Primitive Root)
const uint8_t DH_P = 251;
const uint8_t DH_G = 6;

uint8_t myPrivate;    // Alice's secret private key (a)
uint8_t myPublic;     // Alice's public key (A = g^a mod p)
uint8_t peerPublic;   // Bob's public key received over UART (B)
uint8_t sharedSecret; // The final shared secret key (K = B^a mod p)

/**
 * Perform Modular Exponentiation: (base^exp) % mod
 * Uses the "Square-and-Multiply" algorithm for efficiency.
 */
uint8_t dh_lite_modpow(uint8_t base, uint8_t exp, uint8_t mod) {
  uint16_t result = 1;
  uint16_t b = base % mod;
  while (exp > 0) {
    if (exp & 1) result = (result * b) % mod; // Multiply if bit is set
    exp >>= 1;                                // Shift to next bit
    b = (b * b) % mod;                        // Square the base
  }
  return (uint8_t)result;
}

// ── SPECK 32/64 (ULTRA-LIGHTWEIGHT) CIPHER ────────────────────────────────────────────────
// Parameters: Block Size = 32 bits, Key Size = 64 bits, Rounds = 22.
// This variant uses 16-bit words, which is ideal for 8-bit microcontrollers.
#define SPECK_ROUNDS 22
uint16_t roundKeys[SPECK_ROUNDS]; // Array to store pre-calculated subkeys

// Macros for Circular Rotation (essential for ARX ciphers)
#define ROR16(x, r) (((x) >> (r)) | ((x) << (16 - (r)))) // Rotate Right
#define ROL16(x, r) (((x) << (r)) | ((x) >> (16 - (r)))) // Rotate Left

/**
 * Key Schedule: Expands a 64-bit key (4 x 16-bit words) into 22 round keys.
 */
void speck_key_schedule(const uint16_t K[4]) {
  uint16_t l[3] = { K[1], K[2], K[3] };
  roundKeys[0] = K[0];
  for (uint8_t i = 0; i < SPECK_ROUNDS - 1; i++) {
    // Speck Round Function for keys
    l[i % 3] = (ROR16(l[i % 3], 7) + roundKeys[i]) ^ i;
    roundKeys[i + 1] = ROL16(roundKeys[i], 2) ^ l[i % 3];
  }
}

/**
 * Encrypt a single 32-bit block (split into two 16-bit words x and y).
 */
void speck_encrypt(uint16_t* x, uint16_t* y) {
  for (uint8_t i = 0; i < SPECK_ROUNDS; i++) {
    *x = (ROR16(*x, 7) + *y) ^ roundKeys[i]; // Addition-Rotation-XOR
    *y = ROL16(*y, 2) ^ *x;                 // Rotation-XOR
  }
}

/**
 * Derive the 64-bit Speck key from the 8-bit DH shared secret.
 * Uses bitwise constants to spread the 8-bit entropy across all 64 bits.
 */
void buildSpeckKey(uint8_t secret) {
  uint16_t K[4];
  K[0] = (uint16_t)secret | ((uint16_t)secret << 8); // Repeated byte
  K[1] = K[0] ^ 0xAAAA; // XOR with pattern
  K[2] = K[0] ^ 0x5555; // XOR with inverse pattern
  K[3] = K[0] ^ 0x0F0F; // XOR with nibble pattern
  speck_key_schedule(K);
}

// ── STATE MACHINE & LOGIC ──────────────────────────────────────────────────────────────────
enum Phase { 
  PH_KEYGEN,    // Phase 0: Generate own keys
  PH_SEND_PK,   // Phase 1: Broadcast PK and wait for Bob
  PH_DERIVE,    // Phase 2: Calculate the shared secret
  PH_READY      // Phase 3: Encryption link established
};

Phase phase = PH_KEYGEN;
unsigned long timer = 0;
const int BTN = 8;
bool lastBtn = HIGH;
bool btnReady = false;

void setup() {
  Serial.begin(9600);
  pinMode(BTN, INPUT_PULLUP);
  delay(200);
  
  // Use floating A1 pin for entropy to ensure keys are different every time
  randomSeed(analogRead(A1));
  myPrivate = (random(2, 250)); // Alice's secret 'a'
}

void loop() {
  switch (phase) {
    case PH_KEYGEN:
      // A = g^a mod p
      myPublic = dh_lite_modpow(DH_G, myPrivate, DH_P);
      phase = PH_SEND_PK;
      timer = millis();
      break;

    case PH_SEND_PK:
      // Periodically broadcast public key to Bob
      if (millis() - timer >= 1500) {
        Serial.write(0xD1);      // Header for Alice's Public Key
        Serial.write(myPublic);
        timer = millis();
      }
      // Check for Bob's response
      if (Serial.available() >= 2) {
        if (Serial.peek() == 0xD2) { // Header for Bob's Public Key
          Serial.read();
          peerPublic = Serial.read();
          phase = PH_DERIVE;
        } else {
          Serial.read(); // Discard noise
        }
      }
      break;

    case PH_DERIVE:
      // K = B^a mod p
      sharedSecret = dh_lite_modpow(peerPublic, myPrivate, DH_P);
      buildSpeckKey(sharedSecret);
      delay(500);
      lastBtn = digitalRead(BTN);
      btnReady = true;
      phase = PH_READY;
      break;

    case PH_READY:
      // Normal Operation: Wait for button press to send data
      if (btnReady) {
        bool btn = digitalRead(BTN);
        if (lastBtn == HIGH && btn == LOW) { // Detect falling edge
          delay(50); // Debounce
          if (digitalRead(BTN) == LOW) {
            // Read LM35 Temperature
            int raw = analogRead(A0);
            int tempC = (int)((long)raw * 500 / 1024);

            // Format message (max 8 bytes for two Speck blocks)
            char msg[9];
            snprintf(msg, 9, "TMP:%dC", tempC);
            sendEncrypted(msg);
          }
        }
        lastBtn = btn;
      }
      break;
  }
  delay(5);
}

/**
 * Encrypt and transmit the message.
 * Framing: [0xAA (Start)][8 bytes Ciphertext][0x55 (End)]
 */
void sendEncrypted(const char* msg) {
  Serial.write(0xAA); // Start Frame
  
  // Encrypt in two 4-byte (32-bit) blocks
  for (uint8_t b = 0; b < 2; b++) {
    uint16_t x = 0, y = 0;
    // Pack bytes into 16-bit words (Little Endian)
    x = (uint16_t)msg[b*4] | ((uint16_t)msg[b*4 + 1] << 8);
    y = (uint16_t)msg[b*4 + 2] | ((uint16_t)msg[b*4 + 3] << 8);
    
    speck_encrypt(&x, &y);
    
    // Send encrypted words byte-by-byte
    Serial.write((uint8_t*)&x, 2);
    Serial.write((uint8_t*)&y, 2);
  }
  
  Serial.write(0x55); // End Frame
}
