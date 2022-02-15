
#include "tlssocketconnection.h"

#include "fdutils.h"

#if !(defined(WIN32) || defined(__WIN32) || defined(__WIN32__) || defined(FORCE_WIN32))
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#else
#include <winsock2.h>
typedef SSIZE_T ssize_t;
#endif

#include <openssl/ssl.h>
#include <openssl/err.h>
#include <iostream>



using namespace std;

typedef int(*SSL_func)(SSL*);
int SSL_op_timeout(SSL_func fun, SSL* sslHandle, int file_descriptor, int timeout_seconds);

static pthread_mutex_t _tls_socket_connection_mutex = PTHREAD_MUTEX_INITIALIZER;
static uint32_t _num_outstanding_tls_socket_connections = 0;

static pthread_mutex_t _server_ssl_context_mutex = PTHREAD_MUTEX_INITIALIZER;
static uint32_t _server_ssl_context_refcount = 0;
SSL_CTX* TLSSocketConnection::_server_ssl_context = NULL;

static pthread_mutex_t _client_ssl_context_mutex = PTHREAD_MUTEX_INITIALIZER;
static uint32_t _client_ssl_context_refcount = 0;
SSL_CTX* TLSSocketConnection::_client_ssl_context = NULL;

void TLSSocketConnection::StaticInit()
{
    pthread_mutex_lock(&_tls_socket_connection_mutex);
    if (_num_outstanding_tls_socket_connections == UINT32_MAX)
        throw("Fatal error: overflow");
    if (_num_outstanding_tls_socket_connections == 0)
    {
        SSL_load_error_strings();
        SSL_library_init();
    }
    _num_outstanding_tls_socket_connections++;
    pthread_mutex_unlock(&_tls_socket_connection_mutex);
}


void TLSSocketConnection::StaticDeinit()
{
    pthread_mutex_lock(&_tls_socket_connection_mutex);
    if (_num_outstanding_tls_socket_connections == 0)
        throw("Fatal error: underflow");
    _num_outstanding_tls_socket_connections--;
    if (_num_outstanding_tls_socket_connections == 0)
    {
        ERR_remove_state(0); // TODO: these need to be moved to a static deallocator
        //ENGINE_cleanup();
        //CONF_modules_unload(1);
        ERR_free_strings();
        EVP_cleanup();
        CRYPTO_cleanup_all_ex_data();
        SSL_COMP_free_compression_methods();
    }
    pthread_mutex_unlock(&_tls_socket_connection_mutex);
}



TLSSocketConnection::TLSSocketConnection(SocketConnectionOwner* owner, PCQueue<Packet*>* input_buffer_ptr)
    : SocketConnection_Base(owner, input_buffer_ptr), active(false), _sslHandle(NULL), _client_vs_server_protect(false), _sslContext(NULL), ssl_handle_mutex(PTHREAD_MUTEX_INITIALIZER)
{
    DEBUG_REPORT_LOCATION;

    StaticInit();
}


TLSSocketConnection::~TLSSocketConnection()
{
    DEBUG_REPORT_LOCATION;

    pthread_mutex_lock(&ssl_handle_mutex);
    if (_sslHandle)
    {
        SSL_free(_sslHandle);
        _sslHandle = NULL;
    }
    pthread_mutex_unlock(&ssl_handle_mutex);

    if (_sslContext)
    {
        bool done_cleaning = false;
        pthread_mutex_lock(&_server_ssl_context_mutex);
        if (_sslContext == _server_ssl_context)
        {
            _server_ssl_context_refcount--;
            if (_server_ssl_context_refcount == 0)
            {
                SSL_CTX_free(_server_ssl_context);
                _server_ssl_context = NULL;
                _sslContext = NULL;
                done_cleaning = true;
            }
        }
        pthread_mutex_unlock(&_server_ssl_context_mutex);

        if (!done_cleaning)
        {
            pthread_mutex_lock(&_client_ssl_context_mutex);
            if (_sslContext == _client_ssl_context)
            {
                _client_ssl_context_refcount--;
                if (_client_ssl_context_refcount == 0)
                {
                    SSL_CTX_free(_client_ssl_context);
                    _client_ssl_context = NULL;
                    _sslContext = NULL;
                }
            }
            pthread_mutex_unlock(&_client_ssl_context_mutex);
        }
    }

    StaticDeinit();
}


void TLSSocketConnection::Activate()
{
    Lock();
    DEBUG_REPORT_LOCATION;
    if(!GetActive())
    {
        //start the reader/writer threads.
        //pthread_create(&reader_writer_id, NULL, TLSSocketConnection::ReaderWriter, this);
        pthread_create(&reader_id, NULL, TLSSocketConnection::Reader, this);
        pthread_create(&writer_id, NULL, TLSSocketConnection::Writer, this);
        active = true;
    }
    Unlock();
}


