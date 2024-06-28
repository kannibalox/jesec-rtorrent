#ifndef RTORRENT_CORE_SESSION_STORE_H
#define RTORRENT_CORE_SESSION_STORE_H

#include <functional>
#include <string>

#include "download_list.h"
#include <torrent/torrent.h>

namespace core {
// The SessionStore operates against std::string keys and retrieves torrent
// objects from the backend. This base class also doubles as the default session
// store, which can never be enabled and ignores all requests to save/store
// data.
class SessionStore {
public:
  static const int flag_skip_static = 0x1;

  SessionStore() {
    m_isEnabled = false;
  }
  using session_key   = const std::string&;
  using session_value = const torrent::Object&;
  using field_value   = torrent::Object;

  struct SessionData {
    SessionData(session_value m, session_value r, session_value l)
      : main(m)
      , rtorrent(r)
      , libtorrent_resume(l) {}
    session_value main;
    session_value rtorrent;
    session_value libtorrent_resume;
  };

  using slot_load_cb = std::function<void(SessionData)>;

  bool is_enabled() {
    return m_isEnabled;
  };

  virtual void enable(bool lock);
  virtual void disable();
  // Options
  void               set_location(const std::string& uri);
  const std::string& location() const {
    return m_uri;
  };
  void               set_lock_location(const std::string& lock_location);
  const std::string& lock_location() const {
    return m_lockLocation;
  };
  void set_load_callback(slot_load_cb cb) {
    m_slot_load = cb;
  }
  bool save_full(Download* d) {
    return save(d, 0);
  }
  bool save_resume(Download* d) {
    return save(d, flag_skip_static);
  }
  void save_download_data(Download* d);
  virtual bool save(Download* d, int flags);
  virtual int  save_resume(DownloadList::const_iterator dstart,
                           DownloadList::const_iterator dend);
  // Set/get operations for downloads
  virtual void remove(session_key);
  virtual void remove(Download* d);
  virtual void load_all();
  // Allow saving arbitrary fields in a separate place
  virtual bool        save_field(session_key key, const torrent::Object& obj);
  virtual field_value retrieve_field(session_key key);
  virtual void        remove_field(session_key key);

protected:
  bool         m_isEnabled;
  slot_load_cb m_slot_load;
  std::string  m_uri;
  std::string  m_lockLocation;
};
SessionStore*
create_session_store(const std::string& uri);
} // namespace core

#endif
