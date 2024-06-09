#include <server.h>
#include <iostream>
#include <thread>
#include <vector>
#include <winsock2.h>
#include <ws2tcpip.h>

#pragma comment(lib, "Ws2_32.lib")

int main(int argc, char* argv[]) {
	setlocale(LC_ALL, "Russian");

	if (argc < 2) { g_server.PrintInfo(); g_server.PrintUsage(argv[0]); return 1; }
	std::string QuarantinePath(argv[1]);
	if(!g_server.CheckQuarantinePath(QuarantinePath)) { return 1; }

	if (argc == 3) {
		std::string ThreadsCount(argv[2]);
		if (!g_server.SetMaxThreads(ThreadsCount)) { std::cerr << "Incorrect input threads count\tSet max available count\n"; }
	}
	
	g_server.AcceptConnections();

	return 0;
}