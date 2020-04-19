/* Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
   file Copyright.txt or https://cmake.org/licensing for details.  */
#ifndef cmListFileCache_h
#define cmListFileCache_h

#include "cmConfigure.h" // IWYU pragma: keep

#include <cstddef>
#include <functional>
#include <iosfwd>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <cm/optional>

#include "cmStateSnapshot.h"

#include "duktape.h"

/** \class cmListFileCache
 * \brief A class to cache list file contents.
 *
 * cmListFileCache is a class used to cache the contents of parsed
 * cmake list files.
 */

class cmMessenger;
class cmExecutionStatus;

struct cmCommandContext
{
  struct cmCommandName
  {
    std::string Lower;
    std::string Original;
    cmCommandName() = default;
    cmCommandName(std::string const& name) { *this = name; }
    cmCommandName& operator=(std::string const& name);
  } Name;
  long Line = 0;
  cmCommandContext() = default;
  cmCommandContext(const char* name, int line)
    : Name(name)
    , Line(line)
  {
  }
};

struct cmListFileArgument
{
  enum Delimiter
  {
    Unquoted,
    Quoted,
    Bracket
  };
  cmListFileArgument() = default;
  cmListFileArgument(std::string v, Delimiter d, long line)
    : Value(std::move(v))
    , Delim(d)
    , Line(line)
  {
  }
  bool operator==(const cmListFileArgument& r) const
  {
    return (this->Value == r.Value) && (this->Delim == r.Delim);
  }
  bool operator!=(const cmListFileArgument& r) const { return !(*this == r); }
  std::string Value;
  Delimiter Delim = Unquoted;
  long Line = 0;
};

class cmListFileContext
{
public:
  std::string Name;
  std::string FilePath;
  long Line = 0;

  static cmListFileContext FromCommandContext(cmCommandContext const& lfcc,
                                              std::string const& fileName)
  {
    cmListFileContext lfc;
    lfc.FilePath = fileName;
    lfc.Line = lfcc.Line;
    lfc.Name = lfcc.Name.Original;
    return lfc;
  }
};

std::ostream& operator<<(std::ostream&, cmListFileContext const&);
bool operator<(const cmListFileContext& lhs, const cmListFileContext& rhs);
bool operator==(cmListFileContext const& lhs, cmListFileContext const& rhs);
bool operator!=(cmListFileContext const& lhs, cmListFileContext const& rhs);

struct cmListFileFunction : public cmCommandContext
{
  std::vector<cmListFileArgument> Arguments;
};

// Represent a backtrace (call stack).  Provide value semantics
// but use efficient reference-counting underneath to avoid copies.
class cmListFileBacktrace
{
public:
  // Default-constructed backtrace may not be used until after
  // set via assignment from a backtrace constructed with a
  // valid snapshot.
  cmListFileBacktrace() = default;

  // Construct an empty backtrace whose bottom sits in the directory
  // indicated by the given valid snapshot.
  cmListFileBacktrace(cmStateSnapshot const& snapshot);

  cmStateSnapshot GetBottom() const;

  // Get a backtrace with the given file scope added to the top.
  // May not be called until after construction with a valid snapshot.
  cmListFileBacktrace Push(std::string const& file) const;

  // Get a backtrace with the given call context added to the top.
  // May not be called until after construction with a valid snapshot.
  cmListFileBacktrace Push(cmListFileContext const& lfc) const;

  // Get a backtrace with the top level removed.
  // May not be called until after a matching Push.
  cmListFileBacktrace Pop() const;

  // Get the context at the top of the backtrace.
  // This may be called only if Empty() would return false.
  cmListFileContext const& Top() const;

  // Print the top of the backtrace.
  void PrintTitle(std::ostream& out) const;

  // Print the call stack below the top of the backtrace.
  void PrintCallStack(std::ostream& out) const;

  // Get the number of 'frames' in this backtrace
  size_t Depth() const;

  // Return true if this backtrace is empty.
  bool Empty() const;

private:
  struct Entry;
  std::shared_ptr<Entry const> TopEntry;
  cmListFileBacktrace(std::shared_ptr<Entry const> parent,
                      cmListFileContext const& lfc);
  cmListFileBacktrace(std::shared_ptr<Entry const> top);
};

// Wrap type T as a value with a backtrace.  For purposes of
// ordering and equality comparison, only the original value is
// used.  The backtrace is considered incidental.
template <typename T>
class BT
{
public:
  BT(T v = T(), cmListFileBacktrace bt = cmListFileBacktrace())
    : Value(std::move(v))
    , Backtrace(std::move(bt))
  {
  }
  T Value;
  cmListFileBacktrace Backtrace;
  friend bool operator==(BT<T> const& l, BT<T> const& r)
  {
    return l.Value == r.Value;
  }
  friend bool operator<(BT<T> const& l, BT<T> const& r)
  {
    return l.Value < r.Value;
  }
  friend bool operator==(BT<T> const& l, T const& r) { return l.Value == r; }
  friend bool operator==(T const& l, BT<T> const& r) { return l == r.Value; }
};

std::ostream& operator<<(std::ostream& os, BT<std::string> const& s);

std::vector<BT<std::string>> ExpandListWithBacktrace(
  std::string const& list,
  cmListFileBacktrace const& bt = cmListFileBacktrace());

struct cmListFileBase
{
  using CommandExecutor =
    std::function<cmExecutionStatus(const cmListFileFunction&)>;
  using VariableResolver = std::function<const char*(const std::string&)>;

  cmListFileBase() = default;
  virtual ~cmListFileBase() = default;
  cmListFileBase(const cmListFileBase&) = delete;
  cmListFileBase& operator=(const cmListFileBase&) = delete;
  cmListFileBase(cmListFileBase&&) = delete;
  cmListFileBase& operator=(cmListFileBase&&) = delete;

  virtual void Execute(const CommandExecutor& commandExecutor,
                       const VariableResolver& variableResolver,
                       cmMessenger* messenger) const = 0;
};

struct cmListFile : public cmListFileBase
{
  cmListFile() = default;

  void Execute(const CommandExecutor& commandExecutor,
               const VariableResolver& variableResolver,
               cmMessenger* messenger) const override;

  bool ParseFile(const char* path, cmMessenger* messenger,
                 cmListFileBacktrace const& lfbt);

  bool ParseString(const char* str, const char* virtual_filename,
                   cmMessenger* messenger, cmListFileBacktrace const& lfbt);

  std::vector<cmListFileFunction> Functions;
};

class cmJavaScriptFile : public cmListFileBase
{
public:
  cmJavaScriptFile(const char* str, const char* virtual_filename,
                   cmMessenger* messenger, cmListFileBacktrace const& lfbt);
  void Execute(const CommandExecutor& commandExecutor,
               const VariableResolver& variableResolver,
               cmMessenger* messenger) const override;

private:
  std::unique_ptr<duk_context, decltype(&duk_destroy_heap)> Context;
};

namespace cmListFileFactory {
cm::optional<std::unique_ptr<cmListFileBase>> ParseFile(
  const char* path, cmMessenger* messenger, cmListFileBacktrace const& lfbt);

cm::optional<std::unique_ptr<cmListFileBase>> ParseString(
  const char* str, const char* virtual_filename, cmMessenger* messenger,
  cmListFileBacktrace const& lfbt);
}

#endif
