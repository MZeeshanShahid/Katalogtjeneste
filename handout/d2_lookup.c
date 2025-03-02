/* ======================================================================
 * YOU ARE EXPECTED TO MODIFY THIS FILE.
 * ====================================================================== */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>  
#include "d2_lookup.h"

D2Client *d2_client_create(const char *server_name, uint16_t server_port)
{
    if (server_name == NULL)
    {
        fprintf(stderr, "Servernavn er NULL.\n");
        return NULL;
    }
   
    D2Client *client = malloc(sizeof(D2Client));
    if (client == NULL)
    {
        perror("Feilet til å allokere minne for D2Client ");
        return NULL;
    }
    
    // Oppretter en D1Peer forbindelse
    client->peer = d1_create_client();
    if (client->peer == NULL)
    {
        free(client);
        return NULL;
    }
    // Setter server informasjon for D1Peer
    if (d1_get_peer_info(client->peer, server_name, server_port) == 0)
    {
        d1_delete(client->peer); 
        free(client);
        return NULL;
    }

    return client;
}

D2Client *d2_client_delete(D2Client *client)
{
    if (client != NULL)
    {   
        d1_delete(client->peer); 
        free(client);
    }
    return NULL;
}

int d2_send_request(D2Client *client, uint32_t id)
{
    if (client == NULL || client->peer == NULL)
    {
        fprintf(stderr, "Client eller peer er ikke intialisert.\n");
        return -1; 
    }

    PacketRequest request;
    memset(&request, 0, sizeof(request)); 

    request.type = htons(TYPE_REQUEST); 
    request.id = htonl(id);    

    char buffer[sizeof(PacketRequest)];
    memcpy(buffer, &request, sizeof(request)); // Copying the structured data into a buffer

   
    int bytes_sent = d1_send_data(client->peer, buffer, sizeof(PacketRequest));
    if (bytes_sent <= 0)
    {
        fprintf(stderr, "Feil: sending av PacketRequest.\n");
        return -1; 
    }

    return bytes_sent;
}

int d2_recv_response_size(D2Client* client) {
    if (client == NULL) {
        fprintf(stderr, "Client er NULL\n");
        return -1;
    }

    char buffer[sizeof(PacketResponseSize) + 8];
    int recvlen;
    // Motta en pakke
    recvlen = d1_recv_data(client->peer, buffer, sizeof(PacketResponseSize) + 8);

    if (recvlen < 0) {
        perror("Feil: Mottakelse av data: d2_recv_response_size ");
        return -1;
    }
    
    PacketResponseSize *responseSize = (PacketResponseSize*) buffer;


    uint16_t receivedType = ntohs(responseSize->type);
    uint16_t receivedSize = ntohs(responseSize->size);

    //Sjekk om den mottatte pakken er en PacketResponseSize
    if ((receivedType != TYPE_RESPONSE_SIZE)) {
        fprintf(stderr, "Motatt pakke er ikke en PacketResponseSize\n");
        fprintf(stderr, "Mottatt pakketype: %d\n", receivedType);
        return -1;
    }
    return (int)receivedSize;
}

int d2_recv_response(D2Client* client, char* buffer, size_t sz) {
    if (client == NULL || buffer == NULL)
    {
        fprintf(stderr, "Ugyldie parametere\n");
        return -1;
    }
    int n = d1_recv_data(client->peer, buffer, sz);
 
    if (n < 0)
    {
        fprintf(stderr, "Data er ikke mottatt\n");
        return -1;
    }
    return n;
}
LocalTreeStore *d2_alloc_local_tree(int num_nodes)
{
    LocalTreeStore *store = malloc(sizeof(LocalTreeStore));
    if (store == NULL) {
        fprintf(stderr, "Feil: Kunne ikke allokere minne for LocalTreeStore\n");
        return NULL;
    }

    store->nodes = malloc(num_nodes * sizeof(NetNode));

    if (store->nodes == NULL) {
        fprintf(stderr, "Feil: Kunne ikke allokere minne for noder i LocalTreeStore\n");
        free(store);
        return NULL;
    }
    store->number_of_nodes = num_nodes;
    return store;
}

void d2_free_local_tree(LocalTreeStore *store)
{
   if (store != NULL) {
        free(store->nodes);
        free(store); 
    } 
 }

int d2_add_to_local_tree(LocalTreeStore *s, int node_idx, char *buffer, int buflen) {
    if (s == NULL || buffer == NULL || buflen < 0) {
      
        fprintf(stderr, "Invalid parameter\n");
        return -1;
    }

    int index = 0; // Index for buffer
    while (index < buflen) {
        if (node_idx >= s->number_of_nodes) {
            fprintf(stderr, "Node index exceeds number of allocated nodes\n");
            return -1;
        }
        
        NetNode* node = &s->nodes[node_idx];
        memcpy(&node->id, buffer + index, sizeof(node->id));
        index += sizeof(uint32_t);
        node->id = ntohl(node->id);
        
        memcpy(&node->value, buffer + index, sizeof(node->value));
        index += sizeof(uint32_t);
        node->value = ntohl(node->value);

        memcpy(&node->num_children, buffer + index, sizeof(node->num_children));
        index += sizeof(uint32_t);
        node->num_children = ntohl(node->num_children);

        for (uint32_t i = 0; i < node->num_children; i++) {
            if (index >= buflen) {
                fprintf(stderr, "Buffer har ikke nok plass for child ids\n");
                return -1;
            }
            memcpy(&node->child_id[i], buffer + index, sizeof(uint32_t));
            node->child_id[i] = ntohl(node->child_id[i]);
            index += sizeof(uint32_t);
        }
        node_idx++;
    }
    return node_idx;
}


void print_tree_recursive(LocalTreeStore *store, int node_index, int depth) {
    if (node_index < 0 || node_index >= store->number_of_nodes) {
        return;  
    }
    NetNode node = store->nodes[node_index];
    
    for (int i = 0; i < depth; i++) {
        printf("--");
    }
    printf("%d\n",node.value);
 
    // Rekursvit kall for hvert barn.
    for (uint32_t i = 0; i < node.num_children; i++) {
        int child_index = node.child_id[i];
        print_tree_recursive(store, child_index, depth + 1);
    }
}
 
void d2_print_tree(LocalTreeStore *nodes) {
    if (nodes == NULL || nodes->number_of_nodes == 0) {
        return;  
    }
    print_tree_recursive(nodes, 0, 0);  // Starter fra rotnoden som er på index 0.
    
}
 