/*
    This needs to be modified so we can deactivate politely.  That is,
    we should be able to send out a final packet that tells the client
    "goodbye," rather than just hanging up on them.

    Currently, we can put a message in the output buffer, but there is
    no way to ensure it is flushed before we actually deactivate.  The
    best way to do this is probably to take over the reader thread,
    push out a farewell, then close the connection.  I'm indifferent
    as to whether we flush in packets that may still be in the output buffer.
*/
void TLSSocketConnection::Deactivate()
{
    int rv = 0;
    Lock();
    DEBUG_REPORT_LOCATION;
    do
    {
        if(!active)
            break;

        //close threads
        if (!pthread_equal(pthread_self(), reader_id))
        {
            LOG_DEBUG_OUT("cancelling Reader thread.");
            rv = pthread_cancel(reader_id);
            pthread_join(reader_id, NULL);
            LOG_DEBUG_OUT("cancelled Reader thread.");
        }
        else
        {
            LOG_DEBUG_OUT("Reader thread refused to cancel itself.");
            pthread_detach(reader_id);
        }


        if (!pthread_equal(pthread_self(), writer_id))
        {
            LOG_DEBUG_OUT("cancelling Writer thread.");
            rv = pthread_cancel(writer_id);
            pthread_join(writer_id, NULL);
            LOG_DEBUG_OUT("cancelled Writer thread.");
        }
        else
        {
            LOG_DEBUG_OUT("Writer thread refused to cancel itself.");
            pthread_detach(writer_id);
        }

        /*if (SSL_op_timeout(SSL_shutdown, _sslHandle, GetDescriptor(), 10) <= 0)
        {
            DEBUG_OUT("Shutdown failed.");
        }
        */
        
#if !(defined(WIN32) || defined(__WIN32) || defined(__WIN32__) || defined(FORCE_WIN32))
        close(GetDescriptor());
#else
        closesocket(GetDescriptor());
#endif

        /*
            Clear the output buffer. This assumes we don't want
            to store data that was intended for a particular client,
            then send it after it reconnects.
        */
        string* temp;
        while((temp = output_buffer.pop_front()))
        {
            delete temp;
            DEBUG_REPORT_LOCATION;
        }

        input_buffer->Producer(NULL); /* Tell everyone above us that we died.  This is okay because
                                         because every socket connection has its own packet handler
                                         thread.  However, if we ever implemented a server where
                                         incoming packets were placed into a common queue, then
                                         the death of one socket connection would incorrectly signal
                                         the death of others.

                                         TODO: Should this happen at a level above us, since this
                                            is a contract for calling Deactivate?

                                        TODO: Also this code is largely duplicated between SocketConnection
                                            and TLSSocketConnection.
                                      */

        DEBUG_REPORT_LOCATION;

        active = false;
    } while(0);

    DEBUG_REPORT_LOCATION;
    Unlock();
}


SSL* TLSSocketConnection::GetSSLHandle() const
{
    return _sslHandle;
}

