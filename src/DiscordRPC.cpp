#include "DiscordRPC.h"
#include <QMutex>
#include <QMutexLocker>
#include <cstring>
#include <ctime>
#include <string>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <poll.h>
#include <fcntl.h>
#include <errno.h>
#include <vector>
#endif

class DiscordRPCWorker : public QObject {
    Q_OBJECT
public:
    explicit DiscordRPCWorker(QObject *parent = nullptr) : QObject(parent) {}

    void setParams(int64_t clientId) {
        m_appId     = std::to_string(clientId);
        m_startTime = (int64_t)time(nullptr);
    }

    void updateActivity(const QString &state, const QString &details) {
        QMutexLocker lock(&m_mutex);
        m_state   = state.toUtf8().left(127).toStdString();
        m_details = details.toUtf8().left(127).toStdString();
        m_dirty   = true;
        m_clear   = false;
        m_wake    = true;
    }

    void clearActivity() {
        QMutexLocker lock(&m_mutex);
        m_clear = true;
        m_dirty = true;
        m_wake  = true;
    }

    void stop() {
        m_stop = true;
    }

    void setEnabled(bool on) {
        m_enabled = on;
        QMutexLocker lock(&m_mutex);
        m_dirty = true;
        m_clear = false;
        m_wake = true;
    }

public slots:
    void run() {
        while (!m_stop) {
            if (!m_enabled) {
                if (connected()) {
                    sendClear();
                    disconnect();
                }
                pause(2);
                continue;
            }
            if (!connected()) {
                if (!connect() || !doHandshake()) {
                    disconnect();
                    pause(50);
                    continue;
                }
            }

            bool shouldClear = false;
            bool shouldSend  = false;
            std::string state, details;
            {
                QMutexLocker lock(&m_mutex);
                if (m_dirty) {
                    shouldClear = m_clear;
                    shouldSend  = !m_clear;
                    state   = m_state;
                    details = m_details;
                    m_dirty = false;
                }
            }

            bool ok = true;
            if (shouldClear)
                ok = sendClear();
            else if (shouldSend)
                ok = sendPresence(state, details);
            else
                ok = sendPresence(m_lastState, m_lastDetails);

            if (!ok) {
                disconnect();
                pause(10);
                continue;
            }

            if (shouldSend) {
                m_lastState   = state;
                m_lastDetails = details;
            }

            pause(150);
        }
        disconnect();
    }

private:
    std::string   m_appId;
    int64_t       m_startTime = 0;
    int           m_nonce     = 1;
    std::atomic<bool> m_stop{false};
    std::atomic<bool> m_enabled{true};
    std::atomic<bool> m_wake{false};

    void pause(int ticks) {
        for (int i = 0; i < ticks && !m_stop; i++) {
            if (m_wake.exchange(false)) return;
            QThread::msleep(100);
        }
    }

    QMutex      m_mutex;
    std::string m_state;
    std::string m_details;
    std::string m_lastState;
    std::string m_lastDetails;
    bool        m_dirty = false;
    bool        m_clear = false;

#ifdef _WIN32
    HANDLE m_pipe = INVALID_HANDLE_VALUE;
    bool connected() { return m_pipe != INVALID_HANDLE_VALUE; }
    bool connect() {
        for (int i = 0; i < 10; i++) {
            std::string path = "\\\\.\\pipe\\discord-ipc-" + std::to_string(i);
            m_pipe = CreateFileA(path.c_str(), GENERIC_READ | GENERIC_WRITE,
                                 0, nullptr, OPEN_EXISTING, 0, nullptr);
            if (m_pipe != INVALID_HANDLE_VALUE) return true;
        }
        return false;
    }
    void disconnect() {
        if (m_pipe != INVALID_HANDLE_VALUE) {
            CloseHandle(m_pipe);
            m_pipe = INVALID_HANDLE_VALUE;
        }
    }
    bool writeAll(const void *data, size_t len) {
        DWORD written = 0;
        return WriteFile(m_pipe, data, (DWORD)len, &written, nullptr) && written == (DWORD)len;
    }
    bool readAll(void *buf, size_t len) {
        int waited = 0;
        DWORD avail = 0;
        while (!m_stop) {
            if (!PeekNamedPipe(m_pipe, nullptr, 0, nullptr, &avail, nullptr)) return false;
            if (avail >= (DWORD)len) break;
            if (waited >= 3000) return false;
            Sleep(20);
            waited += 20;
        }
        if (m_stop) return false;
        DWORD got = 0;
        return ReadFile(m_pipe, buf, (DWORD)len, &got, nullptr) && got == (DWORD)len;
    }
#else
    int m_sock = -1;
    bool connected() { return m_sock >= 0; }
    bool connect() {
        std::vector<std::string> dirs;
        for (const char *name : { "XDG_RUNTIME_DIR", "TMPDIR", "TMP", "TEMP" }) {
            const char *v = getenv(name);
            if (v && *v) dirs.push_back(v);
        }
        dirs.push_back("/tmp");
        for (const std::string &dir : dirs) {
            for (int i = 0; i < 10; i++) {
                std::string path = dir + "/discord-ipc-" + std::to_string(i);
                int fd = socket(AF_UNIX, SOCK_STREAM, 0);
                if (fd < 0) continue;
                sockaddr_un addr{};
                addr.sun_family = AF_UNIX;
                strncpy(addr.sun_path, path.c_str(), sizeof(addr.sun_path) - 1);
                int fl = fcntl(fd, F_GETFL, 0);
                fcntl(fd, F_SETFL, fl | O_NONBLOCK);
                if (::connect(fd, (sockaddr *)&addr, sizeof(addr)) == 0) {
                    fcntl(fd, F_SETFL, fl);
#ifdef SO_NOSIGPIPE
                    int one = 1;
                    setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, &one, sizeof(one));
#endif
                    timeval tv{};
                    tv.tv_sec = 3;
                    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
                    m_sock = fd;
                    return true;
                }
                close(fd);
            }
        }
        return false;
    }
    void disconnect() {
        if (m_sock >= 0) { close(m_sock); m_sock = -1; }
    }
    bool writeAll(const void *data, size_t len) {
#ifdef MSG_NOSIGNAL
        const int flags = MSG_NOSIGNAL;
#else
        const int flags = 0;
#endif
        size_t sent = 0;
        while (sent < len) {
            ssize_t r = send(m_sock, (const char *)data + sent, len - sent, flags);
            if (r <= 0) return false;
            sent += (size_t)r;
        }
        return true;
    }
    bool readAll(void *buf, size_t len) {
        size_t got = 0;
        int waited = 0;
        while (got < len) {
            if (m_stop) return false;
            pollfd pfd{};
            pfd.fd = m_sock;
            pfd.events = POLLIN;
            int pr = poll(&pfd, 1, 100);
            if (pr < 0) return false;
            if (pr == 0) {
                waited += 100;
                if (waited >= 3000) return false;
                continue;
            }
            ssize_t r = recv(m_sock, (char *)buf + got, len - got, 0);
            if (r <= 0) return false;
            got += (size_t)r;
        }
        return true;
    }
