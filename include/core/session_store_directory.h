#ifndef RTORRENT_CORE_SESSION_STORE_DIRECTORY_H
#define RTORRENT_CORE_SESSION_STORE_DIRECTORY_H

#include "core/session_store.h"

#include "utils/lockfile.h"

namespace core {
class Download;
class SessionStoreDirectory : public SessionStore {
public:
  virtual void        enable(bool lock);
  virtual void        disable();
  virtual void        load_all();
  virtual bool        save(Download* d, int flags);
  virtual field_value retrieve_field(session_key);
  virtual bool        save_field(session_key key, const torrent::Object& obj);
  virtual int         save_resume(DownloadList::const_iterator dstart,
                                  DownloadList::const_iterator dend);

private:
  bool            write_bencode_file(const std::string& filename,
                                     session_value      obj,
                                     uint32_t           skip_mask);
  std::string     create_filename(Download* d);
  std::string     create_filename(session_key key);
  static bool     is_correct_format(const std::string& f);
  utils::Lockfile m_lockfile;
  field_value     load_input_history();
  bool            save_input_history(const torrent::Object& obj);
};
} // namespace core

#endif
