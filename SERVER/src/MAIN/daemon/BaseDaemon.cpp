#include "BaseDaemon.h"
#include <fcntl.h>
#include <sys/wait.h>
#include "K_slog.h"

BaseDaemon::BaseDaemon(const std::string &name, const std::string& path)
    : m_name(name), m_path(path)
{
}

BaseDaemon::BaseDaemon(const std::string &name, const std::string& path, const int flag)
    : m_name(name), m_path(path), m_flag(flag)
{
}

BaseDaemon::BaseDaemon(const std::string& name, const std::string& path, const std::string& configPath)
    : m_name(name), m_path(path), m_configPath(configPath)
{
}

BaseDaemon::BaseDaemon(const std::string &name, const std::string& path, const std::string& configPath, const int flag)
    : m_name(name), m_path(path), m_configPath(configPath), m_flag(flag)
{
}

pid_t BaseDaemon::Run()
{
    int fd[2];
    pid_t pid;

    if (pipe(fd) < 0)
    {
        perror("pipe");
        return  -1;
    }

    pid = fork();
    if (pid < 0) return -1;

    if (pid == 0) //child
    {
        close(fd[0]); //read close;
        pid_t pid2 = fork();
        if (pid2 < 0) _exit(127);
        if (pid2 > 0)
        {
            if (write(fd[1], (void *)&pid2, sizeof(pid2)) < 0)
            {
                perror("write");
                _exit(127);
            }
            close(fd[1]); //write close;
            _exit(0);
        }
        close(fd[1]); //write close;
        m_pid = getpid();

        // FD 닫기
        close(STDIN_FILENO);
        close(STDOUT_FILENO);
        close(STDERR_FILENO);

        // /dev/null로 연결
        open("/dev/null", O_RDONLY);
        open("/dev/null", O_RDWR);
        open("/dev/null", O_RDWR);

        printf("daemon Start[%s] pid=%d\n", m_name.c_str(), (int)m_pid);

        //./chatD ./chatD
        execl(GetExecPath(), GetExecPath(), "--config", m_configPath.c_str(), static_cast<char*>(nullptr));
        perror("execl");
        _exit(127);
    }

    if (pid > 0) //parent
    {
        pid_t pid2;
        close(fd[1]); //write close;
        if (read(fd[0], (void *)&pid2, sizeof(pid2)) < 0)
        {
            perror("read");
            _exit(127);
        }
        close(fd[0]); //read close;

        waitpid(pid, nullptr, 0);
        K_LOG_DEBUG( "Daemon[%s] started pid=%d", m_name.c_str(), pid2);
        m_pid = pid2;
        return pid2;
    }

    return -1;
}

pid_t BaseDaemon::Run(const int flag)
{
    std::string sflag = std::to_string(flag);
    int fd[2];
    pid_t pid;

    if (pipe(fd) < 0)
    {
        perror("pipe");
        return  -1;
    }

    pid = fork();
    if (pid < 0) return -1;

    if (pid == 0) //child
    {
        close(fd[0]); //read close;
        pid_t pid2 = fork();
        if (pid2 < 0) _exit(127);
        if (pid2 > 0)
        {
            if (write(fd[1], (void *)&pid2, sizeof(pid2)) < 0)
            {
                perror("write");
                _exit(127);
            }
            close(fd[1]); //write close;
            _exit(0);
        }
        close(fd[1]); //write close;
        m_pid = getpid();

        // FD 닫기
        close(STDIN_FILENO);
        close(STDOUT_FILENO);
        close(STDERR_FILENO);

        // /dev/null로 연결
        open("/dev/null", O_RDONLY);
        open("/dev/null", O_RDWR);
        open("/dev/null", O_RDWR);

        printf("daemon Start[%s] pid=%d\n", m_name.c_str(), (int)m_pid);
        K_LOG_DEBUG( "execl %s -[%s]", GetExecPath(), sflag.c_str());

        //./chatD ./chatD ./1
        execl(GetExecPath(), GetExecPath(), sflag.c_str(), "--config", m_configPath.c_str(), static_cast<char*>(nullptr));
        perror("execl");
        _exit(127);
    }

    if (pid > 0) //parent
    {
        pid_t pid2;
        close(fd[1]); //write close;
        if (read(fd[0], (void *)&pid2, sizeof(pid2)) < 0)
        {
            perror("read");
            _exit(127);
        }
        close(fd[0]); //read close;

        waitpid(pid, nullptr, 0);
        K_LOG_DEBUG( "Daemon[%s] started pid=%d", m_name.c_str(), pid2);
        m_pid = pid2;
        return pid2;
    }

    return -1;
}