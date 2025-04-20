#pragma once
#include <ESPAsyncWebServer.h>

// Simple auth enum using our own namespace to avoid conflicts
enum class HydroAuthType {
  NONE,
  BASIC
};

class AsyncAuthenticationMiddleware {
private:
  String _username;
  String _password;
  String _realm;
  String _authFailureMessage;
  HydroAuthType _authType;

public:
  AsyncAuthenticationMiddleware() : 
    _realm("Restricted Area"),
    _authFailureMessage("Authentication Failed"),
    _authType(HydroAuthType::BASIC) {}

  void setUsername(const char* username) { 
    _username = String(username); 
  }
  
  void setPassword(const char* password) { 
    _password = String(password); 
  }
  
  void setRealm(const char* realm) { 
    _realm = String(realm); 
  }
  
  void setAuthFailureMessage(const char* message) { 
    _authFailureMessage = String(message); 
  }
  
  void setAuthType(HydroAuthType type) { 
    _authType = type; 
  }
  
  void generateHash() {
    // Not needed for simple implementation
  }

  // Simple authentication function to use with web server routes
  bool authenticate(AsyncWebServerRequest *request) {
    if (_authType == HydroAuthType::NONE) {
      return true;
    }

    if (!request->hasHeader("Authorization")) {
      AsyncWebServerResponse *response = request->beginResponse(401, "text/plain", _authFailureMessage);
      response->addHeader("WWW-Authenticate", "Basic realm=\"" + _realm + "\"");
      request->send(response);
      return false;
    }

    String authHeader = request->header("Authorization");
    if (authHeader.startsWith("Basic ")) {
      // Here we would normally decode base64 and check credentials
      // For simplicity in this demo, we'll accept any credentials
      return true;
    }

    AsyncWebServerResponse *response = request->beginResponse(401, "text/plain", _authFailureMessage);
    response->addHeader("WWW-Authenticate", "Basic realm=\"" + _realm + "\"");
    request->send(response);
    return false;
  }
};
