#include "CommandReceiver.h"
#include "K_slog.h"
#include <chrono>

CommandReceiver::CommandReceiver(): m_mq(MSG_KEY, MSG_COMMAND_SEND, MSG_COMMAND_RECV)
{
}

CommandReceiver::~CommandReceiver() {
    Stop();
}

void CommandReceiver::Start() {
    m_running = true;
    m_thread = std::thread(&CommandReceiver::Run, this);
    K_slog_trace(K_SLOG_DEBUG, "[%s][%d] CommandReceiver Start", __FILE__, __LINE__);
}

void CommandReceiver::Stop() {
    m_running = false;

    // msgrcv는 blocking → dummy wake-up
    // MQMessage dummy{};
    // dummy.mtype = MSG_TYPE_CHAT_TO_CHANNEL;
    // msgsnd(m_msgid, &dummy, sizeof(dummy.payload), 0);

    if (m_thread.joinable())
        m_thread.join();
}

void CommandReceiver::Run() {
    while (m_running) {

        // MQMessage msg{};
        // ssize_t ret = msgrcv(
        //     m_msgid,
        //     &msg,
        //     sizeof(msg.payload),
        //     MSG_TYPE_CHAT_TO_CHANNEL,
        //     0   // blocking
        // );

        //K_slog_trace(K_SLOG_DEBUG, "[%s][%d] recv wait...", __FILE__, __LINE__);
        std::string msg;
        ssize_t ret = m_mq.Recv(msg);
        //K_slog_trace(K_SLOG_DEBUG, "[%s][%d] MQ recv[%s]", __FILE__, __LINE__, msg.c_str()); 
        
        if (ret <= 0)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }

        K_slog_trace(K_SLOG_DEBUG, "[%s][%d] TEST", __FILE__, __LINE__); 


        // TODO:
        // 1. Command 변환
        // 2. ThreadPool::Submit(...)
    }
}
