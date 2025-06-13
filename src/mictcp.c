#include <mictcp.h>
#include <api/mictcp_core.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

//V2 Actuelle : socket(), accept(), connect()



/*---------------------------------------------------------------------------------
                                    Variables
----------------------------------------------------------------------------------*/
#define MAXIMUM_SOCKETS 69 
#define PORT 5000
#define TIMER 52
#define TAILLE_FENETRE_MAX 10

mic_tcp_sock liste_sockets[MAXIMUM_SOCKETS];
int compteur_socket = 0;
int sockets_crees[MAXIMUM_SOCKETS];
int numSeqPE[MAXIMUM_SOCKETS] = {0};
int numSeqPA[MAXIMUM_SOCKETS] = {0};
int fenetre[TAILLE_FENETRE_MAX];
int fenetre_globale[MAXIMUM_SOCKETS];
int taille_fenetre = 6;
#define TAILLE_FENETRE 10
#define SEUIL 0
int reliabilityIndex[MAXIMUM_SOCKETS] = {0};
int reliabilityWindow[MAXIMUM_SOCKETS][TAILLE_FENETRE] = {{0}};
pthread_mutex_t mutexs[MAXIMUM_SOCKETS];
/*---------------------------------------------------------------------------------
                                Fonctions Persos
----------------------------------------------------------------------------------*/

int socket_exist(int nbr)
{
    if (nbr >= 0 && nbr < MAXIMUM_SOCKETS && sockets_crees[nbr] == 1)
    {
            return -1;
    }
    return 0;
}

int definition_pertes_admissibles(int *fenetre, int taille_fenetre, int seuil) 
{
int total =0;
for (int i =0; i< taille_fenetre;i++)
{
    total = total + fenetre[i];
}
    int pourcentage = (total*100)/taille_fenetre;
    printf("purcentage %d \n", pourcentage);
    return pourcentage > seuil;

}

void send_ack(int socket, mic_tcp_pdu pdu, mic_tcp_ip_addr local_addr, mic_tcp_ip_addr remote_addr)
{
 if (pdu.header.seq_num == numSeqPA[socket])
    {

        numSeqPA[socket] = (numSeqPA[socket]+1) % 2;
        printf("Maj du num séquence %d \n", numSeqPA[socket]);
        app_buffer_put(pdu.payload);
    }
 
    mic_tcp_pdu acka40send = {0};
    acka40send.header.source_port = pdu.header.source_port;
    acka40send.header.dest_port = pdu.header.dest_port;
    acka40send.header.seq_num = 0;
    acka40send.header.ack_num = numSeqPA[socket];
    acka40send.header.syn = 0;
    acka40send.header.ack = 1;
    acka40send.payload.data = "";
    acka40send.payload.size = 0;

    printf("Num sequ du pdu %d \n", pdu.header.seq_num); printf(__FUNCTION__); printf("\n");
    printf("On envoie le ack %d \n", numSeqPA[socket]); printf(__FUNCTION__); printf("\n");
    //int accent = -1;

    IP_send(acka40send, remote_addr);
    printf("Retour IP_Send : %d : \n", IP_send(acka40send, remote_addr));
    printf("Ack envoyé, c'est pesé \n"); printf(__FUNCTION__); printf("\n");
}

    int definition_seuil_dynamique(int socket) {
         if (!socket_exist(socket))
    {
        return -1;
    }
    
    if (liste_sockets[socket].state != ESTABLISHED)
    {
        return -1;
    }
    // creation d'un PDU
    mic_tcp_pdu pdu_envoyer;
    int cpt_ack_recu = 0;
    int nb_paquets_envoyes=10;
    // partie header
    pdu_envoyer.header.dest_port = liste_sockets[socket].remote_addr.port;
    pdu_envoyer.header.source_port = liste_sockets[socket].local_addr.port;
    pdu_envoyer.header.syn = 0;
    // partie données utile
    pdu_envoyer.payload.data = "";
    pdu_envoyer.payload.size =0;
    for (int i=0;i<=nb_paquets_envoyes;i++) {
        //verifie la récéption de l'ack
   
        int envoye = IP_send(pdu_envoyer, liste_sockets[socket].remote_addr.ip_addr);
        if (envoye == -1)
        {
            continue;
        } 

        printf("On essaye d'envoyer un message : %d", numSeqPE[socket]);
        printf(__FUNCTION__);
        printf("\n");
    
        mic_tcp_pdu pdu_recevoir;

        int recu = IP_recv(&pdu_recevoir, &liste_sockets[socket].local_addr.ip_addr, &liste_sockets[socket].remote_addr.ip_addr, 1000); 
        if ((recu != -1) &&  (pdu_recevoir.header.ack_num == numSeqPE[socket]))
        {
            cpt_ack_recu++;
        }
        printf("Message %d envoyé : ", numSeqPE[socket]);
        printf(__FUNCTION__);
        printf("\n");

    }

        return (100 - (cpt_ack_recu * 100) / nb_paquets_envoyes);
    }

    
