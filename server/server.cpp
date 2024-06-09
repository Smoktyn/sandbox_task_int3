#include <server.h>
#include <iostream>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <fstream>
#include <string>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <filesystem>
#include "json.hpp"
#include <io.h>
#include <winsock2.h>
#include <ws2tcpip.h>

namespace fs = std::filesystem;

constexpr size_t SENT_BUFFER_SIZE{ 1024 };

std::mutex g_mutex;
std::condition_variable g_cv;
std::queue<SOCKET> g_clientQueue;
bool g_shutdown = false;

CServer g_server(PORT);

CServer::CServer(int port) : _port(port) {
	WSADATA wsaData;
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
		std::cerr << "WSAStartup failed" << std::endl;
	}

	_socket = socket(AF_INET, SOCK_STREAM, 0);
	if (_socket == INVALID_SOCKET) {
		std::cerr << "Socket creation failed" << std::endl;
		WSACleanup();
	}

	sockaddr_in address;
	address.sin_family = AF_INET;
	address.sin_addr.s_addr = INADDR_ANY;
	address.sin_port = htons(_port);

	if (bind(_socket, (sockaddr*)&address, sizeof(address)) == SOCKET_ERROR) {
		std::cerr << "Bind failed" << std::endl;
		closesocket(_socket);
		WSACleanup();
	}

	if (listen(_socket, 3) == SOCKET_ERROR) {
		std::cerr << "Listen failed" << std::endl;
		closesocket(_socket);
		WSACleanup();
	}
}
CServer::CServer(int port, std::string& _quarantinePath) : CServer(port) {
	CheckQuarantinePath(_quarantinePath);
}

CServer::~CServer() {
	closesocket(_socket);
	WSACleanup();

	{
		std::lock_guard<std::mutex> lock(g_mutex);
		g_shutdown = true;
	}
	g_cv.notify_all();
}

void CServer::RecvAll(const SOCKET clientSocket, std::string& str) {
	size_t RequestLen;
	int bytesRead{ recv(clientSocket, reinterpret_cast<char*>(&RequestLen), sizeof(RequestLen), 0) };
	if (bytesRead <= 0) {
		closesocket(clientSocket);
		return;
	}

	char* buffer{ new char[RequestLen + 1] };
	memset(buffer, 0x0, RequestLen + 1);
	int bytesCount{ 0 };
	while (true) {
		bytesRead = recv(clientSocket, buffer + bytesCount, RequestLen - bytesCount, 0);
		if (bytesRead > 0) {
			bytesCount += bytesRead;
			if (bytesCount == RequestLen)
				break;
		}
		else if (bytesRead <= 0) {
			closesocket(clientSocket);
			return;
		}
	}

	str = buffer;
	delete[] buffer;
}

void CServer::SendResp(const SOCKET clientSocket, const std::string& req) {
	size_t RequestLen{ req.size() };
	send(clientSocket, reinterpret_cast<const char*>(&RequestLen), sizeof(RequestLen), 0);
	const char* pReq{ req.c_str() };
	size_t totalSent = 0;
	while (totalSent < RequestLen) {
		int sent = send(clientSocket, pReq + totalSent, min(SENT_BUFFER_SIZE, RequestLen - totalSent), 0);
		if (sent == SOCKET_ERROR) {
			std::cerr << "Failed to send data" << std::endl;
			return;
		}
		totalSent += sent;
	}
}

void HandleClient(SOCKET clientSocket) {
	std::string request;
	{
		std::lock_guard<std::mutex> lock(g_mutex);
		g_server.RecvAll(clientSocket, request);
	}

	std::string status = g_server.DispatchRequest(request);
	g_server.SendResp(clientSocket, status);

	closesocket(clientSocket);
}

void WorkerThread() {
	while (true) {
		SOCKET clientSocket;
		{
			std::unique_lock<std::mutex> lock(g_mutex);
			g_cv.wait(lock, [] { return !g_clientQueue.empty() || g_shutdown; });

			if (g_shutdown && g_clientQueue.empty()) break;

			clientSocket = g_clientQueue.front();
			g_clientQueue.pop();
		}

		HandleClient(clientSocket);
	}
}

void CServer::AcceptConnections() {
	_maxThreads = (_maxThreads > 0 && (_maxThreads <= std::thread::hardware_concurrency())) ? _maxThreads : std::thread::hardware_concurrency();
	std::vector<std::thread> threads;

	for (int i = 0; i < _maxThreads; ++i) {
		threads.emplace_back(WorkerThread);
	}

	while (true) {
		SOCKET clientSocket = accept(_socket, nullptr, nullptr);
		if (clientSocket == INVALID_SOCKET) {
			std::cerr << "Accept failed" << std::endl;
			closesocket(_socket);
			WSACleanup();
			break;
		}

		{
			std::lock_guard<std::mutex> lock(g_mutex);
			g_clientQueue.push(clientSocket);
		}
		g_cv.notify_one();
	}

	{
		std::lock_guard<std::mutex> lock(g_mutex);
		g_shutdown = true;
	}
	g_cv.notify_all();

	for (auto& th : threads) {
		if (th.joinable()) {
			th.join();
		}
	}
}

