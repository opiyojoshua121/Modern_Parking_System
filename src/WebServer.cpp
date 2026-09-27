#include "WebServer.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <sstream>
#include <thread>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#ifdef _MSC_VER
#pragma comment(lib, "Ws2_32.lib")
#endif
typedef SOCKET SocketType;
static void closeSocket(SocketType s) { closesocket(s); }
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
typedef int SocketType;
static void closeSocket(SocketType s) { close(s); }
#endif

using namespace std;

namespace {
string jsonNumber(double value) {
    ostringstream out;
    out << fixed << setprecision(2) << value;
    return out.str();
}

string jsonString(const string& value) {
    string result = "\"";
    for (char c : value) {
        switch (c) {
            case '\"': result += "\\\""; break;
            case '\\': result += "\\\\"; break;
            case '\n': result += "\\n"; break;
            case '\r': result += "\\r"; break;
            case '\t': result += "\\t"; break;
            default: result += c;
        }
    }
    result += '"';
    return result;
}

string mimeFor(const string& path) {
    const auto dot = path.find_last_of('.');
    if (dot == string::npos) return "text/plain; charset=utf-8";
    const string ext = path.substr(dot);
    if (ext == ".html") return "text/html; charset=utf-8";
    if (ext == ".css") return "text/css; charset=utf-8";
    if (ext == ".js") return "application/javascript; charset=utf-8";
    if (ext == ".json") return "application/json; charset=utf-8";
    if (ext == ".svg") return "image/svg+xml";
    if (ext == ".png") return "image/png";
    if (ext == ".jpg" || ext == ".jpeg") return "image/jpeg";
    return "application/octet-stream";
}
}

WebServer::WebServer(ParkingSystem& system, int serverPort)
    : parkingSystem(system), port(serverPort) {}

string WebServer::urlDecode(const string& value) {
    string decoded;
    for (size_t i = 0; i < value.size(); ++i) {
        if (value[i] == '+') decoded += ' ';
        else if (value[i] == '%' && i + 2 < value.size()) {
            const string hex = value.substr(i + 1, 2);
            try {
                decoded += static_cast<char>(stoi(hex, nullptr, 16));
                i += 2;
            } catch (...) { decoded += value[i]; }
        } else decoded += value[i];
    }
    return decoded;
}

string WebServer::formValue(const string& body, const string& key) {
    string token;
    stringstream ss(body);
    while (getline(ss, token, '&')) {
        const auto equals = token.find('=');
        if (equals == string::npos) continue;
        const string k = urlDecode(token.substr(0, equals));
        if (k == key) return urlDecode(token.substr(equals + 1));
    }
    return "";
}

string WebServer::jsonEscape(const string& value) { return jsonString(value); }

string WebServer::makeJsonResult(const OperationResult& result) {
    ostringstream out;
    out << "{\"success\":" << (result.success ? "true" : "false")
        << ",\"message\":" << jsonString(result.message)
        << ",\"slotId\":" << result.slotId
        << ",\"durationMinutes\":" << result.durationMinutes
        << ",\"fee\":" << jsonNumber(result.fee)
        << ",\"arrival\":" << jsonString(result.arrival)
        << ",\"departure\":" << jsonString(result.departure)
        << ",\"receiptNumber\":" << jsonString(result.receiptNumber)
        << ",\"vatAmount\":" << jsonNumber(result.vatAmount) << "}";
    return out.str();
}

string WebServer::contentType(const string& path) { return mimeFor(path); }

bool WebServer::readFile(const string& path, string& output) {
    ifstream file(path, ios::binary);
    if (!file) return false;
    ostringstream buffer;
    buffer << file.rdbuf();
    output = buffer.str();
    return true;
}

