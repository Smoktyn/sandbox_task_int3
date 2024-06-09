#pragma once
#include <cstddef>
#include <string>
#include <sys/types.h>
#include <vector>
#include <thread>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <BaseTsd.h>

typedef SSIZE_T ssize_t;

constexpr int PORT{ 8888 };

class CServer {
public:
	CServer(int port);
	CServer(int port, std::string& _quarantinePath);
	~CServer();

	void PrintUsage(const char* programName) const;
	void PrintInfo() const;

	void AcceptConnections();
	void RecvAll(const SOCKET clientSocket, std::string& str);
	void SendResp(const SOCKET clientSocket, const std::string& req);

	std::string DispatchRequest(const std::string& req) const;

	bool CheckQuarantinePath(const std::string& QuarantinePath);
	bool SetMaxThreads(const std::string& str);

private:
	std::string QuarantineLocalFile(const std::string& filePath) const;
	std::string CheckLocalFile(const std::string& filePath, std::string& patternStr) const;

	std::vector<uint8_t> ParseHexString(const std::string& hexStr) const;
	std::vector<size_t> FindPattern(const std::string& filePath, const std::vector<uint8_t>& signature) const;

private:
	int _port;
	int _socket;
	std::string _quarantinePath;
	std::vector<std::thread> _threads;
	int _maxThreads{0};
};

void HandleClient(SOCKET clientSocket);

extern CServer g_server;