#endif

    bool sendFrame(uint32_t op, const std::string &json) {
        uint32_t hdr[2] = { op, (uint32_t)json.size() };
        if (!writeAll(hdr, 8)) return false;
        return writeAll(json.data(), json.size());
    }

    bool readFrame(uint32_t &op, std::string &json) {
        uint32_t hdr[2];
        if (!readAll(hdr, 8)) return false;
        op = hdr[0];
        json.resize(hdr[1]);
        return hdr[1] == 0 || readAll(&json[0], hdr[1]);
    }

    bool doHandshake() {
        std::string hs = "{\"v\":1,\"client_id\":\"" + m_appId + "\"}";
        if (!sendFrame(0, hs)) return false;
        uint32_t op; std::string resp;
        return readFrame(op, resp);
    }

    std::string pid() {
#ifdef _WIN32
        return std::to_string((int)GetCurrentProcessId());
#else
        return std::to_string((int)getpid());
#endif
    }

    static std::string escape(const std::string &s) {
        std::string out;
        out.reserve(s.size());
        for (char c : s) {
            if (c == '"')  { out += "\\\""; }
            else if (c == '\\') { out += "\\\\"; }
            else { out += c; }
        }
        return out;
    }

    bool sendPresence(const std::string &state, const std::string &details) {
        std::string nonce = std::to_string(m_nonce++);
        std::string payload =
            "{\"cmd\":\"SET_ACTIVITY\",\"args\":{\"pid\":" + pid() +
            ",\"activity\":{"
            "\"state\":\"" + escape(state) + "\","
            "\"details\":\"" + escape(details) + "\","
            "\"timestamps\":{\"start\":" + std::to_string(m_startTime) + "},"
            "\"assets\":{\"large_image\":\"fgdbuilder\",\"large_text\":\"FGDBuilder\"}"
            "}},\"nonce\":\"" + nonce + "\"}";
        if (!sendFrame(1, payload)) return false;
        uint32_t op; std::string resp;
        return readFrame(op, resp);
    }

    bool sendClear() {
        std::string nonce = std::to_string(m_nonce++);
        std::string payload =
            "{\"cmd\":\"SET_ACTIVITY\",\"args\":{\"pid\":" + pid() +
            ",\"activity\":null},\"nonce\":\"" + nonce + "\"}";
        if (!sendFrame(1, payload)) return false;
        uint32_t op; std::string resp;
        return readFrame(op, resp);
    }
};

#include "DiscordRPC.moc"

DiscordRPC &DiscordRPC::instance() {
    static DiscordRPC inst;
    return inst;
}

DiscordRPC::DiscordRPC(QObject *parent) : QObject(parent) {}

DiscordRPC::~DiscordRPC() {
    shutdown();
}

void DiscordRPC::init(int64_t clientId) {
    if (m_worker) return;
    m_worker = new DiscordRPCWorker();
    m_worker->setParams(clientId);
    m_thread = new QThread();
    m_worker->moveToThread(m_thread);
    connect(m_thread, &QThread::started, m_worker, &DiscordRPCWorker::run);
    m_thread->start();
}

void DiscordRPC::shutdown() {
    if (!m_worker) return;
    m_worker->stop();
    m_thread->quit();
    m_thread->wait();
    delete m_worker;
    delete m_thread;
    m_worker = nullptr;
    m_thread = nullptr;
}

void DiscordRPC::setActivity(const QString &state, const QString &details) {
    if (!m_worker) return;
    m_worker->updateActivity(state, details);
}

void DiscordRPC::clearActivity() {
    if (!m_worker) return;
    m_worker->clearActivity();
}

void DiscordRPC::setEnabled(bool on) {
    if (!m_worker) return;
    m_worker->setEnabled(on);
}