#if 0
/*
    OpenSSL is a beast, ain't it?  I can't believe the human race has
    deferred the securing of its data to this monstrosity.  Our implementation
    of non-encrypted data transmission was clean.  There were no busy-waits
    or polling going on.  However, because OpenSSL insists on us doing
    non-blocking IO (no thread safety guarantees for SSL descriptor, AND
    we're not allowed to call select on the underlying file descriptor),
    we have to poll both the socket for incoming data, and our outgoing
    data buffer.

    For that reason, the ReaderWriter thread is implemented as a state
    machine that ping-pongs between reading mode and writing mode.  This
    is to give balanced service to both incoming and outgoing data -- to
    prevent, for example, a large block of incoming data from preventing
    us writing a block of outgoing data.

    Within the read block, there are two read modes.  The two read modes
    represent reading of the packet header and the packet payload.  This
    is required because packet headers are constant size, whereas payloads
    are not.  Therefore, we have to use different locally scoped data
    to track the process of reading such data (i.e. constant size data can
    be statically allocated, whereas variable size data must be dynamically
    allocated).

    TODO: This can be simplified by consolidating the bimodal reads into
    the dynamic-allocation-based reads.  That is, if packet type and length
    aren't set, then dynamically allocate a buffer the size of the packet
    header, read that header, set the type and length, then read the entire
    payload.  It's necessarily a two-step process, but not necessarilly
    static vs dynamic.

    For robustness, both partial reads and partial writes are supported.
*/
void* TLSSocketConnection::ReaderWriter(void* void_arg)
{
    if(sizeof(PacketDataLength) != sizeof(uint32_t))
        throw("major problem.  PacketDataLength does not equal sizeof(uint32_t), which breaks ntohl");

    enum
    {
        READ,
        WRITE
    };

    enum
    {
        READ_MODE_HEADER,
        READ_MODE_PAYLOAD
    };

    const unsigned int cbuf_size = sizeof(PacketType) + sizeof(PacketDataLength); //size of the type and length fields of a packet (i.e. this is used to store the header of the incoming packet)
    char buf[cbuf_size] = { 0 };
    TLSSocketConnection* sc_arg = (TLSSocketConnection*)void_arg;
    SSL* ssl = sc_arg->GetSSLHandle();
    ssize_t read_length = 0;
    int read_accum_count = 0;
    int write_accum_count = 0;
    uint8_t next_op = READ;
    uint8_t read_mode = READ_MODE_HEADER;
    bool stay_alive = true;
    string* temp = NULL;
    char* packetDataBuffer = NULL;
    char* seek_position = NULL;
    bool read_starved = false;
    bool write_starved = false;

    DEBUG_REPORT_LOCATION;

    /*
        PTHREAD_CANCEL_DEFERRED will ensure that the threads
        are killed only when they're blocked.  This is much
        safer than just killing the threads at random.
    */
    pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, NULL);

    while (stay_alive)
    {
        switch (next_op)
        {
            case READ:
            {
                if (read_mode == READ_MODE_HEADER)
                {
                    read_starved = false;
                    int n = SSL_read(ssl, buf + read_accum_count, cbuf_size - read_accum_count);
                    if (n < 0)
                    {
                        int err = SSL_get_error(ssl, n);
                        if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE)
                        {
                            read_starved = true;
                        }
                        else
                        {
                            LOG_ERROR_OUT("Error: " << ERR_error_string(err, NULL));
                            stay_alive = false;
                            break;
                        }
                    }
                    else if (n == 0)
                    {
                        // disconnected
                        LOG_DEBUG_OUT("Disconnected.");
                        stay_alive = false;
                        break;
                    }
                    else
                    {
                        read_accum_count += n;
                        if (read_accum_count >= cbuf_size)
                        {
                            read_mode = READ_MODE_PAYLOAD;
                        }
                    }
                }
                if (read_mode == READ_MODE_PAYLOAD)
                {
                    PacketType type = buf[0]; // TODO: validate the packet type  // the type is the first byte of the header
                    PacketDataLength data_length = 0;
#if 1
                    char* bytePtr = (char*)&data_length;                        
                    for (int i = 0; i < sizeof(PacketDataLength); i++)
                        bytePtr[i] = buf[i + 1];                                // the length is bytes 2, 3, 4, and 5.
                    data_length = ntohl(data_length);
#else
                    memcpy(&data_length, buf + 1, sizeof(PacketDataLength));
#endif
                    if (data_length > 0)
                    {
                        if (packetDataBuffer == NULL) // if we're reading brand new data
                        {
                            packetDataBuffer = new char[data_length]; // then allocate a new buffer
                            seek_position = packetDataBuffer;
                        }
                        if (packetDataBuffer == NULL) // if allocation failed
                        {
                            // colossal error
                            LOG_ERROR_OUT("Failed to allocated packetDataBuffer.");
                            stay_alive = false;
                            break;
                        }
                        read_starved = false;
                        ssize_t data_read_length = 0;
                        data_read_length = SSL_read(ssl, seek_position, data_length - (seek_position - packetDataBuffer));
                        if (data_read_length < 0)
                        {
                            int err = SSL_get_error(ssl, data_read_length);
                            if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE)
                            {
                                read_starved = true;
                            }
                            else
                            {
                                LOG_ERROR_OUT("Error: " << ERR_error_string(err, NULL));
                                delete[] packetDataBuffer;
                                packetDataBuffer = NULL;
                                seek_position = NULL;
                                stay_alive = false;
                                break;
                            }
                        }
                        else if (data_read_length == 0)
                        {
                            // disconnected
                            LOG_DEBUG_OUT("Disconnected.");
                            delete[] packetDataBuffer;
                            packetDataBuffer = NULL;
                            seek_position = NULL;
                            stay_alive = false;
                            break;
                        }
                        else
                        {
                            seek_position += data_read_length;
                        }
                    }

                    if (data_length == 0 || (size_t)(seek_position - packetDataBuffer) >= data_length)
                    {
                        if (type == Packet::ERR_DISCONNECTED)
                        {
                            // disconnected
                            LOG_DEBUG_OUT("Disconnected.");
                            delete[] packetDataBuffer;
                            packetDataBuffer = NULL;
                            seek_position = NULL;
                            stay_alive = false;
                            break;
                        }

                        try
                        {
                            Packet* new_pkt = NewPacket((SocketConnection_Base*)sc_arg, type, (PacketDataLength)(seek_position - packetDataBuffer), packetDataBuffer, false);
                            if (new_pkt == NULL)
                            {
                                /*
                                    The below deallocation is not necessary because if NewPacket fails, then ~Packet will be called which deallocs
                                    packetDataBuffer.  See C++ spec 15.2.1.
                                */
                                //if (packetDataBuffer)
                                //    delete[] packetDataBuffer;
                                LOG_ERROR_OUT("Failed to instantiate an incoming packet (section 1). ");
                            }
                            else
                            {
                                sc_arg->input_buffer->Producer(new_pkt); // if so, create a packet and stick it in the input_buffer
                            }
                        }
                        CATCHALL
                        {
                            LOG_ERROR_OUT("Failed to instantiate an incoming packet (section 2).");
                        }
                        // packetDataBuffer is now owned by the newly created packet, so don't delete it.
                        packetDataBuffer = NULL;
                        seek_position = NULL;
                        read_accum_count = 0;
                        read_mode = READ_MODE_HEADER;
                    }
                } // READ_MODE_PAYLOAD
                next_op = WRITE;
                break;
            } // case READ


            case WRITE:
            {
                // WRITE
                write_starved = false;
                if (temp == NULL)
                    temp = sc_arg->output_buffer.TryConsumer();

                if (temp == NULL)
                {
                    write_starved = true;
                }
                else
                {
                    int n = SSL_write(ssl, temp->c_str() + write_accum_count, temp->size() - write_accum_count);
                    if (n < 0)
                    {
                        int err = SSL_get_error(ssl, n);
                        if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE)
                        {
                            write_starved = true;
                        }
                        else
                        {
                            LOG_ERROR_OUT("Error: " << ERR_error_string(err, NULL));
                            stay_alive = false;
                            break;
                        }
                    }
                    else if (n == 0)
                    {
                        // disconnected
                        LOG_DEBUG_OUT("Disconnected.");
                        stay_alive = false;
                        break;
                    }
                    else
                    {
                        write_accum_count += n;
                        if (write_accum_count >= temp->size())
                        {
                            delete temp;
                            temp = NULL;
                            write_accum_count = 0;
                        }
                        /*
                        else
                        {
                            // Continue writing because we're not done yet.  But, do it in a way that allows SSL_read to execute also.
                        }
                        */
                    }
                }
                next_op = READ;
                break;
            } // case WRITE
        } // end of switch

        if(!stay_alive)
            break;

        pthread_testcancel(); // TODO: Wait. Won't this leak memory if we were in the middle of reading a packet above?
                                    // Yes, it will.  See pthread_cleanup_push().

        if (read_starved && write_starved)
        {
#if !(defined(WIN32) || defined(__WIN32) || defined(__WIN32__) || defined(FORCE_WIN32))
            usleep(25000);
#elif 1
            Sleep(25);
#else
            WaitUntilReadableOrWritable(sc_arg->GetDescriptor()); /* This is wrong. Regarding writes, the question isn't whether
                                                                     the underlying socket is writable. It's whether there's data
                                                                     in the output_buffer that's available to be written. 
                                                                     
                                                                     What we really want is wait until data arrives for reading
                                                                     in the socket, or until data arrives in the output_buffer
                                                                     for writing. 
                                                                     
                                                                     In consideration of converting this all to blocking IO, the 
                                                                     write side is ready for that. We can change TryConsumer() to
                                                                     Consumer() and it will be blocking.  For the read side, we
                                                                     have to not set the FD to non-blocking.
                                                                     
                                                                     Actually, I am not at all confident that it's okay to call
                                                                     SSL_write and SSL_read on the same SSL context from multiple
                                                                     threads. So, conversion to blocking isn't the only consideration.


                                                                     ----

                                                                     WaitUntil( readable || (temp == null && output_buffer->TryConsumer() || temp != null && writable );
                                                                     
                                                                     */
#endif
        }

    } // end of while

    SocketConnectionOwner* owner = sc_arg->GetOwner();
    if(owner)
        owner->DeleteSocketConnection((SocketConnection_Base*)sc_arg);
    else
        sc_arg->Deactivate(); // TODO: This is a hack.  Client sockets need to clean up properly.
    pthread_exit((void*)1);
    return NULL;
}
#endif


