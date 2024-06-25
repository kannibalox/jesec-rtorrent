#include <fstream>
#include <string>
#include <torrent/exceptions.h>
#include <torrent/object.h>
#include <torrent/object_stream.h>
#include <torrent/torrent.h>
#include <torrent/utils/error_number.h>
#include <torrent/utils/log.h>
#include <torrent/utils/resume.h>
#include <torrent/utils/string_manip.h>

#include "core/download.h"
#include "core/session_store_directory.h"
#include "utils/directory.h"

namespace core {
bool
SessionStoreDirectory::is_correct_format(const std::string& f) {
  if (f.size() != 48 || f.substr(40) != ".torrent")
    return false;

  for (std::string::const_iterator itr = f.begin(); itr != f.end() - 8; ++itr)
    if (!(*itr >= '0' && *itr <= '9') && !(*itr >= 'A' && *itr <= 'F'))
      return false;

  return true;
}

SessionStore::field_value
SessionStoreDirectory::retrieve_field(session_key key) {
  if (key == "rtorrent.input_history") {
    return load_input_history();
  }
  std::string  key_filename = m_uri + key;
  std::fstream key_stream(key_filename.c_str(),
                          std::ios::in | std::ios::binary);

  torrent::Object result = torrent::Object();

  if (key_stream.is_open()) {
    key_stream >> result;

    // If the file is corrupted we will just discard it with an
    // error message.
    if (key_stream.fail()) {
      lt_log_print(torrent::LOG_DEBUG,
                   "field file corrupted, discarding (path:%s)",
                   key_filename.c_str());
    } else {
      lt_log_print(
        torrent::LOG_DEBUG, "field file read (path:%s)", key_filename.c_str());
    }
  } else {
    lt_log_print(torrent::LOG_DEBUG,
                 "could not open field file (path:%s)",
                 key_filename.c_str());
  }
  return result;
}

SessionStore::field_value
SessionStoreDirectory::load_input_history() {
  if (!is_enabled())
    return torrent::Object::create_list();

  torrent::Object             resultRaw = torrent::Object::create_list();
  torrent::Object::list_type& result    = resultRaw.as_list();

  std::string  history_filename = m_uri + "rtorrent.input_history";
  std::fstream history_file(history_filename.c_str(), std::ios::in);

  if (history_file.is_open()) {
    std::string line;

    while (std::getline(history_file, line)) {
      if (!line.empty()) {
        int delim_pos = line.find("|");

        if (delim_pos != std::string::npos) {
          torrent::Object             rowRaw = torrent::Object::create_list();
          torrent::Object::list_type& row    = rowRaw.as_list();
          row.insert(row.end(), std::atoi(line.substr(0, delim_pos).c_str()));
          row.insert(row.end(),
                     torrent::utils::trim(line.substr(delim_pos + 1)));
          result.insert(result.end(), rowRaw);
        }
      }
    }

    if (history_file.bad()) {
      lt_log_print(torrent::LOG_DEBUG,
                   "input history file corrupted, discarding (path:%s)",
                   history_filename.c_str());
    } else {
      lt_log_print(torrent::LOG_DEBUG,
                   "input history file read (path:%s)",
                   history_filename.c_str());
    }
  } else {
    lt_log_print(torrent::LOG_DEBUG,
                 "could not open input history file (path:%s)",
                 history_filename.c_str());
  }
  return resultRaw;
}

bool
SessionStoreDirectory::save_field(session_key key, const torrent::Object& obj) {
  if (key == "rtorrent.input_history") {
    return save_input_history(obj);
  }
  return write_bencode_file(m_uri + "key", obj, 0);
}

bool
SessionStoreDirectory::save_input_history(const torrent::Object& obj) {
  std::string  history_filename     = m_uri + "rtorrent.input_history";
  std::string  history_filename_tmp = history_filename + ".new";
  std::fstream history_file(history_filename_tmp.c_str(),
                            std::ios::out | std::ios::trunc);
  if (!history_file.is_open()) {
    lt_log_print(torrent::LOG_DEBUG,
                 "could not open input history file for writing (path:%s)",
                 history_filename.c_str());
    return false;
  }
  for (auto const& itr : obj.as_list()) {
    history_file << std::to_string(itr.as_list().front().as_value()) << "|"
                 << itr.as_list().back().as_string();
  }
  if (!history_file.good()) {
    lt_log_print(
      torrent::LOG_DEBUG,
      "input history file corrupted during writing, discarding (path:%s)",
      history_filename.c_str());
    return false;
  } else {
    lt_log_print(torrent::LOG_DEBUG,
                 "input history file written (path:%s)",
                 history_filename.c_str());
  }
  history_file.close();
  std::rename(history_filename_tmp.c_str(), history_filename.c_str());
  return true;
}

bool
load_session_file(torrent::Object& obj, const std::string& filename) {
  std::fstream stream(filename.c_str(), std::ios::in | std::ios::binary);
  if (!stream.is_open())
    return false;
  stream >> obj;
  if (!stream.good())
    return false;
  return true;
}

void
SessionStoreDirectory::load_all() {
  if (!is_enabled()) {
    return;
  }
  utils::Directory d(m_uri);
  if (!d.update(utils::Directory::update_hide_dot))
    throw torrent::storage_error(
      "core::SessionStoreDirectory::load_all() could not open directory \"" +
      m_uri + "\"");

  d.erase(std::remove_if(d.begin(),
                         d.end(),
                         [](const utils::directory_entry& entry) {
                           return !SessionStoreDirectory::is_correct_format(
                             entry.d_name);
                         }),
          d.end());

  for (auto const& itr : d) {
    if (!itr.is_file())
      continue;
    torrent::Object main_data              = torrent::Object::create_map();
    torrent::Object rtorrent_data          = torrent::Object::create_map();
    torrent::Object libtorrent_resume_data = torrent::Object::create_map();
    load_session_file(main_data, m_uri + itr.d_name);
    load_session_file(rtorrent_data, m_uri + itr.d_name + ".rtorrent");
    load_session_file(libtorrent_resume_data,
                      m_uri + itr.d_name + ".libtorrent_resume");
    m_slot_load(SessionData(main_data, rtorrent_data, libtorrent_resume_data));
  }
}

void
SessionStoreDirectory::enable(bool lock) {
  if (is_enabled())
    throw torrent::input_error("Session directory already enabled.");

  if (m_uri.empty())
    return;

  if (lock)
    m_lockfile.set_path(m_uri + "rtorrent.lock");
  else
    m_lockfile.set_path(std::string());

  if (!m_lockfile.try_lock()) {
    if (torrent::utils::error_number::current().is_bad_path())
      throw torrent::input_error(
        "Could not lock session directory: \"" + m_uri + "\", " +
        torrent::utils::error_number::current().message().c_str());
    else
      throw torrent::input_error("Could not lock session directory: \"" +
                                 m_uri + "\", held by \"" +
                                 m_lockfile.locked_by_as_string() + "\".");
  }
  m_isEnabled = true;
}

void
SessionStoreDirectory::disable() {
  if (!is_enabled()) {
    return;
  }
  m_lockfile.unlock();
}

bool
SessionStoreDirectory::write_bencode_file(const std::string& filename,
                                          session_value      obj,
                                          uint32_t           skip_mask) {
  torrent::Object tmp;
  std::fstream    output(filename.c_str(), std::ios::out | std::ios::trunc);

  if (!output.is_open())
    goto download_store_save_error;

  torrent::object_write_bencode(&output, &obj, skip_mask);

  if (!output.good())
    goto download_store_save_error;

  output.close();

  // Test the new file, to ensure it is a valid bencode string.
  output.open(filename.c_str(), std::ios::in);
  output >> tmp;

  if (!output.good())
    goto download_store_save_error;

  output.close();
  return true;

download_store_save_error:
  output.close();
  return false;
}

std::string
SessionStoreDirectory::create_filename(session_key key) {
  return m_uri + key + ".torrent";
}

int
SessionStoreDirectory::save_resume(DownloadList::const_iterator dstart,
                                   DownloadList::const_iterator dend) {
  return std::count_if(
    dstart, dend, [this](Download* download) { return save(download, 0); });
}

bool
SessionStoreDirectory::save(Download* d, int flags) {
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

  std::string base_filename = create_filename(d);

  if (!write_bencode_file(
        base_filename + ".libtorrent_resume.new", *resume_base, 0) ||
      !write_bencode_file(base_filename + ".rtorrent.new", *rtorrent_base, 0))
    return false;

  ::rename((base_filename + ".libtorrent_resume.new").c_str(),
           (base_filename + ".libtorrent_resume").c_str());
  ::rename((base_filename + ".rtorrent.new").c_str(),
           (base_filename + ".rtorrent").c_str());

  if (!(flags & flag_skip_static) &&
      write_bencode_file(base_filename + ".new",
                         *d->bencode(),
                         torrent::Object::flag_session_data))
    ::rename((base_filename + ".new").c_str(), base_filename.c_str());

  return true;
}

std::string
SessionStoreDirectory::create_filename(Download* d) {
  return m_uri +
         torrent::utils::transform_hex(d->info()->hash().begin(),
                                       d->info()->hash().end()) +
         ".torrent";
}
}
