#pragma once
// HTTPS to the remote (Tailscale Funnel) Core URL, verified against an embedded
// CA bundle (src/net/ca_bundle.bin, built by scripts/gen_ca_bundle.py). Never
// setInsecure(): the bearer token must not reach an unverified TLS peer.
//
// NodHttp owns its WiFiClientSecure per request. The TLS context (~40 KB heap
// while connected) is allocated on connect and freed by HTTPClient::end() /
// destruction at scope exit; base-class order guarantees HTTPClient (which
// stops the socket) is destroyed before the TLS client.
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

extern const uint8_t nodCaBundle[] asm("_binary_src_net_ca_bundle_bin_start");

namespace maz {
namespace host {

struct TlsHolder {
    WiFiClientSecure tls;
};

struct NodHttp : TlsHolder, HTTPClient {
    bool open(const String& url, bool remote) {
        if (!remote) return HTTPClient::begin(url);
        tls.setCACertBundle(nodCaBundle);
        return HTTPClient::begin(tls, url);
    }
};

}  // namespace host
}  // namespace maz