void* TLSSocketConnection::Reader(void* void_arg)
{
    if (sizeof(PacketDataLength) != sizeof(uint32_t)) // TODO: Change to compile-time assert / static assert.
        throw("major problem.  PacketDataLength does not equal sizeof(uint32_t), which breaks ntohl");

    //enum
    //{
    //    READ,
    //    WRITE
    //};

    enum
    {
        READ_MODE_HEADER,
        READ_MODE_PAYLOAD
    };

    const unsigned int cbuf_size = sizeof(PacketType) + sizeof(PacketDataLength); //size of the type and length fields of a packet (i.e. this is used to store the header of the incoming packet)
    char buf[cbuf_size] = { 0 };
    TLSSocketConnection* sc_arg = (TLSSocketConnection*)void_arg;
    pthread_mutex_lock(&(sc_arg->ssl_handle_mutex));
    SSL* ssl = sc_arg->GetSSLHandle();
    pthread_mutex_unlock(&(sc_arg->ssl_handle_mutex));
    ssize_t read_length = 0;
    int read_accum_count = 0;
    uint8_t read_mode = READ_MODE_HEADER;
    bool stay_alive = true;
    string* temp = NULL;
    char* packetDataBuffer = NULL;
    char* seek_position = NULL;
    bool read_starved = false;

    DEBUG_REPORT_LOCATION;

    /*
        PTHREAD_CANCEL_DEFERRED will ensure that the threads
        are killed only when they're blocked.  This is much
        safer than just killing the threads at random.
    */
    pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, NULL);

    while (stay_alive)
    {
        int err;
        if (read_mode == READ_MODE_HEADER)
        {
            read_starved = false;
            pthread_mutex_lock(&(sc_arg->ssl_handle_mutex));
            int n = SSL_read(ssl, buf + read_accum_count, cbuf_size - read_accum_count);
            if (n < 0)
            {
                err = SSL_get_error(ssl, n);
                pthread_mutex_unlock(&(sc_arg->ssl_handle_mutex));

                if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE)
                {
                    read_starved = true;
                }
                else
                {
                    LOG_ERROR_OUT("Error: " << ERR_error_string(err, NULL));
                    stay_alive = false;
                    break;
                }
            }
            else if (n == 0)
            {
                pthread_mutex_unlock(&(sc_arg->ssl_handle_mutex));

                // disconnected
                LOG_DEBUG_OUT("Disconnected.");
                stay_alive = false;
                break;
            }
            else
            {
                pthread_mutex_unlock(&(sc_arg->ssl_handle_mutex));

                read_accum_count += n;
                if (read_accum_count >= cbuf_size)
                {
                    read_mode = READ_MODE_PAYLOAD;
                }
            }

        }
        if (read_mode == READ_MODE_PAYLOAD)
        {
            PacketType type = buf[0]; // TODO: validate the packet type  // the type is the first byte of the header
            PacketDataLength data_length = 0;
#if 1
            char* bytePtr = (char*)&data_length;
            for (int i = 0; i < sizeof(PacketDataLength); i++)
                bytePtr[i] = buf[i + 1];                                // the length is bytes 2, 3, 4, and 5.
            data_length = ntohl(data_length);
#else
            memcpy(&data_length, buf + 1, sizeof(PacketDataLength));
#endif
            if (data_length > 0)
            {
                if (packetDataBuffer == NULL) // if we're reading brand new data
                {
                    packetDataBuffer = new char[data_length]; // then allocate a new buffer
                    seek_position = packetDataBuffer;
                }
                if (packetDataBuffer == NULL) // if allocation failed
                {
                    // colossal error
                    LOG_ERROR_OUT("Failed to allocated packetDataBuffer.");
                    stay_alive = false;
                    break;
                }
                read_starved = false;
                pthread_mutex_lock(&(sc_arg->ssl_handle_mutex));

                ssize_t data_read_length = 0;
                data_read_length = SSL_read(ssl, seek_position, data_length - (seek_position - packetDataBuffer));
                if (data_read_length < 0)
                {
                    err = SSL_get_error(ssl, data_read_length);
                    pthread_mutex_unlock(&(sc_arg->ssl_handle_mutex));
                    if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE)
                    {
                        read_starved = true;
                    }
                    else
                    {
                        LOG_ERROR_OUT("Error: " << ERR_error_string(err, NULL));
                        delete[] packetDataBuffer;
                        packetDataBuffer = NULL;
                        seek_position = NULL;
                        stay_alive = false;
                        break;
                    }
                }
                else if (data_read_length == 0)
                {
                    pthread_mutex_unlock(&(sc_arg->ssl_handle_mutex));

                    // disconnected
                    LOG_DEBUG_OUT("Disconnected.");
                    delete[] packetDataBuffer;
                    packetDataBuffer = NULL;
                    seek_position = NULL;
                    stay_alive = false;
                    break;
                }
                else
                {
                    pthread_mutex_unlock(&(sc_arg->ssl_handle_mutex));

                    seek_position += data_read_length;
                }

            }

            if (data_length == 0 || (size_t)(seek_position - packetDataBuffer) >= data_length)
            {
                if (type == Packet::ERR_DISCONNECTED)
                {
                    // disconnected
                    LOG_DEBUG_OUT("Disconnected.");
                    delete[] packetDataBuffer;
                    packetDataBuffer = NULL;
                    seek_position = NULL;
                    stay_alive = false;
                    break;
                }

                try
                {
                    Packet* new_pkt = NewPacket((SocketConnection_Base*)sc_arg, type, (PacketDataLength)(seek_position - packetDataBuffer), packetDataBuffer, false);
                    if (new_pkt == NULL)
                    {
                        /*
                            The below deallocation is not necessary because if NewPacket fails, then ~Packet will be called which deallocs
                            packetDataBuffer.  See C++ spec 15.2.1.
                        */
                        //if (packetDataBuffer)
                        //    delete[] packetDataBuffer;
                        LOG_ERROR_OUT("Failed to instantiate an incoming packet (section 1). ");
                    }
                    else
                    {
                        sc_arg->input_buffer->Producer(new_pkt); // if so, create a packet and stick it in the input_buffer
                    }
                }
                CATCHALL
                {
                    LOG_ERROR_OUT("Failed to instantiate an incoming packet (section 2).");
                }
                    // packetDataBuffer is now owned by the newly created packet, so don't delete it.
                packetDataBuffer = NULL;
                seek_position = NULL;
                read_accum_count = 0;
                read_mode = READ_MODE_HEADER;
            }
        } // READ_MODE_PAYLOAD


        if (!stay_alive)
            break;

        //pthread_testcancel(); // TODO: Wait. Won't this leak memory if we were in the middle of reading a packet above?
                            // Yes, it will.  See pthread_cleanup_push().

        if (read_starved)
        {
            if (err == SSL_ERROR_WANT_READ)
            {
                int status = 0;
                do
                {
                    pthread_testcancel();
                    status = WaitUntilReadableOrTimeout(sc_arg->GetDescriptor(),250);
                    if (status < 0)
                    {
                        LOG_ERROR_OUT("Unexpected starvation condition.");
                        stay_alive = false;
                        break;
                    }
                } while (status == 0);
            }
            else if (err == SSL_ERROR_WANT_WRITE)
            {
                int status = 0;
                do
                {
                    pthread_testcancel();
                    status = WaitUntilWritableOrTimeout(sc_arg->GetDescriptor(), 250);
                    if (status < 0)
                    {
                        LOG_ERROR_OUT("Unexpected starvation condition.");
                        stay_alive = false;
                        break;
                    }
                } while (status == 0);
            }
            else
            {
                // This should never happen because the code above is should only be setting read_starved when SSL_ERROR_WANT_READ or SSL_ERROR_WANT_WRITE happen.
                LOG_ERROR_OUT("Unexpected starvation condition.");
                stay_alive = false;
                break;
            }
        }

        pthread_testcancel();
    } // end of while

    SocketConnectionOwner* owner = sc_arg->GetOwner();
    if (owner)
        owner->DeleteSocketConnection((SocketConnection_Base*)sc_arg);
    else
        sc_arg->Deactivate(); // TODO: This is a hack.  Client sockets need to clean up properly.
    pthread_exit((void*)1);
    return NULL;
}


