#include <string>

#include "../audio/sfx.h"
#include "../core/notify.h"
#include "../net/host_worker.h"
#include "apps.h"
#include "common.h"

namespace maz {
namespace apps {

using namespace theme;

namespace {

enum class ActionView : uint8_t { Input, Proposing, Confirm, Running, Result };

class PcActionsApp final : public App {
public:
    const char* id() const override { return "pcactions"; }
    const char* title() const override { return "PC ACTION"; }
    const char* hints() const override {
        if (_view == ActionView::Input) return "type or Ctrl+SPACE   ENTER propose";
        if (_view == ActionView::Confirm) return "ENTER confirm   ESC cancel";
        if (_view == ActionView::Result) return "N new   ESC control";
        return "MAZ Core working   ESC safe";
    }

    void onEnter() override {
        consume();
        if (_view == ActionView::Result || _view == ActionView::Confirm) return;
        _request.text.clear();
        _request.limit = 120;
        _message.clear();
        _proposalId.clear();
        _view = ActionView::Input;
        invalidate();
    }

    bool onKey(const KeyEvent& e) override {
        if (!e.down) return false;
        if (_view == ActionView::Input) {
            if (e.code == KEY_ENTER) { propose(); return true; }
            if (_request.onKey(e)) { invalidate(); return true; }
        } else if (_view == ActionView::Confirm && e.code == KEY_ENTER) {
            confirm();
            return true;
        } else if (_view == ActionView::Result && e.code == KEY_N) {
            _view = ActionView::Input;
            _request.text.clear();
            _message.clear();
            _proposalId.clear();
            invalidate();
            return true;
        }
        return false;
    }

    void update() override {
        if (_view == ActionView::Proposing || _view == ActionView::Running) consume();
        if (host_worker::busy()) invalidate();
    }

    std::string contextSnapshot() const override {
        if (_view == ActionView::Confirm) return "PC action awaiting physical confirmation: " + _message;
        return "PC action request; cloud proposes only, Core validates, ENTER confirms";
    }

    void render(M5Canvas& g) override {
        g.fillScreen(BG);
        ui::header(g, "PC ACTION", _view == ActionView::Confirm ? "CONFIRM" : "CLOUD -> CORE");
        g.setFont(&fonts::Font0);
        g.setTextDatum(top_left);
        if (_view == ActionView::Input) {
            g.setTextColor(DIM, BG);
            g.drawString("Ask for YouTube or a new text note", PAD, BODY_Y + 18);
            _request.draw(g, PAD, BODY_Y + 39, SCREEN_W - PAD * 2, "open youtube...");
            g.drawString("Nothing runs until ENTER confirms", PAD, BODY_Y + 75);
            return;
        }
        if (_view == ActionView::Proposing || _view == ActionView::Running) {
            g.setTextColor(ACCENT, BG);
            g.setFont(&fonts::Font2);
            g.drawString(host_worker::stateName(), PAD, BODY_Y + 34);
            g.setFont(&fonts::Font0);
            g.setTextColor(DIM, BG);
            g.drawString(_view == ActionView::Proposing ? "cloud interpreting request" : "Core dispatching action", PAD, BODY_Y + 60);
            return;
        }
        g.setTextColor(_ok ? OK : WARN, BG);
        g.drawString(_view == ActionView::Confirm ? "PROPOSED / CHECK IT" : (_ok ? "RECEIPT" : "NOT RUN"), PAD, BODY_Y + 17);
        drawWrapped(g, _message, BODY_Y + 37);
    }

private:
    static void drawWrapped(M5Canvas& g, const std::string& text, int y) {
        constexpr size_t width = 37;
        g.setTextColor(TEXT, BG);
        for (int row = 0; row < 5; ++row) {
            size_t start = static_cast<size_t>(row) * width;
            if (start >= text.size()) break;
            g.drawString(text.substr(start, width).c_str(), PAD, y + row * 14);
        }
    }

    void propose() {
        if (_request.text.empty() || host_worker::busy()) return;
        if (!host_worker::submitPcProposal(_session, _request.text)) {
            notify::post(Note::Error, "Could not propose", host_worker::stateName());
            return;
        }
        _view = ActionView::Proposing;
        _ok = false;
        invalidate();
    }

    void confirm() {
        if (_proposalId.empty() || host_worker::busy()) return;
        if (!host_worker::submitPcConfirm(_session, _proposalId)) {
            notify::post(Note::Error, "Could not confirm", host_worker::stateName());
            return;
        }
        _view = ActionView::Running;
        invalidate();
    }

    void consume() {
        if (host_worker::state() != host_worker::State::Done) return;
        const auto kind = host_worker::jobKind();
        if (kind != host_worker::JobKind::PcPropose && kind != host_worker::JobKind::PcConfirm) return;
        host_worker::PcProposalResult result;
        if (!host_worker::takePcProposalResult(result)) return;
        _ok = result.reply.ok;
        if (kind == host_worker::JobKind::PcPropose && result.reply.ok && !result.reply.proposalId.empty()) {
            _session = result.session;
            _proposalId = result.reply.proposalId;
            _message = result.reply.preview;
            _view = ActionView::Confirm;
        } else {
            _message = result.reply.ok ? result.reply.text : result.reply.error;
            if (_message.empty()) _message = result.reply.ok ? "launch requested" : "action unavailable";
            _view = ActionView::Result;
        }
        sfx::confirm();
        invalidate();
    }

    TextField _request;
    ActionView _view = ActionView::Input;
    std::string _session;
    std::string _proposalId;
    std::string _message;
    bool _ok = false;
};

}  // namespace

App* makePcActions() { return new PcActionsApp(); }

}  // namespace apps
}  // namespace maz
