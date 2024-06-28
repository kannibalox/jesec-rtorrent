#include <string>

#include "torrent/exceptions.h"
#include <torrent/object.h>
#include <torrent/object_stream.h>
#include <torrent/utils/resume.h>
#include <torrent/torrent.h>

#include "core/session_store.h"
#include "core/download.h"

namespace core {
void
SessionStore::set_location(const std::string& uri) {
  if (is_enabled())
    throw torrent::input_error(
      "Tried to change session location while it is enabled.");
  m_uri = uri;
}

void
SessionStore::set_lock_location(const std::string& lock_location) {
  if (is_enabled())
    throw torrent::input_error(
      "Tried to change session lock while it is enabled.");
  m_lockLocation = lock_location;
}

void
SessionStore::enable(bool) {
  return;
}
void
SessionStore::disable() {
  return;
}

void
SessionStore::save_download_data(Download* d) {
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
}

// Set/get operations for downloads
bool
SessionStore::save(Download* d, int) {
  save_download_data(d);
  return true;
}

int
SessionStore::save_resume(DownloadList::const_iterator dstart,
                          DownloadList::const_iterator dend) {
  return std::distance(dstart, dend);
}
void
SessionStore::remove(session_key) {
  return;
}
void
SessionStore::remove(Download*) {
  return;
}
void
SessionStore::load_all() {
  return;
}
// Allow saving arbitrary fields in a separate place
bool
SessionStore::save_field(session_key, const torrent::Object&) {
  return true;
}
SessionStore::field_value
SessionStore::retrieve_field(session_key) {
  return torrent::Object();
}
void
SessionStore::remove_field(session_key) {
  return;
}

}
