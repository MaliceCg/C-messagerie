#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <signal.h>
#include <sys/socket.h>
#include <dirent.h>

#define MAXNBCLIENTS 20
#define MAXTMESS 200
#define MAXPSEUDO 20
#define SIZE 1024
#define buffer_size 1024

int dSC[MAXNBCLIENTS]; //Tableau des descripteurs de socket
char pseudos[MAXNBCLIENTS][MAXPSEUDO]; //Tableau des pseudos
void *messages(void *arg); //Thread de gestion des messages
void *clean(void *arg); //Thread de nettoyage des descripteurs de socket
void traitement(int signal); //Fonction de traitement du signal
int nb_clients = 0; //Initialisation du nombre de clients connectés
int sockfd, new_sock; //Descripteurs de socket
int rcvsock, sockrcv; //Descripteurs de socket
struct sockaddr_in rcvaddr, addrrcv; //Structure d'adresse pour le socket de réception
struct sockaddr_in server_addr, new_addr; //Structure d'adresse pour le socket principal
socklen_t addr_size; //Taille de la structure d'adresse
socklen_t rcvaddr_size = sizeof(addrrcv); //Taille de la structure d'adresse
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER; //Mutex pour la gestion des threads

void write_file(int sockfd) //Fonction d'écriture de fichier
//prend en paramètre la socket qui gère les fichiers
{
    int n; //voir erreur du recv
    FILE *fp; //pointeur de fichier
    char buffer[SIZE]; //buffer de réception
    char filename[SIZE]; //buffer de réception du nom de fichier

    // Réception du nom du fichier
    recv(sockfd, filename, SIZE, 0); 

    // Génération du chemin du fichier de destination
    char filepath[SIZE + 12];
    snprintf(filepath, SIZE + 12, "surServeur/%s", filename);

    // Ouverture du fichier
    fp = fopen(filepath, "wb");
    if (fp == NULL) //Si le fichier n'existe pas
    {
        perror("[-]Error in opening file.");
        return;
    }

    printf("Reception du fichier %s\n", filename);

    
    while (1)
    {
        n = recv(sockfd, buffer, SIZE, 0); //Réception du fichier


        if (n <= 0) //Si le fichier est vide
        {
            break;
            return;
        }

        fwrite(buffer, 1, n, fp); //Ecriture du fichier
    }
    fclose(fp); //Fermeture du fichier
    return;
}

