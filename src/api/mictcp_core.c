#include <api/mictcp_core.h>
#include <sys/time.h>
#include <sys/queue.h>
#include <math.h>
#include <time.h>
#include <pthread.h>
#include <strings.h>

/*****************
 * API Variables *
 *****************/
int initialized = -1;
int sys_socket;
pthread_t listen_th;
pthread_mutex_t lock;
unsigned short  loss_rate = 0;
struct sockaddr_in remote_addr;

/* This is for the buffer */
TAILQ_HEAD(tailhead, app_buffer_entry) app_buffer_head;
struct tailhead *headp;
struct app_buffer_entry {
     mic_tcp_payload bf;
     TAILQ_ENTRY(app_buffer_entry) entries;
};
//aider par IA GÉnÉrative
/* Condition variable used for passive wait when buffer is empty */
pthread_cond_t sendingBufferEmptyCond;
pthread_t send_th;
pthread_mutex_t sendingLock;

// this is for the sending buffer
TAILQ_HEAD(sendtailhead, send_buffer_entry)
send_buffer_head;
struct sendtailhead *sheadp;
struct send_buffer_entry
{
    send_pdu bf;
    TAILQ_ENTRY(send_buffer_entry)
    sendEntries;
};

/* Condition variable used for passive wait when buffer is empty */
pthread_cond_t buffer_empty_cond;

/*************************
 * Fonctions Utilitaires *
 *************************/
int initialize_components(start_mode mode)
{
    int bnd;
    struct hostent * hp;
    struct sockaddr_in local_addr;

    if(initialized != -1) return initialized;
    if((sys_socket = socket(AF_INET, SOCK_DGRAM, 0)) == -1) return -1;
    else initialized = 1;

    if((mode == SERVER) & (initialized != -1))
    {
        TAILQ_INIT(&app_buffer_head);
        pthread_cond_init(&buffer_empty_cond, 0);
        memset((char *) &local_addr, 0, sizeof(local_addr));
        local_addr.sin_family = AF_INET;
        local_addr.sin_port = htons(API_CS_Port);
        local_addr.sin_addr.s_addr = htonl(INADDR_ANY);
        bnd = bind(sys_socket, (struct sockaddr *) &local_addr, sizeof(local_addr));

        if (bnd == -1)
        {
            initialized = -1;
        }
        else
        {
            memset((char *) &remote_addr, 0, sizeof(remote_addr));
            remote_addr.sin_family = AF_INET;
            remote_addr.sin_port = htons(API_SC_Port);
            hp = gethostbyname("localhost");
            memcpy (&(remote_addr.sin_addr.s_addr), hp->h_addr, hp->h_length);
            initialized = 1;
        }


    }
    else
    {
        if(initialized != -1)
        {
            memset((char *) &remote_addr, 0, sizeof(remote_addr));
            remote_addr.sin_family = AF_INET;
            remote_addr.sin_port = htons(API_CS_Port);
            hp = gethostbyname("localhost");
            memcpy (&(remote_addr.sin_addr.s_addr), hp->h_addr, hp->h_length);

            memset((char *) &local_addr, 0, sizeof(local_addr));
            local_addr.sin_family = AF_INET;
            local_addr.sin_port = htons(API_SC_Port);
            local_addr.sin_addr.s_addr = htonl(INADDR_ANY);
            bnd = bind(sys_socket, (struct sockaddr *) &local_addr, sizeof(local_addr));
        }
    }

    if((initialized == 1) && (mode == SERVER))
    {
        pthread_create (&listen_th, NULL, listening, "1");
    }

    return initialized;
}



int IP_send(mic_tcp_pdu pk, mic_tcp_ip_addr addr)
{

    int result = -1;
    int random = rand();
    int lr_tresh = (int) round(((float)loss_rate/100.0)*RAND_MAX);
    struct hostent * hp;

    if(initialized == -1) {
        result = -1;

    } else {
        mic_tcp_payload tmp = get_full_stream(pk);
        int sent_size = tmp.size;

        if(random > lr_tresh) {
           hp = gethostbyname(addr.addr);
           memcpy (&(remote_addr.sin_addr.s_addr), hp->h_addr, hp->h_length);
           sent_size = sendto(sys_socket, tmp.data, tmp.size, 0, (struct sockaddr *)&remote_addr, sizeof(struct sockaddr));
           printf("[MICTCP-CORE] Envoi d'un paquet IP de taille %d vers l'adresse %s\n", sent_size, addr.addr);
        } else {
           printf("[MICTCP-CORE] Perte du paquet\n");
        }

        free (tmp.data);

        /* Correct the sent size */
        result = (sent_size == -1) ? -1 : sent_size - API_HD_Size;
    }

    return result;
}

