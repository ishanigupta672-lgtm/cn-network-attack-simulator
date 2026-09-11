#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <time.h>

#define ATTACKER_PORT 5001
#define RECEIVER_IP "127.0.0.1"
#define RECEIVER_PORT 5002
#define BUFFER_SIZE 1024
#define DELAY_SECONDS 5

#define NO_ATTACK 1
#define DROP_FRAME 2
#define DELAY_FRAME 3
#define DUPLICATE_FRAME 4
#define MODIFY_FRAME 5
#define DROP_ACK 6
#define RANDOM_ATTACK 7
#define MULTIPLE_ATTACK 8

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
// FORWARD (unchanged)
// ============================================================
void forward_message(int socket_fd, const char *buffer)
{
    send(socket_fd, buffer, strlen(buffer), 0);
}

// ============================================================
// MODIFY FRAME (unchanged) - flips first char of the payload
// ============================================================
void modify_frame(char *buffer)
{
    char temp[BUFFER_SIZE];
    strncpy(temp, buffer, BUFFER_SIZE - 1);
    temp[BUFFER_SIZE - 1] = '\0';

    char *data_start = strchr(temp, '|');
    if (data_start == NULL) return;
    data_start = strchr(data_start + 1, '|');
    if (data_start == NULL) return;
    data_start++;

    char *checksum_separator = strchr(data_start, '|');
    if (checksum_separator == NULL) return;

    if (data_start < checksum_separator) *data_start = 'X';

    strncpy(buffer, temp, BUFFER_SIZE - 1);
    buffer[BUFFER_SIZE - 1] = '\0';
}

// ============================================================
// HANDLE DATA (unchanged logic, still driven by attack_mode)
// ============================================================
void handle_data(int receiver_socket, char *buffer, int attack_mode, int *data_attack_used)
{
    int sequence_number = -1;
    sscanf(buffer, "DATA|%d|", &sequence_number);
    printf("\n[ATTACKER] DATA %d received from Sender.\n", sequence_number);

    if (attack_mode == NO_ATTACK)
    {
        printf("[ATTACKER] FORWARDING DATA.\n");
        forward_message(receiver_socket, buffer);
        return;
    }

    if (attack_mode == DROP_FRAME)
    {
        if (*data_attack_used == 0)
        {
            printf("[ATTACKER] DROPPING DATA %d.\n", sequence_number);
            *data_attack_used = 1;
            return;
        }
        printf("[ATTACKER] Drop attack already used.\n");
        forward_message(receiver_socket, buffer);
        return;
    }

    if (attack_mode == DELAY_FRAME)
    {
        if (*data_attack_used == 0)
        {
            printf("[ATTACKER] DELAYING DATA %d by %d seconds.\n", sequence_number, DELAY_SECONDS);
            *data_attack_used = 1;
            sleep(DELAY_SECONDS);
            printf("[ATTACKER] Forwarding delayed DATA.\n");
            forward_message(receiver_socket, buffer);
            return;
        }
        forward_message(receiver_socket, buffer);
        return;
    }

    if (attack_mode == DUPLICATE_FRAME)
    {
        if (*data_attack_used == 0)
        {
            printf("[ATTACKER] DUPLICATING DATA %d.\n", sequence_number);
            forward_message(receiver_socket, buffer);
            forward_message(receiver_socket, buffer);
            *data_attack_used = 1;
            return;
        }
        forward_message(receiver_socket, buffer);
        return;
    }

    if (attack_mode == MODIFY_FRAME)
    {
        if (*data_attack_used == 0)
        {
            printf("[ATTACKER] MODIFYING DATA %d.\n", sequence_number);
            char modified[BUFFER_SIZE];
            strncpy(modified, buffer, BUFFER_SIZE - 1);
            modified[BUFFER_SIZE - 1] = '\0';
            modify_frame(modified);
            printf("[ATTACKER] Corrupted frame: %s", modified);
            forward_message(receiver_socket, modified);
            *data_attack_used = 1;
            return;
        }
        forward_message(receiver_socket, buffer);
        return;
    }

    if (attack_mode == RANDOM_ATTACK)
    {
        int choice = rand() % 5;
        switch (choice)
        {
            case 0:
                printf("[ATTACKER] RANDOM: FORWARD\n");
                forward_message(receiver_socket, buffer);
                break;
            case 1:
                printf("[ATTACKER] RANDOM: DROP DATA\n");
                break;
            case 2:
                printf("[ATTACKER] RANDOM: DELAY DATA\n");
                sleep(DELAY_SECONDS);
                forward_message(receiver_socket, buffer);
                break;
            case 3:
                printf("[ATTACKER] RANDOM: DUPLICATE DATA\n");
                forward_message(receiver_socket, buffer);
                forward_message(receiver_socket, buffer);
                break;
            case 4:
            {
                printf("[ATTACKER] RANDOM: MODIFY DATA\n");
                char modified[BUFFER_SIZE];
                strncpy(modified, buffer, BUFFER_SIZE - 1);
                modified[BUFFER_SIZE - 1] = '\0';
                modify_frame(modified);
                forward_message(receiver_socket, modified);
                break;
            }
        }
        return;
    }

    if (attack_mode == MULTIPLE_ATTACK)
    {
        printf("[ATTACKER] MULTIPLE ATTACK: DELAY + MODIFY\n");
        char modified[BUFFER_SIZE];
        strncpy(modified, buffer, BUFFER_SIZE - 1);
        modified[BUFFER_SIZE - 1] = '\0';
        printf("[ATTACKER] Delaying frame...\n");
        sleep(DELAY_SECONDS);
        printf("[ATTACKER] Modifying frame...\n");
        modify_frame(modified);
        printf("[ATTACKER] Forwarding modified frame.\n");
        forward_message(receiver_socket, modified);
        return;
    }
}