/*void mise_en_place_fenetre(int taille_fenetre) {
    fenetre[taille_fenetre];
}*/

/*---------------------------------------------------------------------------------
                                  Fonctions TCP
----------------------------------------------------------------------------------*/

/*
 * Permet de créer un socket entre l’application et MIC-TCP
 * Retourne le descripteur du socket ou bien -1 en cas d'erreur
 */
int mic_tcp_socket(start_mode sm)
{
   int result = -1;
   printf("[MIC-TCP] Appel de la fonction: ");  printf(__FUNCTION__); printf("\n");
   result = initialize_components(sm); /* Appel obligatoire */
    if (result == -1)
    {
        printf("[MIC-TCP] ERROR while initializing components\n");
        return -1;
    }
   set_loss_rate(0);

    if (compteur_socket >= MAXIMUM_SOCKETS)
    {
        printf("[MIC-TCP] ERROR: On ne peut crééer que %d sockets\n", MAXIMUM_SOCKETS);
        return -1;   
    }

    mic_tcp_sock nouveau_socket;
    nouveau_socket.fd = compteur_socket;
    nouveau_socket.state  = CLOSED;
    nouveau_socket.local_addr.port = PORT + compteur_socket;
    nouveau_socket.local_addr.ip_addr.addr = "localhost";
    nouveau_socket.local_addr.ip_addr.addr_size = strlen(nouveau_socket.local_addr.ip_addr.addr);

    liste_sockets[compteur_socket] = nouveau_socket;
    sockets_crees[compteur_socket] = 1;
    compteur_socket++;

    return nouveau_socket.fd;
}


/*
 * Permet d’attribuer une adresse à un socket.
 * Retourne 0 si succès, et -1 en cas d’échec
 */
int mic_tcp_bind(int socket, mic_tcp_sock_addr addr) //adresse locale
{
    printf("[MIC-TCP] Appel de la fonction: ");  printf(__FUNCTION__); printf("\n");
    for (int i=0;i<compteur_socket;i++){
        if (addr.port == liste_sockets[i].local_addr.port && strcmp(addr.ip_addr.addr, liste_sockets[i].local_addr.ip_addr.addr) == 0){
            return -1;
        }
    }
    liste_sockets[compteur_socket].fd = socket;
    liste_sockets[compteur_socket].local_addr.ip_addr = addr.ip_addr;
    liste_sockets[compteur_socket].local_addr.port = addr.port;
    compteur_socket += 1;
   return 0;
}

/*
 * Met le socket en état d'acceptation de connexions
 * Retourne 0 si succès, -1 si erreur
 */