void WebServer::sendResponse(long long clientSocket, int statusCode, const string& statusText,
                             const string& type, const string& body) {
    ostringstream response;
    response << "HTTP/1.1 " << statusCode << ' ' << statusText << "\r\n"
             << "Content-Type: " << type << "\r\n"
             << "Content-Length: " << body.size() << "\r\n"
             << "Cache-Control: no-cache\r\n"
             << "Connection: close\r\n\r\n"
             << body;
    const string data = response.str();
    size_t sent = 0;
    while (sent < data.size()) {
#ifdef _WIN32
        const int n = send(static_cast<SOCKET>(clientSocket), data.data() + sent,
                           static_cast<int>(data.size() - sent), 0);
#else
        const ssize_t n = send(static_cast<int>(clientSocket), data.data() + sent, data.size() - sent, 0);
#endif
        if (n <= 0) break;
        sent += static_cast<size_t>(n);
    }
}

void WebServer::sendJson(long long clientSocket, const string& body, int statusCode, const string& statusText) {
    sendResponse(clientSocket, statusCode, statusText, "application/json; charset=utf-8", body);
}

void WebServer::sendNotFound(long long clientSocket) {
    sendJson(clientSocket, "{\"success\":false,\"message\":\"Resource not found\"}", 404, "Not Found");
}

void WebServer::sendMethodNotAllowed(long long clientSocket) {
    sendJson(clientSocket, "{\"success\":false,\"message\":\"Method not allowed\"}", 405, "Method Not Allowed");
}

