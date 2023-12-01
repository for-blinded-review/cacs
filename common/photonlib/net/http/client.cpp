/*
Copyright 2022 The Photon Authors

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/

#include "client.h"
#include <bitset>
#include <algorithm>
#include <random>
#include "../../common/alog-stdstring.h"
#include "../../common/iovector.h"
#include "../../common/string_view.h"
#include "../socket.h"
#include "../../co.h"

namespace photon {
namespace net {

bool ISocketStream::skip_read(size_t count) {
    if (!count) return true;
    while(count) {
        // static char buf[1024];
        size_t len = count < 1024 ? count : 1024;
        ssize_t ret = read(nullptr, len);
        if (ret < (ssize_t)len) return false;
        count -= len;
    }
    return true;
}

namespace http {
static const uint64_t kDNSCacheLife = 3600UL * 1000 * 1000;
static constexpr char USERAGENT[] = "EASE/0.21.6";

struct RingBuffer { // fake ring buffer
    char* buffer = nullptr;
    size_t m_read = 0, m_write = 0;
    size_t m_capacity = 0;

    RingBuffer(size_t capacity) {
        m_capacity = capacity;
        buffer = (char*)malloc(capacity);
    }

    ~RingBuffer() {
        free(buffer);
    }

#if defined(CACSTH) && !defined(CACSYIELD)
    __attribute__((preserve_none))
#endif
    ssize_t read(void *buf, size_t count)
    {
        if (count == 0)
            return 0;
        ssize_t ret = 0;
        while (m_write == 0) {
            LOG_DEBUG("^");
            photon::thread_yield();
        }
        size_t len = std::min(count, m_write - m_read);
        if (buf)
            memcpy(buf, buffer + m_read, len);
        m_read += len;
        ret += len;
        count -= len;
        if (m_read == m_write) {
            m_read = 0;
            m_write = 0;
        }

        return ret;
    }

#if defined(CACSTH) && !defined(CACSYIELD)
    __attribute__((preserve_none))
#endif
    ssize_t write(const void *buf, size_t count) {
        if (count == 0)
            return 0;
        char *p = (char*)buf;
        ssize_t ret = 0;
        while (count) {
            while (m_write) {
                // LOG_DEBUG("^");
                photon::thread_yield();
            }
            size_t len = std::min(count, m_capacity);
            if (buf)
                memcpy(buffer, p, len);
            m_write += len;
            ret += len;
            count -= len;
            p += len;
        }
        return ret;
    }
};


class SimplexMemoryStream
{
public:
    RingBuffer m_ringbuf;
    bool closed = false;
    SimplexMemoryStream(uint32_t capacity) : m_ringbuf(capacity) { }
    int close()
    {
        closed = true;
        return 0;
    }
#if defined(CACSTH) && !defined(CACSYIELD)
    __attribute__((preserve_none))
#endif
    ssize_t read(void *buf, size_t count)
    {
        if (closed) return -1;
        return m_ringbuf.read(buf, count);
    }

#if defined(CACSTH) && !defined(CACSYIELD)
    __attribute__((preserve_none))
#endif
    ssize_t write(const void *buf, size_t count)
    {
        if (closed) return -1;
        return m_ringbuf.write(buf, count);
    }

};

class DuplexMemoryStream
{
public:
    class EndPoint
    {
    public:
        SimplexMemoryStream* s1;
        SimplexMemoryStream* s2;
        bool closed = false;
        EndPoint(SimplexMemoryStream* s1, SimplexMemoryStream* s2) : s1(s1), s2(s2) { }
        virtual int close() 
        {
            closed = true;
            return 0;
        }
#if defined(CACSTH) && !defined(CACSYIELD)
    __attribute__((preserve_none))
#endif
        virtual ssize_t read(void *buf, size_t count) 
        {
            if (closed) return -1;
            return s1->read(buf, count);
        }
#if defined(CACSTH) && !defined(CACSYIELD)
    __attribute__((preserve_none))
#endif
        virtual ssize_t readv(const struct iovec *iov, int iovcnt) 
        {
            return -1;
        }
#if defined(CACSTH) && !defined(CACSYIELD)
    __attribute__((preserve_none))
#endif
        virtual ssize_t write(const void *buf, size_t count) 
        {
            if (closed) return -1;
            return s2->write(buf, count);
        }
#if defined(CACSTH) && !defined(CACSYIELD)
    __attribute__((preserve_none))
#endif
        virtual ssize_t writev(const struct iovec *iov, int iovcnt) 
        {
            return -1;
        }
    };

    EndPoint epa, epb;
    SimplexMemoryStream s1, s2;
    DuplexMemoryStream(uint32_t capacity) :
        epa(&s1, &s2), epb(&s2, &s1), s1(capacity), s2(capacity)
    {
    }
    int close()
    {
        epa.close();
        epb.close();
        return 0;
    }
};

DuplexMemoryStream * get_dxstream() {
    static DuplexMemoryStream dxstream(16*1024);
    return &dxstream;
}

class MemSocket : public ISocketStream {
public:
    
    // DuplexMemoryStream *dxstream = nullptr;
    DuplexMemoryStream::EndPoint *stream = nullptr;
    DuplexMemoryStream::EndPoint *reverse = nullptr;
    // MemSocket() : dxstream(new_duplex_memory_stream(64)) {
    //     stream = dxstream->endpoint_a;
    // }
    MemSocket() {
        stream = &get_dxstream()->epa;
        reverse = &get_dxstream()->epb;
    }

    // ~MemSocket() { close(); delete dxstream; }
    ~MemSocket() { }

#define LAMBDA(expr) [&]() __INLINE__ { return expr; }

template <typename IOCB>
__FORCE_INLINE__ ssize_t doio_n(void *&buf, size_t &count, IOCB iocb) {
    auto count0 = count;
    while (count > 0) {
        ssize_t ret = iocb();
        if (ret <= 0) return ret;
        (char *&)buf += ret;
        count -= ret;
    }
    return count0;
}

template <typename IOCB>
__FORCE_INLINE__ ssize_t doiov_n(iovector_view &v, IOCB iocb) {
    ssize_t count = 0;
    while (v.iovcnt > 0) {
        ssize_t ret = iocb();
        if (ret <= 0) return ret;
        count += ret;

        uint64_t bytes = ret;
        auto extracted = v.extract_front(bytes);
        assert(extracted == bytes);
        _unused(extracted);
    }
    return count;
}

    virtual Object* get_underlay_object(uint64_t recursion = 0) override {
        return nullptr;
    }
    virtual int getsockname(EndPoint& addr) override {
        errno = ENOSYS;
        return -1;
    }
    virtual int getpeername(EndPoint& addr) override {
        errno = ENOSYS;
        return -1;
    }
    virtual int getsockname(char* path, size_t count) override {
        errno = ENOSYS;
        return -1;
    }
    virtual int getpeername(char* path, size_t count) override {
        errno = ENOSYS;
        return -1;
    }
#if defined(CACSTH) && !defined(CACSYIELD)
    __attribute__((preserve_none))
#endif
    virtual ssize_t read(void* buf, size_t count) override {
        bool copy = buf;
        return doio_n(buf, count, LAMBDA(stream->read(copy ? buf : nullptr, count);));
    }
#if defined(CACSTH) && !defined(CACSYIELD)
    __attribute__((preserve_none))
#endif
    virtual ssize_t readv(const struct iovec* iov, int iovcnt) override {
        bool copy = iov->iov_base;
        iovector_view v((struct iovec*)iov, iovcnt);
        return doiov_n(v, [&]() __INLINE__ {
            if (!copy) v.iov->iov_base = nullptr;
            return stream->readv(v.iov, v.iovcnt);
        });
    }
#if defined(CACSTH) && !defined(CACSYIELD)
    __attribute__((preserve_none))
#endif
    virtual ssize_t write(const void* buf, size_t count) override {
        photon::thread_yield();
        return count;
        // bool copy = buf;
        // return doio_n((void*&)buf, count, LAMBDA(stream->write(copy ? buf : nullptr, count)));
    }
#if defined(CACSTH) && !defined(CACSYIELD)
    __attribute__((preserve_none))
#endif
    virtual ssize_t writev(const struct iovec* iov, int iovcnt) override {
        // bool copy = iov->iov_base;
        iovector_view v((struct iovec* )iov, iovcnt);
        // return doiov_n(v, [&]() __INLINE__ {
        //     if (!copy) v.iov->iov_base = nullptr;
        //     return stream->writev(v.iov, v.iovcnt);
        // });
        photon::thread_yield();
        return v.sum();
    }
    // virtual int close() override { return stream->close(); }
#if defined(CACSTH) && !defined(CACSYIELD)
    __attribute__((preserve_none))
#endif
    virtual int close() override { return 0; }
#if defined(CACSTH) && !defined(CACSYIELD)
    __attribute__((preserve_none))
#endif
    virtual ssize_t send(const void* buf, size_t count,
                         int flags = 0) override {
        return stream->write(buf, count);
    }
#if defined(CACSTH) && !defined(CACSYIELD)
    __attribute__((preserve_none))
#endif
    virtual ssize_t send(const struct iovec* iov, int iovcnt,
                         int flags = 0) override {
        return stream->writev(iov, iovcnt);
    }
#if defined(CACSTH) && !defined(CACSYIELD)
    __attribute__((preserve_none))
#endif
    virtual ssize_t recv(void* buf, size_t count, int flags = 0) override {
        return stream->read(buf, count);
    }
#if defined(CACSTH) && !defined(CACSYIELD)
    __attribute__((preserve_none))
#endif
    virtual ssize_t recv(const struct iovec* iov, int iovcnt,
                         int flags = 0) override {
        return stream->readv(iov, iovcnt);
    }
#if defined(CACSTH) && !defined(CACSYIELD)
    __attribute__((preserve_none))
#endif
    virtual ssize_t sendfile(int in_fd, off_t offset, size_t count) override {
        errno = ENOSYS;
        return -1;
    }
    virtual uint64_t timeout() const override { return -1UL; }
    virtual void timeout(uint64_t tm) override { return; }
    virtual int setsockopt(int level, int option_name, const void* option_value,
                           socklen_t option_len) override {
        errno = ENOSYS;
        return -1;
    }
    virtual int getsockopt(int level, int option_name, void* option_value,
                           socklen_t* option_len) override {
        errno = ENOSYS;
        return -1;
    }
};

class PooledDialer {
public:
    // net::TLSContext* tls_ctx = nullptr;
    // std::unique_ptr<ISocketClient> tcpsock;
    // std::unique_ptr<ISocketClient> tlssock;
    // std::unique_ptr<Resolver> resolver;

    //etsocket seems not support multi thread very well, use tcp_socket now. need to find out why
    // PooledDialer() :
    //         tls_ctx(new_tls_context(nullptr, nullptr, nullptr)),
    //         tcpsock(new_tcp_socket_pool(new_tcp_socket_client(), -1, true)),
    //         tlssock(new_tcp_socket_pool(new_tls_client(tls_ctx, new_tcp_socket_client(), true), -1, true)),
    //         resolver(new_default_resolver(kDNSCacheLife)) {
    // }
    PooledDialer() {}

    MemSocket msock;

    // ~PooledDialer() { delete tls_ctx; }
    ~PooledDialer() {}

    ISocketStream* dial(std::string_view host, uint16_t port, bool secure,
                             uint64_t timeout = -1UL);

    template <typename T>
    ISocketStream* dial(const T& x, uint64_t timeout = -1UL) {
        return dial(x.host_no_port(), x.port(), x.secure(), timeout);
    }
};

ISocketStream* PooledDialer::dial(std::string_view host, uint16_t port, bool secure, uint64_t timeout) {
    // LOG_DEBUG("Dial to ` `", host, port);
    // std::string strhost(host);
    // auto ipaddr = resolver->resolve(strhost.c_str());
    // EndPoint ep(ipaddr, port);
    // LOG_DEBUG("Connecting ` ssl: `", ep, secure);
    // ISocketStream *sock = nullptr;
    // if (secure) {
    //     tlssock->timeout(timeout);
    //     sock = tlssock->connect(ep);
    // } else {
    //     tcpsock->timeout(timeout);
    //     sock = tcpsock->connect(ep);
    // }
    // if (sock) {
    //     LOG_DEBUG("Connected ` host : ` ssl: ` `", ep, host, secure, sock);
    //     return sock;
    // }
    // LOG_DEBUG("connect ssl : ` ep : `  host : ` failed", secure, ep, host);
    // if (ipaddr.addr == 0) LOG_DEBUG("No connectable resolve result");
    // // When failed, remove resolved result from dns cache so that following retries can try
    // // different ips.
    // resolver->discard_cache(strhost.c_str());
    return &msock;
}

constexpr uint64_t code3xx() { return 0; }
template<typename...Ts>
constexpr uint64_t code3xx(uint64_t x, Ts...xs)
{
    return (1 << (x-300)) | code3xx(xs...);
}
constexpr static std::bitset<10>
    code_redirect_verb(code3xx(300, 301, 302, 307, 308));

static constexpr size_t kMinimalHeadersSize = 8 * 1024 - 1;
enum RoundtripStatus {
    ROUNDTRIP_SUCCESS,
    ROUNDTRIP_FAILED,
    ROUNDTRIP_REDIRECT,
    ROUNDTRIP_NEED_RETRY,
    ROUNDTRIP_FORCE_RETRY
};

constexpr static char dummyheader[] = "HTTP/1.1 200 OK\r\n"
"Content-Type: application/octet-stream\r\n"
"Content-Length: 1048576\r\n"
"\r\n";

class ClientImpl : public Client {
public:
    PooledDialer m_dialer;
    CommonHeaders<> m_common_headers;
    ICookieJar *m_cookie_jar;
    ClientImpl(ICookieJar *cookie_jar) :
        m_cookie_jar(cookie_jar) {}

    using SocketStream_ptr = std::unique_ptr<ISocketStream>;
    int redirect(Operation* op) {
        if (op->resp.body_size() > 0) {
            op->resp.skip_remain();
        }

        auto location = op->resp.headers["Location"];
        if (location.empty()) {
            LOG_ERROR_RETURN(EINVAL, ROUNDTRIP_FAILED,
                "redirect but has no field location");
        }
        LOG_DEBUG("Redirect to ", location);

        Verb v;
        auto sc = op->status_code - 300;
        if (sc == 3) {  // 303
            v = Verb::GET;
        } else if (sc < 10 && code_redirect_verb[sc]) {
            v = op->req.verb();
        } else {
            LOG_ERROR_RETURN(EINVAL, ROUNDTRIP_FAILED,
                "invalid 3xx status code: ", op->status_code);
        }

        if (op->req.redirect(v, location, op->enable_proxy) < 0) {
            LOG_ERRNO_RETURN(0, ROUNDTRIP_FAILED, "redirect failed");
        }
        return ROUNDTRIP_REDIRECT;
    }

    static void* send_resp(void* arg) {
        MemSocket* msock = (MemSocket*)arg;
        // send header
        ssize_t ret = 0;
        msock->reverse->write(dummyheader, sizeof(dummyheader) - 1);
        LOG_DEBUG("Response header send done", VALUE(ret), VALUE(sizeof(dummyheader) - 1));
        ret = msock->reverse->write(nullptr, 1048576l);
        LOG_DEBUG("Response send done", VALUE(ret));
        return nullptr;
    }

    int concurreny = 0;
#if defined(CACSTH) && !defined(CACSYIELD)
    __attribute__((preserve_none))
#endif
    int do_roundtrip(Operation* op, Timeout tmo) {
        concurreny++;
        LOG_DEBUG(VALUE(concurreny));
        DEFER(concurreny--);
        op->status_code = -1;
        if (tmo.timeout() == 0)
            LOG_ERROR_RETURN(ETIMEDOUT, ROUNDTRIP_FAILED, "connection timedout");
        auto &req = op->req;
        auto s = (m_proxy && !m_proxy_url.empty())
                     ? m_dialer.dial(m_proxy_url, tmo.timeout())
                     : m_dialer.dial(req, tmo.timeout());
        if (!s) LOG_ERROR_RETURN(0, ROUNDTRIP_NEED_RETRY, "connection failed");

        SocketStream_ptr sock(s);
        LOG_DEBUG("Sending request ` `", req.verb(), req.target());
        if (req.send_header(sock.get()) < 0) {
            sock->close();
            req.reset_status();
            LOG_ERROR_RETURN(0, ROUNDTRIP_NEED_RETRY, "send header failed, retry");
        }
        sock->timeout(tmo.timeout());
        if (op->body_stream) {
            // send body_stream
            if (req.write_stream(op->body_stream) < 0) {
                sock->close();
                req.reset_status();
                LOG_ERROR_RETURN(0, ROUNDTRIP_NEED_RETRY, "send body stream failed, retry");
            }
        } else {
            // call body_writer
            if (op->body_writer(&req) < 0) {
                sock->close();
                req.reset_status();
                LOG_ERROR_RETURN(0, ROUNDTRIP_NEED_RETRY, "failed to call body writer, retry");
            }
        }

        if (req.send() < 0) {
            sock->close();
            req.reset_status();
            LOG_ERROR_RETURN(0, ROUNDTRIP_NEED_RETRY, "failed to ensure send");
        }

        photon::thread_create(&ClientImpl::send_resp, (void*)sock.get());

        LOG_DEBUG("Request sent, wait for response ` `", req.verb(), req.target());
        auto space = req.get_remain_space();
        auto &resp = op->resp;

        if (space.second > kMinimalHeadersSize) {
            resp.reset(space.first, space.second, false, sock.release(), true, req.verb());
        } else {
            auto buf = malloc(kMinimalHeadersSize);
            resp.reset((char *)buf, kMinimalHeadersSize, true, sock.release(), true, req.verb());
        }
        if (op->resp.receive_header(tmo.timeout()) != 0) {
            req.reset_status();
            LOG_ERROR_RETURN(0, ROUNDTRIP_NEED_RETRY, "read response header failed");
        }

        op->status_code = resp.status_code();
        LOG_DEBUG("Got response ` ` code=` || content_length=`", req.verb(),
                  req.target(), resp.status_code(), resp.headers.content_length());
        if (m_cookie_jar) m_cookie_jar->get_cookies_from_headers(req.host(), &resp);
        if (resp.status_code() < 400 && resp.status_code() >= 300 && op->follow)
            return redirect(op);
        return ROUNDTRIP_SUCCESS;
    }

#if defined(CACSTH) && !defined(CACSYIELD)
    __attribute__((preserve_none))
#endif
    int call(Operation* /*IN, OUT*/ op) override {
        auto content_length = op->req.headers.content_length();
        auto encoding = op->req.headers["Transfer-Encoding"];
        if ((content_length != 0) && (encoding == "chunked")) {
            op->status_code = -1;
            LOG_ERROR_RETURN(EINVAL, ROUNDTRIP_FAILED,
                            "Content-Length and Transfer-Encoding conflicted");
        }
        op->req.headers.insert("User-Agent", USERAGENT);
        op->req.headers.insert("Connection", "keep-alive");
        op->req.headers.merge(m_common_headers);
        if (m_cookie_jar && m_cookie_jar->set_cookies_to_headers(&op->req) != 0)
            LOG_ERROR_RETURN(0, -1, "set_cookies_to_headers failed");
        Timeout tmo(std::min(op->timeout.timeout(), m_timeout));
        int retry = 0, followed = 0, ret = 0;
        uint64_t sleep_interval = 0;
        while (followed <= op->follow && retry <= op->retry && tmo.timeout() != 0) {
            ret = do_roundtrip(op, tmo);
            if (ret == ROUNDTRIP_SUCCESS || ret == ROUNDTRIP_FAILED) break;
            switch (ret) {
                case ROUNDTRIP_NEED_RETRY:
                    photon::thread_usleep(sleep_interval * 1000UL);
                    sleep_interval = (sleep_interval + 500) * 2;
                    ++retry;
                    break;
                case ROUNDTRIP_REDIRECT:
                    retry = 0;
                    ++followed;
                    break;
                default:
                    break;
            }
            if (tmo.timeout() == 0)
                LOG_ERROR_RETURN(ETIMEDOUT, -1, "connection timedout");
            if (followed > op->follow || retry > op->retry)
                LOG_ERROR_RETURN(ENOENT, -1,  "connection failed");
        }
        if (ret != ROUNDTRIP_SUCCESS) LOG_ERROR_RETURN(0, -1,"too many retry, roundtrip failed");
        return 0;
    }

    ISocketStream* native_connect(std::string_view host, uint16_t port, bool secure, uint64_t timeout) override {
        return m_dialer.dial(host, port, secure, timeout);
    }

    CommonHeaders<>* common_headers() override {
        return &m_common_headers;
    }
};

Client* new_http_client(ICookieJar *cookie_jar) { return new ClientImpl(cookie_jar); }

} // namespace http
} // namespace net
} // namespace photon
