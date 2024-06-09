#pragma once
#include <cstddef>
#include <string>
#include <sys/types.h>
#include <vector>
#include <thread>
#include "json.hpp"
#include <winsock2.h>
#include <ws2tcpip.h>
#include <BaseTsd.h>

typedef SSIZE_T ssize_t;


class CClient {
public:
	CClient(std::string IP, int port);
	~CClient();

	static void PrintUsage(const char* programName);
	static void PrintInfo();

	void SendRequest(const std::string& req) const;
	void RecvResponse(std::string& resp) const;

	static bool CheckRequest(const std::string& req);

	void ParseResponse(const std::string& response) const;
	static std::string ParseArguments(int argc, char* argv[]);

private:
	static std::string ParseMultipleArguments(int argc, char* argv[]);
	static bool ParseSingleArgument(const std::string& arg);
	static nlohmann::json CreateParams(const std::string& command, const std::vector<std::string>& args);
	static nlohmann::json CreateJsonRequest(const std::string& command, const nlohmann::json& params);

private:
	SOCKET _socket;
	std::string _ip;
	int _port;
};