int mic_tcp_accept(int socket, mic_tcp_sock_addr* addr)
{
    printf("[MIC-TCP] Appel de la fonction: ");  printf(__FUNCTION__); printf("\n");
    
    if (!socket_exist(socket))
        return -1;

    numSeqPE[MAXIMUM_SOCKETS] = 0;
    numSeqPA[MAXIMUM_SOCKETS] = 0;
//faire un thread qui envoie des SYN ACK reguliers jusqu a procxess pdu reveived
//pthread_mutex_lock(&mutexs[socket]);
while (liste_sockets[socket].state != ESTABLISHED)
{
    //pthread_mutex_lock(&mutexs[socket]);
}
    //pthread_mutex_unlock(&mutexs[socket]);
    *addr = liste_sockets[socket].remote_addr;
    

    return 0;
}

/*
 * Permet de réclamer l’établissement d’une connexion
 * Retourne 0 si la connexion est établie, et -1 en cas d’échec
 */
int mic_tcp_connect(int socket, mic_tcp_sock_addr addr)
{
    printf("[MIC-TCP] Appel de la fonction: ");  printf(__FUNCTION__); printf("\n");
    
    if (!socket_exist(socket))
    {
        return -1;
    }
    mic_tcp_pdu pdu_syn;
    pdu_syn.header.dest_port = liste_sockets[socket].remote_addr.port;
    pdu_syn.header.source_port = liste_sockets[socket].local_addr.port;
    pdu_syn.header.syn = 1;
    pdu_syn.header.seq_num =0 ;
    pdu_syn.header.ack_num = 0;
    pdu_syn.header.fin = 0;
    pdu_syn.payload.data = "";
    pdu_syn.payload.size = 0;

    liste_sockets[socket].remote_addr = addr;
    liste_sockets[socket].state = ESTABLISHED;
    liste_sockets[socket].seuil_perte=definition_seuil_dynamique(socket);
    numSeqPE[MAXIMUM_SOCKETS] = 0;
    numSeqPA[MAXIMUM_SOCKETS] = 0;
    return 0;
}

/*
 * Permet de réclamer l’envoi d’une donnée applicative
 * Retourne la taille des données envoyées, et -1 en cas d'erreur
 */


int mic_tcp_send (int mic_sock, char* mesg, int mesg_size)
{  int seuil = liste_sockets[mic_sock].seuil_perte;
    // verifier si le socket auquel on veut envoyer l'information existe sinon -1


    if (!socket_exist(mic_sock))
    {
        return -1;
    }
    
    if (liste_sockets[mic_sock].state != ESTABLISHED)
    {
        return -1;
    }

    // creation d'un PDU
    mic_tcp_pdu pdu_envoyer;
    int ack_recu = 0;
    int envoye = 0;
    // partie header
    pdu_envoyer.header.dest_port = liste_sockets[mic_sock].remote_addr.port;
    pdu_envoyer.header.source_port = liste_sockets[mic_sock].local_addr.port;
    pdu_envoyer.header.syn = 0;
    pdu_envoyer.header.seq_num = numSeqPE[mic_sock];
    pdu_envoyer.header.ack_num = 0;
    pdu_envoyer.header.fin = 0;
    //pdu_envoyer.header.ack = 0;
    // partie données utile
    pdu_envoyer.payload.data = mesg;
    pdu_envoyer.payload.size = mesg_size;

    numSeqPE[mic_sock] = (numSeqPE[mic_sock]+1) % 2;
    //verifie la récéption de l'ack
    
    while (!ack_recu) 
    {
        envoye = IP_send(pdu_envoyer, liste_sockets[mic_sock].remote_addr.ip_addr);

        printf("On essaye d'envoyer un message : %d \n", numSeqPE[mic_sock]);
        printf(__FUNCTION__);
        printf("\n");
    
        mic_tcp_pdu pdu_recevoir = {0};

        mic_tcp_ip_addr local_addr ={0}, remote_addr ={0};
        local_addr.addr_size = 0;
        remote_addr.addr_size = 0;
        printf("!!!!!!!!!!!!!!!!! ON ATTEND LE ACK !!!!!!!!!!!!!!!!\n");

        int recu = IP_recv(&pdu_recevoir, &liste_sockets[mic_sock].local_addr.ip_addr, &liste_sockets[mic_sock].remote_addr.ip_addr, 1000); 
        printf("Retour IP_Recv : %d \n", recu);
        printf("ack num pdu rec : %d \n", pdu_recevoir.header.ack_num);
        printf("num seq emission: %d \n", numSeqPE[mic_sock]);
        
        if (recu != -1) 
        { 
            if (pdu_recevoir.header.ack_num == numSeqPE[mic_sock] && pdu_recevoir.header.ack == 1)
            {
                ack_recu =  1;
                reliabilityWindow[mic_sock][reliabilityIndex[mic_sock]++] = 1;
                printf("Fentre avancée \n");
            }
            else
            {
                reliabilityWindow[mic_sock][reliabilityIndex[mic_sock]++] = 0;
                printf("Fentre pas marché, et ack reçu \n");
            }
        }   
        else
        {
            reliabilityWindow[mic_sock][reliabilityIndex[mic_sock]++] = 0;
        }
        reliabilityIndex[mic_sock]=reliabilityIndex[mic_sock]%TAILLE_FENETRE;
        printf("Index, %d \n", reliabilityIndex[mic_sock]);

        if (recu == 0)
        {
            if (definition_pertes_admissibles(reliabilityWindow[mic_sock], TAILLE_FENETRE, seuil))
            {
                printf("ACK perdu \n");
                numSeqPE[mic_sock] = (numSeqPE[mic_sock] + 1) % 2;
                recu = 1;
            }
        }
    }
        printf("Message %d envoyé\n", numSeqPE[mic_sock]);
        printf(__FUNCTION__);
        printf("\n");
        return envoye;
    }