// ============================================================
// HANDLE ACK (unchanged)
// ============================================================
void handle_ack(int sender_socket, char *buffer, int attack_mode, int *ack_attack_used)
{
    int sequence_number = -1;
    sscanf(buffer, "ACK|%d", &sequence_number);
    printf("\n[ATTACKER] ACK %d received from Receiver.\n", sequence_number);

    if (attack_mode == DROP_ACK)
    {
        if (*ack_attack_used == 0)
        {
            printf("[ATTACKER] DROPPING ACK %d.\n", sequence_number);
            *ack_attack_used = 1;
            return;
        }
        forward_message(sender_socket, buffer);
        return;
    }

    if (attack_mode == RANDOM_ATTACK)
    {
        int choice = rand() % 3;
        if (choice == 0)
        {
            printf("[ATTACKER] RANDOM ACK: FORWARD\n");
            forward_message(sender_socket, buffer);
        }
        else if (choice == 1)
        {
            printf("[ATTACKER] RANDOM ACK: DROP\n");
        }
        else
        {
            printf("[ATTACKER] RANDOM ACK: DELAY\n");
            sleep(DELAY_SECONDS);
            forward_message(sender_socket, buffer);
        }
        return;
    }

    if (attack_mode == MULTIPLE_ATTACK)
    {
        printf("[ATTACKER] MULTIPLE ATTACK: DELAY ACK\n");
        sleep(DELAY_SECONDS);
        forward_message(sender_socket, buffer);
        return;
    }

    printf("[ATTACKER] FORWARDING ACK %d.\n", sequence_number);
    forward_message(sender_socket, buffer);
}

/* ============================================================
 * NEW: interactive menu, replaces the old argv-based mode
 * selection. Matches the menu you already showed in your
 * terminal screenshot.
 * ============================================================ */
int ask_attack_mode()
{
    int choice = 0;

    printf("========================================\n");
    printf("        ATTACKER / EMULATOR\n");
    printf("========================================\n");
    printf("\nSelect attack mode:\n\n");
    printf("1. No Attack\n");
    printf("2. Drop one Data Frame\n");
    printf("3. Delay one Data Frame\n");
    printf("4. Duplicate one Data Frame\n");
    printf("5. Modify one Data Frame\n");
    printf("6. Drop one ACK\n");
    printf("7. Random Attack\n");
    printf("8. Multiple Attacks\n");

    while (1)
    {
        printf("\nEnter choice: ");
        if (scanf("%d", &choice) == 1 && choice >= 1 && choice <= 8)
            break;
        printf("[ATTACKER] Invalid choice, try again.\n");
        while (getchar() != '\n'); /* clear bad input */
    }
    while (getchar() != '\n'); /* consume trailing newline before select loop */

    printf("\n[ATTACKER] Selected mode: %d\n", choice);
    return choice;
}

