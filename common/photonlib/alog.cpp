#include "alog.h"
#include <atomic>
#include <mutex>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <limits.h>
#include <algorithm>
using namespace std;

typedef void (*LogOutput)(const char* begin, const char* end);
LogOutput log_output = &log_output_stdout;
int log_output_level = ALOG_DEBUG;

void LogFormatter::put(ALogBuffer& buf, FP x)
{
    char _fmt[64];
    ALogBuffer fmt {_fmt, sizeof(_fmt)};
    put(fmt, '%');
    if (x.width() >= 0)
    {
        put(fmt, (uint64_t)x.width());
    }
    // precision and width should be independent. like %.2f
    if (x.precision() >= 0)
    {
        put(fmt, '.');
        put(fmt, (uint64_t)x.precision());
    }
    put(fmt, x.scientific() ? 'e' : 'f');
    put(fmt, '\0');
    // this snprintf is a method of LogFormatter obj, not the one in stdio

    snprintf(buf, _fmt, x.value());
}

static inline void put_uint64(LogFormatter* log, ALogBuffer& buf, uint64_t x)
{
    do { log->put(buf, (char)('0' + x % 10));
    } while((x /= 10) > 0);
}

void LogFormatter::put_integer(ALogBuffer& buf, uint64_t x)
{
    auto begin = buf.ptr;
    put_uint64(this, buf, x);
    std::reverse(begin, buf.ptr);
}

static void move_and_fill(char* begin, char*& ptr, uint64_t width, char padding)
{
    auto end = begin + width;
    if (end > ptr)
    {
        auto len = ptr - begin;
        auto padding_len = end - ptr;
        memmove(begin + padding_len, begin, len);
        memset(begin, padding, padding_len);
        ptr = end;
    }
}

void LogFormatter::put_integer_hbo(ALogBuffer& buf, ALogInteger X)
{
    // print (in reversed order)
    auto x = X.uvalue();
    auto shift = X.shift();
    unsigned char mask = (1UL << shift) - 1;
    auto begin = buf.ptr;
    do { put(buf, "0123456789ABCDEFH" [x & mask]);
    } while (x >>= shift);

    std::reverse(begin, buf.ptr);
    move_and_fill(begin, buf.ptr, X.width(), X.padding());
}

//static inline void insert_comma(char* begin, char*& ptr, uint64_t width, char padding)
static void insert_commas(char*& digits_end, uint64_t ndigits)
{
    if (ndigits <= 0) return;
    auto ncomma = (ndigits - 1) / 3;
    auto psrc = digits_end;
    digits_end += ncomma;
    auto pdest = digits_end;

    struct temp
    {
        char data[4];
        temp(const char* psrc)
        {
            *(uint32_t*)data = *(uint32_t*)(psrc-1);
            data[0] = ',';
        }
    };

    while (pdest != psrc)
    {
        psrc -= 3; pdest -= 4;
        *(temp*)pdest = temp(psrc);
    }
}

void LogFormatter::put_integer_dec(ALogBuffer& buf, ALogInteger x)
{
    uint64_t ndigits;
    auto begin = buf.ptr;
    // print (in reversed order)
    if (!x.is_signed() || x.svalue() > 0)
    {
        put_uint64(this, buf, x.uvalue());
        ndigits = buf.ptr - begin;
    }
    else
    {
        put_uint64(this, buf, -x.svalue());
        ndigits = buf.ptr - begin;
        put(buf, '-');
    }

    std::reverse(begin, buf.ptr);
    if (x.comma()) insert_commas(buf.ptr, ndigits);
    move_and_fill(begin, buf.ptr, x.width(), x.padding());
}

static time_t dayid = 0;
static struct tm alog_time = {0};
struct tm* alog_update_time(time_t now)
{
    auto now0 = now;
    if (now != dayid) {
        dayid = now;
        localtime_r(&now0, &alog_time);
        alog_time.tm_year+=1900;
        alog_time.tm_mon++;
    } else {
        int sec = now % 60;    now /= 60;
        int min = now % 60;    now /= 60;
        int hor = now % 24;    now /= 24;
        alog_time.tm_sec = sec;
        alog_time.tm_min = min;
        alog_time.tm_hour = hor;
    }
    return &alog_time;
}

static struct tm* alog_update_time()
{
    return alog_update_time(time(0));
}

void log_output_stdout(const char* begin, const char* end)
{
    write(1, begin, end - begin);
}

void log_output_stderr(const char* begin, const char* end)
{
    write(2, begin, end - begin);
}

void log_output_null(const char* begin, const char* end)
{
}

static int log_file_fd = -1;
uint64_t log_file_size_limit = UINT64_MAX;
static char log_file_name[PATH_MAX];
//static atomic_uint64_t log_file_size;
static atomic<uint64_t> log_file_size(0);
static mutex log_file_lock;
static unsigned int log_file_max_cnt=10;