int IP_recv(mic_tcp_pdu* pk, mic_tcp_ip_addr* local_addr, mic_tcp_ip_addr* remote_addr, unsigned long timeout)
{
    int result = -1;

    struct timeval tv;
    struct sockaddr_in tmp_addr;
    socklen_t tmp_addr_size = sizeof(struct sockaddr);

    /* Send data over a fake IP */
    if(initialized == -1) {
        return -1;
    }

    /* Compute the number of entire seconds */
    tv.tv_sec = timeout / 1000;
    /* Convert the remainder to microseconds */
    tv.tv_usec = (timeout - tv.tv_sec * 1000) * 1000;

    /* Create a reception buffer */
    int buffer_size = API_HD_Size + pk->payload.size;
    char *buffer = malloc(buffer_size);

    if ((setsockopt(sys_socket, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv))) >= 0) {
       result = recvfrom(sys_socket, buffer, buffer_size, 0, (struct sockaddr *)&tmp_addr, &tmp_addr_size);
    }

    if (result != -1) {
        /* Create the mic_tcp_pdu */
        memcpy (&(pk->header), buffer, API_HD_Size);
        pk->payload.size = result - API_HD_Size;
        memcpy (pk->payload.data, buffer + API_HD_Size, pk->payload.size);

        /* Generate a stub address */
        if (remote_addr != NULL) {
            //inet_ntop(AF_INET, &(tmp_addr.sin_addr),remote_addr->addr,remote_addr->addr_size);
            remote_addr->addr = "localhost";
            remote_addr->addr_size = strlen(remote_addr->addr) + 1; // don't forget '\0'
        }

        if (local_addr != NULL) {
            local_addr->addr = "localhost";
            local_addr->addr_size = strlen(local_addr->addr) + 1; // don't forget '\0'
        }

        printf("[MICTCP-CORE] Réception d'un paquet IP de taille %d provenant de %s\n", result, remote_addr->addr);

        /* Correct the receved size */
        result -= API_HD_Size;

    }

    /* Free the reception buffer */
    free(buffer);

    return result;
}

mic_tcp_payload get_full_stream(mic_tcp_pdu pk)
{
    /* Get a full packet from data and header */
    mic_tcp_payload tmp;
    tmp.size = API_HD_Size + pk.payload.size;
    tmp.data = malloc (tmp.size);

    memcpy (tmp.data, &pk.header, API_HD_Size);
    memcpy (tmp.data + API_HD_Size, pk.payload.data, pk.payload.size);

    return tmp;
}

mic_tcp_payload get_mic_tcp_data(ip_payload buff)
{
    mic_tcp_payload tmp;
    tmp.size = buff.size-API_HD_Size;
    tmp.data = malloc(tmp.size);
    memcpy(tmp.data, buff.data+API_HD_Size, tmp.size);
    return tmp;
}


mic_tcp_header get_mic_tcp_header(ip_payload packet)
{
    /* Get a struct header from an incoming packet */
    mic_tcp_header tmp;
    memcpy(&tmp, packet.data, API_HD_Size);
    return tmp;
}



int app_buffer_get(mic_tcp_payload app_buff)
{
    /* A pointer to a buffer entry */
    struct app_buffer_entry * entry;

    /* The actual size passed to the application */
    int result = 0;

    /* Lock a mutex to protect the buffer from corruption */
    pthread_mutex_lock(&lock);

    /* If the buffer is empty, we wait for insertion */
    while(app_buffer_head.tqh_first == NULL) {
          pthread_cond_wait(&buffer_empty_cond, &lock);
    }

    /* When we execute the code below, the following conditions are true:
       - The buffer contains at least 1 element
       - We hold the lock on the mutex
    */

    /* The entry we want is the first one in the buffer */
    entry = app_buffer_head.tqh_first;

    /* How much data are we going to deliver to the application ? */
    result = min_size(entry->bf.size, app_buff.size);

    /* We copy the actual data in the application allocated buffer */
    memcpy(app_buff.data, entry->bf.data, result);

    /* We remove the entry from the buffer */
    TAILQ_REMOVE(&app_buffer_head, entry, entries);

    /* Release the mutex */
    pthread_mutex_unlock(&lock);

    /* Clean up memory */
    free(entry->bf.data);
    free(entry);

    return result;
}