void WebServer::handleRequest(long long clientSocket, const string& method, const string& rawPath,
                              const string& body) {
    string path = rawPath;
    const auto query = path.find('?');
    string queryString;
    if (query != string::npos) {
        queryString = path.substr(query + 1);
        path = path.substr(0, query);
    }

    if (path == "/api/status" && method == "GET") {
        const auto slots = parkingSystem.getSlots();
        ostringstream out;
        out << "{\"success\":true,\"totalSlots\":" << parkingSystem.totalSlots()
            << ",\"availableSlots\":" << parkingSystem.availableSlots()
            << ",\"occupiedSlots\":" << parkingSystem.occupiedSlots()
            << ",\"activeVehicles\":" << parkingSystem.activeVehicleCount()
            << ",\"completedTrips\":" << parkingSystem.completedTripCount()
            << ",\"revenue\":" << jsonNumber(parkingSystem.totalRevenue())
            << ",\"vatCollected\":" << jsonNumber(parkingSystem.totalVat()) << ",\"slots\":[";
        for (size_t i = 0; i < slots.size(); ++i) {
            if (i) out << ',';
            out << "{\"id\":" << slots[i].id
                << ",\"occupied\":" << (slots[i].occupied ? "true" : "false")
                << ",\"plate\":" << jsonString(slots[i].plateNumber) << '}';
        }
        out << "]}";
        sendJson(clientSocket, out.str());
        return;
    }

    if (path == "/api/transactions" && method == "GET") {
        const auto transactions = parkingSystem.getTransactions();
        ostringstream out;
        out << "{\"success\":true,\"transactions\":[";
        for (size_t i = 0; i < transactions.size(); ++i) {
            const auto& t = transactions[i];
            if (i) out << ',';
            out << "{\"plate\":" << jsonString(t.plateNumber)
                << ",\"slot\":" << t.slotId
                << ",\"arrival\":" << jsonString(t.arrival)
                << ",\"departure\":" << jsonString(t.departure)
                << ",\"durationMinutes\":" << t.durationMinutes
                << ",\"fee\":" << jsonNumber(t.fee)
                << ",\"paymentStatus\":" << jsonString(t.paymentStatus)
                << ",\"paymentMethod\":" << jsonString(t.paymentMethod)
                << ",\"paymentReference\":" << jsonString(t.paymentReference)
                << ",\"receiptNumber\":" << jsonString(t.receiptNumber)
                << ",\"vatAmount\":" << jsonNumber(t.vatAmount) << '}';
        }
        out << "]}";
        sendJson(clientSocket, out.str());
        return;
    }

    if (path == "/api/rates" && method == "GET") {
        const auto rates = parkingSystem.getRates();
        ostringstream out;
        out << "{\"success\":true,\"freeMinutes\":" << rates.freeMinutes
            << ",\"firstBandMinutes\":" << rates.firstBandMinutes
            << ",\"secondBandMinutes\":" << rates.secondBandMinutes
            << ",\"firstBandFee\":" << jsonNumber(rates.firstBandFee)
            << ",\"secondBandFee\":" << jsonNumber(rates.secondBandFee)
            << ",\"overSecondBandFee\":" << jsonNumber(rates.overSecondBandFee)
            << ",\"vatPercent\":" << jsonNumber(rates.vatPercent) << '}';
        sendJson(clientSocket, out.str());
        return;
    }

    if (path == "/api/arrival" && method == "POST") {
        const auto result = parkingSystem.registerArrival(formValue(body, "plate"), formValue(body, "owner"));
        sendJson(clientSocket, makeJsonResult(result), result.success ? 200 : 400, result.success ? "OK" : "Bad Request");
        return;
    }

    if (path == "/api/exit" && method == "POST") {
        const auto result = parkingSystem.processExit(formValue(body, "plate"));
        sendJson(clientSocket, makeJsonResult(result), result.success ? 200 : 400, result.success ? "OK" : "Bad Request");
        return;
    }

    if (path == "/api/payment" && method == "POST") {
        try {
            const auto result = parkingSystem.confirmPayment(formValue(body, "plate"), formValue(body, "method"),
                                                             formValue(body, "reference"), stod(formValue(body, "quotedFee")));
            sendJson(clientSocket, makeJsonResult(result), result.success ? 200 : 400,
                     result.success ? "OK" : "Bad Request");
        } catch (...) {
            sendJson(clientSocket, "{\"success\":false,\"message\":\"Recalculate the fee before confirming payment.\"}",
                     400, "Bad Request");
        }
        return;
    }

    if (path == "/api/rates" && method == "POST") {
        try {
            ParkingRates rates;
            rates.freeMinutes = stoll(formValue(body, "freeMinutes"));
            rates.firstBandMinutes = stoll(formValue(body, "firstBandMinutes"));
            rates.secondBandMinutes = stoll(formValue(body, "secondBandMinutes"));
            rates.firstBandFee = stod(formValue(body, "firstBandFee"));
            rates.secondBandFee = stod(formValue(body, "secondBandFee"));
            rates.overSecondBandFee = stod(formValue(body, "overSecondBandFee"));
            rates.vatPercent = stod(formValue(body, "vatPercent"));
            string error;
            if (!parkingSystem.updateRates(rates, error)) {
                sendJson(clientSocket, "{\"success\":false,\"message\":" + jsonString(error) + "}", 400, "Bad Request");
                return;
            }
            sendJson(clientSocket, "{\"success\":true,\"message\":\"Rates saved.\"}");
        } catch (...) {
            sendJson(clientSocket, "{\"success\":false,\"message\":\"Enter valid numeric rate settings.\"}", 400, "Bad Request");
        }
        return;
    }

    if (path == "/api/search" && method == "GET") {
        const auto result = parkingSystem.searchVehicle(formValue(queryString, "plate"));
        if (!result.success) {
            sendJson(clientSocket, makeJsonResult(result), 404, "Not Found");
            return;
        }
        ostringstream out;
        out << "{\"success\":true,\"message\":" << jsonString(result.message)
            << ",\"slotId\":" << result.slotId
            << ",\"arrival\":" << jsonString(result.arrival) << "}";
        sendJson(clientSocket, out.str());
        return;
    }

    if (method != "GET") {
        sendMethodNotAllowed(clientSocket);
        return;
    }

    if (path == "/") path = "/index.html";
    if (path == "/board") path = "/board.html";
    if (path.find("..") != string::npos) {
        sendNotFound(clientSocket);
        return;
    }

    string content;
    if (!readFile("web" + path, content)) {
        sendNotFound(clientSocket);
        return;
    }
    sendResponse(clientSocket, 200, "OK", contentType(path), content);
}