void* TLSSocketConnection::Writer(void* void_arg)
{
    if (sizeof(PacketDataLength) != sizeof(uint32_t))
        throw("major problem.  PacketDataLength does not equal sizeof(uint32_t), which breaks ntohl");


    TLSSocketConnection* sc_arg = (TLSSocketConnection*)void_arg;
    pthread_mutex_lock(&(sc_arg->ssl_handle_mutex));
    SSL* ssl = sc_arg->GetSSLHandle();
    pthread_mutex_unlock(&(sc_arg->ssl_handle_mutex));
    int write_accum_count = 0;
    bool stay_alive = true;
    string* temp = NULL;
    bool write_starved = false;

    DEBUG_REPORT_LOCATION;

    /*
        PTHREAD_CANCEL_DEFERRED will ensure that the threads
        are killed only when they're blocked.  This is much
        safer than just killing the threads at random.
    */
    pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, NULL);

    while (stay_alive)
    {
        // WRITE
        int err;
        write_starved = false;
        if (temp == NULL)
            temp = sc_arg->output_buffer.Consumer();

        if (temp == NULL)
        {
            write_starved = true;
        }
        else
        {
            pthread_mutex_lock(&(sc_arg->ssl_handle_mutex));
            int n = SSL_write(ssl, temp->c_str() + write_accum_count, temp->size() - write_accum_count);
            if (n < 0)
            {
                err = SSL_get_error(ssl, n);
                pthread_mutex_unlock(&(sc_arg->ssl_handle_mutex));

                if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE)
                {
                    write_starved = true;
                }
                else
                {
                    LOG_ERROR_OUT("Error: " << ERR_error_string(err, NULL));
                    stay_alive = false;
                    break;
                }
            }
            else if (n == 0)
            {
                pthread_mutex_unlock(&(sc_arg->ssl_handle_mutex));

                // disconnected
                LOG_DEBUG_OUT("Disconnected.");
                stay_alive = false;
                break;
            }
            else
            {
                pthread_mutex_unlock(&(sc_arg->ssl_handle_mutex));

                write_accum_count += n;
                if (write_accum_count >= temp->size())
                {
                    delete temp;
                    temp = NULL;
                    write_accum_count = 0;
                }
                /*
                else
                {
                    // Continue writing because we're not done yet.  But, do it in a way that allows SSL_read to execute also.
                }
                */
            }
        }


        if (!stay_alive)
            break;

        //pthread_testcancel(); // TODO: Wait. Won't this leak memory if we were in the middle of reading a packet above?
                                    // Yes, it will.  See pthread_cleanup_push().

        if (write_starved)
        {
            if (err == SSL_ERROR_WANT_READ)
            {
                int status = 0;
                do
                {
                    pthread_testcancel();
                    status = WaitUntilReadableOrTimeout(sc_arg->GetDescriptor(), 250);
                    if (status < 0)
                    {
                        LOG_ERROR_OUT("Unexpected starvation condition.");
                        stay_alive = false;
                        break;
                    }
                } while (status == 0);
            }
            else if (err == SSL_ERROR_WANT_WRITE)
            {
                int status = 0;
                do
                {
                    pthread_testcancel();
                    status = WaitUntilWritableOrTimeout(sc_arg->GetDescriptor(), 250);
                    if (status < 0)
                    {
                        LOG_ERROR_OUT("Unexpected starvation condition.");
                        stay_alive = false;
                        break;
                    }
                } while (status == 0);
            }
            else
            {
                // This should never happen because the code above is should only be setting read_starved when SSL_ERROR_WANT_READ or SSL_ERROR_WANT_WRITE happen.
                LOG_ERROR_OUT("Unexpected starvation condition.");
                stay_alive = false;
                break;
            }
        }

        pthread_testcancel();

    } // end of while

    SocketConnectionOwner* owner = sc_arg->GetOwner();
    if (owner)
        owner->DeleteSocketConnection((SocketConnection_Base*)sc_arg);
    else
        sc_arg->Deactivate(); // TODO: This is a hack.  Client sockets need to clean up properly.
    pthread_exit((void*)1);
    return NULL;
}



