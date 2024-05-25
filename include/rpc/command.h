// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2005-2011, Jari Sundell <jaris@ifi.uio.no>

#ifndef RTORRENT_RPC_COMMAND_H
#define RTORRENT_RPC_COMMAND_H

#include <functional>
#include <tuple>

#include <torrent/data/file_list_iterator.h>
#include <torrent/object.h>

// Move into config.h or something.
namespace core {
class Download;
}

namespace torrent {
class File;
class FileListIterator;
class Peer;
class Tracker;
}

namespace rpc {

template<typename Target>
struct target_wrapper {
  using target_type  = Target;
  using cleaned_type = Target;
};

template<>
struct target_wrapper<void> {
  struct no_type {};

  using target_type  = void;
  using cleaned_type = no_type*;
};

// Since it gets used so many places we might as well put it in the
// rpc namespace.
// typedef std::pair<int, void*> target_type;
using target_type = std::tuple<int, void*, void*>;

class command_base;

using command_base_call_type =
  const torrent::Object (*)(command_base*, target_type, const torrent::Object&);
using base_function =
  std::function<torrent::Object(target_type, const torrent::Object&)>;

template<typename tmpl>
struct command_base_is_valid {};
template<command_base_call_type tmpl_func>
struct command_base_is_type {};

class command_base {
public:
  using value_type  = torrent::Object::value_type;
  using string_type = torrent::Object::string_type;
  using list_type   = torrent::Object::list_type;
  using map_type    = torrent::Object::map_type;
  using key_type    = torrent::Object::key_type;

  using generic_slot = const torrent::Object (*)(command_base*,
                                                 const torrent::Object&);
  using cleaned_slot =
    const torrent::Object (*)(command_base*,
                              target_wrapper<void>::cleaned_type,
                              const torrent::Object&);
  using any_slot           = const torrent::Object (*)(command_base*,
                                             target_type,
                                             const torrent::Object&);
  using download_slot      = const torrent::Object (*)(command_base*,
                                                  core::Download*,
                                                  const torrent::Object&);
  using file_slot          = const torrent::Object (*)(command_base*,
                                              torrent::File*,
                                              const torrent::Object&);
  using file_itr_slot      = const torrent::Object (*)(command_base*,
                                                  torrent::FileListIterator*,
                                                  const torrent::Object&);
  using peer_slot          = const torrent::Object (*)(command_base*,
                                              torrent::Peer*,
                                              const torrent::Object&);
  using tracker_slot       = const torrent::Object (*)(command_base*,
                                                 torrent::Tracker*,
                                                 const torrent::Object&);
  using download_pair_slot = const torrent::Object (*)(command_base*,
                                                       core::Download*,
                                                       core::Download*,
                                                       const torrent::Object&);

  static constexpr int target_generic  = 0;
  static constexpr int target_any      = 1;
  static constexpr int target_download = 2;
  static constexpr int target_peer     = 3;
  static constexpr int target_tracker  = 4;
  static constexpr int target_file     = 5;
  static constexpr int target_file_itr = 6;

  static constexpr int target_download_pair = 7;

  static constexpr unsigned int max_arguments = 10;

  struct stack_type {
    torrent::Object* begin() {
      return reinterpret_cast<torrent::Object*>(buffer);
    }
    torrent::Object* end() {
      return reinterpret_cast<torrent::Object*>(buffer) + max_arguments;
    }

    const torrent::Object* begin() const {
      return reinterpret_cast<const torrent::Object*>(buffer);
    }
    const torrent::Object* end() const {
      return reinterpret_cast<const torrent::Object*>(buffer) + max_arguments;
    }

    torrent::Object& operator[](unsigned int idx) {
      return *(begin() + idx);
    }
    const torrent::Object& operator[](unsigned int idx) const {
      return *(begin() + idx);
    }

    static stack_type* from_data(char* data) {
      return reinterpret_cast<stack_type*>(data);
    }

    char buffer[sizeof(torrent::Object) * max_arguments];
  };

  command_base() {
    new (&_pod<base_function>()) base_function();
  }
  command_base(const command_base& src) {
    new (&_pod<base_function>()) base_function(src._pod<base_function>());
  }

  ~command_base() {
    _pod<base_function>().~base_function();
  }

  static torrent::Object* argument(unsigned int index) {
    return current_stack.begin() + index;
  }
  static torrent::Object& argument_ref(unsigned int index) {
    return *(current_stack.begin() + index);
  }

  static stack_type current_stack;

  static torrent::Object* stack_begin() {
    return current_stack.begin();
  }
  static torrent::Object* stack_end() {
    return current_stack.end();
  }

  static torrent::Object* push_stack(const torrent::Object::list_type& args,
                                     stack_type*                       stack);
  static torrent::Object* push_stack(const torrent::Object* first_arg,
                                     const torrent::Object* last_arg,
                                     stack_type*            stack);
  static void pop_stack(stack_type* stack, torrent::Object* last_stack);

  template<typename T>
  void set_function(T s, int = command_base_is_valid<T>::value) {
    _pod<T>() = s;
  }

  template<command_base_call_type T>
  void set_function_2(
    typename command_base_is_type<T>::type s,
    int =
      command_base_is_valid<typename command_base_is_type<T>::type>::value) {
    _pod<typename command_base_is_type<T>::type>() = s;
  }

  // The std::function object in GCC is castable between types with a
  // pointer to a struct of ctor/dtor/calls for non-POD slots. As such
  // it should be safe to cast between different std::function
  // template types, yet what the C++0x standard will say about this I
  // have no idea atm.
  template<typename tmpl>
  tmpl& _pod() {
    return reinterpret_cast<tmpl&>(t_pod);
  }
  template<typename tmpl>
  const tmpl& _pod() const {
    return reinterpret_cast<const tmpl&>(t_pod);
  }

