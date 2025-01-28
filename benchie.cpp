#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <vector>
#include <string>
#include <chrono>
using namespace std;

#ifndef _WIN32
    #define redir_str ""
    #include <sys/times.h>
    #include <unistd.h>
    #include <wordexp.h>
    #include <sys/types.h>
    #include <sys/wait.h> 
    #include <fcntl.h>
    bool supports_color()
    {
        if (getenv("FORCECOLOR"))
            return true;
        if (getenv("NOCOLOR"))
            return false;
        if (!isatty(fileno(stdout)))
            return false;
        char * term = getenv("TERM");
        if (term && (strstr(term, "color") || strstr(term, "xterm")))
            return true;
        if (getenv("COLORTERM"))
            return true;
        return false;
    }
    
    double get_cpu_time()
    {
        struct tms my_time;
        clock_t my_clock = times(&my_time);
        (void)my_clock;
        double utime = (double)my_time.tms_cutime / sysconf(_SC_CLK_TCK);
        double stime = (double)my_time.tms_cstime / sysconf(_SC_CLK_TCK);
        auto ret = utime;
        ret += stime;
        return ret;
    }
    void system_prepare();
    
    int system(char * cmd)
    {
        auto pid = fork();
        assert(pid >= 0);
        if (pid == 0)
        {
            wordexp_t p = {};
            auto r2 = wordexp(cmd, &p, 0);
            assert(!r2);
            
            auto dn = open("/dev/null", O_WRONLY);
            dup2(dn, STDOUT_FILENO);
            dup2(dn, STDERR_FILENO);
            close(dn);
            
            auto r = execvp(p.we_wordv[0], p.we_wordv);
            wordfree(&p);
            return r;
        }
        else
        {
            int status;
            waitpid(pid, &status, 0);
            auto r = WIFEXITED(status);
            if (!r) return -r;
            return WEXITSTATUS(status);
        }
    }
