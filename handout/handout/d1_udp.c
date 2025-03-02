/* ======================================================================
 * YOU ARE EXPECTED TO MODIFY THIS FILE.
 * ====================================================================== */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>

#include "d1_udp.h"
#define MAX_RETRY 3

uint16_t swap_uint16(uint16_t val)
{
    return (val >> 8) | (val << 8);
}

uint16_t calculate_checksum(char *buff, size_t len)
{
    uint16_t checksum = 0;
    // når jeg gjør om til uint16 vil byte order byttes fra f eks 0102 til 0201, derfor må man bruke swap for å bytte tilbake igjen.
    const uint16_t *data = (const uint16_t *)buff;

    // fordi vi går gjennom 2 og 2 bytes om gangen (16 bits). Av den grunn kjører vi halvparten av len.
    for (size_t i = 0; i < (len / 2); i++)
    {
        uint16_t corrected_val = swap_uint16(data[i]);
        checksum ^= corrected_val;
    }
    // Dersom lengden av pakken er et oddetall.
    if (len % 2)
    {
        uint16_t last_byte = buff[len - 1];
        checksum ^= (last_byte << 8);
    }
    return checksum;
}

D1Peer *d1_create_client()
{
    D1Peer *peer = malloc(sizeof(D1Peer));
    if (peer == NULL)
    {
        perror("Kunne ikke allokere minne for D1Peer");
        return NULL;
    }

    peer->socket = socket(AF_INET, SOCK_DGRAM, 0);

    if (peer->socket == -1)
    {
        perror("Kunne ikke opprette socket");
        free(peer);
        return NULL;
    }
    memset(&peer->addr, 0, sizeof(peer->addr));

    peer->addr.sin_family = AF_INET;
    peer->next_seqno = 0;

    return peer;
}

D1Peer *d1_delete(D1Peer *peer)
{
    if (peer != NULL)
    {
        if (peer->socket != -1)
        {
            close(peer->socket);
        }
        free(peer);
    }
    return NULL;
}

int d1_get_peer_info(struct D1Peer *peer, const char *peername, uint16_t server_port)
{
    if (peer == NULL || peername == NULL)
    {
        return 0;
    }

    struct addrinfo hints; // hints hjelper med hva slags  type adresser jeg er interessert i.
    struct addrinfo *res;  // En peker til den første noden i en lenket liste av struct addrinfo strukturer som inneholder resultatet fra getaddrinfo().
    int status;
    char portstr[6]; // Buffer for å lagre portnummeret som streng.

    snprintf(portstr, sizeof(portstr), "%u", server_port); // Konverterer portnummeret til streng, fordi getaddrinfo tar inn portnummeret som en streng.

    memset(&hints, 0, sizeof(hints)); // Initialiserer alle elementene i hints-structen med nuller.
    hints.ai_family = AF_INET;        // Spesifiserer at jeg ønsker IPv4-adresser.
    hints.ai_socktype = SOCK_DGRAM;   // Spesifiserer at jeg ønsker en UDP-forbindelse.

    status = getaddrinfo(peername, portstr, &hints, &res);

    if (status != 0)
    {
        fprintf(stderr, "getaddrinfo feil: %s\n", gai_strerror(status));
        return 0;
    }
    // Kopierer den første returnerte adressen til peer->addr
    memcpy(&(peer->addr), res->ai_addr, res->ai_addrlen);

    freeaddrinfo(res); // Frigjør minne som er allokert av getaddrinfo

    return 1;
}

int d1_recv_data(D1Peer *peer, char *buffer, size_t sz)
{
    if (peer == NULL || buffer == NULL || sz < sizeof(D1Header))
    {
        fprintf(stderr, "Feil innsetting av parametere\n");
        return -1;
    }
    int recvlen = recv(peer->socket, buffer, sz, 0);

    if (recvlen < 0)
    {
        perror("Feil: receiving data");
        return -1;
    }

    if ((size_t)recvlen < sizeof(D1Header))
    {
        fprintf(stderr, "Ukomplett pakke mottatt\n");
        return -1;
    }

    D1Header *header = (D1Header *)buffer; // slik at header tar de første plassene i bufferet.

    uint16_t received_checksum = ntohs(header->checksum);
    header->checksum = 0;
    uint16_t calculated_checksum = calculate_checksum(buffer, recvlen);

    header->flags = ntohs(header->flags);
    int seq_no = (header->flags >> 7) & 0x01; // Sekvensnummeret som vi mottar fra serveren.

    if (ntohl(header->size) != (uint32_t)recvlen)
    {
        fprintf(stderr, "Mottat pakke matcher ikke forventet størrelse\n");
        d1_send_ack(peer, !seq_no);
        return -1;
    }

    if (received_checksum != calculated_checksum)
    {
        fprintf(stderr, "Checksum mismatch\n");
        d1_send_ack(peer, !seq_no);
        return -1;
    }

    size_t payload_size = recvlen - sizeof(D1Header);
    memcpy(buffer, buffer + sizeof(D1Header), payload_size);

    d1_send_ack(peer, seq_no);

    return payload_size;
}

