#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/select.h>

#define ATTACKER_IP "127.0.0.1"
#define ATTACKER_PORT 5001
#define BUFFER_SIZE 1024
#define TIMEOUT_SECONDS 3

/* ============================================================
 * NEW: packetization settings.
 * Instead of sending N independent whole messages, we now take
 * ONE message from the user and slice it into fixed-size chunks
 * ("packets"). Each chunk still goes through the full
 * Stop-and-Wait ARQ cycle (send -> wait for ACK -> retransmit
 * on timeout) exactly like before.
 * ============================================================ */
#define PACKET_SIZE 8          /* characters of payload per packet   */
#define MAX_MESSAGE_LENGTH 4096
#define MAX_PACKETS ((MAX_MESSAGE_LENGTH / PACKET_SIZE) + 2)

// ============================================================
// CHECKSUM (unchanged)
// ============================================================
unsigned int calculate_checksum(const char *data)
{
    unsigned int checksum = 0;
    while (*data != '\0') { checksum += (unsigned char)*data; data++; }
    return checksum;
}

// ============================================================
// CREATE FRAME:  DATA|sequence|chunk|checksum\n   (unchanged)
// ============================================================
void create_frame(char *frame, int sequence_number, const char *message)
{
    unsigned int checksum = calculate_checksum(message);
    snprintf(frame, BUFFER_SIZE, "DATA|%d|%s|%u\n", sequence_number, message, checksum);
}

// ============================================================
// SEND FRAME COMPLETELY (unchanged)
// ============================================================
int send_all(int socket_fd, const char *data, int length)
{
    int total_sent = 0;
    while (total_sent < length)
    {
        int n = send(socket_fd, data + total_sent, length - total_sent, 0);
        if (n <= 0) { perror("[SENDER] send()"); return -1; }
        total_sent += n;
    }
    return 0;
}

// ============================================================
// RECEIVE ONE COMPLETE LINE (unchanged)
// ============================================================
int recv_line(int socket_fd, char *buffer, int size)
{
    int i = 0;
    char ch;
    while (i < size - 1)
    {
        int n = recv(socket_fd, &ch, 1, 0);
        if (n == 0) return 0;
        if (n < 0) return -1;
        buffer[i++] = ch;
        if (ch == '\n') break;
    }
    buffer[i] = '\0';
    return i;
}

// ============================================================
// WAIT FOR ACK (unchanged)
// ============================================================
int wait_for_ack(int socket_fd, int expected_sequence)
{
    char buffer[BUFFER_SIZE];
    while (1)
    {
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(socket_fd, &readfds);

        struct timeval timeout;
        timeout.tv_sec = TIMEOUT_SECONDS;
        timeout.tv_usec = 0;

        int result = select(socket_fd + 1, &readfds, NULL, NULL, &timeout);

        if (result == 0)
        {
            printf("[SENDER] TIMEOUT waiting for ACK %d.\n", expected_sequence);
            return 0;
        }
        if (result < 0) { perror("[SENDER] select()"); return -1; }

        if (FD_ISSET(socket_fd, &readfds))
        {
            int n = recv_line(socket_fd, buffer, BUFFER_SIZE);
            if (n == 0) { printf("[SENDER] Attacker disconnected.\n"); return -1; }
            if (n < 0) { perror("[SENDER] recv()"); return -1; }

            printf("[SENDER] Received: %s", buffer);

            if (strncmp(buffer, "ACK|", 4) == 0)
            {
                int ack_number;
                if (sscanf(buffer, "ACK|%d", &ack_number) == 1)
                {
                    if (ack_number == expected_sequence)
                    {
                        printf("[SENDER] Correct ACK %d received.\n", ack_number);
                        return 1;
                    }
                    printf("[SENDER] Old/unexpected ACK %d. Expected %d.\n", ack_number, expected_sequence);
                }
            }
        }
    }
}

// ============================================================
// NEW: split one message into fixed-size packet chunks
// ============================================================
int split_into_packets(const char *message, char packets[][PACKET_SIZE + 1])
{
    int message_length = strlen(message);
    int num_packets = 0;
    int position = 0;

    while (position < message_length)
    {
        int chunk_length = PACKET_SIZE;
        if (position + chunk_length > message_length)
            chunk_length = message_length - position;

        strncpy(packets[num_packets], message + position, chunk_length);
        packets[num_packets][chunk_length] = '\0';

        position += chunk_length;
        num_packets++;
    }
    return num_packets;
}

