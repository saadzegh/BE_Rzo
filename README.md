# Compte rendu - BE RZO
> ZEGHARI Saad - ZRID Kenza

## Compilation du protocole mictcp et lancement des applications de test

Pour compiler mictcp, il faut exécuter :

    make

Les deux applications de test sont :

    ./tsock_texte [-p|-s destination] port fiabilite
    ./tsock_video [-p|-s] [-t (tcp|mictcp)]

## Récapitulatif par version

### V1 - Pas de reprise des pertes
> Version implémentée, fonctionnelle.

### V2 - Reprise des pertes totale
> Version implémentée, fonctionnelle.

### V3 - Reprise des pertes partielle
> Version implémentée, fonctionnelle.

+ Utilisation d'une fenêtre glissante. On initialise en considérant tous les paquets perdus, ce qui garantit une transmission plus fiable au départ.
+ Definition de perte admissible : On divise la somme des ack reçus (correspondant aux paquets bien transmis) par la taille de la fenetre. Si le résultat est superieure au seuil de fiabilité, alors en cas de perte de paquet, on ne le renvoie  pas et on passe au suivant en mettant à jour le numéro de séquence.
+ Bug au niveau de la vidéo

### V4.1 - Phase d'établissement de connexion et négociation
> Version implémentée, fonctionnelle
+ Communication Syn Synack ack par un thread (côté client). On peut envoyer des ack à plusieurs reprises en cas de perte du packet Synack.
+ Un thread envoie des Synack (côté serveur) en boucle et est interrompu à la récépion du pdu.
+ Pour la fiabilité partielle : 
    - On envoie de 10 paquets vide et comptabilisation du nombre d'ACK reçus
    -  On définit le seuil en fonction du nombre d'ACK reçus
Nous avons choisi de mettre en place cette méthode afin de définir le seuil car nous cherchons à l'adapter en fonction de la qualité de transmission.
Nous appelons la fonction qui définit le seuil au sein de mic_tcp_connect afin de ne pas retester le seuil à chaque envoie de paquet. De même, nous avons ajouté un champ dans la structure socket qui renseigne ce seuil. Ce dernier nous permet de récupérer le seuil directement dans la fonction mic_tcp_send une fois attribué dans mic_tcp_connect.
Cela ne favorise pas la vitesse de tranfert, mais ce n'est pas le but du BE.

### V4.2 - Asynchronisme client et serveur
> Version implémentée, presque fonctionnelle
+ Utilisation de mutex pour la phase d'établissement de connexion, pour ne pas consommer 100% de CPU avec un while.
+ Buffer d'émission mis en place. Construction d'un PDU par le serveur et le client indiviuellement et le place en structure send_pdu (voir .h).
+ Les PDU sont stockés sur le tas, le buffer contient des pointeurs vers ces PDU. Lorsque le thread envoie le PDU, il libère la mémoire grâce à ce même pointeur.

Problèmes :
    - Gestion problématique du pdu.fin entre lors de l'acquittement. La gestion des threads doit encore être arrangée
    - Erreur de segmentation lors de l'execution de la vidéo (problème avec la vidéo depuis la V.3)
    