int SSL_op_timeout(SSL_func func, SSL* _sslHandle, int file_descriptor, int timeout_seconds)
{
    timeval timeout;
    timeout.tv_sec = timeout_seconds;
    timeout.tv_usec = 0;
    bool ssl_func_failed = false;
    bool keep_going = false;
    int ssl_err = 0;
    do
    {
        keep_going = false;
        ssl_err = func(_sslHandle);
        if (ssl_err <= 0)
        {
            int sub_err = SSL_get_error(_sslHandle, ssl_err);
            if (sub_err == SSL_ERROR_WANT_READ)
            {
                keep_going = true;
                sub_err = WaitUntilReadableWithTimeval(file_descriptor, &timeout);
                if (sub_err < 0)
                {
                    LOG_ERROR_OUT("select failed.  Error: " << errno);
                    ssl_func_failed = true;
                    ssl_err = -1;
                    break;
                }
                else if (sub_err == 0)
                {
                    LOG_DEBUG_OUT("timed out.");
                    ssl_func_failed = true;
                    ssl_err = -1;
                    break;
                }
            }
            else if (sub_err == SSL_ERROR_WANT_WRITE)
            {
                keep_going = true;
                sub_err = WaitUntilWritableWithTimeval(file_descriptor, &timeout);
                if (sub_err < 0)
                {
                    LOG_ERROR_OUT("select failed.  Error: " << errno);
                    ssl_func_failed = true;
                    ssl_err = -1;
                    break;
                }
                else if (sub_err == 0)
                {
                    LOG_DEBUG_OUT("timed out.");
                    ssl_func_failed = true;
                    ssl_err = -1;
                    break;
                }
            }
            else
            {
                if (func == SSL_shutdown) // OpenSSL sucks so damn bad!  Docs say "erroneous SSL_ERROR_SYSCALL may be flagged."
                {
                    keep_going = true;
                    continue;
                }
                LOG_ERROR_OUT("SSLFunc() failed: ssl_err:" << ssl_err << " sub_err:" << sub_err << " errno:" << errno /*<< " WSAGLE:" << WSAGetLastError()*/);
                ssl_func_failed = true;
                ssl_err = -1;
                break;
            }
        }
    } while (keep_going && !ssl_func_failed);

    return ssl_err;
}


