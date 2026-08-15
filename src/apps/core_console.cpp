#include <algorithm>
#include <string>
#include <vector>

#include "../audio/sfx.h"
#include "../core/shell.h"
#include "../net/action_ids.generated.h"
#include "../net/host_async.h"
#include "../net/mazhost.h"
#include "apps.h"
#include "common.h"

namespace maz {
namespace apps {

using namespace theme;

namespace {

constexpr const char* ACTION_LABELS[] = {"GIT STATUS", "TESTS", "BUILD", "GIT FETCH", "PULL FF-ONLY"};
constexpr const char* ACTION_IDS[] = {
    action_ids::CORE_GIT_STATUS,
    action_ids::CORE_TESTS,
    action_ids::CORE_BUILD,
    action_ids::CORE_GIT_FETCH,
    action_ids::CORE_GIT_PULL_FF,
};
constexpr int ACTION_COUNT = sizeof(ACTION_IDS) / sizeof(ACTION_IDS[0]);

class CoreConsoleApp final : public App {
public:
    const char* id() const override { return "core"; }
    const char* title() const override { return "MAZ CORE"; }
    const char* hints() const override {
        if (_view == View::Result) return _jobReq ? "checking job...  ESC actions" : "R refresh job  ESC actions";
        if (_view == View::Actions) return _jobReq ? "starting...  ESC projects" : "ENTER run  ESC projects";
        return (_statusReq || _projectsReq) ? "refreshing...  ESC back" : "R refresh  ENTER project  ESC back";
    }

    void onEnter() override { refresh(); }
    void onExit() override {
        if (_statusReq) host_async::cancel(_statusReq);
        if (_projectsReq) host_async::cancel(_projectsReq);
        if (_jobReq) host_async::cancel(_jobReq);
    }

    bool onKey(const KeyEvent& e) override {
        if (!e.down) return false;
        if (_view == View::Result) {
            if (e.code == KEY_R && !_jobReq) { requestJobPoll(); return true; }
            if (e.code == KEY_ESC) { _view = View::Actions; invalidate(); return true; }
            return false;
        }
        if (_view == View::Actions) {
            if (e.code == KEY_ESC) { _view = View::Projects; invalidate(); return true; }
            if (_cursor.onKey(e, ACTION_COUNT)) { sfx::select(); invalidate(); return true; }
            if (e.code == KEY_ENTER && !_jobReq) { startAction(_cursor.sel); return true; }
            return false;
        }
        if (e.code == KEY_R && !_statusReq && !_projectsReq) { refresh(); return true; }
        if (_cursor.onKey(e, static_cast<int>(_projects.size()))) { sfx::select(); invalidate(); return true; }
        if (e.code == KEY_ENTER && !_projects.empty()) {
            _project = _projects[_cursor.sel].name;
            _cursor.sel = _cursor.first = 0;
            _view = View::Actions;
            sfx::confirm();
            invalidate();
            return true;
        }
        return false;
    }

    void update() override {
        pumpRefresh();
        pumpJobRequest();
        if (_view == View::Result && !_job.id.empty() && _job.state != "done" &&
            !_jobReq && millis() - _lastPoll > 1500)
            requestJobPoll();
    }

    void render(M5Canvas& g) override {
        g.fillScreen(BG);
        if (_view == View::Result) { renderResult(g); return; }
        if (_view == View::Actions) { renderActions(g); return; }
        renderProjects(g);
    }

private:
    enum class View : uint8_t { Projects, Actions, Result };
    enum class JobRequest : uint8_t { None, Start, Poll };

    void refresh() {
        _error.clear();
        _statusReq = host_async::coreStatus();
        _projectsReq = host_async::coreProjects();
        if (!_statusReq || !_projectsReq) _error = "Host queue busy";
        _cursor.sel = _cursor.first = 0;
        _view = View::Projects;
        invalidate();
    }

    void pumpRefresh() {
        if (_statusReq) {
            host_async::Result result;
            if (host_async::poll(_statusReq, result)) {
                _statusReq = 0;
                if (!result.cancelled) {
                    _status = result.coreStatus;
                    if (!_status.ok && !result.error.empty()) _error = result.error;
                }
                invalidate();
            }
        }
        if (_projectsReq) {
            host_async::Result result;
            if (host_async::poll(_projectsReq, result)) {
                _projectsReq = 0;
                if (!result.cancelled) {
                    _projects = result.projects;
                    if (!result.error.empty()) _error = result.error;
                    _cursor.clamp(static_cast<int>(_projects.size()));
                }
                invalidate();
            }
        }
    }

