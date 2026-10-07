#include <mbedtls/sha256.h>

#include <string>

#include "../core/settings.h"
#include "../net/mazhost.h"
#include "apps.h"
#include "common.h"

namespace maz {
namespace apps {

using namespace theme;

namespace {

std::string tokenId() {
    if (Cfg.hostToken.empty()) return "UNPAIRED";
    unsigned char digest[32] = {};
    mbedtls_sha256_context ctx;
    mbedtls_sha256_init(&ctx);
    if (mbedtls_sha256_starts_ret(&ctx, 0) != 0 ||
        mbedtls_sha256_update_ret(&ctx,
            reinterpret_cast<const unsigned char*>(Cfg.hostToken.data()),
            Cfg.hostToken.size()) != 0 ||
        mbedtls_sha256_finish_ret(&ctx, digest) != 0) {
        mbedtls_sha256_free(&ctx);
        return "ERROR";
    }
    mbedtls_sha256_free(&ctx);
    static const char H[] = "0123456789abcdef";
    char out[13];
    for (int i = 0; i < 6; ++i) {
        out[i * 2] = H[(digest[i] >> 4) & 0xF];
        out[i * 2 + 1] = H[digest[i] & 0xF];
    }
    out[12] = 0;
    return out;
}

class PairingApp final : public App {
public:
    const char* id() const override { return "pairing"; }
    const char* title() const override { return "PAIRING + PHONE"; }
    const char* hints() const override { return "ENTER pair device   ESC back"; }

    bool onKey(const KeyEvent& e) override {
        if (!e.down) return false;
        if (e.code == KEY_ENTER) {
            _pair = host::startPairing();
            invalidate();
            return true;
        }
        return false;
    }

    std::string contextSnapshot() const override {
        return std::string("nod pairing token ID ") + tokenId() +
               "; short-lived phone pairing is hub /pair";
    }

    void render(M5Canvas& g) override {
        g.fillScreen(BG);
        ui::header(g, "PAIRING + PHONE", host::linkName());
        g.setFont(&fonts::Font0);
        g.setTextColor(DIM, BG);
        g.drawString("TOKEN ID", PAD, BODY_Y + 15);
        g.setTextColor(ACCENT, BG);
        g.drawString(tokenId().c_str(), 80, BODY_Y + 15);

        g.setTextColor(ACCENT2, BG);
        g.drawString("PAIR DEVICE", PAD, BODY_Y + 35);
        if (_pair.ok) {
            g.setFont(&fonts::Font2);
            g.setTextColor(TEXT, BG);
            g.drawCentreString(_pair.code.c_str(), 120, BODY_Y + 48);
            g.setFont(&fonts::Font0);
            g.setTextColor(DIM, BG);
            g.drawString("ONE USE  -  EXPIRES IN 5 MIN", PAD, BODY_Y + 70);
        } else {
            g.setTextColor(_pair.error.empty() ? TEXT : WARN, BG);
            g.drawString(ui::ellipsis(_pair.error.empty() ? "Press ENTER for a temporary code" : _pair.error, 38).c_str(), PAD, BODY_Y + 52);
        }

        std::string url;
        if (!Cfg.hostRemoteUrl.empty()) {
            url = Cfg.hostRemoteUrl;
            while (!url.empty() && url.back() == '/') url.pop_back();
            url += "/pair/";
        } else if (!Cfg.hostAddr.empty()) {
            url = "http://" + Cfg.hostAddr + ":" + std::to_string(Cfg.hostPort) + "/pair/";
        } else {
            url = "Configure hub first";
        }
        g.setTextColor(DIM, BG);
        g.drawString("OPEN ON PHONE", PAD, BODY_Y + 87);
        g.setTextColor(ACCENT2, BG);
        g.drawString(ui::ellipsis(url, 37).c_str(), PAD, BODY_Y + 101);
    }

private:
    host::PairCode _pair;
};

}  // namespace

App* makePairing() { return new PairingApp(); }

}  // namespace apps
}  // namespace maz
