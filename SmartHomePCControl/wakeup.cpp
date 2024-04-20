#include <iostream>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define PORT 10675

using namespace std;

// Function to create and send the Wake-on-LAN magic packet
void sendMagicPacket(const string& macAddress) {
    // Parse MAC address
    unsigned char macBytes[6];
    sscanf(macAddress.c_str(), "%2hhx:%2hhx:%2hhx:%2hhx:%2hhx:%2hhx",
           &macBytes[0], &macBytes[1], &macBytes[2],
           &macBytes[3], &macBytes[4], &macBytes[5]);

    // Construct the magic packet (6 bytes of 0xFF followed by 16 repetitions of the MAC address)
    unsigned char magicPacket[102];
    memset(magicPacket, 0xFF, 6);
    for (int i = 6; i < 102; i += 6) {
        memcpy(&magicPacket[i], macBytes, 6);
    }

    // Send the magic packet
    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock == -1) {
        cerr << "Failed to create socket for sending magic packet\n";
        return;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(9); // WOL port
    addr.sin_addr.s_addr = inet_addr("255.255.255.255"); // Broadcast address

    if (sendto(sock, magicPacket, sizeof(magicPacket), 0, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
        cerr << "Failed to send magic packet\n";
    }

    cout << "Magic packet send to " << macAddress << endl;

    close(sock);
}

int main() {
    // Create TCP socket to listen for "wake-my-pc-on" messages
    int serverSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    if (serverSock == -1) {
        cerr << "Failed to create server socket\n";
        return 1;
    }

    struct sockaddr_in serverAddr;
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(PORT);

    if (bind(serverSock, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) == -1) {
        cerr << "Failed to bind server socket\n";
        close(serverSock);
        return 1;
    }

    if (listen(serverSock, 5) == -1) {
        cerr << "Failed to listen on server socket\n";
        close(serverSock);
        return 1;
    }

    cout << "Socket is listening on port " << PORT << endl;

    // Accept connections and handle wake-on-LAN requests
    while (true) {
        struct sockaddr_in clientAddr;
        socklen_t clientAddrLen = sizeof(clientAddr);
        int clientSock = accept(serverSock, (struct sockaddr*)&clientAddr, &clientAddrLen);
        if (clientSock == -1) {
            cerr << "Failed to accept client connection\n";
            continue;
        }

        // Receive data from client
        char buffer[1024];
        ssize_t bytesRead = recv(clientSock, buffer, sizeof(buffer), 0);
        if (bytesRead <= 0) {
            cerr << "Failed to receive data from client\n";
            close(clientSock);
            continue;
        }

        // Process wake-on-LAN request (in this case, just send the magic packet)
        sendMagicPacket("58:11:22:c8:57:67");

        // Close client socket
        close(clientSock);
    }

    // Close server socket
    close(serverSock);

    return 0;
}

