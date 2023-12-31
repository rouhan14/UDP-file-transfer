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

int socket_fd;
struct sockaddr_storage client_addr;
socklen_t client_addr_len = sizeof(struct sockaddr_storage);

// Function to handle the reception of acknowledgments
void *handleAcks(void *args) {
    int *acks = (int *)args;

    for (int i = 0; i < WINDOW_SIZE; i++) {
        int temp_ack = i;

        if (sendto(socket_fd, &temp_ack, sizeof(int), 0, (struct sockaddr *)&client_addr, client_addr_len) < 0) {
            perror("UDP Server: sendto");
            exit(1);
        }

        acks[temp_ack] = 1;
        printf("Ack Sent: %d\n", temp_ack);
    }

    pthread_exit(NULL);
}

// Function to receive a file using UDP
void receiveFile() {
    off_t file_size;
    if (recvfrom(socket_fd, &file_size, sizeof(off_t), 0, (struct sockaddr *)&client_addr, &client_addr_len) < 0) {
        perror("UDP Server: recvfrom");
        exit(1);
    }

    printf("Receiving a file of size %ld bytes\n", file_size);

    FILE *file = fopen("received_file.mp4", "wb");
    if (!file) {
        perror("Error opening the file");
        exit(1);
    }

    struct packet packets[WINDOW_SIZE];
    int acks[WINDOW_SIZE] = {0};
    int no_of_acks = 0;

    while (1) {
        for (int i = 0; i < WINDOW_SIZE; i++) {
            if (recvfrom(socket_fd, &packets[i], sizeof(struct packet), 0, (struct sockaddr *)&client_addr, &client_addr_len) < 0) {
                perror("UDP Server: recvfrom");
                exit(1);
            }

            if (packets[i].packet_size == -1) {
                printf("End of file reached.\n");
                break;
            }

            // Print the received packet information
            printf("Received packet %d: %d bytes\n", packets[i].sequence_no, packets[i].packet_size);

            fwrite(packets[i].data, 1, packets[i].packet_size, file);
        }

        pthread_t thread_id;
        if (pthread_create(&thread_id, NULL, handleAcks, (void *)acks) != 0) {
            perror("UDP Server: pthread_create");
            exit(1);
        }

        pthread_join(thread_id, NULL);

        for (int i = 0; i < WINDOW_SIZE; i++) {
            if (!acks[i]) {
                printf("Resending acknowledgment: %d\n", i);
                if (sendto(socket_fd, &i, sizeof(int), 0, (struct sockaddr *)&client_addr, client_addr_len) < 0) {
                    perror("UDP Server: sendto");
                    exit(1);
                }
            }
        }

        memset(acks, 0, sizeof(acks));

        if (packets[WINDOW_SIZE - 1].packet_size == -1) {
            break;  // End of file
        }
    }

    fclose(file);
}

int main() {
    struct addrinfo hints, *serv_info;
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags = AI_PASSIVE;

    if (getaddrinfo(NULL, SERVER_PORT, &hints, &serv_info) != 0) {
        fprintf(stderr, "UDP Server: getaddrinfo: Failed to get server address\n");
        exit(1);
    }

    for (struct addrinfo *ptr = serv_info; ptr != NULL; ptr = ptr->ai_next) {
        if ((socket_fd = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol)) == -1) {
            perror("UDP Server: socket");
            continue;
        }

        if (bind(socket_fd, ptr->ai_addr, ptr->ai_addrlen) == -1) {
            close(socket_fd);
            perror("UDP Server: bind");
            continue;
        }

        break;
    }

    if (serv_info == NULL) {
        fprintf(stderr, "UDP Server: Failed to create socket\n");
        exit(2);
    }

    freeaddrinfo(serv_info);

    printf("UDP Server is waiting for connections...\n");

    while (1) {
        receiveFile();
    }

    close(socket_fd);

    return 0;
}

