// MGStreamAdapter.cpp
#include "MGStreamAdapter.h"

mg_buf::mg_buf(struct mg_connection* _conn) : conn(_conn) {
    setp(buffer, buffer + BUF_SIZE - 1); // -1 to leave space for overflow '\0'
}

mg_buf::int_type mg_buf::overflow(int_type c) {
    if (c != EOF) {
        *pptr() = c;
        pbump(1);
    }

    if (flush_buffer() == EOF) {
        return EOF;
    }

    return c;
}

int mg_buf::flush_buffer() {
    int len = int(pptr() - pbase());
    if (mg_write(conn, buffer, len) != len) {
        return EOF;
    }
    pbump(-len); // reset put pointer accordingly
    return len;
}

int mg_buf::sync() {
    if (flush_buffer() == EOF) {
        return -1;  // return -1 on error
    }
    return 0;
}

MGStreamAdapter::MGStreamAdapter(struct mg_connection* _conn) : std::ostream(&buf), buf(_conn) {
    rdbuf(&buf);  // set the custom streambuf
}
