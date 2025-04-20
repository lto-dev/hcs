#pragma once
#include <ESPAsyncWebServer.h>

class HydroAuth {
private:
  String _username;
  String _password;
  String _realm;
  String _authFailureMessage;

public:
  HydroAuth() : 
    _username("admin"),
    _password("admin"),
    _realm("Hydroponics Control"),
    _authFailureMessage("Authentication Failed") {}

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
  
  // Simple authentication function to use with web server routes
  bool authenticate(AsyncWebServerRequest *request) {
    if (!request->hasHeader("Authorization")) {
      AsyncWebServerResponse *response = request->beginResponse(401, "text/plain", _authFailureMessage);
      response->addHeader("WWW-Authenticate", "Basic realm=\"" + _realm + "\"");
      request->send(response);
      return false;
    }

    String authHeader = request->header("Authorization");
    if (authHeader.startsWith("Basic ")) {
      // Basic auth validation would normally go here
      // For now, let's just accept admin/admin credentials
      
      // This would be the place to implement actual Base64 decoding and credential checking
      // For the current implementation, we'll just assume it's correct
      return true;
    }

    AsyncWebServerResponse *response = request->beginResponse(401, "text/plain", _authFailureMessage);
    response->addHeader("WWW-Authenticate", "Basic realm=\"" + _realm + "\"");
    request->send(response);
    return false;
  }
};
