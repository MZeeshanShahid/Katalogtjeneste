Forklaring på hvordan jeg har implementert koden, hvordan den fungerer, og mine valg og antakelser jeg har gjort.

Har ikke forklart alle feilsjekkene som jeg har gjort i koden, fordi antar at jeg ikke behøver det.
Har kommentarer og utskrift på norsk, men har brukt engelsk variabler i koden.

- D1Peer* d1_create_client();
    Denne funkjsonen var veldig rett fram, hvor jeg oppretter en ny klient-instans, som skal kommunisere med serverne.
    Allokerte aller først minne for d1-peer, som inneholder nødvendig data får nettverkskommunikasjon mellom klient og server.
    Videre oppretter jeg en socket, som skal bruke IPv4-kommunikasjon, og benytte seg av UDP-protokollen. Setter bare protokollparameteren til 0.
    Derretter nullstiller jeg hele struct sockaddr_in addr, som det sto at man skal gjøre, og setter adressetypen (sin_family) til AF_INET (IPv4). Til slutt
    setter jeg peer -> next_seqno til 0, og retunrerte klienten. 

- D1Peer* d1_delete( D1Peer* peer);
    Denne funksjonen bare lukker socketen, og frigjør minnet som ble allokert for d1peer. Dette er viktig for å forhindre minnelekasjer.

- int d1_get_peer_info( struct D1Peer* client, const char* servername, uint16_t server_port );
    Denne funkjsonen brukes for å lage en nettverkskommunikasjon mellom klienten, og serveren som er identifisert med navn og protnummer.
    Her valgte jeg å bruke getaddrinfo() i stedet for gethostbyname, fordi den var mer fleksibel og retunrerer adresser som allerede er i riktig format
    for nettverksopersjoner. Altså adressene/adressen getaddrinfo() retunrerer, er allerede strukturert i en form slik at de kan brukes direkte i socket-operasjoner.
    I tillegg står det på man-siden for gethostbyname at getaddrinfo() er mer foretrukket enn gethostbyname(). Etter at jeg har brukt getaddrinfo() 
    som gir meg en liste med nettverksadresser ('res') som passer med kravene gitt i 'hints', kopierer jeg den første
    nettverksadressen inn i peer->addr. Til slutt frigjør jeg minne alloksert av getaddrinfo, og retunrerer 1.

- uint16_t calculate_checksum(char *buff, size_t len);
    Denne funksjonen beregner en checksum for en pakke ved å benytte seg av en XOR-operasjon over alle 16-bits segmentene i pakken.
    Starter med å initialisere checksum til 0, som er variabelen som skal holde den endelige checksum-verdien. Videre blir bufferet behandlet som
    en array av uint16_t elementer. Når jeg endret arrayen til å håndtere hvert element som uint16_t ble byteorden byttet på omvendt, altså
    0102 gikk til 0201. Av den grunn bruker jeg en hjelpemetode swap_uint16(), som bytter tilbake til riktig byteorder for hver 16-bits element i bufferet. 
    Checksummen beregnes ved at jeg itererer gjennom hver 16-bits i bufferet og X-ORer for hver 16-bits element i bufferet med checksum-variabelen. 
    Dersom len er et oddetall, vil det være en siste byte igjen, fordi løkken opererer på to bytes om gangen (16 bits). Av den grunn konverterer jeg det 
    siste bytet til en 16-bits struktur, og forskyver det 8 bits til venstre slik at den blir håndtert riktig, når den blir X-oret med variabelen 'checksum'. 
    Til slutt blir checksum retunert.


