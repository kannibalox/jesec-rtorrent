#include <string>
#include <sstream>

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

    w.exec0("CREATE TABLE IF NOT EXISTS session (hash VARCHAR(40) UNIQUE, torrent BYTEA, rtorrent BYTEA, resume BYTEA);");
    w.exec0("CREATE TABLE IF NOT EXISTS session (hash VARCHAR(40) UNIQUE, torrent BYTEA, rtorrent BYTEA, resume BYTEA);");

    w.commit();

    (*m_connection).prepare("insert_session_all", "INSERT INTO session (hash, torrent, rtorrent, resume) VALUES ($1, $2, $3, $4) ON CONFLICT (hash) DO UPDATE SET torrent = excluded.torrent, rtorrent = excluded.rtorrent, resume=excluded.resume;");
    (*m_connection).prepare("insert_session_resume", "INSERT INTO session (hash, rtorrent, resume) VALUES ($1, $2, $3) ON CONFLICT (hash) DO UPDATE SET rtorrent = excluded.rtorrent, resume=excluded.resume;");

    m_isEnabled = true;
  }

  void SessionStorePostgres::disable() {
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

  bool SessionStorePostgres::save(Download* d, int flags) {
    if (!is_enabled())
      return true;

    torrent::Object* resume_base =
      &d->download()->bencode()->get_key("libtorrent_resume");
    torrent::Object* rtorrent_base =
      &d->download()->bencode()->get_key("rtorrent");

    // Move this somewhere else?
    rtorrent_base->insert_key("chunks_done",
                              d->download()->file_list()->completed_chunks());
    rtorrent_base->insert_key("chunks_wanted",
                              d->download()->data()->wanted_chunks());
    rtorrent_base->insert_key("total_uploaded", d->info()->up_rate()->total());
    rtorrent_base->insert_key("total_downloaded",
                              d->info()->down_rate()->total());

    // Don't save for completed torrents when we've cleared the uncertain_pieces.
    torrent::resume_save_progress(*d->download(), *resume_base);
    torrent::resume_save_uncertain_pieces(*d->download(), *resume_base);

    torrent::resume_save_addresses(*d->download(), *resume_base);
    torrent::resume_save_file_priorities(*d->download(), *resume_base);
    torrent::resume_save_tracker_settings(*d->download(), *resume_base);

    // Temp fixing of all flags, move to a better place:
    resume_base->set_flags(torrent::Object::flag_session_data);
    rtorrent_base->set_flags(torrent::Object::flag_session_data);

    pqxx::work tx(*m_connection);

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
    }
    
    tx.exec_prepared0("insert_session_resume", hash, rtorrent_bin, resume_bin);

    tx.commit();
    return true;
  }

  SessionStore::field_value SessionStorePostgres::retrieve_field(session_key) {
    return field_value();
  }
  bool SessionStorePostgres::save_field(session_key key, const torrent::Object& obj) {
    return false;
  }
}
