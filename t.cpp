#pragma once

#include <functional>
#include <memory>
#include <cstdint>
#include <vector>
#include <unordered_map>

using Task        = std::function<void()>;
using ReleaseTask = std::function<void()>;

class TimerTask {
private:
    uint64_t    _id{0};
    uint32_t    _timeout{0};     // 初始超时（秒）
    uint32_t    _rotations{0};   // 需要再转多少圈
    Task        _task{};         // 可能为空的可调用
    ReleaseTask _release{};      // 缺省 no-op
    bool        _isCancel{false};

public:
    TimerTask(uint64_t id, uint32_t timeout, Task fn, uint32_t rotations = 0)
        : _id(id), _timeout(timeout), _rotations(rotations), _task(std::move(fn)), _release([]{}) {}

    ~TimerTask() {
        // 析构即到期：未取消就执行任务，然后回收注册
        if (!_isCancel && _task) _task();
        if (_release) _release();
    }

    void SetReleaseTask(ReleaseTask release) { _release = std::move(release); }

    uint32_t DelayTime() const { return _timeout; }

    void     SetDelayTime(uint32_t timeout) { _timeout = timeout; }
    void     SetRotations(uint32_t r) { _rotations = r; }
    uint32_t Rotations() const { return _rotations; }
    void     DecRotations() { if (_rotations) --_rotations; }

    void Cancel() { _isCancel = true; }
    bool IsCancel() const { return _isCancel; }
    uint64_t Id() const { return _id; }
};

class TimerWheel {
    using PtrTask  = std::shared_ptr<TimerTask>;
    using WeakTask = std::weak_ptr<TimerTask>;

private:
    int _tick;
    int _capacity;
    std::vector<std::vector<PtrTask>> _wheel;
    std::unordered_map<uint64_t, WeakTask> _timers;

    void RemoveTimer(uint64_t id) {
        auto it = _timers.find(id);
        if (it != _timers.end()) _timers.erase(it);
    }

    // 计算放入槽和圈数
    void CalcSlotAndRot(uint32_t timeout, int& slot, uint32_t& rotations) const {
        rotations = timeout / _capacity;
        uint32_t steps = timeout % _capacity;
        slot = (_tick + static_cast<int>(steps)) % _capacity;
    }

public:
    TimerWheel()
        : _tick(0), _capacity(60), _wheel(static_cast<size_t>(_capacity)) {}

    // 添加任务（timeout: 秒）
    void TimerAdd(uint64_t id, uint32_t timeout, Task fn) {
        if (timeout == 0) timeout = 1; // 至少延迟 1 tick，避免立刻到期
        int slot = 0;
        uint32_t rot = 0;
        CalcSlotAndRot(timeout, slot, rot);

        PtrTask pt = std::make_shared<TimerTask>(id, timeout, std::move(fn), rot);
        pt->SetReleaseTask([this, id]{ this->RemoveTimer(id); });

        _timers[id] = WeakTask(pt);
        _wheel[slot].push_back(std::move(pt));
    }

    // 刷新（从“现在”起重新计时）
    void TimerRefresh(uint64_t id) {
        auto it = _timers.find(id);
        if (it == _timers.end()) return;

        PtrTask pt = it->second.lock();
        if (!pt) { _timers.erase(it); return; }

        // 重新计算并放到新的槽（无需从旧槽移除：旧引用保留，直到新的到期时才析构执行）
        int slot = 0;
        uint32_t rot = 0;
        CalcSlotAndRot(pt->DelayTime(), slot, rot);
        pt->SetRotations(rot);
        _wheel[slot].push_back(std::move(pt));
    }

    // 取消任务
    void TimerCancel(uint64_t id) {
        auto it = _timers.find(id);
        if (it == _timers.end()) return;

        PtrTask pt = it->second.lock();
        if (!pt) { _timers.erase(it); return; }
        pt->Cancel();
    }

    // 每秒钟调用一次
    void RunTimerTask() {
        _tick = (_tick + 1) % _capacity;

        // 取出当前槽的所有任务，逐个处理（用移动取出，避免在原容器上反复 push_back）
        auto bucket = std::move(_wheel[_tick]);
        _wheel[_tick].clear();

        for (auto& pt : bucket) {
            if (!pt) continue;
            if (pt->IsCancel()) {
                // 取消：让 shared_ptr 直接释放，触发 ~TimerTask() 时不会执行任务，但会调用 release() 从 map 移除
                pt.reset();
                continue;
            }

            if (pt->Rotations() > 0) {
                pt->DecRotations();
                // 还没到期，放入下一槽
                int next = (_tick + 1) % _capacity;
                _wheel[next].push_back(std::move(pt));
            } else {
                // 到期：让其生命周期在此结束，触发析构执行任务并从 map 移除
                pt.reset();
            }
        }
    }

    // 尽量合并接口：返回是否成功标记取消
    bool SetCancel(uint64_t id) {
        auto it = _timers.find(id);
        if (it == _timers.end()) return false;
        PtrTask pt = it->second.lock();
        if (!pt || pt->IsCancel()) return false;
        pt->Cancel();
        return true;
    }
};