// ============================================================
// MAIN
// ============================================================
int main()
{
    int sender_socket;
    struct sockaddr_in attacker_address;
    char user_message[MAX_MESSAGE_LENGTH];
    char packets[MAX_PACKETS][PACKET_SIZE + 1];
    int number_of_packets;

    printf("========================================\n");
    printf("           SENDER\n");
    printf("        STOP-AND-WAIT ARQ\n");
    printf("========================================\n");

    /* ==================== NEW ====================
     * Take ONE full message from the user instead of N
     * separate messages, then chop it into packets.
     * =============================================== */
    printf("\nEnter the message to send: ");
    fgets(user_message, MAX_MESSAGE_LENGTH, stdin);
    user_message[strcspn(user_message, "\n")] = '\0';

    if (strlen(user_message) == 0) strcpy(user_message, "EMPTY");

    number_of_packets = split_into_packets(user_message, packets);

    printf("\n[SENDER] Original message : \"%s\"\n", user_message);
    printf("[SENDER] Packet size      : %d characters\n", PACKET_SIZE);
    printf("[SENDER] Split into %d packet(s):\n", number_of_packets);
    for (int i = 0; i < number_of_packets; i++)
        printf("   Packet %d: \"%s\"\n", i, packets[i]);

    sender_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (sender_socket < 0) { perror("[SENDER] socket()"); return 1; }

    memset(&attacker_address, 0, sizeof(attacker_address));
    attacker_address.sin_family = AF_INET;
    attacker_address.sin_port = htons(ATTACKER_PORT);
    inet_pton(AF_INET, ATTACKER_IP, &attacker_address.sin_addr);

    printf("\n[SENDER] Connecting to Attacker...\n");
    if (connect(sender_socket, (struct sockaddr *)&attacker_address, sizeof(attacker_address)) < 0)
    {
        perror("[SENDER] connect()");
        printf("[SENDER] Make sure the Attacker program is running first.\n");
        close(sender_socket);
        return 1;
    }
    printf("[SENDER] Connected to Attacker.\n");

    int sequence_number = 0;

    for (int i = 0; i < number_of_packets; i++)
    {
        char frame[BUFFER_SIZE];
        int acknowledged = 0;
        int attempt = 0;

        create_frame(frame, sequence_number, packets[i]);

        printf("\n========================================\n");
        printf("[SENDER] PACKET %d OF %d\n", i + 1, number_of_packets);
        printf("[SENDER] Sequence Number: %d\n", sequence_number);
        printf("[SENDER] Payload: \"%s\"\n", packets[i]);
        printf("========================================\n");

        while (!acknowledged)
        {
            attempt++;
            printf("\n[SENDER] Transmission attempt #%d\n", attempt);
            printf("[SENDER] Sending: %s", frame);

            if (send_all(sender_socket, frame, strlen(frame)) < 0) { close(sender_socket); return 1; }

            printf("[SENDER] Waiting for ACK %d...\n", sequence_number);
            int result = wait_for_ack(sender_socket, sequence_number);

            if (result == 1)
            {
                acknowledged = 1;
                printf("[SENDER] Packet %d delivered successfully.\n", sequence_number);
            }
            else if (result == 0)
            {
                printf("[SENDER] RETRANSMITTING packet %d.\n", sequence_number);
            }
            else
            {
                printf("[SENDER] Communication error.\n");
                close(sender_socket);
                return 1;
            }
        }

        sequence_number = 1 - sequence_number;
        printf("[SENDER] Next sequence number: %d\n", sequence_number);
    }

    printf("\n========================================\n");
    printf("        ALL PACKETS TRANSMITTED\n");
    printf("========================================\n");

    const char *end_message = "END\n";
    send_all(sender_socket, end_message, strlen(end_message));
    printf("[SENDER] END signal sent.\n");

    close(sender_socket);
    printf("[SENDER] Connection closed.\n");
    printf("[SENDER] Program terminated.\n");
    return 0;
}