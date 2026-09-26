#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include "ParkingSystem.h"
#include <string>

class WebServer {
public:
    WebServer(ParkingSystem& parkingSystem, int port = 8080);
    void start();

private:
    ParkingSystem& parkingSystem;
    int port;

    void handleClient(long long clientSocket);
    void handleRequest(long long clientSocket, const std::string& method, const std::string& path,
                       const std::string& body);

    static std::string urlDecode(const std::string& value);
    static std::string formValue(const std::string& body, const std::string& key);
    static std::string jsonEscape(const std::string& value);
    static std::string makeJsonResult(const OperationResult& result);
    static std::string contentType(const std::string& path);
    static bool readFile(const std::string& path, std::string& output);
    static void sendResponse(long long clientSocket, int statusCode, const std::string& statusText,
                             const std::string& type, const std::string& body);
    void sendJson(long long clientSocket, const std::string& body, int statusCode = 200,
                  const std::string& statusText = "OK");
    void sendNotFound(long long clientSocket);
    void sendMethodNotAllowed(long long clientSocket);
};

#endif