void app_buffer_put(mic_tcp_payload bf)
{
    /* Prepare a buffer entry to store the data */
    struct app_buffer_entry * entry = malloc(sizeof(struct app_buffer_entry));
    entry->bf.size = bf.size;
    entry->bf.data = malloc(bf.size);
    memcpy(entry->bf.data, bf.data, bf.size);

    /* Lock a mutex to protect the buffer from corruption */
    pthread_mutex_lock(&lock);

    /* Insert the packet in the buffer, at the end of it */
    TAILQ_INSERT_TAIL(&app_buffer_head, entry, entries);

    /* Release the mutex */
    pthread_mutex_unlock(&lock);

    /* We can now signal to any potential thread waiting that the buffer is
       no longer empty */
    pthread_cond_broadcast(&buffer_empty_cond);
}


void send_buffer_get(send_pdu *send_buff)
{
    /* A pointer to a buffer entry */
    struct send_buffer_entry *entry;

    /* Lock a mutex to protect the buffer from corruption */
    pthread_mutex_lock(&sendingLock);

    /* If the buffer is empty, we wait for insertion */
    while (send_buffer_head.tqh_first == NULL)
    {
        pthread_cond_wait(&sendingBufferEmptyCond, &sendingLock);
    }

    /* When we execute the code below, the following conditions are true:
       - The buffer contains at least 1 element
       - We hold the lock on the mutex
    */

    /* The entry we want is the first one in the buffer */
    entry = send_buffer_head.tqh_first;

    /* We copy the actual data in the sendlication allocated buffer */
    send_buff->pdu = entry->bf.pdu;
    send_buff->remoteAddr = entry->bf.remoteAddr;

    /* We remove the entry from the buffer */
    TAILQ_REMOVE(&send_buffer_head, entry, sendEntries);

    /* Release the mutex */
    pthread_mutex_unlock(&sendingLock);

    /* Clean up memory */
    free(entry);

}

void send_buffer_put(send_pdu bf)
{
    /* Prepare a buffer entry to store the data */
    struct send_buffer_entry *entry = malloc(sizeof(struct send_buffer_entry));
    entry->bf.remoteAddr = bf.remoteAddr;
    entry->bf.pdu = bf.pdu;

    /* Lock a mutex to protect the buffer from corruption */
    pthread_mutex_lock(&sendingLock);

    /* Insert the packet in the buffer, at the end of it */
    TAILQ_INSERT_TAIL(&send_buffer_head, entry, sendEntries);

    /* Release the mutex */
    pthread_mutex_unlock(&sendingLock);

    /* We can now signal to any potential thread waiting that the buffer is
       no longer empty */
    pthread_cond_broadcast(&sendingBufferEmptyCond);
}

void* listening(void* arg)
{
    mic_tcp_pdu pdu_tmp;
    int recv_size;
    mic_tcp_ip_addr remote;
    mic_tcp_ip_addr local;

    pthread_mutex_init(&lock, NULL);

    printf("[MICTCP-CORE] Demarrage du thread de reception reseau...\n");

    const int payload_size = 1500 - API_HD_Size;
    pdu_tmp.payload.size = payload_size;
    pdu_tmp.payload.data = malloc(payload_size);

    remote.addr=malloc(100);
    remote.addr_size=100;


    while(1)
    {
        remote.addr_size=100;
        pdu_tmp.payload.size = payload_size;
        recv_size = IP_recv(&pdu_tmp, &local, &remote, 0);

        if(recv_size != -1)
        {
            process_received_PDU(pdu_tmp, local, remote);
        } else {
            /* This should never happen */
            printf("Error in recv\n");
        }
    }
}


void set_loss_rate(unsigned short rate)
{
    loss_rate = rate;
}

void print_header(mic_tcp_pdu bf)
{
    mic_tcp_header hd = bf.header;
    printf("\nSP: %d, DP: %d, SEQ: %d, ACK: %d", hd.source_port, hd.dest_port, hd.seq_num, hd.ack_num);
}

unsigned long get_now_time_msec()
{
    return ((unsigned long) (get_now_time_usec() / 1000));
}

unsigned long get_now_time_usec()
{
    struct timespec now_time;
    clock_gettime( CLOCK_REALTIME, &now_time);
    return ((unsigned long)((now_time.tv_nsec / 1000) + (now_time.tv_sec * 1000000)));
}

int min_size(int s1, int s2)
{
    if(s1 <= s2) return s1;
    return s2;
}
