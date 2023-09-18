#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <sys/socket.h>
#include <dirent.h>
#include <unistd.h>

#define MAXTMESS 200
#define buffer_size 1024
#define SIZE 1024
char *ip = "127.0.0.1";
int port = 8080;
void *envoi(void *arg);
void *reception(void *arg);
struct sockaddr_in rcv_addr;
socklen_t addr_size;
int sockrcv;

struct ma_struct
{
    int sockfd;
    FILE *fp;
};

void write_file(int sockfd)
{
    int n;
    FILE *fp;
    char filename[SIZE];
    char buffer[SIZE];

    if ((n = recv(sockfd, filename, SIZE, 0)) > 0) // Receive the filename from the server
    {
        filename[n] = '\0'; // null-terminate the received string
        char *base_filename = strrchr(filename, '/');
        if (base_filename == NULL)
        {
            base_filename = filename; // If there is no '/' in the filename, use the whole filename
        }
        else
        {
            base_filename++; // If there is a '/', increment the pointer to get the filename after the '/'
        }
        char file_path[SIZE];
        snprintf(file_path, SIZE, "recu/%s", base_filename); // Append filename to the 'recu/' directory
        printf("Nom du fichier : %s\n", base_filename);

        fp = fopen(file_path, "wb");
        if (fp == NULL)
        {
            perror("[-]Error in opening file");
            return;
        }
        printf("Reception du fichier\n");

        while (1)
        {
            n = recv(sockfd, buffer, SIZE, 0);
            if (n <= 0)
            {
                break;
                return;
            }

            fwrite(buffer, 1, n, fp);
            bzero(buffer, SIZE);
        }
        fclose(fp);
    }
    else
    {
        printf("Erreur de réception du nom de fichier.\n");
        return;
    }
}

void *send_file(void *arg)
{
    struct ma_struct *my_struct = (struct ma_struct *)arg;
    int sockfd = my_struct->sockfd;

    FILE *fp = my_struct->fp;

    char *data = malloc(buffer_size);
    int nb_octet_lu = 0;
    while ((nb_octet_lu = fread(data, 1, buffer_size, fp)) > 0)
    {
       

        if (send(sockfd, data, nb_octet_lu, 0) == -1)
        {
            perror("[-]Error in sending file.");
            fclose(fp);
            exit(1);
        }
        bzero(data, (int)buffer_size);
        // memset(data, 0, buffer_size);
    }
    fclose(fp);

    free(data);
    pthread_exit(NULL);
}

