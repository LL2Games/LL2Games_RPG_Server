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
    K_LOG_DEBUG( "CommandReceiver Start");
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

        //K_LOG_DEBUG( "recv wait...");
        std::string msg;
        ssize_t ret = m_mq.Recv(msg);
        //K_LOG_DEBUG( "MQ recv[%s]", msg.c_str()); 
        
        if (ret <= 0)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }

        K_LOG_DEBUG( "TEST"); 


        // TODO:
        // 1. Command 변환
        // 2. ThreadPool::Submit(...)
    }
}
