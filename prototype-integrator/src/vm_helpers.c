#define _POSIX_C_SOURCE 200809L

#include "vm_helpers.h"

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <getopt.h>
#include <sys/stat.h>
#include <ctype.h>
#include <strings.h>
#include <limits.h>

/* ----------------------------
   Вспомогательные функции времени — инстансные
   ---------------------------- */
void vm_init_timer(vm_state_t *vm) {
    if (!vm) return;
    clock_gettime(CLOCK_MONOTONIC, &vm->last_tick_time);
}

long vm_get_elapsed_ms(struct timespec start, struct timespec end) {
    return (end.tv_sec - start.tv_sec) * 1000L +
           (end.tv_nsec - start.tv_nsec) / 1000000L;
}

void vm_wait_for_tick(vm_state_t *vm) {
    if (!vm) return;
    if (!vm->config.enable_tick_timing || vm->config.clock_rate_hz == 0) {
        return;
    }

    struct timespec current_time;
    clock_gettime(CLOCK_MONOTONIC, &current_time);

    long interval_ns = 1000000000L / (long)vm->config.clock_rate_hz;
    long elapsed_ns = (current_time.tv_sec - vm->last_tick_time.tv_sec) * 1000000000L +
                      (current_time.tv_nsec - vm->last_tick_time.tv_nsec);

    if (elapsed_ns < interval_ns) {
        long remaining_ns = interval_ns - elapsed_ns;
        struct timespec sleep_time = {0, remaining_ns};
        nanosleep(&sleep_time, NULL);
    }

    clock_gettime(CLOCK_MONOTONIC, &vm->last_tick_time);
}

/* ----------------------------
   Вспомогательные функции парсинга
   ---------------------------- */

static bool parse_u32(const char *s, uint32_t *out) {
    if (!s || !out) return false;

    errno = 0;
    char *end = NULL;
    unsigned long value = strtoul(s, &end, 10);

    if (errno != 0 || end == s || *end != '\0' || value > UINT32_MAX) {
        return false;
    }

    *out = (uint32_t)value;
    return true;
}

static bool parse_bool(const char *s, bool *out) {
    if (!s || !out) return false;

    if (strcmp(s, "1") == 0 || strcasecmp(s, "true") == 0 ||
        strcasecmp(s, "yes") == 0 || strcasecmp(s, "on") == 0) {
        *out = true;
        return true;
    }

    if (strcmp(s, "0") == 0 || strcasecmp(s, "false") == 0 ||
        strcasecmp(s, "no") == 0 || strcasecmp(s, "off") == 0) {
        *out = false;
        return true;
    }

    return false;
}

static bool parse_hash_algo(const char *s, hash_algorithm_t *out) {
    if (!s || !out) return false;

    if (strcasecmp(s, "crc32") == 0) {
        *out = HASH_CRC32;
        return true;
    }

    if (strcasecmp(s, "fnv1a") == 0 || strcasecmp(s, "simple_fnv1a") == 0) {
        *out = HASH_SIMPLE_FNV1A;
        return true;
    }

    return false;
}

static int path_is_directory(const char *path) {
    struct stat st;
    if (!path) return 0;
    if (stat(path, &st) != 0) return 0;
    return S_ISDIR(st.st_mode);
}

static void print_usage(FILE *out, const char *progname) {
    if (!out) out = stderr;

    fprintf(out,
        "Usage: %s <program_dir> [options]\n"
        "\n"
        "Options:\n"
        "  --clock-rate-hz N         Set clock rate in Hz\n"
        "  --cycle-time-ms N         Set cycle duration in ms\n"
        "  --enable-cycle-check      Enable overrun check\n"
        "  --disable-cycle-check     Disable overrun check\n"
        "  --enable-hash-check       Enable hash verification\n"
        "  --disable-hash-check      Disable hash verification\n"
        "  --enable-tick-timing      Enable tick timing\n"
        "  --disable-tick-timing     Disable tick timing\n"
        "  --hash-algo crc32|fnv1a   Select hash algorithm\n"
        "  -h, --help                Show this help\n",
        progname ? progname : "vm"
    );
}

/* ----------------------------
   Public helpers
   ---------------------------- */

void vm_cli_options_init(vm_cli_options_t *opts) {
    if (!opts) return;
    memset(opts, 0, sizeof(*opts));
}

const char *hash_algorithm_to_string(hash_algorithm_t algo) {
    switch (algo) {
        case HASH_CRC32: return "crc32";
        case HASH_SIMPLE_FNV1A: return "fnv1a";
        default: return "unknown";
    }
}

void vm_print_config(const vm_state_t *vm, FILE *out) {
    if (!vm) return;
    if (!out) out = stdout;

    fprintf(out, "program_dir: %s\n", vm->program_dir ? vm->program_dir : "(null)");
    fprintf(out, "clock_rate_hz: %u\n", vm->config.clock_rate_hz);
    fprintf(out, "cycle_time_ms: %u\n", vm->config.cycle_time_ms);
    fprintf(out, "enable_cycle_check: %s\n", vm->config.enable_cycle_check ? "true" : "false");
    fprintf(out, "enable_hash_check: %s\n", vm->config.enable_hash_check ? "true" : "false");
    fprintf(out, "enable_tick_timing: %s\n", vm->config.enable_tick_timing ? "true" : "false");
    fprintf(out, "hash_algo: %s\n", hash_algorithm_to_string(vm->config.hash_algo));
}