bool TLSSocketConnection::GetActive() const
{
    return active;
}



void TLSSocketConnection::PrepareServerConnection()
{
    if (_client_vs_server_protect)
        throw("You're only allowed to call PrepareServerConnection() and/or PrepareClientConnection() once on each TLSSocketConnection.");
    else
        _client_vs_server_protect = true;

    if (_sslHandle)
        throw("_sslHandle is already set.");

    pthread_mutex_lock(&_server_ssl_context_mutex);
    if (_server_ssl_context_refcount == UINT32_MAX)
        throw("overflow");
    if (_server_ssl_context_refcount == 0)
        _server_ssl_context = SSL_CTX_new(TLSv1_2_server_method());
    _server_ssl_context_refcount++;
    if (_server_ssl_context == NULL)
        throw("SSL_CTX_new() allocation failed.");
    _sslContext = _server_ssl_context;    
    pthread_mutex_unlock(&_server_ssl_context_mutex);

    int use_cert = SSL_CTX_use_certificate_file(_sslContext, "./server.crt", SSL_FILETYPE_PEM);
    if (use_cert <= 0)
    {
        throw("SSL_CTX_use_certificate_file failed.");
    }

    int use_prv = SSL_CTX_use_PrivateKey_file(_sslContext, "./server.key", SSL_FILETYPE_PEM);
    if (use_prv <= 0)
    {
        throw("SSL_CTX_use_PrivateKey_file() failed.");
    }
    if (1 != SSL_CTX_check_private_key(_sslContext))
    {
        LOG_ERROR_OUT("Private key does not match the certificate public key.");
        throw("Private key does not match the certificate public key.");
    }

    if (!SetNonBlockingMode(GetDescriptor()))
    {
        LOG_ERROR_OUT("Failed to set connection to non-blocking mode.");
        CloseDescriptor(GetDescriptor());
        throw("SetNonBlockingMode() failed.");
    }

    _sslHandle = SSL_new(_sslContext);
    SSL_set_fd(_sslHandle, GetDescriptor());

    int ssl_err = SSL_op_timeout(SSL_accept, _sslHandle, GetDescriptor(), 10);
    if (ssl_err <= 0)
    {
        SSL_free(_sslHandle);
        CloseDescriptor(GetDescriptor());
        throw("SSL_accept() failed.");
    }
}


void TLSSocketConnection::PrepareClientConnection()
{
    if (_client_vs_server_protect)
        throw("You're only allowed to call PrepareServerConnection() and/or PrepareClientConnection() once on each TLSSocketConnection.");
    else
        _client_vs_server_protect = true;

    pthread_mutex_lock(&_client_ssl_context_mutex);
    if (_client_ssl_context_refcount == UINT32_MAX)
        throw("overflow");
    if (_client_ssl_context_refcount == 0)
        _client_ssl_context = SSL_CTX_new(TLSv1_2_client_method());
    _client_ssl_context_refcount++;
    _sslContext = _client_ssl_context;
    pthread_mutex_unlock(&_client_ssl_context_mutex);


    if (!SSLConnect())
        throw("SSLConnect() failed.");
}