void WebServer::handleClient(long long clientSocket) {
    string request;
    char buffer[8192];
    int total = 0;

    while (request.find("\r\n\r\n") == string::npos && total < 65536) {
#ifdef _WIN32
        const int n = recv(static_cast<SOCKET>(clientSocket), buffer, sizeof(buffer), 0);
#else
        const ssize_t n = recv(static_cast<int>(clientSocket), buffer, sizeof(buffer), 0);
#endif
        if (n <= 0) break;
        request.append(buffer, buffer + n);
        total += n;
    }

    const auto headerEnd = request.find("\r\n\r\n");
    if (headerEnd == string::npos) {
        sendNotFound(clientSocket);
        return;
    }

    const string headers = request.substr(0, headerEnd);
    string body = request.substr(headerEnd + 4);
    istringstream headerStream(headers);
    string requestLine;
    getline(headerStream, requestLine);
    if (!requestLine.empty() && requestLine.back() == '\r') requestLine.pop_back();

    istringstream requestLineStream(requestLine);
    string method, path, version;
    requestLineStream >> method >> path >> version;

    size_t contentLength = 0;
    string headerLine;
    while (getline(headerStream, headerLine)) {
        if (!headerLine.empty() && headerLine.back() == '\r') headerLine.pop_back();
        const string prefix = "Content-Length:";
        if (headerLine.size() >= prefix.size() && equal(prefix.begin(), prefix.end(), headerLine.begin(),
                                                       [](char a, char b) { return tolower(a) == tolower(b); })) {
            try { contentLength = static_cast<size_t>(stoul(headerLine.substr(prefix.size()))); } catch (...) {}
        }
    }

    while (body.size() < contentLength && body.size() < 2 * 1024 * 1024) {
#ifdef _WIN32
        const int n = recv(static_cast<SOCKET>(clientSocket), buffer, sizeof(buffer), 0);
#else
        const ssize_t n = recv(static_cast<int>(clientSocket), buffer, sizeof(buffer), 0);
#endif
        if (n <= 0) break;
        body.append(buffer, buffer + n);
    }
    if (body.size() > contentLength) body.resize(contentLength);

    handleRequest(clientSocket, method, path, body);
}

void WebServer::start() {
#ifdef _WIN32
    WSADATA wsaData{};
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        throw runtime_error("WSAStartup failed.");
    }
#endif

    SocketType serverSocket = socket(AF_INET, SOCK_STREAM, 0);
#ifdef _WIN32
    if (serverSocket == INVALID_SOCKET) {
        WSACleanup();
        throw runtime_error("Could not create server socket.");
    }
#else
    if (serverSocket < 0) throw runtime_error("Could not create server socket.");
#endif

    int option = 1;
#ifdef _WIN32
    setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&option), sizeof(option));
#else
    setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option));
#endif

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(static_cast<uint16_t>(port));

    if (bind(serverSocket, reinterpret_cast<sockaddr*>(&address), sizeof(address)) < 0) {
        closeSocket(serverSocket);
#ifdef _WIN32
        WSACleanup();
#endif
        throw runtime_error("Could not bind to port " + to_string(port) + ". Is another program using it?");
    }

    if (listen(serverSocket, 16) < 0) {
        closeSocket(serverSocket);
#ifdef _WIN32
        WSACleanup();
#endif
        throw runtime_error("Could not start listening socket.");
    }

    cout << "\n===============================================\n"
         << "       MMU MODERN PARKING SYSTEM\n"
         << "===============================================\n"
         << "Web server running at: http://localhost:" << port << "\n"
         << "Press Ctrl+C to stop the server.\n\n";

    while (true) {
        sockaddr_in clientAddress{};
#ifdef _WIN32
        int clientLength = sizeof(clientAddress);
#else
        socklen_t clientLength = sizeof(clientAddress);
#endif
        SocketType client = accept(serverSocket, reinterpret_cast<sockaddr*>(&clientAddress), &clientLength);
#ifdef _WIN32
        if (client == INVALID_SOCKET) continue;
#else
        if (client < 0) continue;
#endif
        thread([this, client]() {
            handleClient(static_cast<long long>(client));
            closeSocket(client);
        }).detach();
    }
}
