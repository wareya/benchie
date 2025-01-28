# Benchie
Super tiny single-file benchmark dispatch program. Uses `CreateProcess()` or `exec()`, not `system()`, so a shell is not created for each run. Tested on Windows and Linux, probably supports other unix-like systems too. Uses std::chrono::steady_clock, so system clock stability issues shouldn't negatively impact the numbers often.

Records an array of times and reports the mean, and a 99% confidence interval for what the true implied average time is, and a histogram. Also reports real CPU time combined between multiple cores (`user`+`sys` in the terminology of the `time` tool), as well as the highest and lowest actually-observed times. Has a default warm up period of 1 run, which can be increased with `-w`. The default number of runs is 10, which can be increased with `-c`.

![explorer_2025-01-27_23-34-04](https://github.com/user-attachments/assets/2e5fe781-a2f2-4ea1-81e3-fb34d98577a4)

By default, Benchie excludes extreme outliers (more than 2 stddev away from mean, or 2x the 99% confidence interval for very small sample sizes), which makes it more resilient to benchmarking on shared or desktop systems than other dispatch tools. This can be disabled by setting the `NO_OUTLIER_REJECTION` environment variable.

Command line:
- `-w N`, `-wN`, `-w=N`, or with `--warmup`: control the number of warmup passes, 1 by default
- `-c N`, `-cN`, `-c=N`, or with `--count`: control the number of measured runs per command, 10 by default

Configuration environment variables:

- `FORCECOLOR`: forcibly enable color output
- `NOCOLOR`: forcibly disable color output
- `ALTBLOCKS`: display histogram with possibly-ANSI-bolded halftone blocks instead of percentage bars
- `ALTBLOCKS2`: display histogram with non-ANSI-bolded halftone blocks
- `ALTBLOCKS3`: display histogram with plain ASCII characters
- `NO_OUTLIER_REJECTION`: do not reject extreme outliers when calculating mean, etc.
- `YES_LOW_OUTLIER_REJECTION`: reject low outliers, which by default are not rejected. does nothing if `NO_OUTLIER_REJECTION` is defined.

Environment variable config options are treated as true if they are defined at all (even as an empty string), and false if they are not defined.
