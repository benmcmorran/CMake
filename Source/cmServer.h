/* Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
   file Copyright.txt or https://cmake.org/licensing for details.  */
#pragma once

#include "cmConfigure.h" // IWYU pragma: keep

#include <memory>
#include <string>
#include <vector>

#include "cm_jsoncpp_value.h"

#include "cmServerBase.h"

class cmConnection;
class cmFileMonitor;
class cmServerProtocol;
class cmServerRequest;
class cmServerResponse;

class cmServer : public cmServerBase
{
public:
  class DebugInfo;

  cmServer(cmConnection* conn, bool supportExperimental);
  ~cmServer() override;

  cmServer(cmServer const&) = delete;
  cmServer& operator=(cmServer const&) = delete;

  bool Serve(std::string* errorMessage) override;

  cmFileMonitor* FileMonitor() const;

private:
  void RegisterProtocol(cmServerProtocol* protocol);

  // Callbacks from cmServerConnection:

  void ProcessRequest(cmConnection* connection,
                      const std::string& request) override;
  std::shared_ptr<cmFileMonitor> fileMonitor;

public:
  void OnServeStart() override;

  void StartShutDown() override;

public:
  void OnConnected(cmConnection* connection) override;

private:
  static void reportProgress(const std::string& msg, float progress,
                             const cmServerRequest& request);
  static void reportMessage(const std::string& msg, const char* title,
                            const cmServerRequest& request);

  // Handle requests:
  cmServerResponse SetProtocolVersion(const cmServerRequest& request);

  void PrintHello(cmConnection* connection) const;

  // Write responses:
  void WriteProgress(const cmServerRequest& request, int min, int current,
                     int max, const std::string& message) const;
  void WriteMessage(const cmServerRequest& request, const std::string& message,
                    const std::string& title) const;
  void WriteResponse(cmConnection* connection,
                     const cmServerResponse& response,
                     const DebugInfo* debug) const;
  void WriteParseError(cmConnection* connection,
                       const std::string& message) const;
  void WriteSignal(const std::string& name, const Json::Value& obj) const;

  void WriteJsonObject(Json::Value const& jsonValue,
                       const DebugInfo* debug) const;

  void WriteJsonObject(cmConnection* connection, Json::Value const& jsonValue,
                       const DebugInfo* debug) const;

  static cmServerProtocol* FindMatchingProtocol(
    const std::vector<cmServerProtocol*>& protocols, int major, int minor);

  const bool SupportExperimental;

  cmServerProtocol* Protocol = nullptr;
  std::vector<cmServerProtocol*> SupportedProtocols;

  friend class cmServerProtocol;
  friend class cmServerRequest;
};
