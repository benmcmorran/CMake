/* Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
   file Copyright.txt or https://cmake.org/licensing for details.  */
#include "cmServerBase.h"

#include <algorithm>
#include <cassert>
#include <iostream>

void on_signal(uv_signal_t* signal, int signum)
{
  auto conn = static_cast<cmServerBase*>(signal->data);
  conn->OnSignal(signum);
}

static void on_walk_to_shutdown(uv_handle_t* handle, void* arg)
{
  (void)arg;
  assert(uv_is_closing(handle));
  if (!uv_is_closing(handle)) {
    uv_close(handle, &cmEventBasedConnection::on_close);
  }
}

static void __start_thread(void* arg)
{
  auto server = static_cast<cmServerBase*>(arg);
  std::string error;
  bool success = server->Serve(&error);
  if (!success || !error.empty()) {
    std::cerr << "Error during serve: " << error << std::endl;
  }
}

bool cmServerBase::StartServeThread()
{
  ServeThreadRunning = true;
  uv_thread_create(&ServeThread, __start_thread, this);
  return true;
}

static void __shutdownThread(uv_async_t* arg)
{
  auto server = static_cast<cmServerBase*>(arg->data);
  server->StartShutDown();
}

bool cmServerBase::Serve(std::string* errorMessage)
{
#ifndef NDEBUG
  uv_thread_t blank_thread_t = {};
  assert(uv_thread_equal(&blank_thread_t, &ServeThreadId));
  ServeThreadId = uv_thread_self();
#endif

  errorMessage->clear();

  ShutdownSignal.init(Loop, __shutdownThread, this);

  SIGINTHandler.init(Loop, this);
  SIGHUPHandler.init(Loop, this);

  SIGINTHandler.start(&on_signal, SIGINT);
  SIGHUPHandler.start(&on_signal, SIGHUP);

  OnServeStart();

  {
    cm::shared_lock<cm::shared_mutex> lock(ConnectionsMutex);
    for (auto& connection : Connections) {
      if (!connection->OnServeStart(errorMessage)) {
        return false;
      }
    }
  }

  if (uv_run(&Loop, UV_RUN_DEFAULT) != 0) {
    // It is important we don't ever let the event loop exit with open handles
    // at best this is a memory leak, but it can also introduce race conditions
    // which can hang the program.
    assert(false && "Event loop stopped in unclean state.");

    *errorMessage = "Internal Error: Event loop stopped in unclean state.";
    return false;
  }

  return true;
}

void cmServerBase::OnConnected(cmConnection*)
{
}

void cmServerBase::OnServeStart()
{
}

void cmServerBase::StartShutDown()
{
  ShutdownSignal.reset();
  SIGINTHandler.reset();
  SIGHUPHandler.reset();

  {
    std::unique_lock<cm::shared_mutex> lock(ConnectionsMutex);
    for (auto& connection : Connections) {
      connection->OnConnectionShuttingDown();
    }
    Connections.clear();
  }

  uv_walk(&Loop, on_walk_to_shutdown, nullptr);
}

bool cmServerBase::OnSignal(int signum)
{
  (void)signum;
  StartShutDown();
  return true;
}

cmServerBase::cmServerBase(cmConnection* connection)
{
  auto err = uv_loop_init(&Loop);
  (void)err;
  Loop.data = this;
  assert(err == 0);

  AddNewConnection(connection);
}

void cmServerBase::Close()
{
  if (Loop.data) {
    if (ServeThreadRunning) {
      this->ShutdownSignal.send();
      uv_thread_join(&ServeThread);
    }

    uv_loop_close(&Loop);
    Loop.data = nullptr;
  }
}
cmServerBase::~cmServerBase()
{
  Close();
}

void cmServerBase::AddNewConnection(cmConnection* ownedConnection)
{
  {
    std::unique_lock<cm::shared_mutex> lock(ConnectionsMutex);
    Connections.emplace_back(ownedConnection);
  }
  ownedConnection->SetServer(this);
}

uv_loop_t* cmServerBase::GetLoop()
{
  return &Loop;
}

void cmServerBase::OnDisconnect(cmConnection* pConnection)
{
  auto pred = [pConnection](const std::unique_ptr<cmConnection>& m) {
    return m.get() == pConnection;
  };
  {
    std::unique_lock<cm::shared_mutex> lock(ConnectionsMutex);
    Connections.erase(
      std::remove_if(Connections.begin(), Connections.end(), pred),
      Connections.end());
  }

  if (Connections.empty()) {
    this->ShutdownSignal.send();
  }
}
