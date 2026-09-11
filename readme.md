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
Sender

Takes a custom message from the user, divides it into packets, and sends the packets using Stop-and-Wait ARQ.

Attacker

Acts as an intermediary between the Sender and Receiver and can simulate different network attack conditions.

Receiver

Receives, validates, acknowledges, and reassembles the packets into the original user-readable message.

Technologies
C
TCP Sockets
Stop-and-Wait ARQ
Checksum
Linux/Ubuntu
How to Run

Compile the three programs:

gcc sender.c -o sender
gcc attacker.c -o attacker
gcc receiver.c -o receiver

Run them in three separate terminals.

Terminal 1 — Receiver
./receiver
Terminal 2 — Attacker
./attacker

Select the desired attack mode when prompted.

Terminal 3 — Sender
./sender

Enter your custom message when prompted.

The message is divided into packets, transmitted through the Attacker, and reassembled at the Receiver.

Project Context

This project was developed as part of the Computer Networks Laboratory assessment at VIT Vellore.

Future Improvements
Interactive GUI
Packet visualization
Real-time transmission logs
Additional network attack simulations
Improved packet parsing and error handling