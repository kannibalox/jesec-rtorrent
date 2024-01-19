#ifndef RTORRENT_LUA_H
#define RTORRENT_LUA_H

#include <memory>
#include <torrent/object.h>

#include "lua.h"
#include <cstdint>
#include <lua.hpp>

namespace rpc {

class LuaEngine {
public:
  static const int flag_string           = 0x1;
  static const int flag_autocall_upvalue = 0x2;
  LuaEngine();
  ~LuaEngine();
  torrent::Object execute_file(const torrent::Object& rawArgs);
  lua_State*      state() {
    return m_LuaState;
  }

private:
  lua_State* m_LuaState;
};
torrent::Object
execute_lua(LuaEngine* engine, const torrent::Object& rawArgs, int flags);
int
rtorrent_call(lua_State* L);
void
init_rtorrent_module(lua_State* L);

} // namespace rpc

#endif
