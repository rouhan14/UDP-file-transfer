#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <fcntl.h>
#include <pthread.h>
#include <sys/stat.h>
#include <sys/sendfile.h>
#include <time.h>

#define SERVER_PORT "4950"
#define BUFFER_SIZE 500
#define WINDOW_SIZE 5

struct packet {
    int sequence_no;
    int packet_size;
    char data[BUFFER_SIZE];
};

struct ThreadArgs {
    struct packet *packets;
    int *acks;
    int *no_of_acks;
};

int socket_fd;
struct sockaddr_storage server_addr;
socklen_t server_addr_len = sizeof(struct sockaddr_storage);
int window_start = 0;

// Function to handle resending packets
void resendPackets(struct packet packets[], int acks[], int no_of_packets) {
    for (int i = 0; i < no_of_packets; i++) {
        if (!acks[i]) {
            printf("Resending packet: %d\n", packets[i].sequence_no);
            if (sendto(socket_fd, &packets[i], sizeof(struct packet), 0, (struct sockaddr *)&server_addr, server_addr_len) < 0) {
                perror("UDP Client: sendto");
                exit(1);
            }
        }
    }
}

// Function to handle the reception of acknowledgments
void *handleAcks(void *args) {
    struct ThreadArgs *threadArgs = (struct ThreadArgs *)args;
    int temp_ack;

    for (int i = 0; i < WINDOW_SIZE; i++) {
        if (recvfrom(socket_fd, &temp_ack, sizeof(int), 0, (struct sockaddr *)&server_addr, &server_addr_len) < 0) {
            perror("UDP Client: recvfrom");
            exit(1);
        }

        if (!threadArgs->acks[temp_ack]) {
            printf("Ack Received: %d\n", temp_ack);
            threadArgs->acks[temp_ack] = 1;
            (*threadArgs->no_of_acks)++;
        }
    }

    pthread_exit(NULL);
}

// Function to send a file using UDP
void sendFile(const char *hostname, const char *filename) {
    FILE *file = fopen(filename, "rb");
    if (!file) {
        perror("Error opening the file");
        exit(1);
    }

    fseek(file, 0, SEEK_END);
    off_t file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    printf("Size of Video File: %ld bytes\n", file_size);

    if (sendto(socket_fd, &file_size, sizeof(off_t), 0, (struct sockaddr *)&server_addr, server_addr_len) < 0) {
        perror("UDP Client: sendto");
        exit(1);
    }

    struct packet packets[WINDOW_SIZE];
    int acks[WINDOW_SIZE] = {0};
    int no_of_acks = 0;

    int remaining_data = file_size;

    while (remaining_data > 0) {
        for (int i = 0; i < WINDOW_SIZE; i++) {
            int data = fread(packets[i].data, 1, BUFFER_SIZE, file);

            packets[i].sequence_no = window_start + i;
            packets[i].packet_size = data;

            if (data == 0) {
                printf("End of file reached.\n");
                packets[i].packet_size = -1;
                no_of_acks = i + 1;
                remaining_data = 0;
                break;
            }

            // Print the packet being sent
            printf("Sending packet %d: %d bytes\n", packets[i].sequence_no, packets[i].packet_size);
        }

        struct ThreadArgs threadArgs = {packets, acks, &no_of_acks};
        pthread_t thread_id;
        if (pthread_create(&thread_id, NULL, handleAcks, (void *)&threadArgs) != 0) {
            perror("UDP Client: pthread_create");
            exit(1);
        }

        struct timespec time1, time2;
        time1.tv_sec = 0;
        time1.tv_nsec = 300000000L;
        nanosleep(&time1, &time2);

        pthread_cancel(thread_id);

        resendPackets(packets, acks, no_of_acks);

        window_start += WINDOW_SIZE;
        no_of_acks = 0;
    }

    fclose(file);
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "UDP Client: usage: %s hostname filename\n", argv[0]);
        exit(1);
    }

    const char *hostname = argv[1];
    const char *filename = argv[2];

    struct addrinfo hints, *serv_info;
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;

    if (getaddrinfo(hostname, SERVER_PORT, &hints, &serv_info) != 0) {
        fprintf(stderr, "UDP Client: getaddrinfo: Failed to get server address\n");
        exit(1);
    }

    for (struct addrinfo *ptr = serv_info; ptr != NULL; ptr = ptr->ai_next) {
        if ((socket_fd = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol)) == -1) {
            perror("UDP Client: socket");
            continue;
        }

        if (connect(socket_fd, ptr->ai_addr, ptr->ai_addrlen) == -1) {
            close(socket_fd);
            perror("UDP Client: connect");
            continue;
        }

        memcpy(&server_addr, ptr->ai_addr, ptr->ai_addrlen);
        break;
    }

    if (serv_info == NULL) {
        fprintf(stderr, "UDP Client: Failed to create socket\n");
        exit(2);
    }

    freeaddrinfo(serv_info);

    sendFile(hostname, filename);

    printf("\nFile transfer completed successfully!\n");

    close(socket_fd);

    return 0;
}

