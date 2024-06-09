#include <iostream>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <algorithm>
#include <client.hpp>
#include "json.hpp"

#pragma comment(lib, "Ws2_32.lib")

constexpr int PORT{ 8888 };
const char* IP{ "127.0.0.1" };

int main(int argc, char* argv[]) {
    if (argc < 2) { 
        CClient::PrintInfo();
        CClient::PrintUsage(argv[0]);
        return 1;
    }

    std::string request{ CClient::ParseArguments(argc, argv) };

    CClient client(IP, PORT);
    
    client.SendRequest(request);

    std::string response;
    client.RecvResponse(response);

    std::cout << "\nResponse from server: \n" << std::endl;
    client.ParseResponse(response);

    return 0;
}