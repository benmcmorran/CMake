/* Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
   file Copyright.txt or https://cmake.org/licensing for details.  */
#pragma once

#include "cmConfigure.h" // IWYU pragma: keep

#include <string>

#include "cmConnection.h"
#include "cmPipeConnection.h"
#include "cmStdIoConnection.h"

/***
 * This connection buffer strategy accepts messages in the form of
 * [== "CMake Server" ==[
{
  ... some JSON message ...
}
]== "CMake Server" ==]
 * and only passes on the core json; it discards the envelope.
 */
class cmServerBufferStrategy : public cmConnectionBufferStrategy
{
public:
  std::string BufferMessage(std::string& rawBuffer) override;
  std::string BufferOutMessage(const std::string& rawBuffer) const override;

private:
  std::string RequestBuffer;
};

/***
 * These specific connections use the cmake server
 * buffering strategy.
 */
class cmServerStdIoConnection : public cmStdIoConnection
{
public:
  cmServerStdIoConnection();
};

class cmServerPipeConnection : public cmPipeConnection
{
public:
  cmServerPipeConnection(const std::string& name);
};
