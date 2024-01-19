#include <fstream>
#include <sys/stat.h>
#include <unistd.h>

#include "rpc/lua.h"
#include <cstdint>
#include <lua.hpp>

#include <torrent/object.h>
#include <torrent/utils/error_number.h>
#include <torrent/utils/path.h>

#include "rpc/command.h"
#include "rpc/command_map.h"
#include "rpc/parse_commands.h"
#include "rpc/rpc_xml.h"

namespace rpc {

const int LuaEngine::flag_string;
const int LuaEngine::flag_autocall_upvalue;

LuaEngine::LuaEngine() {
  m_LuaState = luaL_newstate();
  luaL_openlibs(m_LuaState);
  init_rtorrent_module(m_LuaState);
}

LuaEngine::~LuaEngine() {
  lua_close(m_LuaState);
}

void
object_to_lua(lua_State* L, torrent::Object const& object) {
  // Converts an object to a single Lua stack object
  switch (object.type()) {
    case torrent::Object::TYPE_VALUE:
      lua_pushnumber(L, object.as_value());
      break;
    case torrent::Object::TYPE_NONE:
      lua_pushnil(L);
      break;
    case torrent::Object::TYPE_STRING:
      lua_pushstring(L, object.as_string().c_str());
      break;
    case torrent::Object::TYPE_LIST: {
      lua_createtable(L, object.as_list().size(), 0);
      int index      = 1;
      int tableIndex = lua_gettop(L);
      for (auto itr = object.as_list().begin(), last = object.as_list().end();
           itr != last;
           itr++) {
        object_to_lua(L, *itr);
        lua_rawseti(L, tableIndex, lua_Integer(index++));
      }
      break;
    }
    case torrent::Object::TYPE_MAP: {
      lua_createtable(L, 0, object.as_map().size());
      int tableIndex = lua_gettop(L);
      for (auto itr = object.as_map().begin(), last = object.as_map().end();
           itr != last;
           itr++) {
        object_to_lua(L, itr->second);
        lua_pushlstring(L, itr->first.c_str(), itr->first.size());
        lua_settable(L, tableIndex);
      }
      break;
    }
    default:
      lua_pushnumber(L, lua_Number(object.type()));
      break;
  }
}

torrent::Object
lua_to_object(lua_State* L, int index) {
  torrent::Object object;
  switch (lua_type(L, index)) {
    case LUA_TNUMBER:
      return torrent::Object(lua_tonumber(L, index));
    case LUA_TSTRING:
      return torrent::Object(lua_tostring(L, index));
    case LUA_TBOOLEAN:
      return torrent::Object(lua_toboolean(L, index));
    case LUA_TTABLE: {
      lua_pushnil(L);
      int status = lua_next(L, -2);
      if (status == 0)
        return torrent::Object::create_map();
      // If the table starts at 1, assume it's an array
      if (lua_isnumber(L, -2) && (lua_tonumber(L, -2) == 1)) {
        torrent::Object list = torrent::Object::create_list();
        list.insert_back(lua_to_object(L, -1));
        lua_pop(L, 1);
        while (lua_next(L, -2) != 0) {
          list.insert_back(lua_to_object(L, -1));
          lua_pop(L, 1);
        }
        return list;
      } else {
        torrent::Object map = torrent::Object::create_map();
        // Reset the stack and start from the first element
        lua_pop(L, 2);
        lua_pushnil(L);
        while (lua_next(L, -2) != 0) {
          std::string key;
          // Bencode dictionaries require string keys, but naively calling
          // lua_tostring would auto-convert any numbers to strings,
          // which would throw off lua_next
          switch (lua_type(L, -2)) {
            case LUA_TNUMBER:
              key = std::to_string(lua_tonumber(L, -2));
            case LUA_TSTRING:
              key = lua_tostring(L, -2);
          }
          map.insert_key(key, lua_to_object(L, -1));
          lua_pop(L, 1);
        }
        return map;
      }
      return torrent::Object(lua_tostring(L, index));
    }
    default:
      std::string result = luaL_tolstring(L, index, NULL);
      lua_pop(L, index + 1);
      return torrent::Object(result);
  }
  return object;
}

void
string_to_target(std::string       targetString,
                 bool              requireIndex,
                 rpc::target_type* target) {
  // target_any: ''
  // target_download: <hash>
  // target_file: <hash>:f<index>
  // target_peer: <hash>:p<index>
  // target_tracker: <hash>:t<index>

  if (targetString.size() == 0 && !requireIndex) {
    return;
  }

  // Length of SHA1 hash is 40
  if (targetString.size() < 40) {
    throw torrent::input_error("invalid parameters: invalid target");
  }

  std::string hash;
  char        type = 'd';
  std::string index;

  const auto& delimPos = targetString.find_first_of(':', 40);
  if (delimPos == std::string::npos || delimPos + 2 >= targetString.size()) {
    if (requireIndex) {
      throw torrent::input_error("invalid parameters: no index");
    }
    hash = targetString;
  } else {
    hash  = targetString.substr(0, delimPos);
    type  = targetString[delimPos + 1];
    index = targetString.substr(delimPos + 2, std::string::npos);
  }

  core::Download* download =
    rpc.slot_find_download()(std::string(hash).c_str());

  if (download == nullptr) {
    throw torrent::input_error("invalid parameters: info-hash not found");
  }

  try {
    switch (type) {
      case 'd':
        *target = rpc::make_target(download);
        break;
      case 'f':
        *target = rpc::make_target(
          command_base::target_file,
          rpc.slot_find_file()(download, std::stoi(std::string(index))));
        break;
      case 't':
        *target = rpc::make_target(
          command_base::target_tracker,
          rpc.slot_find_tracker()(download, std::stoi(std::string(index))));
        break;
      case 'p':
        torrent::HashString peerHash;
        torrent::hash_string_from_hex_c_str(index.c_str(), peerHash);
        *target = rpc::make_target(command_base::target_peer,
                                   rpc.slot_find_peer()(download, peerHash));
        break;
      default:
        throw torrent::input_error(
          "invalid parameters: unexpected target type");
    }
  } catch (const std::logic_error&) {
    throw torrent::input_error("invalid parameters: invalid index");
  }

  if (target == nullptr || (std::get<1>(*target)) == nullptr) {
    throw torrent::input_error(
      "invalid parameters: unable to find requested target");
  }
}

torrent::Object
lua_callstack_to_object(lua_State*        L,
                        int               commandFlags,
                        rpc::target_type* target) {
  torrent::Object object;
  if (lua_gettop(L) == 0) {
    return torrent::Object();
  }

  if (!lua_isstring(L, 1)) {
    throw torrent::input_error("invalid parameters: target must be a string");
  }
  string_to_target(std::string(lua_tostring(L, 1)),
                   (commandFlags & (CommandMap::flag_tracker_target &
                                    CommandMap::flag_file_target)),
                   target);

  // start from the second argument since the first is the target
  lua_remove(L, 1);

  if (lua_gettop(L) == 0) {
    return torrent::Object();
  } else {
    torrent::Object             result  = torrent::Object::create_list();
    torrent::Object::list_type& listRef = result.as_list();
    while (lua_gettop(L) != 0) {
      listRef.insert(listRef.begin(), lua_to_object(L, -1));
      lua_remove(L, -1);
    }
    return result;
  }

  return object;
}

int
rtorrent_call(lua_State* L) {
  auto method = lua_tostring(L, 1);
  lua_remove(L, 1);
  torrent::Object  object;
  rpc::target_type target = rpc::make_target();
  ;
  rpc::CommandMap::iterator itr =
    rpc::commands.find(std::string(method).c_str());
  if (itr == rpc::commands.end()) {
    throw torrent::input_error("method not found: " + std::string(method));
  }
  object = lua_callstack_to_object(L, itr->second.m_flags, &target);
  try {
    const auto& result = rpc::commands.call_command(itr, object, target);
    object_to_lua(L, result);
    return 1;
  } catch (torrent::base_error& e) {
    throw luaL_error(L, e.what());
  }
}

void
init_rtorrent_module(lua_State* L) {
  lua_createtable(L, 0, 1);
  int tableIndex = lua_gettop(L);
  lua_pushliteral(L, "call");
  lua_pushcfunction(L, rtorrent_call);
  lua_settable(L, tableIndex);
  // Allows use of the dot syntax for calling RPC, through metatables
  lua_pushliteral(L, "autocall");
  luaL_dostring(L, "\
     local mt = {} \n\
     function mt.__call (t, ...) \n\
       return rtorrent.call(table.concat(rawget(t, \"__namestack\"),\".\"), ...) \n\
     end \n\
     function mt.__index (t, key) \n\
       -- Create a new sub-table, preserving the name of the key in a stack \n\
       ns = rawget(t, \"__namestack\") \n\
       if ns == nil then \n\
         ns = {} \n\
       end \n\
       table.insert(ns, key) \n\
       return setmetatable({__namestack=ns}, mt) \n\
     end \n\
     return setmetatable({}, mt) \n\
  ");
  lua_settable(L, tableIndex);
  // Variant on the above that auto-provides an empty target
  lua_pushliteral(L, "autocall_config");
  luaL_dostring(L, "\
     local mt = {} \n\
     function mt.__call (t, ...) \n\
       return rtorrent.call(table.concat(rawget(t, \"__namestack\"), \".\"), \"\", ...) \n\
     end \n\
     function mt.__index (t, key) \n\
       -- Create a new sub-table, preserving the name of the key in a stack \n\
       ns = rawget(t, \"__namestack\") \n\
       if ns == nil then \n\
         -- Allow loading top-level global names \n\
         if _G[key] ~= nil then \n\
           return _G[key] \n\
         end \n\
         ns = {} \n\
       end \n\
       table.insert(ns, key) \n\
       return setmetatable({__namestack=ns}, mt) \n\
     end \n\
     return setmetatable({}, mt) \n\
  ");
  lua_settable(L, tableIndex);
  lua_setglobal(L, "rtorrent");
}

void
check_lua_status(lua_State* L, int status) {
  if (status != LUA_OK) {
    std::string str = lua_tostring(L, -1);
    throw torrent::input_error(str);
  }
}

torrent::Object
execute_lua(LuaEngine* engine, torrent::Object const& rawArgs, int flags) {
  int        lua_argc = 0;
  lua_State* L        = engine->state();
  if (rawArgs.is_list()) {
    const torrent::Object::list_type& args = rawArgs.as_list();
    if (flags & LuaEngine::flag_string) {
      check_lua_status(L,
                       luaL_loadstring(L, args.begin()->as_string().c_str()));
    } else {
      check_lua_status(L, luaL_loadfile(L, args.begin()->as_string().c_str()));
    }
    for (auto itr = std::next(args.begin()), last = args.end(); itr != last;
         itr++) {
      object_to_lua(L, *itr);
    }
    lua_argc = args.size() - 1;
  } else {
    const torrent::Object::string_type& target = rawArgs.as_string();
    if (flags & LuaEngine::flag_string) {
      check_lua_status(L, luaL_loadstring(L, target.c_str()));
    } else {
      check_lua_status(L, luaL_loadfile(L, target.c_str()));
    }
  }
  if (flags & LuaEngine::flag_autocall_upvalue) {
    lua_getglobal(L, "rtorrent");
    lua_getfield(L, -1, "autocall_config");
    lua_remove(L, -2);
    lua_setupvalue(L, -2, 1);
  }
  check_lua_status(L, lua_pcall(L, lua_argc, LUA_MULTRET, 0));
  return lua_to_object(L, -1);
}
}