int vm_parse_cli(
    int argc,
    char **argv,
    vm_cli_options_t *opts,
    FILE *err
) {
    if (!opts) return -1;
    if (!err) err = stderr;

    vm_cli_options_init(opts);

    static struct option long_options[] = {
        {"clock-rate-hz",     required_argument, 0,  1},
        {"cycle-time-ms",     required_argument, 0,  2},
        {"enable-cycle-check",   no_argument,    0,  3},
        {"disable-cycle-check",  no_argument,    0,  4},
        {"enable-hash-check",    no_argument,    0,  5},
        {"disable-hash-check",   no_argument,    0,  6},
        {"enable-tick-timing",   no_argument,    0,  7},
        {"disable-tick-timing",  no_argument,    0,  8},
        {"hash-algo",         required_argument, 0,  9},
        {"help",              no_argument,       0, 'h'},
        {0, 0, 0, 0}
    };

    int option_index = 0;
    int c;

    opterr = 0;
    optind = 1;

    while ((c = getopt_long(argc, argv, "h", long_options, &option_index)) != -1) {
        switch (c) {
            case 1:
                opts->has_clock_rate_hz = true;
                if (!parse_u32(optarg, &opts->clock_rate_hz)) {
                    fprintf(err, "Invalid value for --clock-rate-hz: %s\n", optarg);
                    return -1;
                }
                break;

            case 2:
                opts->has_cycle_time_ms = true;
                if (!parse_u32(optarg, &opts->cycle_time_ms)) {
                    fprintf(err, "Invalid value for --cycle-time-ms: %s\n", optarg);
                    return -1;
                }
                break;

            case 3:
                opts->set_enable_cycle_check = true;
                opts->enable_cycle_check = true;
                break;

            case 4:
                opts->set_enable_cycle_check = true;
                opts->enable_cycle_check = false;
                break;

            case 5:
                opts->set_enable_hash_check = true;
                opts->enable_hash_check = true;
                break;

            case 6:
                opts->set_enable_hash_check = true;
                opts->enable_hash_check = false;
                break;

            case 7:
                opts->set_enable_tick_timing = true;
                opts->enable_tick_timing = true;
                break;

            case 8:
                opts->set_enable_tick_timing = true;
                opts->enable_tick_timing = false;
                break;

            case 9:
                opts->has_hash_algo = true;
                if (!parse_hash_algo(optarg, &opts->hash_algo)) {
                    fprintf(err, "Invalid value for --hash-algo: %s\n", optarg);
                    return -1;
                }
                break;

            case 'h':
                print_usage(stdout, argv[0]);
                return 1;

            case '?':
            default:
                fprintf(err, "Unknown or malformed option\n");
                print_usage(err, argv[0]);
                return -1;
        }
    }

    if (optind < argc) {
        opts->program_dir = argv[optind++];
    }

    if (optind < argc) {
        fprintf(err, "Too many positional arguments\n");
        print_usage(err, argv[0]);
        return -1;
    }

    return 0;
}

int vm_validate_config(const vm_state_t *vm, FILE *err) {
    if (!vm) return -1;
    if (!err) err = stderr;

    if (!vm->program_dir || vm->program_dir[0] == '\0') {
        fprintf(err, "Program directory is not set\n");
        return -1;
    }

    if (!path_is_directory(vm->program_dir)) {
        fprintf(err, "Program directory does not exist or is not a directory: %s\n", vm->program_dir);
        return -1;
    }

    if (vm->config.clock_rate_hz == 0) {
        fprintf(err, "clock_rate_hz must be > 0\n");
        return -1;
    }

    if (vm->config.cycle_time_ms == 0) {
        fprintf(err, "cycle_time_ms must be > 0\n");
        return -1;
    }

    if (vm->config.hash_algo != HASH_CRC32 &&
        vm->config.hash_algo != HASH_SIMPLE_FNV1A) {
        fprintf(err, "Invalid hash algorithm\n");
        return -1;
    }

    return 0;
}

int vm_apply_cli_options(
    vm_state_t *vm,
    const vm_cli_options_t *opts,
    FILE *err
) {
    if (!vm || !opts) return -1;
    if (!err) err = stderr;

    if (opts->program_dir) {
        free(vm->program_dir);
        vm->program_dir = strdup(opts->program_dir);
        if (!vm->program_dir) {
            fprintf(err, "Failed to allocate program_dir\n");
            return -1;
        }
    }

    if (opts->has_clock_rate_hz) {
        if (opts->clock_rate_hz == 0) {
            fprintf(err, "--clock-rate-hz must be > 0\n");
            return -1;
        }
        vm->config.clock_rate_hz = opts->clock_rate_hz;
    }

    if (opts->has_cycle_time_ms) {
        if (opts->cycle_time_ms == 0) {
            fprintf(err, "--cycle-time-ms must be > 0\n");
            return -1;
        }
        vm->config.cycle_time_ms = opts->cycle_time_ms;
    }

    if (opts->set_enable_cycle_check) {
        vm->config.enable_cycle_check = opts->enable_cycle_check;
    }

    if (opts->set_enable_hash_check) {
        vm->config.enable_hash_check = opts->enable_hash_check;
    }

    if (opts->set_enable_tick_timing) {
        vm->config.enable_tick_timing = opts->enable_tick_timing;
    }

    if (opts->has_hash_algo) {
        vm->config.hash_algo = opts->hash_algo;
    }

    return 0;
}