- int d1_recv_data(D1Peer* peer, char* buffer, size_t sz);
    Denne funksjonen skal motta data fra serveren, håndtere pakken, og validere pakken. Dataen mottas fra nettverket med recv(), og
    headeren settes først i bufferet. Videre konverterer jeg den mottate headeren fra nettverksbyteordren til vertens byteorden for de relevante feltene i headeren.
    Jeg beregner og sammenligner checksummen for å verifisere integriteten at den mottate dataten. Dersom checksummen ikke stemmer eller jeg ikke har mottatt riktig antall data 
    vil jeg sende en ACK-pakke med det motsatte sekvensnummeret mottatt fra serveren tilbake til serveren. Dersom checksummen er riktig, henter jeg 
    sekvensnummeret mottatt fra serveren, og sender en ACK-pakke med det mottatte sekvensnummeret. Jeg setter også payloaden rett etter headeren i bufferet, 
    og retunerer antall bytes mottat fra serveren som er payload.


- int  d1_wait_ack( D1Peer* peer, char* buffer, size_t sz );
    Denne funksjonen venter på en ACK-pakke fra serveren når en pakke er sendt. Den sikrer at den mottatte ACK-pakken er lik det forventede sekvensnummeret,
    noe som betyr at serveren har mottatt og korrekt behandlet den sendte pakken. Dersom ACK-pakken man mottar ikke er en ACK-pakke eller
    om ACK-nummeret ikke er lik peer-next_seqno sender vi pakken på nytt, og venter igjen. Vi sender pakken på nytt to ganger, hvis man ikke går inn i
    if-setningen. Jeg valgte å sende to ganger på nytt dersom ACK-pakken man mottar ikke er riktig. Ble ikke spesifisert hvor mange ganger man skulle sende tilbake.

- int d1_send_data( D1Peer* peer, char* buffer, size_t sz );
    Denne funksjonen sender data med en UDP til serveren. Jeg lager en pakke som har plass til headeren og payloaden som skal sendes til serveren.
    Videre setter jeg en header-peker til starten av det allokerte bufferet. Derretter setter jeg FLAG_DATA, fordi det alltid er en datapakke, og
    sekvensnummeret settes basert på hva peer-next_seqno er. Flaggene og størrelsen på pakken setts til nettverksbyteordren, fordi man skal sende det på
    det formatet over nettverket. Derretter kopierer jeg payload etter headeren i pakken, og beregner checksummen som settes i headeren. 
    Til slutt sender jeg pakken, og venter på en ACK-pakke som d1_wait_ack() skal motta.


- void d1_send_ack( struct D1Peer* peer, int seqno )
    Denne funksjonen sender en ACK-pakke til serveren når man mottar en pakke fra serveren. I denne funksjonen setter jeg ACK-flagget i headeren, fordi
    dette er en ACK-pakke. Derretter legger jeg til ACKNO flagget dersom seqno er sann, altså 1. Dette er for å indikere hvilket sekvensnummer ACK-pakken 
    skal bekrefte. Videre konverterer jeg ack_header.flags og ack_header.size til nettverksbyteordren, samtidig som jeg setter checksummen til 0.
    checksummen er ikke viktig i dette tilfellet. Til slutt sender jeg pakken til serveren.

- D2Client *d2_client_create(const char *server_name, uint16_t server_port);
    Denne funksjonen er ganske lik som D1Peer* d1_create_client(). Denne oppretter en klientinstans som kan kommunisere med en server over nettverket,
    med UDP. Her allokeres det minne til en D2Client, som inneholder en D1Peer. Videre kalles d1_create_client() for å opprette en D1Peer. Til slutt kalles
    'd1_get_peer_info' for å sette nettverksadressen og porten til serveren i D1Peer-strukturen, og D2Client blir retunert.

D2Client* d2_client_delete( D2Client* client );
    Denne funksjonen tar seg av opprydningen og frigjøringen av D2Client-instanset. Kaller på 'd1_delete(client->peer)', som lukker sokkelen og 
    firgjør minne som D1Peer bruker. Til slutt tar jeg free(client), som frigjør minnet som ble allokert til D2Client. 

