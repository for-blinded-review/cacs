#include "common/photonlib/co.h"
#include "common/photonlib/utility.h"
#include "common/photonlib/net/http/client.h"
#include "common/photonlib/alog.h"

static constexpr size_t N = 1000000;

int main() {
    photon::init();
    DEFER(photon::fini());
    set_log_output_level(ALOG_INFO);
    auto cli = photon::net::http::new_http_client();
    auto start = std::chrono::high_resolution_clock::now();
    for (int i=0;i<N;i++) {
        photon::net::http::Client::OperationOnStack<32768> op(cli, photon::net::http::Verb::GET, "http://dummy");
        auto ret = cli->call(&op);
        if (ret < 0) {
            LOG_ERROR("http call failed");
        }
        if (op.status_code != 200) {
            LOG_ERROR("http call failed");
        }
        LOG_DEBUG(VALUE(op.resp.headers.content_length()));
        ret = op.resp.read(nullptr, op.resp.headers.content_length());
        if (ret != op.resp.headers.content_length())
            LOG_ERROR("content-length mismatch", VALUE(ret), VALUE(op.resp.headers.content_length()));
    }
    auto end = std::chrono::high_resolution_clock::now();
    LOG_INFO("req ` times used ` us", N, std::chrono::duration_cast<std::chrono::microseconds>(end - start).count());
    LOG_INFO("QPS `", N * 1.0 * 1000000 / std::chrono::duration_cast<std::chrono::microseconds>(end - start).count());
    return 0;
}