/*
 * Permet à l’application réceptrice de réclamer la récupération d’une donnée
 * stockée dans les buffers de réception du socket
 * Retourne le nombre d’octets lu ou bien -1 en cas d’erreur
 * NB : cette fonction fait appel à la fonction app_buffer_get()
 */

int mic_tcp_recv(int socket, char *mesg, int max_mesg_size)
{
    printf("[MIC-TCP] Appel de la fonction: "); printf(__FUNCTION__); printf("\n");

    if (!socket_exist(socket))
    {
        return -1;
    }
    
    mic_tcp_payload payload;
    payload.data = mesg;
    payload.size = max_mesg_size;
    
    return app_buffer_get(payload);
}

/*
 * Permet de réclamer la destruction d’un socket.
 * Engendre la fermeture de la connexion suivant le modèle de TCP.
 * Retourne 0 si tout se passe bien et -1 en cas d'erreur
 */
int mic_tcp_close (int socket)
{
    printf("[MIC-TCP] Appel de la fonction :  "); printf(__FUNCTION__); printf("\n");
    liste_sockets[compteur_socket].fd = socket;
    if (!socket_exist(compteur_socket))
    {
        return -1;
    }
    liste_sockets[compteur_socket].state = CLOSED;
    sockets_crees[compteur_socket] = 0;
    return 0;
}

/*
 * Traitement d’un PDU MIC-TCP reçu (mise à jour des numéros de séquence
 * et d'acquittement, etc.) puis insère les données utiles du PDU dans
 * le buffer de réception du socket. Cette fonction utilise la fonction
 * app_buffer_put().
 */
void process_received_PDU(mic_tcp_pdu pdu, mic_tcp_ip_addr local_addr, mic_tcp_ip_addr remote_addr)
{
    printf("[MIC-TCP] Appel de la fonction: "); printf(__FUNCTION__); printf("\n");


    if (pdu.header.ack == 1)
    {
        printf("On a reçu un ack \n");
        return;
    }

    int socket = 0;
    if (socket == -1)
    {
        printf("Pas de socket pour cette adresse... \n"); printf(__FUNCTION__); printf("\n");
        return;
    }

    printf("Num sequ du pdu %d \n", pdu.header.seq_num);
    printf("Num sequ acquittement %d \n", numSeqPA[socket]);
   
    send_ack(socket, pdu, local_addr, remote_addr);
}
