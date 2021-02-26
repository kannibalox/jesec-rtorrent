// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2005-2011, Jari Sundell <jaris@ifi.uio.no>

#ifndef RTORRENT_DISPLAY_FRAME_H
#define RTORRENT_DISPLAY_FRAME_H

#include <cinttypes>

namespace display {

class Window;

class Frame {
public:
  using extent_type = uint32_t;
  using size_type   = uint32_t;

  enum Type { TYPE_NONE, TYPE_WINDOW, TYPE_ROW, TYPE_COLUMN };

  struct bounds_type {
    bounds_type() = default;
    bounds_type(extent_type minW,
                extent_type minH,
                extent_type maxW,
                extent_type maxH)
      : minWidth(minW)
      , minHeight(minH)
      , maxWidth(maxW)
      , maxHeight(maxH) {}

    extent_type min_width() const {
      return minWidth;
    }
    extent_type min_height() const {
      return minHeight;
    }

    extent_type minWidth;
    extent_type minHeight;

    extent_type maxWidth;
    extent_type maxHeight;
  };

  using dynamic_type = std::pair<Frame*, bounds_type>;

  static constexpr size_type max_size = 5;

  bool is_width_dynamic() const;
  bool is_height_dynamic() const;

  bool has_left_frame() const;
  bool has_bottom_frame() const;

  bounds_type preferred_size() const;

  uint32_t position_x() const {
    return m_positionX;
  }
  uint32_t position_y() const {
    return m_positionY;
  }

  uint32_t width() const {
    return m_width;
  }
  uint32_t height() const {
    return m_height;
  }

  Type get_type() const {
    return m_type;
  }
  void set_type(Type t);

  Window* window() const {
    return m_window;
  }

  Frame* frame(size_type idx) {
    return m_container.data[idx];
  }

  size_type container_size() const {
    return m_container.size;
  }
  void set_container_size(size_type size);

  void initialize_window(Window* window);
  void initialize_row(size_type size);
  void initialize_column(size_type size);

  void clear();

  void refresh();
  void redraw();

  void balance(uint32_t x, uint32_t y, uint32_t width, uint32_t height);

private:
  inline void balance_window(uint32_t x,
                             uint32_t y,
                             uint32_t width,
                             uint32_t height);
  inline void balance_row(uint32_t x,
                          uint32_t y,
                          uint32_t width,
                          uint32_t height);
  inline void balance_column(uint32_t x,
                             uint32_t y,
                             uint32_t width,
                             uint32_t height);

  Type m_type{ TYPE_NONE };

  uint32_t m_positionX{ 0 };
  uint32_t m_positionY{ 0 };
  uint32_t m_width{ 0 };
  uint32_t m_height{ 0 };

  Window* m_window;

  struct {
    size_type size;
    Frame*    data[max_size];
  } m_container;
};

}

#endif