// ============================================================
// MAIN
// ============================================================
int main()
{
    int attacker_server;
    int sender_socket;
    int receiver_socket;
    struct sockaddr_in attacker_address;
    struct sockaddr_in sender_address;
    struct sockaddr_in receiver_address;
    socklen_t address_length = sizeof(sender_address);
    char buffer[BUFFER_SIZE];

    int attack_mode = ask_attack_mode();  /* NEW: interactive instead of argv */

    srand(time(NULL));

    printf("\n========================================\n");
    printf("[ATTACKER] Attack mode: %d\n", attack_mode);

    attacker_server = socket(AF_INET, SOCK_STREAM, 0);
    if (attacker_server < 0) { perror("[ATTACKER] socket()"); return 1; }

    int option = 1;
    setsockopt(attacker_server, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option));

    memset(&attacker_address, 0, sizeof(attacker_address));
    attacker_address.sin_family = AF_INET;
    attacker_address.sin_addr.s_addr = INADDR_ANY;
    attacker_address.sin_port = htons(ATTACKER_PORT);

    if (bind(attacker_server, (struct sockaddr *)&attacker_address, sizeof(attacker_address)) < 0)
    {
        perror("[ATTACKER] bind()");
        close(attacker_server);
        return 1;
    }

    listen(attacker_server, 5);
    printf("[ATTACKER] Listening for Sender on port %d...\n", ATTACKER_PORT);

    receiver_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (receiver_socket < 0) { perror("[ATTACKER] receiver socket()"); return 1; }

    memset(&receiver_address, 0, sizeof(receiver_address));
    receiver_address.sin_family = AF_INET;
    receiver_address.sin_port = htons(RECEIVER_PORT);
    inet_pton(AF_INET, RECEIVER_IP, &receiver_address.sin_addr);

    printf("[ATTACKER] Connecting to Receiver...\n");
    if (connect(receiver_socket, (struct sockaddr *)&receiver_address, sizeof(receiver_address)) < 0)
    {
        perror("[ATTACKER] connect()");
        return 1;
    }
    printf("[ATTACKER] Connected to Receiver.\n");

    printf("[ATTACKER] Waiting for Sender...\n");
    sender_socket = accept(attacker_server, (struct sockaddr *)&sender_address, &address_length);
    if (sender_socket < 0) { perror("[ATTACKER] accept()"); return 1; }

    printf("[ATTACKER] Sender connected.\n");
    printf("\n========================================\n");
    printf("        ATTACKER SYSTEM READY\n");
    printf("========================================\n");
    printf("[ATTACKER] Sender   <--> Attacker :%d\n", ATTACKER_PORT);
    printf("[ATTACKER] Attacker <--> Receiver :%d\n", RECEIVER_PORT);
    printf("[ATTACKER] Attack mode: %d\n", attack_mode);
    printf("[ATTACKER] Monitoring traffic...\n");

    int data_attack_used = 0;
    int ack_attack_used = 0;

    while (1)
    {
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(sender_socket, &readfds);
        FD_SET(receiver_socket, &readfds);

        int max_fd = sender_socket > receiver_socket ? sender_socket : receiver_socket;
        int result = select(max_fd + 1, &readfds, NULL, NULL, NULL);
        if (result < 0) { perror("[ATTACKER] select()"); break; }

        if (FD_ISSET(sender_socket, &readfds))
        {
            memset(buffer, 0, sizeof(buffer));
            int n = recv_line(sender_socket, buffer, BUFFER_SIZE);
            if (n <= 0) { printf("[ATTACKER] Sender disconnected.\n"); break; }

            printf("\n[ATTACKER] From Sender: %s", buffer);

            if (strncmp(buffer, "END", 3) == 0)
            {
                forward_message(receiver_socket, buffer);
                break;
            }
            if (strncmp(buffer, "DATA|", 5) == 0)
                handle_data(receiver_socket, buffer, attack_mode, &data_attack_used);
        }

        if (FD_ISSET(receiver_socket, &readfds))
        {
            memset(buffer, 0, sizeof(buffer));
            int n = recv_line(receiver_socket, buffer, BUFFER_SIZE);
            if (n <= 0) { printf("[ATTACKER] Receiver disconnected.\n"); break; }

            printf("\n[ATTACKER] From Receiver: %s", buffer);

            if (strncmp(buffer, "ACK|", 4) == 0)
                handle_ack(sender_socket, buffer, attack_mode, &ack_attack_used);
        }
    }

    close(sender_socket);
    close(receiver_socket);
    close(attacker_server);
    printf("\n[ATTACKER] Terminated.\n");
    return 0;
}