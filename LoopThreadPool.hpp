#pragma once
#include "EventLoop.hpp"
#include "LoopThread.hpp"
#include <memory>
#include <vector>
#include <functional>
#include <atomic>
#include <mutex>

class LoopThreadPool {
public:
    using ThreadInitCallback = std::function<void(EventLoop*)>;

    // 必须把 baseLoop（主 Reactor）传进来
    explicit LoopThreadPool(EventLoop* baseLoop)
        : _baseLoop(baseLoop)
        , _started(false)
        , _next(0)
        , _threadCnt(0) {}

    // 禁止拷贝/移动，避免多重管理线程
    LoopThreadPool(const LoopThreadPool&) = delete;
    LoopThreadPool& operator=(const LoopThreadPool&) = delete;
    LoopThreadPool(LoopThreadPool&&) = delete;
    LoopThreadPool& operator=(LoopThreadPool&&) = delete;

    ~LoopThreadPool() { Stop(); }

    void SetThreadCount(int n) {
        if (_started) return;
        _threadCnt = (n < 0 ? 0 : n);
    }

    void SetThreadInitCallback(ThreadInitCallback cb) {
        _initCb = std::move(cb);
    }

    // 启动并阻塞直到每个子 EventLoop 就绪
    void Start() {
        if (_started) return;
        _started = true;

        _threads.reserve(_threadCnt);
        _loops.reserve(_threadCnt);

        for (int i = 0; i < _threadCnt; ++i) {
            auto th = std::make_unique<LoopThread>();
            EventLoop* loop = th->GetLoop();       // 阻塞等子 loop 就绪
            
            if (_initCb) {
                loop->RunInLoop([cb = _initCb, loop] { cb(loop); });
            }
            _loops.push_back(loop);                 // 观察指针
            _threads.push_back(std::move(th));     // 拥有线程对象
        }

        // 单线程模式（0 个子线程）也可在 baseLoop 上执行 init
        if (_threadCnt == 0 && _initCb) {
            _baseLoop->RunInLoop([this]{ _initCb(_baseLoop); });
        }
    }

    // Round-Robin 选择
    EventLoop* NextLoop() {
        if (!_started || _loops.empty()) return _baseLoop;
        size_t idx = _next.fetch_add(1, std::memory_order_relaxed);
        return _loops[idx % _loops.size()];
    }

    // 会话粘滞：同一个 hash 永远落在同一个 loop
    EventLoop* GetLoopForHash(size_t hash) const {
        if (!_started || _loops.empty()) return _baseLoop;
        return _loops[hash % _loops.size()];
    }

    // 返回所有可用 loops（若无子线程则仅 baseLoop）
    std::vector<EventLoop*> GetAllLoops() const {
        if (_loops.empty()) return { _baseLoop };
        return _loops;
    }

    void Stop() {
        std::lock_guard<std::mutex> lk(_stopMtx);
        if (!_started) return;

        // 先请求各子 loop 退出
        for (auto* loop : _loops) {
            if (loop) loop->Quit();
        }
        // 再逐个 join
        for (auto& th : _threads) {
            if (th) th->Stop();
        }
        _threads.clear();
        _loops.clear();

        _started = false;
        _next.store(0, std::memory_order_relaxed);
    }

    bool Started() const { return _started; }
    int Size() const { return static_cast<int>(_loops.size()); }

private:
    EventLoop* _baseLoop;                                    // 不拥有
    std::vector<std::unique_ptr<LoopThread>> _threads;       // 拥有从线程
    std::vector<EventLoop*> _loops;                          // 子 loops（观察指针）
    ThreadInitCallback _initCb{};

    std::atomic<size_t> _next;                               // 轮询计数
    int _threadCnt;
    bool _started;
    mutable std::mutex _stopMtx;                             // 并发 Stop 保护
};