void sendFileToServer(void *arg)
{
    // socket envoi de fichiers
    int *dSP = (int *)arg;
    int sockfd;
    struct sockaddr_in server_addr;
    FILE *fp;
    char *filename = malloc(256 * sizeof(char));
    if (filename == NULL)
    {
        perror("[-]Error in memory allocation.");
        exit(1);
    }

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
    {
        perror("[-]Error in socket");
        exit(1);
    }
    printf("[+]Server socket created successfully.\n");

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = port;
    server_addr.sin_addr.s_addr = inet_addr(ip);

    DIR *d;
    struct dirent *dir;
    int total_files = 0; // Ajoutez ceci

    // Comptez le nombre total de fichiers dans le répertoire 'aEnvoyer'
    d = opendir("aEnvoyer");
    if (d)
    {
        while ((dir = readdir(d)) != NULL)
        {
            if (dir->d_type == DT_REG) // only regular files
            {
                total_files++;
            }
        }
        closedir(d);
    }

    // Imprimez les fichiers disponibles
    d = opendir("aEnvoyer");
    if (d)
    {
        int file_index = 1;
        printf("Files available for upload:\n");
        while ((dir = readdir(d)) != NULL)
        {
            if (dir->d_type == DT_REG) // only regular files
            {
                printf("%d. %s\n", file_index, dir->d_name);
                file_index++;
            }
        }
        closedir(d);
    }

    // Demandez à l'utilisateur de choisir un fichier à envoyer
    char file_choice[256]; // Vous aviez déjà ceci
    int chosen_file;       // Ajoutez ceci
    do
    {
        printf("Enter the number of the file to send (1-%d): ", total_files);
        fgets(file_choice, 256, stdin);
        chosen_file = atoi(file_choice);
        if (chosen_file < 1 || chosen_file > total_files)
        {
            printf("Invalid choice, please choose a number between 1 and %d.\n", total_files);
        }
    } while (chosen_file < 1 || chosen_file > total_files);

    // Ouvrez le fichier que l'utilisateur a choisi
    d = opendir("aEnvoyer");
    if (d)
    {
        int file_index = 1;
        while ((dir = readdir(d)) != NULL)
        {
            if (dir->d_type == DT_REG) // only regular files
            {
                if (file_index == chosen_file)
                {
                    strncpy(filename, dir->d_name, 256);
                    break;
                }
                file_index++;
            }
        }
        closedir(d);
    }

    // ouverture du fichier
    char filepath[512];
    snprintf(filepath, 512, "aEnvoyer/%s", filename);
    fp = fopen(filepath, "rb");
    if (fp == NULL)
    {
        perror("[-]Error in reading file.");
        free(filename);
        return;
    }
    char *msg = "/send";
    send(*dSP, msg, strlen(msg) + 1, 0);

    // send(sockfd, filename, strlen(filename) + 1, 0); //##################################### ICI #############################//

    int e;
    // connexion du serveur
    e = connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr));
    if (e == -1)
    {
        perror("[-]Error in socket");
        exit(1);
    }

    // Envoi du nom du fichier
    send(sockfd, filename, strlen(filename) + 1, 0); // rajout ca @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@

    printf("[+]Connected to Server.\n");

    // creation du thread d'envoi de fichier
    struct ma_struct *my_struct = malloc(sizeof(struct ma_struct));
    my_struct->sockfd = sockfd;
    my_struct->fp = fp;

    pthread_t thread_envoifichier;
    pthread_create(&thread_envoifichier, NULL, send_file, (void *)my_struct);
    free(filename);
    pthread_join(thread_envoifichier, NULL);

    printf("[+]File data sent successfully.\n");

    printf("[+]Closing the connection.\n");
    shutdown(sockfd, 2);
}

int main(int argc, char *argv[])
{
    system("clear");

    // printf("Début programme\n");
    int dS = socket(PF_INET, SOCK_STREAM, 0);
    printf("Socket Créé\n");

    struct sockaddr_in aS;
    aS.sin_family = AF_INET;
    inet_pton(AF_INET, argv[1], &(aS.sin_addr));
    aS.sin_port = htons(atoi(argv[2]));
    socklen_t lgA = sizeof(struct sockaddr_in);
    connect(dS, (struct sockaddr *)&aS, lgA);
    printf("Socket Connecté\n");

    // création des threads
    pthread_t thread_envoi, thread_reception;
    int thread_id_envoi = 1, thread_id_reception = 2;
    // crréation des threads
    char *pseudo;
    pseudo = argv[3];
    printf("pseudo : %s \n", pseudo);
    send(dS, pseudo, sizeof(pseudo), 0);

    pthread_create(&thread_envoi, NULL, envoi, (void *)&dS);
    pthread_create(&thread_reception, NULL, reception, (void *)&dS);

    // attente de la fin des threads
    pthread_join(thread_envoi, NULL);
    pthread_join(thread_reception, NULL);
    shutdown(dS, 2);
    // printf("Fin du programme");
}

