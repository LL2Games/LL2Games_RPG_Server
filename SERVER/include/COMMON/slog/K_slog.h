#pragma once
#include "slog.h"

//매크로 설정
#define K_LOG_TRACE(fmt, ...) \
    K_slog_trace(K_SLOG_TRACE, "[%s:%s(%d)] " fmt, __FILE__, __FUNCTION__, __LINE__, ##__VA_ARGS__)

#define K_LOG_DEBUG(fmt, ...) \
    K_slog_trace(K_SLOG_DEBUG, "[%s:%s(%d)] " fmt, __FILE__, __FUNCTION__, __LINE__, ##__VA_ARGS__)

#define K_LOG_ERROR(fmt, ...) \
    K_slog_trace(K_SLOG_ERROR, "[%s:%s(%d)] " fmt, __FILE__, __FUNCTION__, __LINE__, ##__VA_ARGS__)

enum e_slog
{
    K_SLOG_NONE = 0, // 로그 출력 없음
    K_SLOG_ERROR,    // 보안수준 lev = 1
    K_SLOG_DEBUG,    // 보안수준 lev = 2
    K_SLOG_TRACE,    // 보안수준 무시

    // K_SLOG_WARN,			//보안수준 lev = 2
};

int K_slog_init(const char* path, const char *fileName, int logLevel = K_SLOG_DEBUG);
int K_slog_close();
int K_slog_trace(enum e_slog lev, const char *pszFmt, ...);
