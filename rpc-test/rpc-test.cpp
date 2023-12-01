#include "common/photonlib/alog.h"
#include "common/photonlib/co.h"
#include "common/photonlib/rpc/rpc.h"
#include "common/photonlib/rpc/serialize.h"
#include "common/photonlib/utility.h"

static constexpr size_t N = 10000000;
static constexpr ssize_t CSIZE = 16 * 1024;

struct P2PReadV {
    const static uint32_t IID = 1;
    const static uint32_t FID = 1;
    struct Request : public photon::rpc::Message {
        photon::rpc::string filename;
        photon::rpc::string domain;
        off_t offset;
        size_t count;
        photon::net::EndPoint id;
        int ttl;

        photon::rpc::array<photon::net::EndPoint>
            blacklist;  // try NOT to redirect me to these nodes

        PROCESS_FIELDS(filename, domain, offset, count, id, ttl, blacklist);
    };
    struct Response : public photon::rpc::Message {
        ssize_t ret;  // size actually read, or -errno in case of error;
                      // ret == -EAGAIN indicates server currently has no data,
                      //     and the client should try again some time later;
                      // ret == -EBUSY indicates redirection;
        photon::net::EndPoint serverid;   // server id
        photon::net::EndPoint redirect2;  // redirect to this node, if needed

        photon::rpc::aligned_iovec_array
            buf;  // may contain error string in case of error

        PROCESS_FIELDS(ret, serverid, redirect2, buf);
    };
};

class FakeSocket : public photon::net::ISocketStream,
                   public photon::net::ISocketClient {
public:
    FakeSocket() {}

    ~FakeSocket() {}

    virtual Object* get_underlay_object(uint64_t recursion = 0) override {
        return nullptr;
    }
    virtual int getsockname(photon::net::EndPoint& addr) override {
        errno = ENOSYS;
        return -1;
    }
    virtual int getpeername(photon::net::EndPoint& addr) override {
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
    virtual ssize_t
    read(void* buf, size_t count) override {
        bool copy = buf;
        photon::thread_yield();
        for (ssize_t x = count; x > CSIZE; x -= CSIZE)
            photon::thread_yield();
        return count;
    }
#if defined(CACSTH) && !defined(CACSYIELD)
    __attribute__((preserve_none))
#endif
    virtual ssize_t
    readv(const struct iovec* iov, int iovcnt) override {
        // bool copy = iov->iov_base;
        iovector_view v((struct iovec*)iov, iovcnt);
        auto count = v.sum();
        photon::thread_yield();
        for (ssize_t x = count; x > CSIZE; x -= CSIZE)
            photon::thread_yield();
        return count;
    }
#if defined(CACSTH) && !defined(CACSYIELD)
    __attribute__((preserve_none))
#endif
    virtual ssize_t
    write(const void* buf, size_t count) override {
        photon::thread_yield();
        for (ssize_t x = count; x > CSIZE; x -= CSIZE)
            photon::thread_yield();
        return count;
    }
#if defined(CACSTH) && !defined(CACSYIELD)
    __attribute__((preserve_none))
#endif
    virtual ssize_t
    writev(const struct iovec* iov, int iovcnt) override {
        // bool copy = iov->iov_base;
        iovector_view v((struct iovec*)iov, iovcnt);
        auto count = v.sum();
        photon::thread_yield();
        for (ssize_t x = count; x > CSIZE; x -= CSIZE)
            photon::thread_yield();
        return count;
    }
    // virtual int close() override { return stream->close(); }
#if defined(CACSTH) && !defined(CACSYIELD)
    __attribute__((preserve_none))
#endif
    virtual int
    close() override {
        return 0;
    }
#if defined(CACSTH) && !defined(CACSYIELD)
    __attribute__((preserve_none))
#endif
    virtual ssize_t
    send(const void* buf, size_t count, int flags = 0) override {
        photon::thread_yield();
        for (ssize_t x = count; x > CSIZE; x -= CSIZE)
            photon::thread_yield();
        return count;
    }
#if defined(CACSTH) && !defined(CACSYIELD)
    __attribute__((preserve_none))
#endif
    virtual ssize_t
    send(const struct iovec* iov, int iovcnt, int flags = 0) override {
        photon::thread_yield();
        iovector_view v((struct iovec*)iov, iovcnt);
        auto count = v.sum();
        for (ssize_t x = count; x > CSIZE; x -= CSIZE)
            photon::thread_yield();
        return count;
    }
#if defined(CACSTH) && !defined(CACSYIELD)
    __attribute__((preserve_none))
#endif
    virtual ssize_t
    recv(void* buf, size_t count, int flags = 0) override {
        photon::thread_yield();
        for (ssize_t x = count; x > CSIZE; x -= CSIZE)
            photon::thread_yield();
        return count;
    }
#if defined(CACSTH) && !defined(CACSYIELD)
    __attribute__((preserve_none))
#endif
    virtual ssize_t
    recv(const struct iovec* iov, int iovcnt, int flags = 0) override {
        photon::thread_yield();
        iovector_view v((struct iovec*)iov, iovcnt);
        auto count = v.sum();
        for (ssize_t x = count; x > CSIZE; x -= CSIZE)
            photon::thread_yield();
        return count;
    }
#if defined(CACSTH) && !defined(CACSYIELD)
    __attribute__((preserve_none))
#endif
    virtual ssize_t
    sendfile(int in_fd, off_t offset, size_t count) override {
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
    virtual ISocketStream* connect(
        photon::net::EndPoint remote,
        photon::net::EndPoint local = photon::net::EndPoint()) override {
        return new FakeSocket();
    }
    // Connect to a Unix Domain Socket.
    virtual ISocketStream* connect(const char* path,
                                   size_t count = 0) override {
        return new FakeSocket();
    }
};

int main() {
    photon::init();
    DEFER(photon::fini());
    set_log_output_level(ALOG_INFO);
    P2PReadV::Request req;
    P2PReadV::Response resp;
    IOVector iov;
    iov.push_back(1024 * 1024);
    resp.buf.assign(iov.iovec(), iov.iovcnt());
    resp.redirect2 = {};
    resp.ret = iov.sum();
    resp.serverid = {};
    auto stream = new FakeSocket();
    DEFER(delete stream);
    auto stub = photon::rpc::new_rpc_stub(stream);
    DEFER(delete stub);
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < N; i++) {
        req.id = {};
        req.filename = "WTF";
        req.blacklist = {};
        req.domain = "";
        req.ttl = 255;
        req.offset = 0;
        req.count = 1024 * 1024;
        auto ret = stub->call<P2PReadV>(req, resp);
        if (ret < 0) {
            LOG_ERROR("rpc call failed");
        }
    }
    auto end = std::chrono::high_resolution_clock::now();
    LOG_INFO("req ` times used ` us", N,
             std::chrono::duration_cast<std::chrono::microseconds>(end - start)
                 .count());
    LOG_INFO("QPS `", N * 1.0 * 1000000 /
                          std::chrono::duration_cast<std::chrono::microseconds>(
                              end - start)
                              .count());
    return 0;
}