static inline void add_generation(char* buf, int size, unsigned int generation)
{
    if (generation == 0) {
        buf[0] = '\0';
    } else {
        snprintf(buf, size, ".%u", generation);
    }
}

static void log_file_rotate()
{
    if(access(log_file_name, F_OK) != 0)
        return;

    int fn_length = (int)strlen(log_file_name);
    char fn0[PATH_MAX], fn1[PATH_MAX];
    strcpy(fn0, log_file_name);
    strcpy(fn1, log_file_name);

    unsigned int last_generation = 1; // not include
    while(true)
    {
        add_generation(fn0 + fn_length, sizeof(fn0) - fn_length, last_generation);
        if(0 != access(fn0, F_OK)) break;
        last_generation++;
    }

    while(last_generation >= 1)
    {
        add_generation(fn0 + fn_length, sizeof(fn0) - fn_length, last_generation - 1);
        add_generation(fn1 + fn_length, sizeof(fn1) - fn_length, last_generation);

        if (last_generation >= log_file_max_cnt) {
            unlink(fn0);
        } else {
            rename(fn0, fn1);
        }
        last_generation--;
    }
}

static void do_log_output_file(const char* begin, const char* end)
{
    if (log_file_fd < 0) return;
    uint64_t length = end - begin;
    write(log_file_fd, begin, length);
    if ((log_file_size += length) > log_file_size_limit)
    {
        lock_guard<mutex> guard(log_file_lock);
        if (log_file_size > log_file_size_limit)
        {
            log_file_rotate();
            // close(log_file_fd);

            const auto mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
            int fd = open(log_file_name, O_CREAT | O_WRONLY | O_APPEND, mode);
            if (fd < 0)
            {
                char enter = '\n';
                static char msg[] = "failed to open log output file: ";
                write(log_file_fd, msg, sizeof(msg) - 1);
                write(log_file_fd, log_file_name, strlen(log_file_name));
                write(log_file_fd, &enter, 1);
                return;
            }

            log_file_size = 0;
            dup2(fd, log_file_fd);  // to make sure log_file_fd
            close(fd);              // doesn't change
        }
    }
}

void log_output_file(int fd, uint64_t rotate_limit)
{
    if (fd < 0) return;
    if (log_file_fd > 2 && log_file_fd != fd)
        close(log_file_fd);

    log_file_fd = fd;
    log_file_name[0] = '\0';
    log_output = &do_log_output_file;
    log_file_size_limit = max(rotate_limit, (uint64_t)(1024 * 1024));
}

int log_output_file(const char* fn, uint64_t rotate_limit, int max_log_files)
{
    const auto mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
    int fd = open(fn, O_CREAT | O_WRONLY | O_APPEND, mode);
    if (fd < 0)
        return -1;

    log_file_size.store(lseek(fd, 0, SEEK_END));
    log_output_file(fd, rotate_limit);
    strcpy(log_file_name, fn);  // must comes after log_output_file(fd, rotate_limit);

    log_file_max_cnt = min(max_log_files, 30);
    return 0;
}

int log_output_file_close()
{
    if (log_file_fd == -1)
        return errno = EALREADY, -1;
    if (log_output == &do_log_output_file)
        return errno = EINPROGRESS, -1;
    close(log_file_fd);
    log_file_fd = -1;
    return 0;
}

int get_log_file_fd()
{
    return log_file_fd >= 0 ? log_file_fd    :
        log_output == &log_output_stdout ? 1 :
        log_output == &log_output_stderr ? 2 : -1;
}

namespace photon
{
    struct thread;
#ifndef LOGCURRENT
    static
#else
    extern
#endif
    thread* CURRENT;
}

static inline ALogInteger DEC_W2P0(uint64_t x)
{
    return DEC(x).width(2).padding('0');
}

LogBuffer& operator << (LogBuffer& log, const Prologue& pro)
{
#ifdef LOG_BENCHMARK
    auto t = &alog_time;
#else
    auto t = alog_update_time();
#endif
    log.printf(t->tm_year, '/');
    log.printf(DEC_W2P0(t->tm_mon),  '/');
    log.printf(DEC_W2P0(t->tm_mday), ' ');
    log.printf(DEC_W2P0(t->tm_hour), ':');
    log.printf(DEC_W2P0(t->tm_min),  ':');
    log.printf(DEC_W2P0(t->tm_sec));

    static const char levels[] = "|DEBUG|th=|INFO |th=|WARN |th=|ERROR|th=|FATAL|th=";
    log.printf(ALogString(&levels[pro.level * 10], 10));
    log.printf(photon::CURRENT, '|');
    log.printf(ALogString((char*)pro.addr_file, pro.len_file), ':');
    log.printf(pro.line, '|');
    log.printf(ALogString((char*)pro.addr_func, pro.len_func), ':');
    return log;
}