    void renderProjects(M5Canvas& g) {
        std::string right = (_statusReq || _projectsReq) ? "REFRESHING" :
                            (_status.ok ? (_status.ollama ? "AI + PC" : "PC / AI OFF") : "OFFLINE");
        ui::header(g, "MAZ CORE", right.c_str());
        if (!_status.ok && !_statusReq) {
            line(g, 0, _error.empty() ? _status.error.c_str() : _error.c_str(), WARN);
            line(g, 2, "Configure CONTROL > MAZ CORE", DIM);
            line(g, 3, "then press R to refresh", DIM);
            return;
        }
        if (_statusReq || _projectsReq) {
            line(g, 0, "Reading PC state...", ACCENT);
            line(g, 2, "Keyboard + UI remain live", DIM);
        } else {
            char meta[96];
            snprintf(meta, sizeof(meta), "%s / %d projects", _status.hostname.c_str(), _status.projects);
            line(g, 0, meta, ACCENT);
        }
        if (_projects.empty()) {
            if (!_projectsReq) line(g, 3, _error.empty() ? "No projects found" : _error.c_str(), WARN);
            return;
        }
        constexpr int visible = 4;
        if (_cursor.sel >= _cursor.first + visible) _cursor.first = _cursor.sel - visible + 1;
        if (_cursor.sel < _cursor.first) _cursor.first = _cursor.sel;
        for (int row = 0; row < visible; ++row) {
            const int idx = _cursor.first + row;
            if (idx >= static_cast<int>(_projects.size())) break;
            char sub[32];
            snprintf(sub, sizeof(sub), "%s%s%d", _projects[idx].branch.c_str(),
                     _projects[idx].dirty ? " *" : " ", _projects[idx].dirty);
            ui::listRow(g, row + 2, idx == _cursor.sel, _projects[idx].name.c_str(), sub);
        }
        ui::scrollBar(g, static_cast<int>(_projects.size()), _cursor.first, visible);
    }

    void renderActions(M5Canvas& g) {
        ui::header(g, _project.c_str(), _jobReq ? "STARTING" : "CORE ACTIONS");
        for (int row = 0; row < ACTION_COUNT; ++row)
            ui::listRow(g, row + 1, row == _cursor.sel, ACTION_LABELS[row],
                        row >= 1 && row <= 2 ? "job" : "safe");
    }

    void startAction(int idx) {
        if (idx < 0 || idx >= ACTION_COUNT || _jobReq) return;
        _result.clear();
        _job = {};
        _jobReq = host_async::coreStartJob(ACTION_IDS[idx], _project);
        _jobRequestKind = JobRequest::Start;
        if (!_jobReq) {
            _result = "Host queue busy";
            _jobRequestKind = JobRequest::None;
        }
        _view = View::Result;
        _lastPoll = millis();
        sfx::confirm();
        invalidate();
    }

    void requestJobPoll() {
        _lastPoll = millis();
        if (_jobReq || _job.id.empty()) { invalidate(); return; }
        _jobReq = host_async::coreJob(_job.id);
        _jobRequestKind = JobRequest::Poll;
        if (!_jobReq) {
            _result = "Host queue busy";
            _jobRequestKind = JobRequest::None;
        }
        invalidate();
    }

    void pumpJobRequest() {
        if (!_jobReq) return;
        host_async::Result result;
        if (!host_async::poll(_jobReq, result)) return;
        _jobReq = 0;
        const auto kind = _jobRequestKind;
        _jobRequestKind = JobRequest::None;
        if (result.cancelled) return;
        _job = result.coreJob;
        if (!_job.error.empty() && _job.id.empty()) _result = _job.error;
        if (_job.state == "done") {
            _result = !_job.output.empty() ? _job.output : (_job.ok ? "Completed" : _job.error);
            sfx::confirm();
        } else if (kind == JobRequest::Start && !_job.id.empty()) {
            _lastPoll = millis();
        }
        invalidate();
    }

    void renderResult(M5Canvas& g) {
        std::string right = _jobReq ? (_jobRequestKind == JobRequest::Start ? "STARTING" : "CHECKING") :
                            (_job.state.empty() ? (_result.empty() ? "JOB" : "ERROR") : _job.state);
        ui::header(g, _project.c_str(), right.c_str());
        std::string heading = _job.action.empty() ? "CORE JOB" : _job.action;
        line(g, 0, heading.c_str(), ACCENT);
        if (_jobReq || (_job.state != "done" && !_job.id.empty())) {
            line(g, 2, _jobReq ? "Talking to Core..." : "Running on PC...", TEXT);
            line(g, 3, "You can leave this screen.", DIM);
            line(g, 4, "No network call blocks UI.", DIM);
            return;
        }
        const std::string text = !_result.empty() ? _result : (!_job.error.empty() ? _job.error : "No output");
        size_t pos = 0;
        int row = 1;
        while (pos < text.size() && row < 6) {
            size_t end = text.find('\n', pos);
            if (end == std::string::npos) end = text.size();
            std::string part = text.substr(pos, end - pos);
            if (part.size() > 37) part.resize(37);
            line(g, row++, part.c_str(), _job.ok ? TEXT : WARN);
            pos = end + 1;
        }
    }

    static void line(M5Canvas& g, int row, const char* text, uint16_t color) {
        g.setFont(&fonts::Font0);
        g.setTextDatum(top_left);
        g.setTextColor(color, BG);
        g.drawString(text, PAD, BODY_Y + 17 + row * 15);
    }

    View _view = View::Projects;
    ListCursor _cursor;
    host::CoreStatus _status;
    std::vector<host::CoreProject> _projects;
    host::CoreJob _job;
    std::string _project;
    std::string _error;
    std::string _result;
    uint32_t _lastPoll = 0;
    uint32_t _statusReq = 0;
    uint32_t _projectsReq = 0;
    uint32_t _jobReq = 0;
    JobRequest _jobRequestKind = JobRequest::None;
};

}  // namespace

App* makeCoreConsole() { return new CoreConsoleApp(); }

}  // namespace apps
}  // namespace maz
