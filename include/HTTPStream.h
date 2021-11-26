#ifndef BELL_HTTP_STREAM_H
#define BELL_HTTP_STREAM_H

#include <string>
#include <BellLogger.h>
#include <ByteStream.h>

/*
* HTTPStream
*
* A class for reading and writing HTTP streams implementing the ByteStream interface.
*
*/
namespace bell
{
    enum class StreamStatus
    {
        OPENING,
        READING_HEADERS,
        READING_DATA,
        CLOSED
    };

    class HTTPStream: public ByteStream
    {
    public:
        HTTPStream();
        ~HTTPStream();

        int sockFd;

        StreamStatus status = StreamStatus::OPENING;

        /*
        * opens connection to given url and reads header
        *
        * @param url the http url to connect to
        */
        void connectToUrl(std::string url);

        /*
        * Reads data from the stream.
        *
        * @param buf The buffer to read data into.
        * @param nbytes The size of the buffer.
        * @return The number of bytes read.
        * @throws std::runtime_error if the stream is closed.
        */
        size_t read(uint8_t *buf, size_t nbytes);

        /*
        * Skips nbytes bytes in the stream.
        */
        size_t skip(size_t nbytes);

        size_t position() { return 0; }
        
        size_t size() { return 0; }


        // Closes the connection
        void close();
    };
}
#endif
