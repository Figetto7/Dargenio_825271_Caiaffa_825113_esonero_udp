/*
 * main.c
 *
 * UDP Client - Computer Networks assignment
 *
 * This file contains the UDP client implementation
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

void capitalize_city(char *city) {
    if (city[0] != '\0') {
        city[0] = toupper((unsigned char)city[0]);
        for (int i = 1; city[i] != '\0'; i++) {
            if (city[i-1] == ' ') {
                city[i] = toupper((unsigned char)city[i]);
            }
        }
    }
}

int main(int argc, char *argv[]) {
    char *server_input = "localhost";
    int port = SERVER_PORT;
    char *request_str = NULL;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-s") == 0 && i + 1 < argc) {
            server_input = argv[i + 1];
            i++;
        }
        else if (strcmp(argv[i], "-p") == 0 && i + 1 < argc) {
            port = atoi(argv[i + 1]);
            if (port <= 0 || port > 65535) {
                printf("Errore: numero di porta non valido\n");
                return 1;
            }
            i++;
        }
        else if (strcmp(argv[i], "-r") == 0 && i + 1 < argc) {
            request_str = argv[i + 1];
            i++;
        }
        else {
            printf("Utilizzo: %s [-s server] [-p port] -r \"tipo citta\"\n", argv[0]);
            return 1;
        }
    }

    if (request_str == NULL) {
        printf("Errore: -r e' obbligatorio!\n");
        printf("Uso: %s [-s server] [-p port] -r \"tipo citta\"\n", argv[0]);
        return 1;
    }


    char type;
    char city[64];
    char *space_pos = strchr(request_str, ' ');

    if (space_pos == NULL || space_pos == request_str) {
        printf("Errore: la richiesta e' espressa in modo errato\n");
        printf("Forma corretta: \"t citta\" (esempio: \"t bari\")\n");
        return 1;
    }


    if (space_pos - request_str != 1) {
        printf("Errore: la richiesta e' espressa in modo errato\n");
        printf("Forma corretta: \"t citta\" (esempio: \"t bari\")\n");
        return 1;
    }

    type = request_str[0];


    char *city_start = space_pos + 1;
    while (*city_start == ' ') city_start++;

    if (*city_start == '\0') {
        printf("Errore: la richiesta e' espressa in modo errato\n");
        printf("Forma corretta: \"t citta\" (esempio: \"t bari\")\n");
        return 1;
    }

    strncpy(city, city_start, 63);
    city[63] = '\0';

    if (strlen(city) > 63) {
        printf("Errore: il nome della citta' e' troppo lungo (max 63 caratteri)\n");
        return 1;
    }

#if defined WIN32
    WSADATA wsa_data;
    int result = WSAStartup(MAKEWORD(2,2), &wsa_data);
    if (result != NO_ERROR) {
        printf("Errore nella funzione WSAStartup()\n");
        return 0;
    }
#endif

    int c_socket;


    c_socket = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);

    if (c_socket < 0) {
        printf("Creazione della socket fallita\n");
        clearwinsock();
        return 1;
    }


    struct hostent *server_host;
    struct in_addr server_ip_addr;
    char *server_hostname;
    char *server_ip_str;


    unsigned long ip = inet_addr(server_input);

    if (ip != INADDR_NONE) {

        server_ip_addr.s_addr = ip;
        server_ip_str = inet_ntoa(server_ip_addr);

        server_host = gethostbyaddr((char *)&server_ip_addr,
                                   sizeof(server_ip_addr), AF_INET);

        if (server_host != NULL && server_host->h_name != NULL) {
            server_hostname = server_host->h_name;
        } else {
            server_hostname = server_ip_str;
        }
    } else {

        server_host = gethostbyname(server_input);

        if (server_host == NULL) {
            printf("Errore: impossibile risolvere il server '%s'\n", server_input);
            closesocket(c_socket);
            clearwinsock();
            return 1;
        }

        server_hostname = server_host->h_name;
        struct in_addr *ina = (struct in_addr *)server_host->h_addr_list[0];
        server_ip_addr = *ina;
        server_ip_str = inet_ntoa(*ina);
    }

    struct sockaddr_in sad;
    memset(&sad, 0, sizeof(sad));
    sad.sin_family = AF_INET;
    sad.sin_addr = server_ip_addr;
    sad.sin_port = htons(port);


    char send_buffer[sizeof(char) + 64];
    int offset = 0;

    memcpy(send_buffer + offset, &type, sizeof(char));
    offset += sizeof(char);

    int city_len = strlen(city);
    memcpy(send_buffer + offset, city, city_len);
    offset += city_len;

    if (sendto(c_socket, send_buffer, offset, 0, (struct sockaddr *)&sad, sizeof(sad)) < 0) {
        printf("Errore: funzione sendto() fallita\n");
        closesocket(c_socket);
        clearwinsock();
        return 1;
    }

    char recv_buffer[sizeof(uint32_t) + sizeof(char) + sizeof(float)];
    struct sockaddr_in from_addr;
    socklen_t from_len = sizeof(from_addr);

    int bytes_rcvd = recvfrom(c_socket, recv_buffer, sizeof(recv_buffer), 0,
                              (struct sockaddr *)&from_addr, &from_len);

    if (bytes_rcvd <= 0) {
        printf("Errore: funzione recvfrom() fallita\n");
        closesocket(c_socket);
        clearwinsock();
        return 1;
    }

    if (sad.sin_addr.s_addr != from_addr.sin_addr.s_addr) {
        printf("Errore: ricevuto pacchetto da sorgente sconosciuta\n");
        closesocket(c_socket);
        clearwinsock();
        return 1;
    }

    if (bytes_rcvd != (int)(sizeof(uint32_t) + sizeof(char) + sizeof(float))) {
        printf("Errore: dati incompleti ricevuti (%d bytes invece di %lu)\n",
               bytes_rcvd, (unsigned long)(sizeof(uint32_t) + sizeof(char) + sizeof(float)));
        closesocket(c_socket);
        clearwinsock();
        return 1;
    }

    weather_response_t response;
    offset = 0;

    uint32_t net_status;
    memcpy(&net_status, recv_buffer + offset, sizeof(uint32_t));
    response.status = ntohl(net_status);
    offset += sizeof(uint32_t);

    memcpy(&response.type, recv_buffer + offset, sizeof(char));
    offset += sizeof(char);

    uint32_t temp;
    memcpy(&temp, recv_buffer + offset, sizeof(float));
    temp = ntohl(temp);
    memcpy(&response.value, &temp, sizeof(float));
    capitalize_city(city);

    printf("Ricevuto risultato dal server %s (ip %s). ", server_hostname, server_ip_str);

    if (response.status == STATUS_OK) {
        switch (response.type) {
            case 't':
                printf("%s: Temperatura = %.1f°C\n", city, response.value);
                break;

            case 'h':
                printf("%s: Umidita' = %.1f%%\n", city, response.value);
                break;

            case 'w':
                printf("%s: Vento = %.1f km/h\n", city, response.value);
                break;

            case 'p':
                printf("%s: Pressione = %.1f hPa\n", city, response.value);
                break;

            default:
                printf("Unknown response type\n");
                break;
        }

    } else {
        switch (response.status) {
            case STATUS_CITY_NOT_FOUND:
                printf("Citta' non disponibile\n");
                break;

            case STATUS_INVALID_REQUEST:
                printf("Richiesta non valida\n");
                break;

            default:
                printf("Errore sconosciuto\n");
                break;
        }
    }

    closesocket(c_socket);
    clearwinsock();
    return 0;
}


