#include "Online.h"
#include <spdlog/spdlog.h>
#include <PR/ultra64/gbi.h>

#define MAX_CLIENTS 2

extern "C" void Rupees_ChangeBy(s16 rupeeChange);
extern "C" u8 rupeesReceived;

namespace Ship {
    namespace Online {
        Server::Server() {
            port = 25565;
        }

        void Server::GetPlayerInfo(ENetPeer* peer) {
            PlayerInfo* player_info = new PlayerInfo();
            player_info->player_id = player_count;
            peer->data = player_info;
            player_count++;
        }

        void Server::RunServer() {
            /* Wait up to 1000 milliseconds for an event. (WARNING: blocking) */
            while (true) {
                while (enet_host_service(server, &event, 1000) > 0) {
                    switch (event.type) {
                    case ENET_EVENT_TYPE_CONNECT:
                        printf("A new client connected from %x:%u.\n", event.peer->address.host, event.peer->address.port);
                        /* Store any relevant client information here. */
                        GetPlayerInfo(event.peer);
                        break;

                    case ENET_EVENT_TYPE_RECEIVE:
                        printf("A packet has been received!",
                            event.packet->dataLength,
                            event.packet->data,
                            event.peer->data,
                            event.channelID);
                        /* Clean up the packet now that we're done using it. */
                        rupeesReceived = 1;
                        Rupees_ChangeBy(1);
                        enet_packet_destroy(event.packet);
                        break;

                    case ENET_EVENT_TYPE_DISCONNECT:
                        printf("%s disconnected.\n", event.peer->data);
                        /* Reset the peer's client information. */
                        event.peer->data = NULL;
                        break;

                    case ENET_EVENT_TYPE_DISCONNECT_TIMEOUT:
                        printf("%s disconnected due to timeout.\n", event.peer->data);
                        /* Reset the peer's client information. */
                        event.peer->data = NULL;
                        break;

                    case ENET_EVENT_TYPE_NONE:
                        break;
                    }
                }
            }

            player_count = 0;
            enet_host_destroy(server);
            enet_deinitialize();
        }

        void Server::CreateServer() {
            if (enet_initialize() != 0) {
                printf("An error occurred while initializing ENet.\n");
                return;
            }

            ENetAddress address = { 0 };

            address.host = ENET_HOST_ANY; /* Bind the server to the default localhost.     */
            address.port = 25565; /* Bind the server to port 7777. */

            /* create a server */
            server = enet_host_create(&address, MAX_CLIENTS, 1, 0, 0);

            if (server == NULL) {
                printf("An error occurred while trying to create an ENet server host.\n");
                return;
            }

            printf("Started a server!\n");

            onlineThread = std::thread(&Server::RunServer, this);
        }

        void Client::RunClient()
        {
            /* Allow up to 3 seconds for the disconnect to succeed
             * and drop any packets received packets.
             */
            while (true) {
                while (enet_host_service(client, &event, 3000) > 0) {
                    switch (event.type) {
                    case ENET_EVENT_TYPE_RECEIVE:
                        enet_packet_destroy(event.packet);
                        break;
                    case ENET_EVENT_TYPE_DISCONNECT:
                        puts("Disconnection succeeded.");
                        disconnected = true;
                        break;
                    }
                }
            }

            // Drop connection, since disconnection didn't successed
            if (!disconnected) {
                enet_peer_reset(peer);
            }

            enet_host_destroy(client);
            enet_deinitialize();
        }

        void Client::SendPacketMessage(Ship::Online::OnlinePacket_Rupees* packet) {
            ENetPacket* packetToSend = enet_packet_create(packet, sizeof(packet), ENET_PACKET_FLAG_RELIABLE);

            /* Send the packet to the peer over channel id 0. */
            /* One could also broadcast the packet by         */
            /* enet_host_broadcast (host, 0, packet);         */
            enet_peer_send(peer, 0, packetToSend);
        }

        void Client::ConnectToServer() {
            if (enet_initialize() != 0) {
                fprintf(stderr, "An error occurred while initializing ENet.\n");
                return;
            }

            client = enet_host_create(NULL /* create a client host */,
                1 /* only allow 1 outgoing connection */,
                1 /* allow up 2 channels to be used, 0 and 1 */,
                0 /* assume any amount of incoming bandwidth */,
                0 /* assume any amount of outgoing bandwidth */);

            if (client == NULL) {
                fprintf(stderr,
                    "An error occurred while trying to create an ENet client host.\n");
                exit(EXIT_FAILURE);
            }

            ENetAddress address = { 0 };

            /* Connect to some.server.net:1234. */
            enet_address_set_host(&address, "127.0.0.1");
            address.port = 25565;
            /* Initiate the connection, allocating the two channels 0 and 1. */
            peer = enet_host_connect(client, &address, 1, 0);
            if (peer == NULL) {
                fprintf(stderr,
                    "No available peers for initiating an ENet connection.\n");
                exit(EXIT_FAILURE);
            }
            /* Wait up to 5 seconds for the connection attempt to succeed. */
            if (enet_host_service(client, &event, 5000) > 0 &&
                event.type == ENET_EVENT_TYPE_CONNECT) {
                puts("Connection to some.server.net:1234 succeeded.");
            }
            else {
                /* Either the 5 seconds are up or a disconnect event was */
                /* received. Reset the peer in the event the 5 seconds   */
                /* had run out without any significant event.            */
                enet_peer_reset(peer);
                puts("Connection to some.server.net:1234 failed.");
            }

            onlineThread = std::thread(&Client::RunClient, this);
        }

        Client::Client() {
            ipAddress = "127.0.0.1";
            port = 25565;
        }

        void OnlinePacket_Rupees::OnExecute()
        {
            Rupees_ChangeBy(rupeeAmountChanged);
        }
    }
}