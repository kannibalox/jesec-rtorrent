#include <string>

#include "torrent/exceptions.h"
#include "utils/lockfile.h"
#include <fstream>
#include <torrent/object.h>
#include <torrent/object_stream.h>
#include <torrent/torrent.h>

#include "core/session_store.h"

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
SessionStore::enable(bool lock) {
  return;
}
void
SessionStore::disable() {
  return;
}
// Set/get operations for downloads
bool
SessionStore::save(Download* d, int flags) {
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
SessionStore::remove(Download* d) {
  return;
}
void
SessionStore::load_all() {
  return;
}
// Allow saving arbitrary fields in a separate place
bool
SessionStore::save_field(session_key key, const torrent::Object& obj) {
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
