#pragma once
#include <vector>
#include <memory>
#include "WorkerThread.h"
#include <mutex>
#include <cstdint>

class ThreadPool {

public:
    explicit ThreadPool(size_t threadCount);
    ~ThreadPool();

    void Start();
    void Stop();
    void Submit(std::unique_ptr<Task> task);
    void SubmitByKey(uint64_t key, std::unique_ptr<Task> task);
    size_t GetPoolSize() const { return m_poolSize; }

private:
    std::vector<std::unique_ptr<WorkerThread>> m_workers;
    size_t m_nextIndex{0};

    std::mutex m_submitMutex;
    size_t m_poolSize{0};


};