int main(int argc, char *argv[])
{


    for (int i = 0; i < MAXNBCLIENTS; i++) //Initialisation des descripteurs de socket et des pseudos
    {
        dSC[i] = -1; //Initialisation des descripteurs de socket
        pseudos[i][0] = '\0'; //Initialisation des pseudos
    }

    int dS = socket(PF_INET, SOCK_STREAM, 0); //Création du socket
    printf("Socket Créé\n");

    struct sockaddr_in ad; //Structure d'adresse
    ad.sin_family = AF_INET; //Famille d'adresse
    ad.sin_addr.s_addr = INADDR_ANY; //Adresse IP
    ad.sin_port = htons(atoi(argv[1])); //Port
    bind(dS, (struct sockaddr *)&ad, sizeof(ad)); //Bind du socket

    printf("Socket Nommé\n");

    listen(dS, MAXNBCLIENTS); //Mise en écoute du socket

    printf("Mode écoute\n");

    signal(SIGINT, traitement); //Traitement du signal CTRLC
    int e; //Variable d'erreur

    char *ip = "127.0.0.1"; //Adresse IP
    int port = 8080; //Port

    char buffer[SIZE]; //Buffer de réception

    sockfd = socket(AF_INET, SOCK_STREAM, 0); //Création du socket
    if (sockfd < 0) //Si le socket n'a pas été créé
    {
        perror("[-]Error in socket");
        exit(1);
    }
    printf("[+]Server socket created successfully.\n");

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = port;
    server_addr.sin_addr.s_addr = inet_addr(ip);

    e = bind(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr));
    if (e < 0)
    {
        perror("[-]Error in bind");
        exit(1);
    }
    printf("[+]Binding successfull.\n");

    if (listen(sockfd, 10) == 0)
    {
        printf("[+]Listening....\n");
    }
    else
    {
        perror("[-]Error in listening");
        exit(1);
    }

    rcvsock = socket(AF_INET, SOCK_STREAM, 0); //Création du socket fichier
    if (rcvsock == -1) //Si le socket n'a pas été créé
    {
        perror("Erreur de création de socket");
        pthread_exit(NULL);
    }
    printf("socket créée\n");
    rcvaddr.sin_family = AF_INET;
    rcvaddr.sin_port = htons(8081);
    rcvaddr.sin_addr.s_addr = INADDR_ANY;
    sockrcv = bind(rcvsock, (struct sockaddr *)&rcvaddr, sizeof(rcvaddr));
    if (sockrcv == -1)
    {
        perror("Erreur de bind");
        pthread_exit(NULL);
    }
    printf("bind réussi\n");
    sockrcv = listen(rcvsock, 5);
    if (sockrcv == -1)
    {
        perror("Erreur de listen");
        pthread_exit(NULL);
    }
    printf("listen réussi\n");

    struct sockaddr_in aC[MAXNBCLIENTS];
    socklen_t lg = sizeof(struct sockaddr_in);

    int nbclient = 1;
    pthread_t thread[MAXNBCLIENTS]; //Tableau de threads des clients
    pthread_t thread_clean; //Thread de nettoyage

    // autorise la connexion du client, toc toc toc
    while (1)
    {
        if (nbclient < MAXNBCLIENTS)
        {
            pthread_mutex_lock(&mutex); //Verrouillage du mutex pour modifier le tableau de socket
            dSC[nbclient] = accept(dS, (struct sockaddr *)&aC[nbclient], &lg); //Acceptation de la connexion
            pthread_mutex_unlock(&mutex); //Déverrouillage du mutex

            int res = recv(dSC[nbclient], pseudos[nbclient], sizeof(pseudos[nbclient]), 0); // reception du pseudo

            for (int i = 0; i < nbclient; i++) //Vérification de l'unicité du pseudo
            {

                while (strcmp(pseudos[nbclient], pseudos[i]) == 0)
                {
                    send(dSC[nbclient], "Pseudo déjà existant, veuillez en choisir un autre : ", 56, 0);
                    res = recv(dSC[nbclient], pseudos[nbclient], sizeof(pseudos[nbclient]), 0); // réception du nouveau pseudo
                }
            }
            printf("Client connecté %d sur %d, pseudo : %s\n", nbclient, MAXNBCLIENTS, pseudos[nbclient]);

            nb_clients++; //Incrémentation du nombre de clients connectés
            printf("nombre de client connectés en ce moment : %d\n", nb_clients);
            pthread_t thread; //Création du thread
            int thread_id = nbclient;
            pthread_create(&thread, NULL, messages, (void *)thread_id); //Lancement du thread

            nbclient++;
        }
        else
        {
            printf("%d clients sont déjà connectés, connexion impossible \n", nb_clients);
            break;
        }
    }

    // attente de la fin des threads
    for (int i = 0; i < nbclient; i++)
    {
        pthread_join(thread[i], NULL);
    }
    // fermeture des sockets
    for (int i = 0; i < nbclient; i++)
    {
        shutdown(dSC[i], 2);
    }
    pthread_create(&thread_clean, NULL, clean, (void *)thread);
    pthread_join(thread_clean, NULL);
    shutdown(dS, 2);
    printf("Fin du programme");
}

void *clean(void *arg) //Thread de nettoyage
//prend en paramètre le tableau de socket

{
    while (nb_clients == 0)
    {
        for (int i = 0; i < MAXNBCLIENTS; i++) //Parcours du tableau de socket
        {
            if (dSC[i] == -1) //Si le socket est vide
            {
                for (int j = i; j < MAXNBCLIENTS - 1; j++)
                {
                    pthread_mutex_lock(&mutex);
                    dSC[j] = dSC[j + 1];     //Décalage des sockets
                    pthread_mutex_unlock(&mutex);
                    strcpy(pseudos[j], pseudos[j + 1]); //Décalage des pseudos
                }
                dSC[MAXNBCLIENTS - 1] = -1; //Remplissage du dernier socket avec -1
                pseudos[MAXNBCLIENTS - 1][0] = '\0'; //Remplissage du dernier pseudo avec \0
            }
        }
    }
    pthread_exit(NULL);
}

