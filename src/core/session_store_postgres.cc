#include <string>
#include <sstream>
#include <unistd.h>

#include "core/session_store_postgres.h"
#include "core/session_store.h"
#include <torrent/object.h>
#include <torrent/object_stream.h>
#include <torrent/utils/resume.h>
#include <torrent/utils/string_manip.h>
#include "torrent/exceptions.h"
#include "core/download.h"

namespace core {
  void SessionStorePostgres::enable(bool lock) {
    if (is_enabled())
      throw torrent::input_error("Session database already enabled.");

    if (m_uri.empty())
      return;

    m_connection = std::make_unique<pqxx::connection>(m_uri);

    pqxx::work w(*m_connection);

    w.exec0("CREATE TABLE IF NOT EXISTS session (hash CHAR(40) UNIQUE, torrent BYTEA, rtorrent BYTEA, resume BYTEA);");
    w.exec0("CREATE TABLE IF NOT EXISTS field (key VARCHAR UNIQUE, value BYTEA);");

    w.commit();

    (*m_connection).prepare("insert_session_all", "INSERT INTO session (hash, torrent, rtorrent, resume) VALUES ($1, $2, $3, $4) ON CONFLICT (hash) DO UPDATE SET torrent = excluded.torrent, rtorrent = excluded.rtorrent, resume=excluded.resume;");
    (*m_connection).prepare("insert_session_resume", "INSERT INTO session (hash, rtorrent, resume) VALUES ($1, $2, $3) ON CONFLICT (hash) DO UPDATE SET rtorrent = excluded.rtorrent, resume=excluded.resume;");
    (*m_connection).prepare("insert_field", "INSERT INTO field (key, value) VALUES ($1, $2) ON CONFLICT (key) DO UPDATE SET value =  excluded.value;");

    m_isEnabled = true;

    if (lock) {
      torrent::Object rawLock = retrieve_field("rtorrent.lock");
      std::string lock;
      if (rawLock.is_empty()) {
        lock = "";
      } else {
        lock = rawLock.as_string();
      }
      if (lock == "") {

        char buf[256];
        int  pos = ::gethostname(buf, 255);

        if (pos == 0) {
          ::snprintf(buf + std::strlen(buf), 255, ":+%i", ::getpid());
        }

        save_field("rtorrent.lock", torrent::Object(buf));
        m_isLocked = true;
      } else {
        throw torrent::input_error("Could not lock session field, held by \"" + lock + "\"");
      }
    }
  }

  void SessionStorePostgres::disable() {
    if (m_isLocked) {
      remove_field("rtorrent.lock");
    }
    m_isEnabled = false;
  }

  void read_bytea_to_obj(const pqxx::bytes& bytea, torrent::Object* obj) {
    auto first = reinterpret_cast<const char*>(bytea.c_str());
    auto last = reinterpret_cast<const char*>(bytea.c_str()) + bytea.size();
    torrent::object_read_bencode_c(first, last, obj);
  }

  void SessionStorePostgres::load_all() {
    pqxx::read_transaction tx(*m_connection);
    std::string q = "SELECT * FROM session";
    for (auto [hash, torrent, rtorrent, resume] : tx.stream<std::string_view, pqxx::bytes, pqxx::bytes, pqxx::bytes>(q)) {
      torrent::Object main_data = torrent::Object::create_map();
      torrent::Object rtorrent_data          = torrent::Object::create_map();
      torrent::Object libtorrent_resume_data = torrent::Object::create_map();
      read_bytea_to_obj(torrent, &main_data);
      read_bytea_to_obj(rtorrent, &rtorrent_data);
      read_bytea_to_obj(resume, &libtorrent_resume_data);
      m_slot_load(SessionData(main_data, rtorrent_data, libtorrent_resume_data));
    }
    tx.commit();
    return;
  }

  void SessionStorePostgres::remove(Download* d) {
    pqxx::work tx(*m_connection);
    std::string hash = torrent::utils::transform_hex(d->info()->hash().begin(), d->info()->hash().end());
    tx.exec_params0("DELETE FROM session WHERE (hash = $1);", hash);
    tx.commit();
  }

  bool SessionStorePostgres::save_in_transaction(Download* d, pqxx::work& tx, int flags) {
    torrent::Object* resume_base =
      &d->download()->bencode()->get_key("libtorrent_resume");
    torrent::Object* rtorrent_base =
      &d->download()->bencode()->get_key("rtorrent");

    std::string hash = torrent::utils::transform_hex(d->info()->hash().begin(), d->info()->hash().end());

    std::ostringstream rtorrent_buffer("");
    torrent::object_write_bencode(&rtorrent_buffer, rtorrent_base, 0);
    std::string rtorrent_string = rtorrent_buffer.str();
    auto rtorrent_bin = pqxx::binary_cast(rtorrent_string);

    std::ostringstream resume_buffer("");
    torrent::object_write_bencode(&resume_buffer, resume_base, 0);
    std::string resume_string = resume_buffer.str();
    auto resume_bin = pqxx::binary_cast(resume_string);

    if (!(flags & flag_skip_static)) {
      std::ostringstream torrent_buffer("");
      torrent::object_write_bencode(&torrent_buffer, d->bencode(), torrent::Object::flag_session_data);
      std::string torrent_string = torrent_buffer.str();
      auto torrent_bin = pqxx::binary_cast(torrent_string);
      tx.exec_prepared0("insert_session_all", hash, torrent_bin, rtorrent_bin, resume_bin);
    } else {
      tx.exec_prepared0("insert_session_resume", hash, rtorrent_bin, resume_bin);
    }
    return true;
  }

  bool SessionStorePostgres::save(Download* d, int flags) {
    if (!is_enabled())
      return true;

    pqxx::work tx(*m_connection);
    save_download_data(d);
    bool result = save_in_transaction(d, tx, flags);
    tx.commit();
    return result;
  }

  SessionStore::field_value SessionStorePostgres::retrieve_field(session_key key) {
    pqxx::read_transaction tx(*m_connection);
    std::string q = "SELECT value FROM field WHERE key = $1;";
    for (auto [value] : tx.query<pqxx::bytes>(q, key)) {
      torrent::Object value_data = torrent::Object();
      read_bytea_to_obj(value, &value_data);
      tx.commit();
      return value_data;
    }
    tx.commit();
    return field_value();
  }

  bool SessionStorePostgres::save_field(session_key key, const torrent::Object& obj) {
    std::ostringstream value_buffer("");
    torrent::object_write_bencode(&value_buffer, &obj, 0);
    std::string value_string = value_buffer.str();
    auto value_bin = pqxx::binary_cast(value_string);
    pqxx::work tx(*m_connection);
    tx.exec_prepared0("insert_field", key, value_bin);
    tx.commit();
    return true;
  }

  int SessionStorePostgres::save_resume(DownloadList::const_iterator dstart, DownloadList::const_iterator dend) {
    int count = 0;
    pqxx::work tx(*m_connection);
    for (auto itr = dstart; dstart != dend; itr++) {
      if (save_in_transaction(*itr, tx, 0))
        count++;
    }
    tx.commit();
    return count;
  }

  void SessionStorePostgres::remove_field(session_key key) {
    pqxx::work tx(*m_connection);
    tx.exec_params0("DELETE FROM field WHERE (key = $1);", key);
    tx.commit();
  }
}
