/*
 * main.c
 *
 * UDP Server - Computer Networks assignment
 *
 * This file contains the UDP server implementation
 * portable across Windows, Linux and macOS.
 */

#if defined WIN32
#include <winsock.h>
#else
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>
#define closesocket close
#endif

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <time.h>
#include <stdint.h>
#include "protocol.h"

#ifndef NO_ERROR
#define NO_ERROR 0
#endif

#if defined WIN32
typedef int socklen_t;
#endif

void clearwinsock() {
#if defined WIN32
    WSACleanup();
#endif
}

int is_valid_city(const char *city) {
    const char* available_cities[] = {"bari", "roma", "milano", "napoli", "torino",
                                       "palermo", "genova", "bologna", "firenze", "venezia"};
    size_t n = sizeof(available_cities) / sizeof(available_cities[0]);

    for (size_t i = 0; i < n; i++) {
        #if defined WIN32
        if (_stricmp(city, available_cities[i]) == 0) {
        #else
        if (strcasecmp(city, available_cities[i]) == 0) {
        #endif
            return 1;
        }
    }
    return 0;
}

int is_valid_type(char type) {
    char lower_type = tolower(type);
    return (lower_type == 't' || lower_type == 'h' || lower_type == 'w' || lower_type == 'p');
}

float get_temperature(void) {
    return -10.0 + (rand() / (float)RAND_MAX) * 50.0f;
}

float get_humidity(void) {
    return 20.0 + (rand() / (float)RAND_MAX) * 80.0f;
}

float get_wind(void) {
    return (rand() / (float)RAND_MAX) * 100.0f;
}

float get_pressure(void) {
    return 950.0 + (rand() / (float)RAND_MAX) * 100.0f;
}

int main(int argc, char *argv[]) {
    srand(time(NULL));

    int port = SERVER_PORT;

    if (argc == 3) {
        if (strcmp(argv[1], "-p") == 0) {
            port = atoi(argv[2]);
            if (port <= 0 || port > 65535) {
                printf("Errore: numero di porta non valido (deve essere tra 1-65535)\n");
                return 1;
            }
        } else {
            printf("Uso: %s [-p port]\n", argv[0]);
            return 1;
        }
    } else if (argc != 1) {
        printf("Uso: %s [-p port]\n", argv[0]);
        return 1;
    }

    printf("Server in ascolto sulla porta %d...\n", port);

#if defined WIN32
    WSADATA wsa_data;
    int result = WSAStartup(MAKEWORD(2,2), &wsa_data);
    if (result != NO_ERROR) {
        printf("Errore in WSAStartup()\n");
        return 0;
    }
#endif

    int my_socket;

    // UDP: SOCK_DGRAM invece di SOCK_STREAM
    my_socket = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);

    if (my_socket < 0) {
        printf("Creazione socket fallita.\n");
        clearwinsock();
        return 1;
    }

    struct sockaddr_in sad;
    memset(&sad, 0, sizeof(sad));
    sad.sin_family = AF_INET;
    sad.sin_addr.s_addr = INADDR_ANY;
    sad.sin_port = htons(port);

    if (bind(my_socket, (struct sockaddr*) &sad, sizeof(sad)) < 0) {
        printf("Errore: bind() fallita\n");
        closesocket(my_socket);
        clearwinsock();
        return 1;
    }

    // UDP: non serve listen()
    printf("In attesa di richieste client...\n");

    struct sockaddr_in cad;
    socklen_t client_len;

    while (1) {
        char recv_buffer[sizeof(char) + 64];
        client_len = sizeof(cad);

        // UDP: recvfrom invece di accept + recv
        int bytes_rcvd = recvfrom(my_socket, recv_buffer, sizeof(recv_buffer), 0,
                                  (struct sockaddr *)&cad, &client_len);

        if (bytes_rcvd < 0) {
            printf("Errore: recvfrom() fallita\n");
            continue;
        }


        weather_request_t request;
        int offset = 0;

        if (bytes_rcvd < (int)(sizeof(char) + 1)) {
            printf("Errore: richiesta troppo corta\n");
            continue;
        }

        memcpy(&request.type, recv_buffer + offset, sizeof(char));
        offset += sizeof(char);

        int city_len_rcvd = bytes_rcvd - offset;
        if (city_len_rcvd >= 64) city_len_rcvd = 63;
        memcpy(request.city, recv_buffer + offset, city_len_rcvd);
        request.city[city_len_rcvd] = '\0';

        struct hostent *client_host;
        char *client_hostname;
        char *client_ip = inet_ntoa(cad.sin_addr);

        client_host = gethostbyaddr((char *)&cad.sin_addr,
                                   sizeof(cad.sin_addr), AF_INET);

        if (client_host != NULL && client_host->h_name != NULL) {
            client_hostname = client_host->h_name;
        } else {
            client_hostname = client_ip;
        }

        printf("Richiesta ricevuta da %s (ip %s): type='%c', city='%s'\n",
               client_hostname, client_ip, request.type, request.city);

        weather_response_t response;
        char req_type = (char) tolower((unsigned char) request.type);

        if (!is_valid_type(req_type)) {
            response.status = STATUS_INVALID_REQUEST;
            response.type = '\0';
            response.value = 0.0f;
        } else if (!is_valid_city(request.city)) {
            response.status = STATUS_CITY_NOT_FOUND;
            response.type = '\0';
            response.value = 0.0f;
        } else {
            response.status = STATUS_OK;
            response.type = req_type;
            switch (req_type) {
                case 't': response.value = get_temperature(); break;
                case 'h': response.value = get_humidity(); break;
                case 'w': response.value = get_wind(); break;
                case 'p': response.value = get_pressure(); break;
                default:
                    response.status = STATUS_INVALID_REQUEST;
                    response.type = '\0';
                    response.value = 0.0f;
            }
        }


        char send_buffer[sizeof(uint32_t) + sizeof(char) + sizeof(float)];
        offset = 0;


        uint32_t net_status = htonl(response.status);
        memcpy(send_buffer + offset, &net_status, sizeof(uint32_t));
        offset += sizeof(uint32_t);


        memcpy(send_buffer + offset, &response.type, sizeof(char));
        offset += sizeof(char);


        uint32_t temp;
        memcpy(&temp, &response.value, sizeof(float));
        temp = htonl(temp);
        memcpy(send_buffer + offset, &temp, sizeof(float));
        offset += sizeof(float);

        // UDP: sendto invece di send
        sendto(my_socket, send_buffer, offset, 0, (struct sockaddr *)&cad, client_len);
    }

    printf("Server terminato.\n");
    closesocket(my_socket);
    clearwinsock();
    return 0;
}
