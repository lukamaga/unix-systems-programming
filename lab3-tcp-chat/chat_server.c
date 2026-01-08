// Lukash M., Informatika 1 grupe (4 kursas)
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <time.h>
#include <signal.h>

#define PORT 8888
#define MAX_CLIENTS 20
#define BUFFER_SIZE 1024
#define NAME_LEN 32

//volatile sig_atomic_t server_running = 1; // serverio busena

//void handle_sigint(int sig) { // Signalo apdorojimas ctrl+c
//    server_running = 0;
//}

void get_timestamp(char *buf) { // Funkcija laikui gauti
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    sprintf(buf, "[%02d:%02d]", t->tm_hour, t->tm_min);
}

int main(int argc, char *argv[]) {
    int master_socket, addrlen, new_socket, client_socket[MAX_CLIENTS];
    char client_names[MAX_CLIENTS][NAME_LEN];
    int max_sd, sd, activity, valread;
    struct sockaddr_in address;
    char buffer[BUFFER_SIZE];
    char message[BUFFER_SIZE + NAME_LEN + 16];
    char time_str[10];
    
    fd_set readfds; // Failu deskriptoriu aibe

    //signal(SIGINT, handle_sigint);

    for (int i = 0; i < MAX_CLIENTS; i++) { // Inicijuojami klientu masyvai
        client_socket[i] = 0;
        memset(client_names[i], 0, NAME_LEN);
    }

    if ((master_socket = socket(AF_INET, SOCK_STREAM, 0)) == 0) { // Sukuriamas pagrindinis lizdas (socket)
        exit(EXIT_FAILURE);
    }

    int opt = 1; // Galima pasakyti kad leidzia lizdui naudoti porta is naujo is karto po isjungimo
    if (setsockopt(master_socket, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt)) < 0) {
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET; // Nustatomas adresas ir portas
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(master_socket, (struct sockaddr *)&address, sizeof(address)) < 0) { // Prisijungiam prie porto
        exit(EXIT_FAILURE);
    }

    if (listen(master_socket, 3) < 0) { // gaunam ateinanciu rysiu
        exit(EXIT_FAILURE);
    }

    addrlen = sizeof(address);
    printf("Server running on port %d\n", PORT); // Pagrindinis ciklas

    //while (server_running) {
    while (1) {
        FD_ZERO(&readfds);
        FD_SET(master_socket, &readfds); // Pridedamas pagrindinis lizdas (master socket)
        max_sd = master_socket;

        for (int i = 0; i < MAX_CLIENTS; i++) { // Pridedami aktyvus klientu lizdai i stebimuju sarasa
            sd = client_socket[i];
            if (sd > 0)
                FD_SET(sd, &readfds);
            if (sd > max_sd)
                max_sd = sd;
        }

        struct timeval timeout;
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;

        activity = select(max_sd + 1, &readfds, NULL, NULL, &timeout);

        if ((activity < 0) && (errno != EINTR)) {
            continue;
        }

       // if (FD_ISSET(master_socket, &readfds)) { // Naujas prisijungimas
          //  if ((new_socket = accept(master_socket, (struct sockaddr *)&address, (socklen_t *)&addrlen)) < 0) {
           //     if (server_running) exit(EXIT_FAILURE);
            //    break;
          //  }

        if (FD_ISSET(master_socket, &readfds)) { // Naujas prisijungimas
            new_socket = accept(master_socket, (struct sockaddr *)&address, (socklen_t *)&addrlen);
            if (new_socket < 0) {
                perror("accept");
                exit(EXIT_FAILURE);
            }

            printf("New connection from IP: %s, Port: %d\n", 
                   inet_ntoa(address.sin_addr), ntohs(address.sin_port));

            int slot_found = 0;
            for (int i = 0; i < MAX_CLIENTS; i++) { // Pridedamas naujas lizdas i masyva
                if (client_socket[i] == 0) {
                    client_socket[i] = new_socket;
                    memset(client_names[i], 0, NAME_LEN); 
                    slot_found = 1;
                    break;
                }
            }
            if(!slot_found){
                const char *full_msg = "Server is full. Try again later.\n";   // Jei nera laisvos vietos, pranesa, kad serveris pilnas
                send(new_socket, full_msg, strlen(full_msg), 0);
                close(new_socket);
                printf("Connection rejected: server full\n");
            }
            
        }

        for (int i = 0; i < MAX_CLIENTS; i++) {  // Tikrinami visi klientu lizdai
            sd = client_socket[i];

            if (FD_ISSET(sd, &readfds)) {
                valread = recv(sd, buffer, BUFFER_SIZE - 1, 0);

                if (valread == 0) { // Jei recv grazina 0, klientas atsijunge
                    getpeername(sd, (struct sockaddr *)&address, (socklen_t *)&addrlen);
                    
                    if (strlen(client_names[i]) > 0) { // pranesame kitiems klientams apie atsijungima
                        get_timestamp(time_str);
                        sprintf(message, "%s User %s disconnected\n", time_str, client_names[i]);
                        printf("%s", message);
                        for (int j = 0; j < MAX_CLIENTS; j++) {
                            if (client_socket[j] != 0 && client_socket[j] != sd) {
                                send(client_socket[j], message, strlen(message), 0);
                            }
                        }
                    }

                    close(sd); // Uzdarome lizda
                    client_socket[i] = 0;
                    memset(client_names[i], 0, NAME_LEN);
                } 
                else if (valread > 0) {
                    buffer[valread] = '\0';

                    if (strlen(client_names[i]) == 0) { // Jei vartotojas dar neturi vardo, pirma zinute laikoma vardu
                        int name_taken = 0;
                        for (int k = 0; k < MAX_CLIENTS; k++) { // Tikriname ar vardas uzimtas
                            if (strcmp(client_names[k], buffer) == 0) {
                                name_taken = 1;
                                break;
                            }
                        }

                        if (name_taken) {
                            char *error_msg = "Name taken. Disconnecting.\n";
                            send(sd, error_msg, strlen(error_msg), 0);
                            close(sd);
                            client_socket[i] = 0;
                        } else {
                            strncpy(client_names[i], buffer, NAME_LEN - 1);
                            get_timestamp(time_str);
                            sprintf(message, "%s User %s has joined\n", time_str, client_names[i]);
                            printf("%s", message);
                            
                            for (int j = 0; j < MAX_CLIENTS; j++) { // Pranesame kitiems apie nauja klienta (vartotoja)
                                if (client_socket[j] != 0 && client_socket[j] != sd) {
                                    send(client_socket[j], message, strlen(message), 0);
                                }
                            }
                        }
                    } else { // Komandu apdorojimas
                        if (strncmp(buffer, "/help", 5) == 0) {
                            char *help_msg = "Commands:\n/list - Online users\n/msg <name> <text> - Private message\n/help - Show this message\n";
                            send(sd, help_msg, strlen(help_msg), 0);
                        }
                        else if (strncmp(buffer, "/list", 5) == 0) { // Saraso komanda
                            char list_msg[BUFFER_SIZE];
                            strcpy(list_msg, "Online users:\n");
                            for(int k = 0; k < MAX_CLIENTS; k++) {
                                if(client_socket[k] != 0 && strlen(client_names[k]) > 0) {
                                    strcat(list_msg, "- ");
                                    strcat(list_msg, client_names[k]);
                                    strcat(list_msg, "\n");
                                }
                            }
                            send(sd, list_msg, strlen(list_msg), 0);
                        }
                        else if (strncmp(buffer, "/msg", 4) == 0) { // Privati zinute
                            char target_name[NAME_LEN];
                            char *ptr = buffer + 5;
                            char *space = strchr(ptr, ' ');
                            
                            if (space != NULL) {
                                *space = '\0';
                                strcpy(target_name, ptr);
                                
                                int target_found = 0;
                                for(int k = 0; k < MAX_CLIENTS; k++) {
                                    if (client_socket[k] != 0 && strcmp(client_names[k], target_name) == 0) {
                                        char pm_msg[BUFFER_SIZE];
                                        get_timestamp(time_str);
                                        sprintf(pm_msg, "%s [PM from %s]: %s", time_str, client_names[i], space + 1);
                                        send(client_socket[k], pm_msg, strlen(pm_msg), 0);
                                        target_found = 1;
                                        break;
                                    }
                                }
                                if (!target_found) {
                                    char *err = "User not found\n";
                                    send(sd, err, strlen(err), 0);
                                }
                            } else {
                                char *err = "Usage: /msg <name> <text>\n";
                                send(sd, err, strlen(err), 0);
                            }
                        }
                        else { // paprasta zinute visiems
                            get_timestamp(time_str);
                            sprintf(message, "%s %s: %s", time_str, client_names[i], buffer);
                            for (int j = 0; j < MAX_CLIENTS; j++) {
                                if (client_socket[j] != 0 && client_socket[j] != sd) {
                                    send(client_socket[j], message, strlen(message), 0);
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    printf("\nServer shutting down...\n"); // Serverio isjungimas ir visu klientu atjungimas
    char *shutdown_msg = "Server shutting down.\n";
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (client_socket[i] != 0) {
            send(client_socket[i], shutdown_msg, strlen(shutdown_msg), 0);
            close(client_socket[i]);
        }
    }
    close(master_socket);

    return 0;
}