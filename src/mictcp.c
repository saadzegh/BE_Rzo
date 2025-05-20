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
mic_tcp_sock liste_sockets[MAXIMUM_SOCKETS];
int compteur_socket = 0;
int sockets_crees[MAXIMUM_SOCKETS];
int numSeqPE[MAXIMUM_SOCKETS] = {0};
int numSeqPA[MAXIMUM_SOCKETS] = {0};

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

// Accept et Connect identique car pas d'acquittement de la données

/*
 * Met le socket en état d'acceptation de connexions
 * Retourne 0 si succès, -1 si erreur
 */

int mic_tcp_accept(int socket, mic_tcp_sock_addr* addr)
{
    printf("[MIC-TCP] Appel de la fonction: ");  printf(__FUNCTION__); printf("\n");
    
    if (!socket_exist(socket))
        return -1;

    liste_sockets[socket].remote_addr = *addr;
    liste_sockets[socket].state = ESTABLISHED;
    numSeqPE[MAXIMUM_SOCKETS] = 0;
    numSeqPA[MAXIMUM_SOCKETS] = 0;
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

    liste_sockets[socket].remote_addr = addr;
    liste_sockets[socket].state = ESTABLISHED;
    numSeqPE[MAXIMUM_SOCKETS] = 0;
    numSeqPA[MAXIMUM_SOCKETS] = 0;
    return 0;
}

/*
 * Permet de réclamer l’envoi d’une donnée applicative
 * Retourne la taille des données envoyées, et -1 en cas d'erreur
 */
int mic_tcp_send (int mic_sock, char* mesg, int mesg_size)
{  
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
    // partie header
    pdu_envoyer.header.dest_port = liste_sockets[mic_sock].remote_addr.port;
    pdu_envoyer.header.source_port = liste_sockets[mic_sock].local_addr.port;
    pdu_envoyer.header.syn = 0;
    // partie données utile
    pdu_envoyer.payload.data = mesg;
    pdu_envoyer.payload.size = mesg_size;

    //verifie la récéption de l'ack
    while (!ack_recu) 
    {
        int envoye = IP_send(pdu_envoyer, liste_sockets[mic_sock].remote_addr.ip_addr);
        if (envoye == -1)
        {
            continue;
        } 

        printf("On essaye d'envoyer un message : %d", numSeqPE[mic_sock]);
        printf(__FUNCTION__);
        printf("\n");
    
        mic_tcp_pdu pdu_recevoir;

        int recu = IP_recv(&pdu_recevoir, &liste_sockets[mic_sock].local_addr.ip_addr, &liste_sockets[mic_sock].remote_addr.ip_addr, 1000); 
        if ((recu != -1) &&  (pdu_recevoir.header.ack_num == numSeqPE[mic_sock]))
        {
            ack_recu =  1;
        }
    }
        printf("Message %d envoyé : ", numSeqPE[mic_sock]);
        printf(__FUNCTION__);
        printf("\n");
        return 0;
    }


 /*
 // On définit un temps maximal
    unsigned long temps_max;
    unsigned long debut_timer= get_now_time_msec();
    while ((get_now_time_msec() - debut_timer) < temps_max)
    {


    }

    return IP_send(pdu_envoyer, liste_sockets[mic_sock].remote_addr.ip_addr);
*/

/*
int mic_tcp_send_ANCIEN (int mic_sock, char* mesg, int mesg_size)
{   // verifier si le socket auquel on veut envoyer l'information existe sinon -1
  // on prend le socket disponible 
    liste_sockets[compteur_socket].fd = mic_sock;
 // envoyer le message 
    // creation d'un PDU
    mic_tcp_pdu pdu_envoyer;
    // partie données utile
    pdu_envoyer.payload.data = mesg;
    pdu_envoyer.payload.size = mesg_size;
    // partie header
    pdu_envoyer.header.source_port = liste_sockets[compteur_socket].local_addr.port;
    pdu_envoyer.header.dest_port = liste_sockets[compteur_socket].remote_addr.port;
   // recuperation de l'adresse ip a laquelle on envoie
   mic_tcp_ip_addr addresse_ip_dist = liste_sockets[compteur_socket].remote_addr.ip_addr;
    // changer num seq
    pdu_envoyer.header.seq_num++;
    // retourner la taille du message



    printf("[MIC-TCP] Appel de la fonction: "); printf(__FUNCTION__); printf("\n");
    return IP_send(pdu_envoyer,addresse_ip_dist);
}

*/


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
        printf("On a reçu un ack");
        return;
    }

    int socket = 0;
    if (socket == -1)
    {
        printf("Pas de chaussette pour cette adresse..."); printf(__FUNCTION__); printf("\n");
        return;
    }


    if (pdu.header.seq_num == numSeqPA[socket])
    {
        printf("Maj du num séquence %d", numSeqPA[socket]);
        numSeqPA[socket] = (numSeqPA[socket]+1) % 2;
        app_buffer_put(pdu.payload);
    }

    mic_tcp_pdu acka40send = {0};
    acka40send.header.source_port = pdu.header.source_port;
    acka40send.header.dest_port = pdu.header.dest_port;
    acka40send.header.seq_num = pdu.header.seq_num;
    acka40send.header.ack_num = pdu.header.ack_num;
    acka40send.header.syn = pdu.header.syn;
    acka40send.header.ack = pdu.header.ack;
    acka40send.payload.data = pdu.payload.data;
    acka40send.payload.size = pdu.payload.size;

    printf("On envoie le ack %d", numSeqPA[socket]); printf(__FUNCTION__); printf("\n");
    //int accent = -1;

    IP_send(acka40send, remote_addr);
    printf("Ack envoyé, c'est pesé"); printf(__FUNCTION__); printf("\n");
    
}