#else
    #define redir_str ""
    #define WIN32_LEAN_AND_MEAN
    #define VC_EXTRALEAN
    #include <windows.h>
    bool supports_color()
    {
        if (getenv("FORCECOLOR"))
            return true;
        if (getenv("NOCOLOR"))
            return false;
        CONSOLE_SCREEN_BUFFER_INFO info;
        GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &info);
        if (info.dwSize.X && info.dwSize.Y)
            return true;
        return false;
    }
    double _time = 0.0;
    double get_cpu_time() { return _time; }
    HANDLE subproc_id = 0;
    int system(char * cmd)
    {
        STARTUPINFO sinfo = {};
        sinfo.cb = sizeof(STARTUPINFO);
        sinfo.dwFlags = STARTF_USESTDHANDLES;
        
        HANDLE hNul = CreateFile("NUL", GENERIC_WRITE, FILE_SHARE_WRITE, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
        sinfo.hStdOutput = hNul;
        sinfo.hStdError = hNul;
        
        PROCESS_INFORMATION pinfo = {};
        if (!CreateProcess(0, cmd, 0, 0, 0, 0, 0, 0, &sinfo, &pinfo))
            return -1;
        WaitForSingleObject(pinfo.hProcess, INFINITE);
        
        FILETIME ctime, xtime, stime, utime;
        GetProcessTimes(pinfo.hProcess, &ctime, &xtime, &stime, &utime);

        ULARGE_INTEGER stime_u, utime_u;
        stime_u.LowPart = stime.dwLowDateTime;
        stime_u.HighPart = stime.dwHighDateTime;
        utime_u.LowPart = utime.dwLowDateTime;
        utime_u.HighPart = utime.dwHighDateTime;

        // Convert to milliseconds
        double stime_s = stime_u.QuadPart / 10000000.0;
        double utime_s = utime_u.QuadPart / 10000000.0;
        _time += utime_s;
        _time += stime_s;

        DWORD ret;
        GetExitCodeProcess(pinfo.hProcess, &ret);
        
        CloseHandle(hNul);
        CloseHandle(pinfo.hProcess);
        CloseHandle(pinfo.hThread);
        return ret;
    }
#endif
    
static inline double get_time()
{
    auto now = std::chrono::steady_clock::now();
    auto duration = now.time_since_epoch();
    double ret = std::chrono::duration<double>(duration).count();
    return ret;
}

const char * c_reset = "";
const char * c_green = "";
const char * c_blue = "";
const char * c_pink = "";
const char * c_yellow = "";

static inline char * f2s(double f, int sizelimit, const char * color)
{
    if (f == 0.0) f = 0.0;
    static char x[256][256] = {{}};
    static size_t i = 0;
    i &= 0xFF;
    memset(x[i], 0, 256);
    auto m = pow(10.0, (double)sizelimit-2);
    auto m2 = pow(10.0, (double)sizelimit-1);
    if (f < 0.0) m2 *= 10;
    if (f > m2)
        snprintf(x[i], 255, "%s%.*e%s", color, sizelimit-6, f, c_reset);
    else
    {
        int lten = max(0, (int)ceil(log10(fabs(f)))-1);
        if (f < 0.0) lten += 1;
        f *= m;
        f = round(f);
        f /= m;
        snprintf(x[i], 255, "%s%*.*f%s", color, sizelimit, sizelimit-2-lten, f, c_reset);
    }
    
    return x[i++];
}

double get_conf_r(int degrees)
{
    if (degrees < 0)
        return 1.0/0.0;
    // critical values of student's t distribution for 99% confidence interval
    if (degrees <= 16)
    {
        double asdf[17] = {
            1000000000000.0,
            63.6551,
            9.9247,
            5.8408,
            4.6041,
            4.0322,
            3.7074,
            3.4995,
            3.3554,
            3.2498,
            3.1693,
            3.1058,
            3.0545,
            3.0123,
            2.9768,
            2.9467,
            2.9208,
        };
        return asdf[degrees];
    }
    // cruddy approximation (varies from +0.0 to +0.01ish the real value, conservatively)
    return 61.4 / exp(pow((log(log(degrees) + 1.0)) * 18.1, 0.9)) + 2.576;
}

double unlerp(double a, double b, double x)
{
    if (a == b)
    {
        if (x > b) return 1.0;
        else       return 0.0;
    }
    if (x > b) return 1.0;
    if (x < a) return 0.0;
    return (x-a)/(b-a);
}

void print_histogram(vector<double> times)
{
    if (times.size() == 0)
        return (void)puts("");
    const char * blocks[] = {" ", "▁", "▂", "▃", "▄", "▅", "▆", "▇", "█"};
    const char * blocks2[] = {" ", "░", "\033[1m░\033[0m", "▒",  "\033[1m▒\033[0m", "▓", "\033[1m▓\033[0m", "█", "\033[1m█\033[0m"};
    const char * blocks3[] = {" ", "░", "░", "▒", "▒", "▒", "▓", "▓", "█"};
    const char * blocks4[] = {" ", "_", "_", ".", ".", ".", "-", "-", "^"};
    if (getenv("ALTBLOCKS"))
        memcpy(blocks, blocks2, sizeof(blocks));
    if (getenv("ALTBLOCKS2"))
        memcpy(blocks, blocks3, sizeof(blocks));
    if (getenv("ALTBLOCKS3"))
        memcpy(blocks, blocks4, sizeof(blocks));
    double lo = 1.0/0.0;
    double hi = 0.0;
    for (auto & t : times)
    {
        if (t < 0.0) continue;
        lo = min(t, lo);
        hi = max(t, hi);
    }
    size_t h_size = max((size_t)10, min((size_t)40, (size_t)(times.size()*1.6)));
    vector<double> bins;
    for (size_t i = 0; i < h_size; i++)
        bins.push_back(0.0);
    
    double maxbin = 0.0;
    for (auto & t : times)
    {
        if (t < 0.0) continue;
        double i = unlerp(lo, hi, t);
        i = floor(i * h_size * 0.99999999);
        bins[i] += 1.0;
        maxbin = max(maxbin, bins[i]);
    }
    printf(">");
    for (auto & n : bins)
        printf("%s", blocks[(int)floor(n/maxbin * 9 * 0.9999999)]);
    printf("<");
    puts("");
    printf(" ^%s", f2s(lo, 6, c_green));
    for (size_t i = 0; i < h_size-8; i++)
        printf(" ");
    printf("^%s\n", f2s(hi, 6, c_green));
    fflush(stdout);
}

int main(int argc, char ** argv)
{
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif
    if (argc == 1)
        return puts("usage: benchify \"cmd1\" \"cmd2\" ... [-w N|--warmup N] [-c N|--count N]");
    
    size_t warmups = 1;
    size_t runs = 10;
    vector<string> cmds;
    vector<vector<double>> times;
    bool next_c = false;
    bool next_w = false;
    for (size_t i = 1; i < (unsigned)argc; i++)
    {
        auto c = strstr(argv[i], "-c") == argv[i];
        if (strcmp(argv[i], "-c") == 0) { c = 0; next_c = true; continue; }
        auto clong = strstr(argv[i], "-c=") == argv[i];
        auto count = strstr(argv[i], "--count") == argv[i];
        if (strcmp(argv[i], "--count") == 0) { count = 0; next_c = true; continue; }
        auto countlong = strstr(argv[i], "--count=") == argv[i];
        if (next_c || c || clong || count || countlong)
        {
            auto n = next_c ? 0 : clong ? 3 : c ? 2 : countlong ? 8 : 7;
            next_c = false;
            char * end = 0;
            auto r = strtoull(argv[i]+n, &end, 10);
            if (end != argv[i]+n) runs = r;
            continue;
        }
        
        auto w = strstr(argv[i], "-w") == argv[i];
        if (strcmp(argv[i], "-w") == 0) { w = 0; next_w = true; continue; }
        auto wlong = strstr(argv[i], "-w=") == argv[i];
        auto warmup = strstr(argv[i], "--warmup") == argv[i];
        if (strcmp(argv[i], "--warmup") == 0) { warmup = 0; next_w = true; continue; }
        auto warmuplong = strstr(argv[i], "--warmup=") == argv[i];
        if (next_w || w || wlong || warmup || warmuplong)
        {
            auto n = next_w ? 0 : wlong ? 3 : w ? 2 : warmuplong ? 8 : 7;
            next_w = false;
            char * end = 0;
            auto r = strtoull(argv[i]+n, &end, 10);
            if (end != argv[i]+n) warmups = r;
            continue;
        }
        
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0)
            return puts("usage: benchify \"cmd1\" \"cmd2\" ... [-w N|--warmup N] [-c N|--count N]");
        
        if (argv[i][0] == '-')
            return printf("unknowng option `%s`\n", argv[i]), -1;
        
        cmds.push_back(argv[i]);
        times.push_back({});
    }
    
    if (supports_color())
    {
        c_reset = "\033[0m";
        c_green = "\033[92m";
        c_blue = "\033[36m";
        c_pink = "\033[35m";
        c_yellow = "\033[93m";
    }
    
    
    for (size_t i = 0; i < cmds.size(); i++)
    {
        size_t max_runs = warmups + runs;
        printf("command %zd/%zd: %s%s%s...\n", i+1, cmds.size(), c_yellow, cmds[i].data(), c_reset);
        double t2 = 0.0;
        double t_min = 1.0/0.0;
        double t_max = -1.0;
        
        int num_outliers = 0;
        
        double outlying_lo = 0.0;
        double outlying_hi = 1.0/0.0;
        
        for (size_t j = 0; j < max_runs; j++)
        {
            if (j < warmups)
                printf("\r%s...Warmup %s%zd%s/%s%zd%s...%s\r", c_green, c_reset, j+1, c_green, c_reset, warmups, c_green, c_reset);
            else if (j == warmups)
                printf("\r%s...Initial run...%s                     \r", c_green, c_reset);
            fflush(stdout);
            double start = get_time();
            double ustart = get_cpu_time();
            
            auto ret = system((cmds[i] + redir_str).data());
            
            /*
            // for debugging extreme outlier rejection
            if (j == 5)
            #ifndef _WIN32
                sleep(3);
            #else
                Sleep(3000);
            #endif
            */
            
            double uend = get_cpu_time();
            double end = get_time();
            if (ret)
            {
                printf("subcommand failed: %s\n", cmds[i].data());
                exit(ret);
            }
            if (j < warmups) continue;
            times[i].push_back(end - start);
            t2 += uend-ustart;
            
            double mean = 0.0;
            size_t n = 0;
            for (size_t k = 0; k < times[i].size(); k++)
            {
                if (times[i][k] < 0.0)
                    continue;
                mean += times[i][k];
                n += 1;
            }
            mean /= n;
            
            double stddev = 0.0;
            for (size_t k = 0; k < times[i].size(); k++)
            {
                if (times[i][k] < 0.0)
                    continue;
                double x = times[i][k]-mean;
                stddev += x * x;
            }
            if (stddev == 0.0) stddev += 0.000001;
            stddev /= n-1;
            stddev = sqrt(stddev);
            if (times[i].back() < t_min) t_min = times[i].back();
            if (times[i].back() > t_max) t_max = times[i].back();
            //printf("\r...%f   ", times[i].back());
            double r = get_conf_r(n-1) / sqrt(double(n));
            printf("\r%4zd/%-4zd... %ss  (%s cpu)  range:%s~%s  99%%:%s~%s  \r",
                j+1-warmups, runs,
                f2s(mean, 8, c_green),
                f2s(t2 / (j + 1- warmups), 8, c_blue),
                f2s(t_min, 6, c_pink),
                f2s(t_max, 6, c_pink),
                f2s(mean-stddev*r, 6, c_pink),
                f2s(mean+stddev*r, 6, c_pink));
            fflush(stdout);
            
            outlying_lo = mean-stddev*max(0.0, r)*2.0 - 0.1;
            outlying_hi = mean+stddev*max(0.0, r)*2.0 + 0.1;
            
            // extreme outlier discarding
            if (!getenv("NO_OUTLIER_REJECTION") && j - warmups < runs*2)
            {
                int new_num_outliers = 0;
                for (size_t k = 0; k < times[i].size(); k++)
                {
                    if (times[i][k] < 0.0 || times[i][k] < outlying_lo || times[i][k] > outlying_hi)
                    {
                        new_num_outliers += 1;
                        times[i][k] = -1.0;
                    }
                }
                max_runs += new_num_outliers - num_outliers;
                num_outliers = new_num_outliers;
            }
        }
        double mean = 0.0;
        size_t n = 0;
        for (size_t k = 0; k < times[i].size(); k++)
        {
            if (times[i][k] < 0.0)
                continue;
            mean += times[i][k];
            n += 1;
        }
        mean /= n;
        double stddev = 0.0;
        for (size_t k = 0; k < times[i].size(); k++)
        {
            if (times[i][k] < 0.0)
                continue;
            double x = times[i][k]-mean;
            stddev += x * x;
        }
        stddev /= n-1;
        stddev = sqrt(stddev);
        double r = get_conf_r(n-1) / (sqrt((double)n));
        printf("\r%4zd/%-4zd    %ss  (%s cpu)  range:%s~%s  99%%:%s~%s  \n",
            max_runs-warmups, runs,
            f2s(mean, 8, c_green),
            f2s(t2 / runs, 8, c_blue),
            f2s(t_min, 6, c_pink),
            f2s(t_max, 6, c_pink),
            f2s(mean-stddev*r, 6, c_pink),
            f2s(mean+stddev*r, 6, c_pink));
        print_histogram(times[i]);
    }

    return 0;
}