  template<typename Func, typename T, typename Args>
  static const torrent::Object _call(command_base* cmd,
                                     target_type   target,
                                     Args          args);

  command_base& operator=(const command_base& src) {
    _pod<base_function>() = src._pod<base_function>();
    return *this;
  }

protected:
  // For use by functions that need to use placeholders to arguments
  // within commands. E.d. callable command strings where one of the
  // arguments within the command needs to be supplied by the caller.
  union {
    char t_pod[sizeof(base_function)];
  };
};

template<typename T1 = void, typename T2 = void>
struct target_type_id {
  // Nothing here, so we cause an error.
};

template<typename T>
inline bool
is_target_compatible(const target_type& target) {
  return std::get<0>(target) == target_type_id<T>::value;
}

// Splitting pairs into separate targets.
inline bool
is_target_pair(const target_type& target) {
  return std::get<0>(target) >= command_base::target_download_pair;
}

template<typename T>
inline T
get_target_cast(target_type target, int = target_type_id<T>::value) {
  return (T)std::get<1>(target);
}

inline target_type
get_target_left(const target_type& target) {
  return { std::get<0>(target) - 5, std::get<1>(target), nullptr };
}
inline target_type
get_target_right(const target_type& target) {
  return { std::get<0>(target) - 5, std::get<2>(target), nullptr };
}

}

#include "command_impl.h"

namespace rpc {

template<typename Func, typename T, typename Args>
inline const torrent::Object
command_base::_call(command_base* cmd, target_type target, Args args) {
  return static_cast<command_base*>(cmd)->_pod<Func>()(
    get_target_cast<T>(target), args);
}

#define COMMAND_BASE_TEMPLATE_TYPE(func_type, func_parm)                       \
  template<typename T, int proper = target_type_id<T>::proper_type>            \
  struct func_type {                                                           \
    typedef std::function<func_parm> type;                                     \
  };                                                                           \
                                                                               \
  template<>                                                                   \
  struct command_base_is_valid<func_type<target_type>::type> {                 \
    static constexpr int value = 1;                                            \
  };                                                                           \
  template<>                                                                   \
  struct command_base_is_valid<func_type<core::Download*>::type> {             \
    static constexpr int value = 1;                                            \
  };                                                                           \
  template<>                                                                   \
  struct command_base_is_valid<func_type<torrent::Peer*>::type> {              \
    static constexpr int value = 1;                                            \
  };                                                                           \
  template<>                                                                   \
  struct command_base_is_valid<func_type<torrent::Tracker*>::type> {           \
    static constexpr int value = 1;                                            \
  };                                                                           \
  template<>                                                                   \
  struct command_base_is_valid<func_type<torrent::File*>::type> {              \
    static constexpr int value = 1;                                            \
  };                                                                           \
  template<>                                                                   \
  struct command_base_is_valid<func_type<torrent::FileListIterator*>::type> {  \
    static constexpr int value = 1;                                            \
  };

//  template <typename Q> struct command_base_is_valid<typename
//  func_type<Q>::type > { static constexpr int value = 1; };

COMMAND_BASE_TEMPLATE_TYPE(command_function,
                           torrent::Object(T, const torrent::Object&))
COMMAND_BASE_TEMPLATE_TYPE(command_value_function,
                           torrent::Object(T,
                                           const torrent::Object::value_type&))
COMMAND_BASE_TEMPLATE_TYPE(command_string_function,
                           torrent::Object(T, const std::string&))
COMMAND_BASE_TEMPLATE_TYPE(command_list_function,
                           torrent::Object(T,
                                           const torrent::Object::list_type&))

#define COMMAND_BASE_TEMPLATE_CALL(func_name, func_type)                       \
  template<typename T>                                                         \
  const torrent::Object func_name(command_base*          rawCommand,           \
                                  target_type            target,               \
                                  const torrent::Object& args);                \
                                                                               \
  template<>                                                                   \
  struct command_base_is_type<func_name<target_type>> {                        \
    static constexpr int                 value = 1;                            \
    typedef func_type<target_type>::type type;                                 \
  };                                                                           \
  template<>                                                                   \
  struct command_base_is_type<func_name<core::Download*>> {                    \
    static constexpr int                     value = 1;                        \
    typedef func_type<core::Download*>::type type;                             \
  };                                                                           \
  template<>                                                                   \
  struct command_base_is_type<func_name<torrent::Peer*>> {                     \
    static constexpr int                    value = 1;                         \
    typedef func_type<torrent::Peer*>::type type;                              \
  };                                                                           \
  template<>                                                                   \
  struct command_base_is_type<func_name<torrent::Tracker*>> {                  \
    static constexpr int                       value = 1;                      \
    typedef func_type<torrent::Tracker*>::type type;                           \
  };                                                                           \
  template<>                                                                   \
  struct command_base_is_type<func_name<torrent::File*>> {                     \
    static constexpr int                    value = 1;                         \
    typedef func_type<torrent::File*>::type type;                              \
  };                                                                           \
  template<>                                                                   \
  struct command_base_is_type<func_name<torrent::FileListIterator*>> {         \
    static constexpr int                                value = 1;             \
    typedef func_type<torrent::FileListIterator*>::type type;                  \
  };

COMMAND_BASE_TEMPLATE_CALL(command_base_call, command_function)
COMMAND_BASE_TEMPLATE_CALL(command_base_call_value, command_value_function)
COMMAND_BASE_TEMPLATE_CALL(command_base_call_value_kb, command_value_function)
COMMAND_BASE_TEMPLATE_CALL(command_base_call_string, command_string_function)
COMMAND_BASE_TEMPLATE_CALL(command_base_call_list, command_list_function)

}

#endif
