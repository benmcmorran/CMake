/* Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
   file Copyright.txt or https://cmake.org/licensing for details.  */
#pragma once

#include "cmConfigure.h" // IWYU pragma: keep

#include <string>

#include "cmConnection.h"
#include "cmUVHandlePtr.h"

class cmServerBase;

/***
 * Generic connection over std io interfaces -- tty
 */
class cmStdIoConnection : public cmEventBasedConnection
{
public:
  cmStdIoConnection(cmConnectionBufferStrategy* bufferStrategy);

  void SetServer(cmServerBase* s) override;

  bool OnConnectionShuttingDown() override;

  bool OnServeStart(std::string* pString) override;

private:
  cm::uv_stream_ptr SetupStream(int file_id);
  cm::uv_stream_ptr ReadStream;
};