std::string CServer::DispatchRequest(const std::string& req) const {
	nlohmann::json parsed_req{ nlohmann::json::parse(req) };

	if (parsed_req.contains("command") && parsed_req.contains("params")) {
		std::string command = parsed_req["command"];
		auto params = parsed_req["params"];

		if (command == "CheckLocalFile" && params.contains("filePath") && params.contains("signature")) {
			std::string filePath = params["filePath"];
			std::string pattern = params["signature"];
			return CheckLocalFile(filePath, pattern);
		}
		else if (command == "QuarantineLocalFile" && params.contains("filePath")) {
			std::string filePath = params["filePath"];
			return QuarantineLocalFile(filePath);
		}
		else {
			std::cerr << "Invalid command or missing parameters" << std::endl;
			return std::string("Invalid command or missing parameters\n");
		}
	}
	else {
		std::cerr << "Invalid JSON format" << std::endl;
		return std::string("Invalid JSON format\n");
	}
}

std::string CServer::QuarantineLocalFile(const std::string& filePath) const {
	try {
		if (fs::exists(filePath)) {
			fs::rename(filePath, _quarantinePath + '/' + fs::path(filePath).filename().string());

			std::cout << "File moved to quarantine: " << _quarantinePath << std::endl;
			return std::string("File moved to quarantine: " + _quarantinePath + '\n');
		}
		else {
			std::cout << "File does not exist: " << filePath << std::endl;
			return std::string("File does not exist: " + filePath + '\n');
		}
	}
	catch (const fs::filesystem_error& e) {
		std::cerr << "Error during file quarantine: " << e.what() << std::endl;
		return std::string("Error during file quarantine\n");
	}
}

bool CServer::CheckQuarantinePath(const std::string& QuarantinePath) {
	if (QuarantinePath.empty()) { g_server.PrintInfo(); g_server.PrintUsage("server"); return 1; }
	if (!fs::exists(QuarantinePath)) {
		char ch{};
		std::cout << "\nThe directory for quarantine does not exist, create? [Y] / [N]\n";
		std::cin >> ch;
		if (ch == 'N' || ch == 'n') return false;
		else if (ch == 'Y' || ch == 'y') { fs::create_directories(QuarantinePath); }
		else { return false; }
	}
	_quarantinePath = QuarantinePath;
	return true;
}

std::vector<uint8_t> CServer::ParseHexString(const std::string& hexStr) const {
	std::vector<uint8_t> bytes;
	for (size_t i = 0; i < hexStr.length(); i += 2) {
		std::string byteString = hexStr.substr(i, 2);
		uint8_t byte = static_cast<uint8_t>(strtol(byteString.c_str(), nullptr, 16));
		bytes.push_back(byte);
	}
	return bytes;
}

std::vector<size_t> CServer::FindPattern(const std::string& filePath, const std::vector<uint8_t>& signature) const {
	std::vector<size_t> offsets;
	std::ifstream file(filePath, std::ios::binary);

	if (!file.is_open()) {
		std::cerr << "Failed to open file: " << filePath << std::endl;
		return offsets;
	}

	std::vector<uint8_t> buffer(std::istreambuf_iterator<char>(file), {});

	if (signature.size() > buffer.size()) return offsets;

	for (size_t i = 0; i <= buffer.size() - signature.size(); ++i) {
		if (std::equal(signature.begin(), signature.end(), buffer.begin() + i)) {
			offsets.push_back(i);
		}
	}

	return offsets;
}

std::string CServer::CheckLocalFile(const std::string& filePath, std::string& patternStr) const {
	if (patternStr.empty())  return std::string("Signature is not set\n");
	if (fs::exists(filePath)) {
		std::vector<uint8_t> patternHex{ ParseHexString(patternStr) };
		std::vector<size_t> offsets{ FindPattern(filePath, patternHex) };
		if (offsets.empty()) return std::string("Offsets not found\n");

		nlohmann::json response;
		response["offsets"] = offsets;

		std::cout << response.dump() << std::endl;
		return std::string(response.dump() + '\n');
	}
	else {
		std::cout << "File does not exist: " << filePath << std::endl;
		return std::string("File does not exist: " + filePath + '\n');
	}
}

bool CServer::SetMaxThreads(const std::string& str) {
	for (char const& c : str) {
		if (!std::isdigit(c)) return false;
	}

	_maxThreads = std::stoi(str);
	return true;
}

void CServer::PrintUsage(const char* programName) const {
	std::cout << "Server Usage:" << std::endl;
	std::cout << "  " << programName << " \"quarantine_directory\" (optional)<threads in pool>" << std::endl;
}

void CServer::PrintInfo() const {
	std::cout << "\nServer Program" << std::endl;
	std::cout << "This program receives JSON requests from clients and processes them." << std::endl;
	std::cout << "Supported commands: CheckLocalFile, QuarantineLocalFile" << std::endl;
}