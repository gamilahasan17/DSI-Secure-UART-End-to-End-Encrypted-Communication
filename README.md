# 🔐 Secure UART — End-to-End Encrypted Communication

An embedded systems project implementing an **encrypted UART communication channel between microcontrollers** using a lightweight key exchange protocol and block cipher.

The system demonstrates how low-power 8-bit microcontrollers can establish a shared secret and securely exchange sensor data over an otherwise insecure UART connection.

A third microcontroller acts as an **attacker/sniffer node**, intercepting the UART traffic and demonstrating that the encrypted payload cannot be directly understood without the cryptographic key.

## 🎯 Project Overview

UART is widely used in embedded systems because of its simplicity and low overhead. However, standard UART communication transmits data in plaintext, making it vulnerable to interception.

This project addresses that vulnerability by adding a lightweight cryptographic layer over the UART communication channel.

The system consists of:

* **Alice** — Sends encrypted sensor data
* **Bob** — Receives and decrypts the data
* **Attacker** — Passively intercepts the UART communication

The project was designed for resource-constrained **ATmega328P** microcontrollers.

## ✨ Features

* 🔐 End-to-end encrypted UART communication
* 🔑 DH-Lite shared secret generation
* 🛡️ Speck 32/64 lightweight encryption
* 📡 UART-based communication
* 🕵️ Attacker/sniffer node
* 🌡️ LM35 temperature sensor integration
* 📺 Real-time LCD output
* 🔄 Synchronized cryptographic handshake
* 📦 Encrypted message framing
* ⚡ Integer-based calculations for embedded efficiency

## 🏗️ System Architecture

```text
                 UART Communication
        ┌──────────────────────────────┐
        │                              │
        ▼                              ▼
   ┌─────────┐      Encrypted      ┌─────────┐
   │  Alice  │ ──────────────────► │   Bob   │
   │  TX     │                     │   RX    │
   └─────────┘                     └─────────┘
        │                              │
        │                              │
   LM35 Sensor                     16x2 LCD
        │
        │
        ▼
   ┌─────────────┐
   │   Attacker  │
   │   / Sniffer │
   └─────────────┘
        │
        ▼
   Intercepted encrypted
   UART traffic
```

The attacker has physical access to the UART communication lines but does not possess the cryptographic keys required to decrypt the payload.

## 🔑 Cryptographic Design

The system uses two cryptographic components:

### DH-Lite — Key Exchange

A lightweight 8-bit implementation of the Diffie-Hellman key exchange protocol is used to establish a shared secret between Alice and Bob.

Parameters used:

| Parameter     | Value |
| ------------- | ----: |
| Prime `p`     |   251 |
| Generator `g` |     6 |

The protocol allows Alice and Bob to establish a shared secret over an insecure communication channel.

> **Note:** The 8-bit key exchange is intended as an educational proof-of-concept and is not suitable for production-level security. A significantly larger key space would be required for real-world applications.

### Speck 32/64 — Encryption

The project uses the **Speck 32/64 lightweight block cipher** for encrypting transmitted data.

| Property   |   Value |
| ---------- | ------: |
| Block Size | 32 bits |
| Key Size   | 64 bits |
| Word Size  | 16 bits |
| Rounds     |      22 |
| Design     |     ARX |

Speck was selected because its 16-bit word operations are well suited to the resource constraints of the ATmega328P.

## 🔄 Communication Flow

The system uses a synchronized state machine for the cryptographic handshake:

```text
KeyGen
   ↓
Send Public Key
   ↓
Receive Public Key
   ↓
Derive Shared Secret
   ↓
Ready
   ↓
Send Encrypted Data
```

Both communicating nodes follow the same handshake sequence to establish synchronization before encrypted data transmission begins.

## 💻 Implementation Details

### Randomness

Private keys are seeded using noise obtained from an unconnected analog pin through `analogRead()`.

### Message Framing

Encrypted messages use framing bytes:

```text
Start Byte: 0xAA
End Byte:   0x55
```

This allows the receiving microcontroller to identify the beginning and end of an encrypted block even when communication noise is present.

### Integer Arithmetic

Integer arithmetic is used instead of floating-point calculations for temperature processing and cryptographic operations to reduce memory usage and execution time.

### Dual-Line Sniffing

The attacker node monitors both directions of communication.

Because the ATmega328P has a single hardware UART, `SoftwareSerial` is used to monitor one communication direction while the hardware RX line monitors the other.

## 🧰 Hardware

* 3 × Arduino Uno / ATmega328P
* 2 × 16×2 LCD displays
* LM35 temperature sensor
* Push button
* Resistors
* UART communication lines

## 💻 Software

* Arduino IDE
* C++
* DH-Lite
* Speck 32/64
* LiquidCrystal library
* SoftwareSerial

## 🧪 Results

The system successfully established an encrypted communication link.

Testing demonstrated that:

* Alice and Bob generated the same shared secret.
* Temperature data was successfully encrypted and transmitted.
* Bob correctly decrypted the received temperature message.
* The attacker successfully intercepted the transmitted bytes.
* The attacker could only observe unreadable hexadecimal ciphertext rather than the original message.

For example, Bob successfully received and decrypted a message such as:

```text
TMP:25C
```

while the attacker observed the encrypted transmission instead.

## 📊 Security Demonstration

The attacker/sniffer provides a practical demonstration of the difference between:

**Unencrypted UART**

```text
Temperature: 25°C
```

and:

**Encrypted UART**

```text
[Encrypted Ciphertext]
```

Although the attacker can observe the communication and the exchanged public information, the shared secret is not directly exposed.

This demonstrates the project's core objective of protecting UART payloads against **passive eavesdropping**.

## ⚠️ Limitations

This project is an educational proof-of-concept rather than a production-ready cryptographic system.

Current limitations include:

* 8-bit DH-Lite provides a very small key space.
* The system does not currently use a MAC or AEAD mode for message authentication.
* `analogRead()` noise is used as the source of randomness.
* The system primarily demonstrates protection against passive attacks.
* A larger cryptographic key space would be required for real-world deployment.

## 🚀 Future Improvements

Potential improvements include:

* 🔑 Upgrade DH-Lite to a 16-bit or 32-bit prime
* 🛡️ Add a MAC or AEAD mode for message integrity
* 🎲 Use a dedicated hardware random number generator
* 🔄 Implement automatic key rotation
* 🔐 Improve protection against active Man-in-the-Middle attacks
* 📡 Extend the system to larger sensor networks
* 🖥️ Develop a graphical monitoring interface

These improvements would make the system more appropriate for practical secure embedded communication.

## 📁 Suggested Project Structure

```text
Secure-UART/
│
├── Alice/
│   └── Alice.ino
│
├── Bob/
│   └── Bob.ino
│
├── Attacker/
│   └── Attacker.ino
│
├── README.md
└── Secure_UART_Report.pdf
```

> Adjust the folder and file names to match your actual project files.

## 👥 Team

* **Youssef Basem**
* **Moaaz Tamer**
* **Marina Ayman**
* **Gamila Hasan**
* **Ramy Emad**

## 🎓 Academic Project

**Course:** Digital System Interfacing
**Instructor:** Gehad Mohey
**University:** MSA University
**Date:** May 13, 2026

## 📚 Topics Demonstrated

This project combines several areas of computer engineering:

* Embedded Systems
* Microcontrollers
* UART Communication
* C++
* Cryptography
* Key Exchange
* Symmetric Encryption
* Sensor Interfacing
* LCD Interfacing
* State Machines
* Serial Communication
* Cybersecurity
