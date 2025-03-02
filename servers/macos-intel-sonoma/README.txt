These binaries were built for Intel Macs running Sonoma (MacOS 14.4)

d1_dump:
  This binary tries to analyze the D1 layer of your packets.
  You can start it with a given port, and send UDP packets to it.
  It will attempt to parse the first 8 bytes (the D1 header) and print out whether
  the header fields look reasonable.

d1_server:
  This binary is a server that the client d1_test_client should connect to.
  You start it with a UDP port.
  After that, the client attempts to send a packet containing the string connect,
  and this server will ACK it. The client will attempt to the a packet containing
  the string "ping". The server will ACK it and attempt to send a packet containing
  the string "pong". This will be repeated once. Finally, the client will attempt
  to send a packet with the string "disconnect" and the server will ACK it.
  Both will terminate.

d2_server:
  This binary is a server that the client d2_test_client should connec to.
  You start it with a UDP port.
  The client will use the PacketRequest message to send an ID to the server.
  The server will retrieve a tree structure for the given ID. It will
  send to the client the number of nodes in the tree (PacketResponseSize),
  followed by several packets, where each contains up to 5 nodes of the tree (obviously
  without pointer and sequentialized for reconstruction on the client side).
  These packets are PacketResponse packets, where all except the last one have
  the type flag TYPE_RESPONSE. The last one has the type TYPE_RESPONSE_LAST.
  The client should reconstruct the tree and print it to the screen in a defined
  format.