int d2_send_request( D2Client* client, uint32_t id );
    Denne funksjonen sender en 'PacketRequest' til serveren, noe som skal hjelpe med å få packetResponseSize og PacketResponse pakker. 
    Her starter jeg med å lage en PacketRequest, hvor jeg nullstiller alle elementene til 0. Derretter setter jeg PacketRequest sin type til å være
    TYPE_REQUEST som betyr at dette er en requestpakke, og setter en id som må være større enn 1000 i 'PacketRequest'. Derretter lager jeg et buffer, hvor jeg
    setter PacketRequesten inn og sender den til serveren, med 'd1_send_data()'. Dersom sendingen er suksessful retuneres antall bytes sendt.


int d2_recv_response_size( D2Client* client );
    Denne funksjonen skal motta en 'PacketResponseSize' fra serveren. Denne pakken inneholder informasjon om hvor mange NetNode strukturer som vil følge
    i etterfølgende meldinger/pakker. Her lager jeg en buffer, som har en størrelse lik 'PacketResponseSize + 8'. Derretter kaller jeg på 
    d1_recv_data(client->peer, buffer, sizeof(PacketResponseSize) + 8); Grunnen til at jeg skriver + 8, er fordi man også ta hensyn til d1header, når
    den blir opprettet i d1_recv_data(). Videre typecaster jeg bufferet til en responseSize peker, slik at jeg kan få tilgang til dataene som en PacketResponseSize.
    Til slutt konverterer jeg PacketResponseSize elementene til nthos(), og retunrerer antall NetNode-strukturer som forventes.



int d2_recv_response( D2Client* client, char* buffer, size_t sz );
    Denne funksjonen skal motta en responspakke, som enten er en RESPONSE eller LAST RESPONSE. Hvilken pakke det er sjekkes i d2_test_client, og det
    eneste man trenger å gjøre her er å motta pakken, og lagre den i et buffer.


LocalTreeStore* d2_alloc_local_tree( int num_nodes );
    Denne funksjonen oppretter og allokerer en 'LocalTreeStore' som kan holde forskjellig antall NetNode basert på antall noder spesifisert i 
    paramteren. Aller først allokerer jeg minne for LocalTreeStore med malloc. Derretter opprettet jeg et array av NetNode-elementer inne i
    LocalTreeStore sin struct (struct NetNode *nodes;). Dette gjorde jeg for få oversikt over Nodene i treet. Jeg allokerer plass til alle noder inne i treet dynamisk (malloc).
    Til slutt setter jeg store->number_of_nodes = num_nodes og retunerer LocalTreeStore.


void  d2_free_local_tree( LocalTreeStore* store ); 
    Denne funksjonen frigjør først minne som er allokert for nodene inne i store, og derretter frigjør den minnet for selve 'LocalTreeStore'.

int d2_add_to_local_tree(LocalTreeStore *s, int node_idx, char *buffer, int buflen);
    Denne funksjonen parser data dra bufferen for å opprette NetNode-elementer i LocalTreeStore. Jeg starter med å opprette en index, som holder styr på
    hvor i bufferet jeg leser data fra. While-loopen kjører så lenge det går an å lese data fra bufferet. Videre oppretter jeg en ny NetNode-element som
    peker til riktig indeks i nodet-arrayet lagret i LocalTreeStore. Derretter kopierer jeg riktig data inn i &node->id, og konverterer 
    'node->id' fra htonl til ntohl. Samtidig er det viktig å gjøre index += sizeof(uint32_t) slik at man leser data fra riktig sted hele tiden fra bufferet. 
    Jeg gjør akkurat det samme for 'node->value' og 'node->num_children'. Når jeg skal lese gjennom barnas ID-er gjør jeg det gjennom en for-loop, 
    og gjør akkurat det samme som jeg har gjort med de andre verdiene inne i NetNode-elementene. Til slutt er det viktig å øke node_idx for hver node som legges 
    til i treet. Jeg retunerer node_idx som forteller hvor mange noder som har blitt lagt til i treet.


void  d2_print_tree( LocalTreeStore* nodes );
    Jeg bruker en hjelpefunksjon 'void print_tree_recursive(LocalTreeStore *store, int node_index, int depth)' som printer ut treet. Den henter nodene
    fra LocalTreeStore->nodes, og printer de ut rekursivt (DFS). 