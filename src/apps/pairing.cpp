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
    const char* hints() const override { return "P hide/show token   ESC back"; }

    bool onKey(const KeyEvent& e) override {
        if (!e.down) return false;
        if (e.code == KEY_P) {
            _show = !_show;
            invalidate();
            return true;
        }
        return false;
    }

    std::string contextSnapshot() const override {
        return std::string("MAZ pairing token ID ") + tokenId() +
               "; phone control is MAZ Core /control";
    }

    void render(M5Canvas& g) override {
        g.fillScreen(BG);
        ui::header(g, "PAIRING + PHONE", host::linkName());
        g.setFont(&fonts::Font0);
        g.setTextColor(DIM, BG);
        g.drawString("TOKEN ID", PAD, BODY_Y + 15);
        g.setTextColor(ACCENT, BG);
        g.drawString(tokenId().c_str(), 80, BODY_Y + 15);

        g.setTextColor(DIM, BG);
        g.drawString("PAIRING TOKEN", PAD, BODY_Y + 34);
        const std::string shown = Cfg.hostToken.empty()
            ? "NOT SET"
            : (_show ? Cfg.hostToken : std::string(Cfg.hostToken.size(), '*'));
        g.setTextColor(_show ? TEXT : DIM, BG);
        constexpr size_t W = 36;
        g.drawString(shown.substr(0, W).c_str(), PAD, BODY_Y + 50);
        if (shown.size() > W)
            g.drawString(shown.substr(W, W).c_str(), PAD, BODY_Y + 64);

        g.setTextColor(DIM, BG);
        g.drawString("PHONE APPROVALS", PAD, BODY_Y + 84);
        std::string url;
        if (!Cfg.hostRemoteUrl.empty()) {
            url = Cfg.hostRemoteUrl;
            while (!url.empty() && url.back() == '/') url.pop_back();
            url += "/control/";
        } else if (!Cfg.hostAddr.empty()) {
            url = "http://" + Cfg.hostAddr + ":" + std::to_string(Cfg.hostPort) + "/control/";
        } else {
            url = "Configure MAZ Core first";
        }
        g.setTextColor(ACCENT2, BG);
        g.drawString(ui::ellipsis(url, 37).c_str(), PAD, BODY_Y + 99);
    }

private:
    bool _show = false;
};

}  // namespace

App* makePairing() { return new PairingApp(); }

}  // namespace apps
}  // namespace maz