void *messages(void *arg)
// prend en paramètre l'indice du tableau de socket
// gère l'nevoi et reception de messages
{
    int ind = (long)arg; // indice du tableau de socket
    int dSCp = dSC[ind]; // socket du client
    char msg[MAXTMESS]; // message à envoyer
    int fin = 0; // fin de connexion

    while (fin == 0)
    {
        int res = recv(dSCp, msg, sizeof(msg), 0); // réception du message  du client
        printf("message reçu : %s\n", msg);
        if (res == -1)
        {
            perror("Erreur de réception");
            pthread_exit(NULL);
        }
        printf("%d", res);

        if (res == 0)
        { // déconnexion du client
            printf("déconnecté\n");
            shutdown(dSCp, 2); // fermeture du dSC du client

            dSC[ind] = -1;          // on vide le dSC du client
            pseudos[ind][0] = '\0'; // on vide le pseudo du client
            nb_clients--;
            printf("nombre de client connectés en ce moment : %d\n", nb_clients);
            pthread_exit(NULL);
        }

        if (strncmp(msg, "$", 1) == 0)
        {                                                 // message privé
            char *dest_pseudo_str = strtok(msg + 1, " "); // récupère le pseudo du destinataire, +1 pour sauter le $ et " " pour séparer le pseudo du message privé
            char *priv_msg = strtok(NULL, "");            // récupère le message privé, NULL pour continuer à chercher dans la chaine msg
            int dest_dSC = -1;
            for (int i = 0; i < MAXNBCLIENTS; i++)
            { // cherche le dSC du destinataire
                if (dSC[i] != -1 && strncmp(pseudos[i], dest_pseudo_str, MAXPSEUDO) == 0)
                { // si le dSC n'est pas vide et que le pseudo correspond
                    // pthread_mutex_lock(&mutex);
                    dest_dSC = dSC[i]; // on récupère le dSC du destinataire
                    // pthread_mutex_unlock(&mutex);
                    break;
                }
            }
            if (dest_dSC != -1)
            { // si le destinataire est connecté
                char msg_priv[MAXTMESS + 20];
                // position
                int len_msg_priv = strlen(msg_priv);
                int width = 80;
                int n_cols_right = width - len_msg_priv;
                // fin position

                sprintf(msg_priv, "\033[%dC\033[31mmp de %s : %s\033[0m", n_cols_right, pseudos[ind], priv_msg); // msg_priv = "[pseudo] message privé" //priv_msg = "message privé"
                send(dest_dSC, msg_priv, strlen(msg_priv) + 1, 0);                                               // on envoie le message privé
                printf("\033[1;36mMessage privé envoyé de %s à %s : %s\033[0m\n", pseudos[ind], dest_pseudo_str, priv_msg);
            }
            else
            {                                                             // sinon
                send(dSCp, "Le destinataire n'a pas été trouvé ", 39, 0); // on envoie un message d'erreur
                printf("Destinataire non trouvé pour le message privé : %s\n", msg);
            }
        }
 
        else if (strcmp(msg, "/send") == 0) //Si le client veut envoyer un fichier
        {
            addr_size = sizeof(new_addr);
            printf("En attente de connexion...\n");
            new_sock = accept(sockfd, (struct sockaddr *)&new_addr, &addr_size); //Acceptation de la connexion
            write_file(new_sock); //Appel de la fonction write_file
            printf("[+]Data written in the file successfully.\n");
            for (int i = 0; i < MAXNBCLIENTS; i++)
            {
                if (i != ind && dSC[i] != -1)
                {
                    send(dSC[i], "Un fichier a été ajouté au serveur.\n", 39, 0); //Envoi d'un message à tous les clients
                }
            }
        }
        // Serveur
        else if (strcmp(msg, "download") == 0) //Si le client veut télécharger un fichier
        {

            printf("download\n");
            sockrcv = accept(rcvsock, (struct sockaddr *)&addrrcv, &rcvaddr_size); //Acceptation de la connexion
            if (sockrcv == -1)
            {
                perror("Erreur de accept");
                pthread_exit(NULL);
            }
            printf("accept réussi\n");
            char nomfc[MAXTMESS];
            printf("avant\n");
            int e = recv(sockrcv, nomfc, sizeof(nomfc), 0);
            if (e == -1)
            {
                perror("Erreur de réception");
                break;
            }
            printf("nom du fichier reçu : %s\n", nomfc);
            send(sockrcv, nomfc, strlen(nomfc) + 1, 0); // Send the filename to the client
            FILE *f;    // File pointer
            f = fopen(nomfc, "rb"); // Ouverture du fihcier en binaire
            if (f == NULL)
            {

                perror("Erreur d'ouverture du fichier");
                pthread_exit(NULL);
            }

            printf("fichier ouvert\n");

            char *data = malloc(buffer_size); //Buffer pour mettre les données du fichier
            int nb_octets_lus = 0; //Nombre d'octets lus
            while ((nb_octets_lus = fread(data, 1, buffer_size, f)) > 0) //Tant qu'il y a des octets à lire
            {
              /* printf("nb_octets_lus : %d\n", nb_octets_lus); //Affichage du nombre d'octets lus*/

                int nb_octets_env = send(sockrcv, data, nb_octets_lus, 0);
                if (nb_octets_env == -1)
                {
                    perror("Erreur d'envoi");
                    break;
                }
            }
            printf("fichier envoyé\n");
            fclose(f); //Fermeture du fichier
            shutdown(sockrcv, 2); //Fermeture du socket
            shutdown(rcvsock, 2); //Fermeture du socket de réception
        }

        else if (strcmp(msg, "list") == 0) //Si le client veut lister les fichiers
        {
            sockrcv = accept(rcvsock, (struct sockaddr *)&addrrcv, &rcvaddr_size); //Acceptation de la connexion
            if (sockrcv == -1)
            {
                perror("Erreur de accept");
                pthread_exit(NULL);
            }

            printf("listing files\n"); //Affichage sur le serveur
            DIR *d; // Directory pointer
            struct dirent *dir; // Directory entry pointer
            char fileList[2048] = ""; // Buffer to store the file list
            d = opendir("surServeur"); // Open the directory
            if (d)
            {
                int file_index = 1;
                while ((dir = readdir(d)) != NULL) // Read the directory entries
                {
                    if (dir->d_type == DT_REG) // If the entry is a regular file
                    { // only regular files
                        char file_entry[SIZE]; // Buffer to store the file entry
                        snprintf(file_entry, SIZE, "%d. %s\n", file_index, dir->d_name); // Format the file entry string
                        strncat(fileList, file_entry, sizeof(fileList) - strlen(fileList) - 1); // Add the file entry to the list
                        file_index++;
                    }
                }
                closedir(d);           // Close the directory
            }
            send(rcvsock, fileList, strlen(fileList) + 1, 0); // Send the complete file list
            shutdown(rcvsock, 2);
            // shutdown(rcvsock, 2); @@@@@@@@@@@@@@@@@@@@@@@@@@ g commenté ca @@@@@@@@@@@@@@
        }

        else if (strcmp(msg, "dispo") == 0) //lister les fichiers disponibles pour le telechrgment
        {
            sockrcv = accept(rcvsock, (struct sockaddr *)&addrrcv, &rcvaddr_size); //Acceptation de la connexion
            if (sockrcv == -1)
            {
                perror("Erreur de accept");
                pthread_exit(NULL);
            }

            DIR *d;
            struct dirent *dir;
            char fileList[2048] = ""; // Buffer to store the file list
            d = opendir("surServeur");
            if (d)
            {
                int file_index = 1;
                while ((dir = readdir(d)) != NULL)
                {
                    if (dir->d_type == DT_REG) // only regular files
                    {
                        char file_entry[SIZE];
                        snprintf(file_entry, SIZE, "%d. %s\n", file_index, dir->d_name);
                        strncat(fileList, file_entry, sizeof(fileList) - strlen(fileList) - 1); // Add the file entry to the list
                        file_index++;
                    }
                }
                closedir(d);
            }
            send(sockrcv, fileList, strlen(fileList) + 1, 0); // Send the complete file list
            shutdown(sockrcv, 2);
        }

        else
        { // message public
            for (int i = 0; i < MAXNBCLIENTS; i++)//envoi du message à tous les clients
            {
                if (i != ind && dSC[i] != -1)
                {
                    char msg_pub[MAXTMESS + 20];
                    // position
                    int len_msg_pub = strlen(msg_pub); // longueur du message
                    int width = 80; // largeur du terminal
                    int n_cols_right = width - len_msg_pub; // nombre de colonnes à droite
                    // fin position
                    sprintf(msg_pub, "\033[%dC\033[32mDe %s à all: %s\033[0m", n_cols_right, pseudos[ind], msg); // message à envoyer
                    send(dSC[i], msg_pub, strlen(msg_pub) + 1, 0); // envoi du message
                    /*if (rep == -1) {
                      perror("Erreur d'envoi");
                      pthread_exit(NULL);
                    }*/
                    printf("\033[1;30mMessage public reçu de %s : %s\033[0m\n", pseudos[ind], msg);
                    printf("\033[1;36mMessage public envoyé à %s : %s\033[0m\n", pseudos[i], msg);
                }
            }

            if (strcmp(msg, "fin") == 0) //Si le client veut se déconnecter
            {
                printf("déconnecté\n");
                shutdown(dSCp, 2); // fermeture du dSC du client

                dSC[ind] = -1;          // on vide le dSC du client
                pseudos[ind][0] = '\0'; // on vide le pseudo du client
                nb_clients--;
                printf("nombre de client connectés en ce moemnt : %d\n", nb_clients);
                pthread_exit(NULL);
            }
        }
    }
    pthread_exit(NULL);
}

void traitement(int signal)
{

    char *msg = "Le serveur s'est arrêté\n"; // message à envoyer
    for (int i = 0; i < MAXNBCLIENTS; i++) // envoi du message à tous les clients
    {
        send(dSC[i], msg, strlen(msg), 0);
        shutdown(dSC[i], 2);
    }
    exit(0);
}
