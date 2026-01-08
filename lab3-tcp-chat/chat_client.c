// Lukash M., Informatika 1 grupe (4 kursas)
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netdb.h>

#define PORT "8888"
#define HOST "localhost"
#define BUFFER_SIZE 1024
#define NAME_LEN 32

int main() {
    int sock = 0;
    struct addrinfo hints, *res, *p;
    int status;
    char buffer[BUFFER_SIZE];
    char message[BUFFER_SIZE];
    char name[NAME_LEN];
    fd_set readfds; // Failu deskriptoriu aibe

    printf("Enter your username: "); // Vartotojo vardo ivestis
    fgets(name, NAME_LEN, stdin);
    name[strcspn(name, "\n")] = 0; // Pasalina naujos eilutes simboli

    if (strlen(name) < 1) { // Patikrina ar vardas nera tuscias
        printf("Invalid name\n");
        return -1;
    }

    memset(&hints, 0, sizeof hints);  // Nustato parametrus serveriui IPv4, TCP
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    if ((status = getaddrinfo(HOST, PORT, &hints, &res)) != 0) { // Gauna serverio adreso informacija
        return -1;
    }

    for(p = res; p != NULL; p = p->ai_next) { // Bando prisijungti prie vieno is rastu adresu
        if ((sock = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) { // Sukuria socketa
            continue;
        }

        if (connect(sock, p->ai_addr, p->ai_addrlen) == -1) { // Bando padaryti connecta
            close(sock);
            continue;
        }
        break;
    }

    if (p == NULL) {
        return -1; // Nepavyko prisijungti
    }

    freeaddrinfo(res); // Atlaisvina atminti

    send(sock, name, strlen(name), 0); // Issiuncia varda serveriui kaip 1ma zinute
    printf("Connected as %s.\n", name);
    printf("Commands: /list, /msg <name> <text>, /help\n");

    while (1) { // Isvalo ir nustato stebimus lizdus
        FD_ZERO(&readfds);
        FD_SET(sock, &readfds); // Serverio lizdas
        FD_SET(STDIN_FILENO, &readfds); // Klaviaturos ivestis

        if (select(sock + 1, &readfds, NULL, NULL, NULL) < 0) { // laukia kol bus duomenu viename is srautu
            exit(EXIT_FAILURE);
        }


        if (FD_ISSET(sock, &readfds)) { // Jei yra duomenys is serverio
            int valread = recv(sock, buffer, BUFFER_SIZE - 1, 0);
            if (valread == 0) {
                printf("Server disconnected.\n");
                break;
            } else if (valread > 0) {
                buffer[valread] = '\0'; // Prideda eil. pabaigos simboli
                printf("%s", buffer);
            }
        }

        if (FD_ISSET(STDIN_FILENO, &readfds)) { // Jei vartotojas parase teksta
            if (fgets(message, BUFFER_SIZE, stdin) != NULL) {
                if (strncmp(message, "exit", 4) == 0) { // tikrina ar vartotojas nori padaryti exit
                    break;
                }
                send(sock, message, strlen(message), 0); // Siuncia zinute serveriui
            }
        }
    }

    shutdown(sock, SHUT_RDWR); // darom shutdown
    close(sock);

    return 0;
}