# CN Network Attack Simulator

A Computer Networks lab project that simulates reliable data transmission between a Sender and Receiver through an Attacker.

The project demonstrates packetization, Stop-and-Wait ARQ, acknowledgements, checksums, retransmission, and different network attack scenarios.

## Features

- User-defined message input at the Sender
- Message divided into fixed-size packets
- Stop-and-Wait ARQ for reliable transmission
- Sequence numbers and acknowledgements
- Checksum-based error detection
- Automatic retransmission of lost/corrupted packets
- Attacker placed between Sender and Receiver
- Multiple attack modes:
  - No Attack
  - Drop Frame
  - Delay Frame
  - Duplicate Frame
  - Modify Frame
  - Drop ACK
  - Random Attack
  - Multiple Attack
- Receiver reassembles the packets into the original message

## Architecture

```text
Sender → Attacker → Receiver
           ↕
        Interference