#include <mbedtls/sha256.h>

#include <WiFi.h>

#include <cctype>
#include <string>

#include "../core/notify.h"
#include "../core/settings.h"
#include "../core/shell.h"
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
        return std::string("MAZ pairing token ID ") + tokenId() +
               "; short-lived phone pairing is MAZ Core /pair";
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
            url = "Configure MAZ Core first";
        }
        g.setTextColor(DIM, BG);
        g.drawString("OPEN ON PHONE", PAD, BODY_Y + 87);
        g.setTextColor(ACCENT2, BG);
        g.drawString(ui::ellipsis(url, 37).c_str(), PAD, BODY_Y + 101);
    }

private:
    host::PairCode _pair;
};

// CONNECT TO PC: the PC shows an 8-character code (nod-setup); the device finds Core by itself,
// the user types the code, Core hands back the long key which is stored in flash and never shown.
class ConnectPcApp final : public App {
public:
    const char* id() const override { return "connectpc"; }
    const char* title() const override { return "CONNECT TO PC"; }
    const char* hints() const override {
        switch (_state) {
            case State::Enter: return "type code  ENTER connect  ESC back";
            case State::Error: return "ENTER try again  ESC back";
            case State::Done:  return "ENTER done";
            default:           return "ESC cancel";
        }
    }

    void onEnter() override { begin(); }

    bool onKey(const KeyEvent& e) override {
        if (!e.down) return false;
        if (e.code == KEY_ESC) return false;  // shell pops the screen
        switch (_state) {
            case State::Error:
                if (e.code == KEY_ENTER) { begin(); return true; }
                return false;
            case State::Done:
                if (e.code == KEY_ENTER) { shell::pop(); return true; }
                return false;
            case State::Enter:
                if (e.code == KEY_BACKSPACE) {
                    if (!_code.empty()) _code.pop_back();
                    _msg.clear();
                    invalidate();
                    return true;
                }
                if (e.code == KEY_ENTER) {
                    if (_code.size() == 8) { _state = State::Claim; _at = millis(); invalidate(); }
                    else { _msg = "The code has 8 characters"; invalidate(); }
                    return true;
                }
                if (e.ch >= 32 && e.ch < 127 && !e.ctrl() && !e.fn()) {
                    const char c = static_cast<char>(toupper(e.ch));
                    if (isalnum(static_cast<unsigned char>(c)) && _code.size() < 8) {
                        _code.push_back(c);
                        _msg.clear();
                        invalidate();
                    }
                    return true;
                }
                return false;
            default:
                return false;
        }
    }

    void update() override {
        if (_state == State::Find) {
            if (_finder.step()) {
                if (!_finder.found.empty()) { _addr = _finder.found; _state = State::Enter; }
                else fail("Core not found", "Is nod-setup done on the PC, same Wi-Fi?");
            }
            invalidate();
        } else if (_state == State::Claim && millis() - _at > 80) {
            using host::ClaimResult;
            switch (host::pairClaim(_addr, _finder.port, _code)) {
                case ClaimResult::Ok:
                    _state = State::Done;
                    notify::post(Note::Success, "Connected to PC", _addr);
                    break;
                case ClaimResult::WrongCode:
                    _code.clear();
                    _state = State::Enter;
                    _msg = "Wrong code - or expired. Rerun nod-setup";
                    break;
                case ClaimResult::TooMany:
                    fail("Too many tries", "Wait a minute, then try again");
                    break;
                case ClaimResult::Refused:
                    fail("PC refused the code", "Use the same home Wi-Fi as the PC");
                    break;
                case ClaimResult::NotFound:
                    fail("Core not found", "PC asleep? Run nod-setup again");
                    break;
                default:
                    fail("Unexpected reply from PC", "Rerun nod-setup, try again");
                    break;
            }
            invalidate();
        }
    }

    void render(M5Canvas& g) override {
        g.fillScreen(BG);
        ui::header(g, "CONNECT TO PC", nullptr);
        g.setFont(&fonts::Font0);
        g.setTextDatum(top_left);
        if (_state == State::Find) {
            g.setTextColor(TEXT, BG);
            g.drawString("Looking for your PC...", PAD, BODY_Y + 20);
            g.drawRect(PAD, BODY_Y + 44, SCREEN_W - 2 * PAD, 10, DIM);
            g.fillRect(PAD + 1, BODY_Y + 45, (SCREEN_W - 2 * PAD - 2) * _finder.percent() / 100, 8, ACCENT);
            g.setTextColor(DIM, BG);
            g.drawString("Same Wi-Fi as the PC. nod-setup must be done.", PAD, BODY_Y + 66);
            return;
        }
        if (_state == State::Error) {
            g.setTextColor(WARN, BG);
            g.drawString(_title.c_str(), PAD, BODY_Y + 24);
            g.setTextColor(TEXT, BG);
            g.drawString(ui::ellipsis(_detail, 38).c_str(), PAD, BODY_Y + 44);
            return;
        }
        if (_state == State::Done) {
            g.setTextColor(OK, BG);
            g.drawString("Connected!", PAD, BODY_Y + 24);
            g.setTextColor(TEXT, BG);
            g.drawString(ui::ellipsis(_addr, 38).c_str(), PAD, BODY_Y + 44);
            return;
        }
        g.setTextColor(TEXT, BG);
        g.drawString("Enter code from your PC", PAD, BODY_Y + 14);
        g.setFont(&fonts::Font4);
        g.setTextColor(ACCENT, BG);
        std::string shown = _code;
        while (shown.size() < 8) shown.push_back('_');
        g.drawCentreString(shown.c_str(), SCREEN_W / 2, BODY_Y + 36);
        g.setFont(&fonts::Font0);
        g.setTextColor(_state == State::Claim ? ACCENT2 : WARN, BG);
        g.drawString(_state == State::Claim ? "Checking..." : ui::ellipsis(_msg, 38).c_str(), PAD, BODY_Y + 76);
        g.setTextColor(DIM, BG);
        g.drawString(("PC: " + _addr).c_str(), PAD, BODY_Y + 92);
    }

private:
    enum class State : uint8_t { Find, Enter, Claim, Error, Done };

    void begin() {
        _finder = host::FindCore();
        _code.clear();
        _msg.clear();
        _addr.clear();
        if (WiFi.status() != WL_CONNECTED) {
            fail("Wi-Fi is not connected", "Join Wi-Fi first: CONTROL > Wi-Fi");
            return;
        }
        _state = State::Find;
        invalidate();
    }

    void fail(const char* title, const char* detail) {
        _title = title;
        _detail = detail;
        _state = State::Error;
    }

    State          _state = State::Find;
    host::FindCore _finder;
    std::string    _code, _msg, _addr, _title, _detail;
    uint32_t       _at = 0;
};

}  // namespace

App* makePairing() { return new PairingApp(); }
App* makeConnectPc() { return new ConnectPcApp(); }

}  // namespace apps
}  // namespace maz
