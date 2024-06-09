#include <client.hpp>
#include <iostream>

constexpr size_t SENT_BUFFER_SIZE{ 1024 };

CClient::CClient(std::string IP, int port): _ip(IP), _port(port) {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "WSAStartup failed" << std::endl;
    }

    _socket = socket(AF_INET, SOCK_STREAM, 0);
    if (_socket == INVALID_SOCKET) {
        std::cerr << "Socket creation error" << std::endl;
        WSACleanup();
    }

    sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(_port);

    if (inet_pton(AF_INET, _ip.c_str(), &serv_addr.sin_addr) <= 0) {
        std::cerr << "Invalid address/ Address not supported" << std::endl;
        closesocket(_socket);
        WSACleanup();
    }

    if (connect(_socket, (sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        std::cerr << "Connection Failed" << std::endl;
        closesocket(_socket);
        WSACleanup();
    }
}

CClient::~CClient() {
    closesocket(_socket);
    WSACleanup();
}

void CClient::SendRequest(const std::string& req) const {
    size_t RequestLen{ req.size() };
    send(_socket, reinterpret_cast<const char*>(&RequestLen), sizeof(RequestLen), 0);

    const char* pReq{ req.c_str() };
    size_t totalSent{ 0 };
    while (totalSent < RequestLen) {
        int sent = send(_socket, pReq + totalSent, min(SENT_BUFFER_SIZE, RequestLen - totalSent), 0);
        if (sent == SOCKET_ERROR) {
            std::cerr << "Failed to send data" << std::endl;
            exit(1);
            return;
        }
        totalSent += sent;
    }
}

void CClient::RecvResponse(std::string& resp) const {
    size_t ResponseLen;
    int bytesRead{ recv(_socket, reinterpret_cast<char*>(&ResponseLen), sizeof(ResponseLen), 0) };
    if (bytesRead <= 0) {
        closesocket(_socket);
        return;
    }

    char* buffer{ new char[ResponseLen + 1] };
    memset(buffer, 0x0, ResponseLen + 1);
    int bytesCount{ 0 };
    while (true) {
        bytesRead = recv(_socket, buffer + bytesCount, ResponseLen - bytesCount, 0);
        if (bytesRead > 0) {
            bytesCount += bytesRead;
            if (bytesCount == ResponseLen)
                break;
        }
        else if (bytesRead <= 0) {
            closesocket(_socket);
            return;
        }
    }

    resp = buffer;
    delete[] buffer;
}

bool CClient::CheckRequest(const std::string& req) {
    return nlohmann::json::accept(req);
}

void CClient::PrintUsage(const char* programName) {
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    std::cout << "Usage: " << programName << " \"<command>\"" << " \"[<params>]\"" << std::endl;
    std::cout << "Usage: " << programName << " \"<JSON request>/\"" << std::endl;
    SetConsoleTextAttribute(hConsole, 10);
    std::cout << "Example: " << programName << " CheckLocalFile C:\\test.exe 4d5a" << std::endl;
    std::cout << "Example: " << programName << " QuarantineLocalFile C:\\test.exe" << std::endl;
    std::cout << "Example: " << programName << " \"{\\\"command\\\":\\\"QuarantineLocalFile\\\", \\\"params\\\":{\\\"filePath\\\":\\\"C:\\\\test.exe\\\"}}\"" << std::endl;
    std::cout << "Example: " << programName << " \"{\\\"command\\\":\\\"CheckLocalFile\\\", \\\"params\\\":{\\\"filePath\\\":\\\"C:\\\\test.exe\\\", \\\"signature\\\":\\\"4d5a\\\"}}\"" << std::endl;
    SetConsoleTextAttribute(hConsole, 7);
    std::cout << "Commands:" << std::endl;
    std::cout << "  CheckLocalFile: Checks the specified file for a given signature." << std::endl;
    std::cout << "  QuarantineLocalFile: Moves the specified file to quarantine." << std::endl;
}

void CClient::PrintInfo() {
    std::cout << "\nClient Program" << std::endl;
    std::cout << "This program sends JSON requests to a server and processes the responses." << std::endl;
    std::cout << "Supported commands: CheckLocalFile, QuarantineLocalFile" << std::endl;
}

void CClient::ParseResponse(const std::string& response) const {
    if (!nlohmann::json::accept(response)) {
        std::cout << response << std::endl;
        return;
    }

    nlohmann::json json;
    try {
        json = nlohmann::json::parse(response);
    }
    catch (const nlohmann::json::parse_error& e) {
        std::cerr << "JSON parsing error: " << e.what() << std::endl;
        return;
    }

    std::cout << std::setw(16) << "dec" << " : " << std::setw(18) << "hex" << std::endl;
    for (const auto& element : json) {
        for (const auto& el : element) {
            if (el.is_number()) {
                int val = el.get<int>();
                std::cout << std::dec << std::setw(16) << val;
                std::cout << " : 0x" << std::hex << std::setw(16) << std::setfill('0') << val << '\n';
            }
        }
    }
}

bool CClient::ParseSingleArgument(const std::string& arg) {
    if (!CheckRequest(arg)) {
        std::cout << "\nInvalid JSON format or missing params\n\n";
        exit(1);
    }
    return true;
}

nlohmann::json CClient::CreateParams(const std::string& command, const std::vector<std::string>& args) {
    nlohmann::json params;
    if (command == "CheckLocalFile" && args.size() == 2) {
        params["filePath"] = args[0];
        params["signature"] = args[1];
    }
    else if (command == "QuarantineLocalFile" && args.size() == 1) {
        params["filePath"] = args[0];
    }
    else {
        std::cout << "\nMissing params or wrong command\n\n";
        exit(1);
    }
    return params;
}

std::string CClient::ParseMultipleArguments(int argc, char* argv[]) {
    std::string command = argv[1];
    std::vector<std::string> args(argv + 2, argv + argc);

    nlohmann::json params = CreateParams(command, args);
    nlohmann::json jsonRequest = CreateJsonRequest(command, params);

    return jsonRequest.dump();
}

nlohmann::json CClient::CreateJsonRequest(const std::string& command, const nlohmann::json& params) {
    nlohmann::json json;
    json["command"] = command;
    json["params"] = params;
    return json;
}

std::string CClient::ParseArguments(int argc, char* argv[]) {
    if (argc == 2) {
        return (ParseSingleArgument(argv[1])) ? argv[1] : "";
    }
    else {
        return ParseMultipleArguments(argc, argv);
    }
}
