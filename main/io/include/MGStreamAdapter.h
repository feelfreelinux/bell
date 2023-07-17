#pragma once

#include <iostream>
#include <ostream>
#include "civetweb.h"

const size_t BUF_SIZE = 1024;

// Custom streambuf
class mg_buf : public std::streambuf {
 private:
  struct mg_connection* conn;
  char buffer[BUF_SIZE];

 public:
  mg_buf(struct mg_connection* _conn);

 protected:
  virtual int_type overflow(int_type c);
  int flush_buffer();
  virtual int sync();
};

/**
 * @brief Adapts ostream to mg_write
 * 
 */
class MGStreamAdapter : public std::ostream {
 private:
  mg_buf buf;

 public:
  MGStreamAdapter(struct mg_connection* _conn);
};