int d1_wait_ack(D1Peer *peer, char *packet, size_t packet_size)
{
    int recvlen;
    char ack[8];         // Buffer for ACK-pakke
    int retry_count = 0; // antall

    while (retry_count < MAX_RETRY)
    {
        recvlen = recv(peer->socket, ack, sizeof(ack), 0);

        if (recvlen < 0)
        {
            perror("recv feilet");
            return -1;
        }

        uint16_t flags = ntohs(((D1Header *)ack)->flags);

        // Sjekker om pakken er en ACK-pakke, og om sekvensnummeret er lik ACKNO.
        if ((flags & FLAG_ACK) && ((flags & ACKNO) == peer->next_seqno))
        {
            peer->next_seqno = !peer->next_seqno;
            return recvlen;
        }
        else
        {
            // Feil. Sender pakken på nytt.
            if (retry_count < MAX_RETRY - 1)
            {
                printf("Feil ACK mottat: sender pakken på nytt\n");
                if (sendto(peer->socket, packet, packet_size, 0, (struct sockaddr *)&peer->addr, sizeof(peer->addr)) < 0)
                {
                    perror("Feilet å sende pakken på nytt");
                }
            }
            retry_count++;
            sleep(1); // Vent 1 sekund før man prøver igjen.
        }
    }
    fprintf(stderr, "Kunne ikke motta riktig ACK etter: %d forsøk\n", MAX_RETRY);
    return -1;
}

int d1_send_data(D1Peer *peer, char *buffer, size_t sz)
{
    if (peer == NULL || buffer == NULL)
    {
        return 0;
    }

    if (sz + sizeof(D1Header) > 1024)
    {
        fprintf(stderr, "Data storrelsen er for stor. Maksstorrelse er 1024.\n");
        return -1;
    }
    char *packet = malloc(sz + sizeof(D1Header)); // pakken inkludert headeren.

    if (packet == NULL)
    {
        perror("malloc failed");
        return -1;
    }
    D1Header *header = (D1Header *)packet; // Setter header-pekeren til å peke på starten av pakken.
    memset(header, 0, sizeof(D1Header));

    uint16_t flags = FLAG_DATA;
    // Legger til sekvensnummer til flagget basert på peer sin next_seqno.
    if (peer->next_seqno)
    {
        flags |= SEQNO; // Legger til SEQNO flagget hvis sekvensnummeret er satt
    }

    header->flags = htons(flags);

    // Størrelsen på pakken inkluderer headeren og eventuell payload
    header->size = htonl(sizeof(D1Header) + sz);

    // Kopierer dataene til plassen etter headeren i pakken.
    memcpy(packet + sizeof(D1Header), buffer, sz);

    header->checksum = htons(calculate_checksum(packet, sizeof(D1Header) + sz));

    int sent_bytes = sendto(peer->socket, packet, sizeof(D1Header) + sz, 0,
                            (struct sockaddr *)&peer->addr, sizeof(peer->addr));

    if (sent_bytes == -1)
    {
        perror("Kunne ikke sende data");
        free(packet);
        return -1;
    }

    if (d1_wait_ack(peer, packet, sizeof(D1Header) + sz) < 0)
    {
        free(packet);
        return -1;
    }
    free(packet);
    return sent_bytes;
}

void d1_send_ack(struct D1Peer *peer, int seqno)
{
    D1Header ack_header;
    memset(&ack_header, 0, sizeof(ack_header));
    ack_header.flags |= FLAG_ACK;
    if (seqno)
    {
        ack_header.flags |= ACKNO;
    }
    ack_header.flags = htons(ack_header.flags);

    ack_header.size = htonl(sizeof(D1Header));
    ack_header.checksum = 0; // antok at jeg ikke trenger checksum for å sende ack.

    if (sendto(peer->socket, &ack_header, sizeof(ack_header), 0,
               (struct sockaddr *)&peer->addr, sizeof(peer->addr)) < 0)
    {
        perror("Sending av ACK feilet.");
    }
}