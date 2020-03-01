/* Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
   file Copyright.txt or https://cmake.org/licensing for details.  */
#include "cmConfigure.h"

#include "cmServerConnection.h"

#include "cm_uv.h"

#include "cmServer.h"
#include "cmServerDictionary.h"

#ifdef _WIN32
#  include "io.h"
#else
#  include <unistd.h>
#endif
#include <cassert>
#include <utility>

cmServerPipeConnection::cmServerPipeConnection(const std::string& name)
  : cmPipeConnection(name, new cmServerBufferStrategy)
{
}

cmServerStdIoConnection::cmServerStdIoConnection()
  : cmStdIoConnection(new cmServerBufferStrategy)
{
}

cmConnectionBufferStrategy::~cmConnectionBufferStrategy() = default;

void cmConnectionBufferStrategy::clear()
{
}

std::string cmServerBufferStrategy::BufferOutMessage(
  const std::string& rawBuffer) const
{
  return std::string("\n") + kSTART_MAGIC + std::string("\n") + rawBuffer +
    kEND_MAGIC + std::string("\n");
}

std::string cmServerBufferStrategy::BufferMessage(std::string& RawReadBuffer)
{
  for (;;) {
    auto needle = RawReadBuffer.find('\n');

    if (needle == std::string::npos) {
      return "";
    }
    std::string line = RawReadBuffer.substr(0, needle);
    const auto ls = line.size();
    if (ls > 1 && line.at(ls - 1) == '\r') {
      line.erase(ls - 1, 1);
    }
    RawReadBuffer.erase(RawReadBuffer.begin(),
                        RawReadBuffer.begin() + static_cast<long>(needle) + 1);
    if (line == kSTART_MAGIC) {
      RequestBuffer.clear();
      continue;
    }
    if (line == kEND_MAGIC) {
      std::string rtn;
      rtn.swap(this->RequestBuffer);
      return rtn;
    }

    this->RequestBuffer += line;
    this->RequestBuffer += "\n";
  }
}