void *envoi(void *arg)
{
    int *dSP = (int *)arg;
    char msg[MAXTMESS];
    int fin = 0;
    FILE *man_file;
    char line[256];

    while (fin == 0)
    {
        printf("\033[1;34mVous : \033[0m");
        fgets(msg, MAXTMESS, stdin);
        msg[strlen(msg) - 1] = '\0';
        printf("\033[A\033[2K");                     // effacer la ligne de l'entrée utilisateur
        printf("\033[1;34mVous : %s\033[0m\n", msg); // afficher le message envoyé en bleu à gauche
        if (strcmp(msg, "$fin") == 0)
        {
            fin = 1;
            send(*dSP, "fin", 4, 0); // envoi du message spécial pour signaler la déconnexion
            printf("Déconnexion en cours...\n");
        }
        else if (strcmp(msg, "$send") == 0)
        {

            sendFileToServer(dSP);
        }

        // Client
        else if (strcmp(msg, "$download") == 0)
        {
            send(*dSP, "download", 9, 0);

            struct sockaddr_in addr_rcv;
            sockrcv = socket(AF_INET, SOCK_STREAM, 0);
            addr_rcv.sin_family = AF_INET;
            addr_rcv.sin_port = htons(8081);
            addr_rcv.sin_addr.s_addr = inet_addr(ip);
            int e;
            e = connect(sockrcv, (struct sockaddr *)&addr_rcv, sizeof(addr_rcv));
            if (e == -1)
            {
                perror("[-]Error in socket");
                exit(1);
            }
            char fc[MAXTMESS];
            char m[MAXTMESS];
            printf("[+]Connected to Server.\n");
            printf("Entrez le nom du fichier à télécharger\n");
            fgets(fc, MAXTMESS, stdin);
            fc[strlen(fc) - 1] = '\0'; // remove newline character

            char fullFilePath[MAXTMESS];
            snprintf(fullFilePath, sizeof(fullFilePath), "surServeur/%s", fc);

            send(sockrcv, fullFilePath, strlen(fullFilePath) + 1, 0);
            printf("envoyé \n");

            printf("Téléchargement en cours...\n");
            write_file(sockrcv);
        }

        else if (strcmp(msg, "$dispo") == 0)
        {
            send(*dSP, "dispo", 6, 0);

            struct sockaddr_in addr_rcv;
            sockrcv = socket(AF_INET, SOCK_STREAM, 0);
            addr_rcv.sin_family = AF_INET;
            addr_rcv.sin_port = htons(8081);
            addr_rcv.sin_addr.s_addr = inet_addr(ip);
            int e;
            e = connect(sockrcv, (struct sockaddr *)&addr_rcv, sizeof(addr_rcv));
            if (e == -1)
            {
                perror("[-]Error in socket");
                exit(1);
            }
            printf("[+]Connected to Server.\n");

            char fileList[2048]; // Buffer to receive the file list
            if (recv(sockrcv, fileList, sizeof(fileList), 0) > 0)
            {
                printf("Fichiers disponibles :\n%s", fileList);
            }
            shutdown(sockrcv, 2);
        }

        else if (strcmp(msg, "-man") == 0)
        {
            man_file = fopen("man.txt", "r");
            if (man_file == NULL)
            {
                perror("Erreur d'ouverture du fichier man.txt");
            }
            else
            {
                printf("\nAffichage du manuel :\n");
                while (fgets(line, sizeof(line), man_file) != NULL)
                {
                    printf("%s", line);
                }
                printf("\n");
                fclose(man_file);
            }
        }

        else
        {
            send(*dSP, msg, strlen(msg) + 1, 0);
        }
    }

    pthread_exit(NULL);
}

void *reception(void *arg)
{
    int *dSP = (int *)arg;
    char m[MAXTMESS];
    while (1)
    {
        int res = recv(*dSP, m, sizeof(m), 0);
        if (res == -1)
        {
            perror("fin de reception");
            exit(1);
        }
        if (res == 0)
        {
            printf("Déconnexion du serveur\n");
            exit(0);
        }

        // printf("\033[A\033[2K"); // effacer la ligne de l'entrée utilisateur

        printf("\033[K");
        if (m[0] == '\033')
        {                      // si le message est un message de couleur
            printf("%s\n", m); // afficher le message reçu tel quel
        }

        else
        { // sinon, afficher le message en noir à gauche
            printf("\033[0;30m%s\n", m);
        }

        printf("\033[1;34mVous : \033[0m"); // réafficher la ligne de l'entrée utilisateur en bleu
        fflush(stdout);                     // vider le buffer de sortie
    }
    pthread_exit(NULL);
}