void TLSSocketConnection::SetSSLHandle(SSL* ssl)
{
    _sslHandle = ssl;
}


bool TLSSocketConnection::SSLConnect()
{
    pthread_mutex_lock(&ssl_handle_mutex);

    if (_sslHandle)
    {
        pthread_mutex_unlock(&ssl_handle_mutex);
        return false;
    }
    if (!_sslContext)
    {
        pthread_mutex_unlock(&ssl_handle_mutex);
        return false;
    }

    _sslHandle = NULL;

    int fd = GetDescriptor();
    if (fd <= 0)
    {
        pthread_mutex_unlock(&ssl_handle_mutex);
        throw("descriptor isn't set");
    }
    else
    {
        // TODO: rewrite this as a do/while(0) cleanup routine

        // Create an SSL struct for the connection
        _sslHandle = SSL_new(_sslContext);
        if (_sslHandle == NULL)
        {
            pthread_mutex_unlock(&ssl_handle_mutex);
            LOG_ERROR_OUT("SSL_new() failed.");
            return false;
        }

        // Connect the SSL struct to our connection
        if (!SSL_set_fd(_sslHandle, fd))
        {
            LOG_ERROR_OUT("SSL_set_fd() failed.");
            SSL_free(_sslHandle);
            _sslHandle = NULL;
            pthread_mutex_unlock(&ssl_handle_mutex);
            return false;
        }

        if (!SetNonBlockingMode(fd))
        {
            LOG_ERROR_OUT("Failed to set non-blocking IO mode.");
            SSL_free(_sslHandle);
            _sslHandle = NULL;
            pthread_mutex_unlock(&ssl_handle_mutex);
            return false;
        }

        int ssl_err = SSL_op_timeout(SSL_connect, _sslHandle, fd, 10);
        if (ssl_err <= 0)
        {
            //shutdown(fd, SHUT_RDWR);
            if (SSL_op_timeout(SSL_shutdown, _sslHandle, fd, 10) <= 0)
            {
                LOG_ERROR_OUT("Shutdown failed.");
            }

            SSL_free(_sslHandle);
            _sslHandle = NULL;
            //CloseDescriptor(fd);
            pthread_mutex_unlock(&ssl_handle_mutex);
            return false;
        }
        else
        {
            cout << "SSL connection using " << SSL_get_cipher(_sslHandle) << endl;

            /* Get server's certificate (note: beware of dynamic allocation) - opt */

            X509* server_cert = SSL_get_peer_certificate(_sslHandle);
            cout << "Server certificate:" << endl;

            char* str = X509_NAME_oneline(X509_get_subject_name(server_cert), 0, 0);
            if (str == NULL)
                cerr << "X509_NAME failed." << endl;
            else
            {
                cout << "subject: " << str << endl;
                OPENSSL_free(str);
            }

            str = X509_NAME_oneline(X509_get_issuer_name(server_cert), 0, 0);
            if (str == NULL)
            {
                cerr << "X509_NAME failed." << endl;
            }
            else
            {
                cout << "issuer: " << str << endl;
                OPENSSL_free(str);
            }
            /*
            int result = SSL_get_verify_result(_sslHandle);
            X509_free(server_cert);
            if (result != X509_V_OK)
            {
            cerr << "Certificate verification failed with code: " << result << endl;
            SSL_free(_sslHandle);
            SSL_CTX_free(sslContext);
            _sslHandle = NULL;
            sslContext = NULL;
            return false; // TODO: this did not call SSL_shutdown
            }
            */
            /* We could do all sorts of certificate verification stuff here before
            deallocating the certificate. */

            pthread_mutex_unlock(&ssl_handle_mutex);
            return true;
        }

    }

    pthread_mutex_unlock(&ssl_handle_mutex);
    return false;
}


/*
------------------------------------------------------------------------------
This software is available under 2 licenses -- choose whichever you prefer.
------------------------------------------------------------------------------
ALTERNATIVE A - MIT License
Copyright (c) 2003-2019 Bobby G. Burrough
Permission is hereby granted, free of charge, to any person obtaining a copy of 
this software and associated documentation files (the "Software"), to deal in 
the Software without restriction, including without limitation the rights to 
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies 
of the Software, and to permit persons to whom the Software is furnished to do 
so, subject to the following conditions:
The above copyright notice and this permission notice shall be included in all 
copies or substantial portions of the Software.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR 
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, 
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE 
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER 
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, 
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE 
SOFTWARE.
------------------------------------------------------------------------------
ALTERNATIVE B - Public Domain (www.unlicense.org)
This is free and unencumbered software released into the public domain.
Anyone is free to copy, modify, publish, use, compile, sell, or distribute this 
software, either in source code form or as a compiled binary, for any purpose, 
commercial or non-commercial, and by any means.
In jurisdictions that recognize copyright laws, the author or authors of this 
software dedicate any and all copyright interest in the software to the public 
domain. We make this dedication for the benefit of the public at large and to 
the detriment of our heirs and successors. We intend this dedication to be an 
overt act of relinquishment in perpetuity of all present and future rights to 
this software under copyright law.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR 
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, 
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE 
AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN 
ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION 
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
------------------------------------------------------------------------------
*/
