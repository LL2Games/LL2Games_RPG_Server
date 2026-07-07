#include "WorkerThread.h"

WorkerThread::WorkerThread()
{

}
WorkerThread::~WorkerThread()
{
    Stop();
}

void WorkerThread::Start()
{
    m_running = true;
    m_worker = std::thread(&WorkerThread::Run, this);
}

void WorkerThread::Stop()
{
    m_running = false;
    m_cv.notify_all();
    if (m_worker.joinable())
        m_worker.join();
}

void WorkerThread::PushTask(std::unique_ptr<Task> task)
{
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        // Stop 이후에는 worker가 종료될 수 있으므로 새 작업을 받지 않는다.
        if(!m_running)
            return;

        m_tasks.push(std::move(task));
    }
    m_cv.notify_one();
}

void WorkerThread::Run()
{
    while (true)
    {
        std::unique_ptr<Task> task;

        {
            std::unique_lock<std::mutex> lock(m_mutex);

            m_cv.wait(lock, [&]() {
                return !m_tasks.empty() || !m_running;
            });

            // m_running만 확인할 경우 작업이 남아 있어도 종료될 수 있음
            // 그래서 m_tasks.empty() 확인을 통해
            // Stop 요청이 와도 큐에 남은 작업은 모두 처리한 뒤 종료한다.
            if (!m_running && m_tasks.empty()) break;

            task = std::move(m_tasks.front());
            m_tasks.pop();
        }

        if (task)
        {
            try
            {
                task->Execute();
            }
            catch (const std::exception& e)
            {
               
            }
            catch (...)
            {
             
            }
        }
    }
}
