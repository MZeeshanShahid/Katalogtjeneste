/* ======================================================================
 * YOU CAN MODIFY THIS FILE.
 * ====================================================================== */

#ifndef D2_LOOKUP_MOD_H
#define D2_LOOKUP_MOD_H

#include "d1_udp.h"

struct D2Client
{
    D1Peer* peer;
};

typedef struct D2Client D2Client;

struct NetNode;  // måtte legge til denne slik at jeg kan kalle på NetNode structen inne i denne filen.

struct LocalTreeStore
{
    int number_of_nodes;
    struct NetNode *nodes;  // Peker til et array av NetNode-strukturer
};
typedef struct LocalTreeStore LocalTreeStore;

#endif /* D2_LOOKUP_MOD_H */

