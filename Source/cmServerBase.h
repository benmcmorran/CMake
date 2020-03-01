/* Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
   file Copyright.txt or https://cmake.org/licensing for details.  */
#pragma once

#include "cmConfigure.h" // IWYU pragma: keep

#include <vector>

#include <cm/shared_mutex>

#include "cmConnection.h"

/***
 * This essentially hold and manages a libuv event queue and responds to
 * messages
 * on any of its connections.
 */
class cmServerBase
{
public:
  cmServerBase(cmConnection* connection);
  virtual ~cmServerBase();

  virtual void AddNewConnection(cmConnection* ownedConnection);

  /***
   * The main override responsible for tailoring behavior towards
   * whatever the given server is supposed to do
   *
   * This should almost always be called by the given connections
   * directly.
   *
   * @param connection The connection the request was received on
   * @param request The actual request
   */
  virtual void ProcessRequest(cmConnection* connection,
                              const std::string& request) = 0;
  virtual void OnConnected(cmConnection* connection);

  /***
   * Start a dedicated thread. If this is used to start the server, it will
   * join on the
   * servers dtor.
   */
  virtual bool StartServeThread();
  virtual bool Serve(std::string* errorMessage);

  virtual void OnServeStart();
  virtual void StartShutDown();

  virtual bool OnSignal(int signum);
  uv_loop_t* GetLoop();
  void Close();
  void OnDisconnect(cmConnection* pConnection);

protected:
  mutable cm::shared_mutex ConnectionsMutex;
  std::vector<std::unique_ptr<cmConnection>> Connections;

  bool ServeThreadRunning = false;
  uv_thread_t ServeThread;
  cm::uv_async_ptr ShutdownSignal;
#ifndef NDEBUG
public:
  // When the server starts it will mark down it's current thread ID,
  // which is useful in other contexts to just assert that operations
  // are performed on that same thread.
  uv_thread_t ServeThreadId = {};

protected:
#endif

  uv_loop_t Loop;

  cm::uv_signal_ptr SIGINTHandler;
  cm::uv_signal_ptr SIGHUPHandler;
};