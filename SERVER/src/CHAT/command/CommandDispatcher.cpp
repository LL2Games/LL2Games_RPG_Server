#include "CommandDispatcher.h"
#include <memory>
#include "FindUserCommand.h"
#include "K_slog.h"

CommandDispatcher::CommandDispatcher(const int msgKey, const int typeSend, const int typeRecv) : m_mq(msgKey, typeSend, typeRecv)
{
    // /찾기
    this->Add(std::make_unique<FindUserCommand>(m_mq));
}

void CommandDispatcher::Add(std::unique_ptr<ICommand> cmd)
{
    m_commands.push_back(std::move(cmd));
}

bool CommandDispatcher::Dispatch(const std::string& msg)
{
    for (auto& cmd : m_commands)
    {
        if (cmd->Match(msg))
        {
            cmd->Execute(msg);
            K_LOG_DEBUG( "MQ send[%s]", msg.c_str()); 
            return true;
        }
    }

    return false; //일반채팅
}