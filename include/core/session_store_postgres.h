#ifndef RTORRENT_CORE_SESSION_STORE_POSTGRES_H
#define RTORRENT_CORE_SESSION_STORE_POSTGRES_H

#include <pqxx/pqxx>

#include "core/session_store.h"

namespace core {
class Download;
class SessionStorePostgres : public SessionStore {
public:
  virtual void        enable(bool lock);
  virtual void        disable();
  virtual void        load_all();
  virtual bool        save(Download* d, int flags);
  virtual field_value retrieve_field(session_key);
  virtual bool        save_field(session_key key, const torrent::Object& obj);
  //    virtual int save_resume(DownloadList::const_iterator dstart,
  //    DownloadList::const_iterator dend);
private:
  std::unique_ptr<pqxx::connection> m_connection;
};
} // namespace core

#endif
