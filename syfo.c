#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/sysinfo.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <sys/wait.h>
#include <ctype.h>
#include <fcntl.h>
#include <signal.h>
#include <GLFW/glfw3.h>

#define MAX_LINE 1024
#define MAX_OUTPUT 256

// ANSI color codes
#define RESET "\033[0m"
#define DGRAY "\033[90m"
#define RED "\033[31m"
#define GREEN "\033[32m"
#define YELLOW "\033[33m"
#define BLUE "\033[34m"
#define MAGENTA "\033[35m"
#define CYAN "\033[36m"
#define LGRAY "\033[37m"
#define GRAY "\033[90m"
#define LRED "\033[91m"
#define LGREEN "\033[92m"
#define LYELLOW "\033[93m"
#define LBLUE "\033[94m"
#define LMAGENTA "\033[95m"
#define LCYAN "\033[96m"
#define WHITE "\033[97m"
#define BLACK "\033[30m"

// Package manager paths
#define EMERGE_PKGS "/var/db/pkg"
#define PACMAN_PKGS "/var/lib/pacman/local"
#define APT_PKGS "/var/lib/dpkg/status"
#define RPM_PKGS "/var/lib/rpm/Packages"
#define NIX_PKGS "/run/current-system/sw/bin/"

// Function to trim whitespace
char* trim(char* str) {
  char* end;
  while(isspace((unsigned char)*str)) str++;
  if(*str == 0) return str;
  end = str + strlen(str) - 1;
  while(end > str && isspace((unsigned char)*end)) end--;
  end[1] = '\0';
  return str;
}

static int read_file_fast(const char* path, char* buffer, size_t size) {
  int fd = open(path, O_RDONLY);
  if (fd == -1) return 0;

  ssize_t bytes = read(fd, buffer, size - 1);
  close(fd);

  if (bytes > 0) {
    buffer[bytes] = '\0';
    return 1;
  }
  return 0;
}

// Function to execute command and get output
void exec_cmd(const char* cmd, char* output, size_t size) {
  FILE* fp = popen(cmd, "r");
  if (fp == NULL) {
    output[0] = '\0';
    return;
  }
  if (fgets(output, size, fp) == NULL) {
    output[0] = '\0';
  } else {
    output[strcspn(output, "\n")] = 0;
  }
  pclose(fp);
}

// Get distribution name
void getdist(char* output) {
  FILE* fp = fopen("/etc/os-release", "r");
  if (fp == NULL) {
    strcpy(output, "unknown");
    return;
  }

  char line[MAX_LINE];
  while (fgets(line, sizeof(line), fp)) {
    if (strncmp(line, "ID=", 3) == 0) {
      char* value = line + 3;
      value[strcspn(value, "\n")] = 0;
      // Remove quotes if present
      if (value[0] == '"') value++;
      if (value[strlen(value)-1] == '"') value[strlen(value)-1] = 0;
      strcpy(output, value);
      fclose(fp);
      return;
    }
  }
  fclose(fp);
  strcpy(output, "unknown");
}

// Get kernel version
void getkernel(char* output) {
  DIR* dir = opendir("/boot");
  if (dir == NULL) {
    strcpy(output, "unknown");
    return;
  }

  struct dirent* entry;
  output[0] = '\0';
  while ((entry = readdir(dir)) != NULL) {
    if (strncmp(entry->d_name, "vmlinuz-", 8) == 0) {
      strcpy(output, entry->d_name + 8);
      break;
    }
  }
  closedir(dir);

  if (output[0] == '\0') {
    strcpy(output, "unknown");
  }
}

void getuptime(char* output) {
    struct sysinfo info;
    if (sysinfo(&info) != 0) {
        strcpy(output, "unknown");
        return;
    }

    long uptime = info.uptime;
    int days = uptime / (24 * 3600);
    uptime %= (24 * 3600);
    int hours = uptime / 3600;
    uptime %= 3600;
    int minutes = uptime / 60;

    // Write formatted uptime string into output buffer
    snprintf(output, MAX_OUTPUT, "%d days, %d hours, %d minutes", days, hours, minutes);
}

// Get window manager
void getwm(char* output) {
  char cmd1[MAX_LINE];
  char id[MAX_OUTPUT];

  exec_cmd("xprop -root -notype | awk '$1==\"_NET_SUPPORTING_WM_CHECK:\"{print $5}'", id, sizeof(id));

  if (strlen(id) > 0) {
    snprintf(cmd1, sizeof(cmd1), 
             "xprop -id %s -notype -f *NET*WM_NAME 8t | grep \"_NET_WM_NAME = \" | cut --delimiter=' ' --fields=3 | cut --delimiter='\"' --fields=2", 
             id);
    exec_cmd(cmd1, output, MAX_OUTPUT);
  } else {
    strcpy(output, "unknown");
  }
}

// Count directories in a path
int count_dirs(const char* path) {
  DIR* dir = opendir(path);
  if (dir == NULL) return 0;

  int count = 0;
  struct dirent* entry;
  struct stat st;
  char full_path[PATH_MAX];

  while ((entry = readdir(dir)) != NULL) {
    if (entry->d_name[0] == '.') continue;
    snprintf(full_path, sizeof(full_path), "%s/%s", path, entry->d_name);
    if (stat(full_path, &st) == 0 && S_ISDIR(st.st_mode)) {
      count++;
    }
  }
  closedir(dir);
  return count;
}

int count_subdirs(const char *path) {
  DIR *dir;
  struct dirent *entry;
  int count = 0;

  // Open directory
  dir = opendir(path);
  if (dir == NULL) {
    perror("Error opening directory");
    return 0;
  }

  // Read directory entries
  while ((entry = readdir(dir)) != NULL) {
    // Skip "." and ".." entries
    if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
      continue;
    }

    // Construct full path for the entry
    char full_path[1024];
    snprintf(full_path, sizeof(full_path), "%s/%s", path, entry->d_name);

    // Check if entry is a directory
    if (entry->d_type == DT_DIR) {
      count++; // Increment for current directory
      // Recursively count directories in subdirectory
      count += count_subdirs(full_path);
    }
  }

  closedir(dir);
  return count;
}

// Get package count
void getpkgs(char* output) {
  struct stat st;

  if (stat(EMERGE_PKGS, &st) == 0 && S_ISDIR(st.st_mode)) {
    int count = count_subdirs(EMERGE_PKGS);
    snprintf(output, MAX_OUTPUT, "%d (emerge)", count);
  }

  else if (stat(PACMAN_PKGS, &st) == 0 && S_ISDIR(st.st_mode)) {
    int count = count_dirs(PACMAN_PKGS);
    snprintf(output, MAX_OUTPUT, "%d (pacman)", count);
  }

  else if (stat(NIX_PKGS, &st) == 0 && S_ISDIR(st.st_mode)) {
    int count = count_dirs(NIX_PKGS);
    snprintf(output, MAX_OUTPUT, "%d (nix)", count);
  }

  else if (stat(APT_PKGS, &st) == 0) {

    // For APT, count Package entries in the status file
    FILE* fp = fopen(APT_PKGS, "r");

    if (fp) {
      int count = 0;
      char line[MAX_LINE];
      while (fgets(line, sizeof(line), fp)) {
        if (strncmp(line, "Package:", 8) == 0) count++;
      }
      fclose(fp);
      snprintf(output, MAX_OUTPUT, "%d (apt)", count);
    }

    else {
      strcpy(output, "0 (apt)");
    }
  }

  else if (stat(RPM_PKGS, &st) == 0) {
    // For RPM, use rpm -qa | wc -l
    exec_cmd("rpm -qa 2>/dev/null | wc -l", output, MAX_OUTPUT);
    char* num = trim(output);
    snprintf(output, MAX_OUTPUT, "%s (rpm)", num);
  }

  else {
    strcpy(output, "unknown");
  }
}

// Get terminal
void getterm(char* output) {
  char* term = getenv("TERM");
  if (term == NULL) {
    strcpy(output, "unknown");
    return;
  }

  if (strncmp(term, "xterm-", 6) == 0) {
    strcpy(output, term + 6);
  } else {
    strcpy(output, term);
  }
}

// Get shell
void getshell(char* output) {
  pid_t ppid = getppid();

  // Build path to parent's cmdline
  char path[64];
  snprintf(path, sizeof(path), "/proc/%d/cmdline", ppid);

  FILE* f = fopen(path, "r");
  if (!f) {
    strcpy(output, "unknown");
    return;
  }

  // cmdline contains process args separated by null chars; the first is executable path
  char cmdline[MAX_OUTPUT] = {0};
  size_t r = fread(cmdline, 1, sizeof(cmdline) - 1, f);
  fclose(f);
  if (r == 0) {
    strcpy(output, "unknown");
    return;
  }

  // Extract just the executable name from the full path
  const char* exe_name = strrchr(cmdline, '/');
  if (exe_name)
    exe_name++; // skip '/'
  else
    exe_name = cmdline;

  strncpy(output, exe_name, MAX_OUTPUT - 1);
  output[MAX_OUTPUT - 1] = '\0';
}

void getcpu(char* output) {
  FILE* fp = fopen("/proc/cpuinfo", "r");
  if (fp == NULL) {
    strcpy(output, "unknown");
    return;
  }
  char line[MAX_LINE];
  while (fgets(line, sizeof(line), fp)) {
    if (strncmp(line, "model name", 10) == 0) {
      char* colon = strchr(line, ':');
      if (colon) {
        strcpy(output, trim(colon + 1));
        fclose(fp);
        return;
      }
    }
  }
  fclose(fp);
  strcpy(output, "unknown");
}

// Get CPU info
//void getcpu(char* output) {
//  FILE* fp = fopen("/proc/cpuinfo", "r");
//  if (fp == NULL) {
//    strcpy(output, "unknown");
//    return;
//  }
//
//  char line[MAX_LINE];
//  while (fgets(line, sizeof(line), fp)) {
//    if (strncmp(line, "model name", 10) == 0) {
//      char* colon = strchr(line, ':');
//      if (colon) {
//        strcpy(output, trim(colon + 1));
//        fclose(fp);
//        return;
//      }
//    }
//  }
//  fclose(fp);
//  strcpy(output, "unknown");
//}

void getgpu(char* output) {
    DIR* drm_dir = opendir("/sys/class/drm");
    if (!drm_dir) {
        strcpy(output, "unknown");
        return;
    }
    
    struct dirent* entry;
    char path[512], buffer[256];
    int found = 0;

    while ((entry = readdir(drm_dir)) != NULL) {
        if (strncmp(entry->d_name, "card", 4) != 0 || !isdigit(entry->d_name[4]))
            continue;
        
        // Vendor ID
        snprintf(path, sizeof(path), "/sys/class/drm/%s/device/vendor", entry->d_name);
        if (!read_file_fast(path, buffer, sizeof(buffer)))
            continue;
        unsigned int vendor_id = (unsigned int)strtoul(trim(buffer), NULL, 16);
        
        // Device ID
        snprintf(path, sizeof(path), "/sys/class/drm/%s/device/device", entry->d_name);
        char device_buffer[64] = "";
        read_file_fast(path, device_buffer, sizeof(device_buffer));
        unsigned int device_id = (unsigned int)strtoul(trim(device_buffer), NULL, 16);

        // Match vendor + device to GPU name
        switch (vendor_id) {
            case 0x10de: // NVIDIA
                if (device_id == 0x2482) {
                    strncpy(output, "NVIDIA GeForce RTX 3070 Ti [Discrete]", MAX_OUTPUT - 1);
                } else if (device_id >= 0x2400 && device_id <= 0x24ff) {
                    strncpy(output, "NVIDIA GeForce RTX 30 Series [Discrete]", MAX_OUTPUT - 1);
                } else if (device_id >= 0x2200 && device_id <= 0x22ff) {
                    strncpy(output, "NVIDIA GeForce RTX 20 Series [Discrete]", MAX_OUTPUT - 1);
                } else {
                    strncpy(output, "NVIDIA Graphics [Discrete]", MAX_OUTPUT - 1);
                }
                found = 1;
                break;
                
            case 0x1002: // AMD
            case 0x1022:
                strncpy(output, "AMD Radeon Graphics [Discrete]", MAX_OUTPUT - 1);
                found = 1;
                break;
                
            case 0x8086: // Intel
                strncpy(output, "Intel Graphics [Integrated]", MAX_OUTPUT - 1);
                found = 1;
                break;
                
            default:
                strncpy(output, "Graphics Controller [Unknown]", MAX_OUTPUT - 1);
                found = 1;
                break;
        }

        if (found) {
            output[MAX_OUTPUT - 1] = '\0';
            closedir(drm_dir);
            return;
        }
    }
    
    closedir(drm_dir);
    strcpy(output, "unknown");
}

//void getgpu(char* output) {
//    FILE *fp = popen("lspci -vnn | grep -m 1 -E 'VGA compatible controller|3D controller'", "r");
//    if (!fp) {
//        strcpy(output, "unknown");
//        return;
//    }
//
//    char line[512];
//    if (!fgets(line, sizeof(line), fp)) {
//        pclose(fp);
//        strcpy(output, "unknown");
//        return;
//    }
//    pclose(fp);
//
//    // Find the third pair of brackets []
//    char* p = line;
//    char* third_open = NULL;
//    char* third_close = NULL;
//    int count = 0;
//
//    while (*p && count < 3) {
//        char* open = strchr(p, '[');
//        if (!open) break;
//        char* close = strchr(open, ']');
//        if (!close) break;
//        count++;
//        if (count == 3) {
//            third_open = open;
//            third_close = close;
//        }
//        p = close + 1;
//    }
//
//    if (third_open && third_close && third_close > third_open) {
//        size_t len = third_close - third_open - 1;
//        if (len >= MAX_OUTPUT) len = MAX_OUTPUT - 1;
//        strncpy(output, third_open + 1, len);
//        output[len] = '\0';
//    } else {
//        strcpy(output, "unknown");
//    }
//}

//void getgpu(char* output) {
//    if (!glfwInit()) {
//        strcpy(output, "unknown");
//        return;
//    }
//
//    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE); // Make the window hidden
//    GLFWwindow* window = glfwCreateWindow(1, 1, "", NULL, NULL);
//    if (!window) {
//        glfwTerminate();
//        strcpy(output, "unknown");
//        return;
//    }
//
//    glfwMakeContextCurrent(window);
//    const char* renderer = (const char*)glGetString(GL_RENDERER);
//    if (renderer) {
//        char gpu_name[256];
//        strncpy(gpu_name, renderer, sizeof(gpu_name) - 1);
//        gpu_name[sizeof(gpu_name) - 1] = '\0';
//
//        char* p = strchr(gpu_name, '(');
//        if (!p) {
//            p = strchr(gpu_name, ',');
//        }
//        if (p) {
//            *p = '\0';
//        }
//
//        strncpy(output, gpu_name, MAX_OUTPUT - 1);
//        output[MAX_OUTPUT - 1] = '\0';
//    } else {
//        strcpy(output, "unknown");
//    }
//
//    glfwDestroyWindow(window);
//    glfwTerminate();
//}

// Get hostname
void gethostname_wrapper(char* output) {
  if (gethostname(output, MAX_OUTPUT) != 0) {
    strcpy(output, "localhost");
  }
}

// Calculate string length for display (without color codes)
size_t display_len(const char* str) {
  size_t len = 0;
  int in_escape = 0;

  for (const char* p = str; *p; p++) {
    if (*p == '\033') {
      in_escape = 1;
    } else if (in_escape && *p == 'm') {
      in_escape = 0;
    } else if (!in_escape) {
      len++;
    }
  }
  return len;
}

const char** getart(char* distro) {
  static const char* default_art[]={
    GRAY"│──────────────"YELLOW""GRAY"───────────────────│"RESET,
    GRAY"│──────────────"YELLOW""GRAY"───────────────────│"RESET,
    GRAY"│──────────────"YELLOW"▄▀▀▄"GRAY"───────────────│"RESET,
    GRAY"│─────────────"YELLOW"▐ ▘▘ ▚"GRAY"──────────────│"RESET,
    GRAY"│─────────────"YELLOW"▐ ▚▞ ▐"GRAY"──────────────│"RESET,
    GRAY"│─────────────"YELLOW"█▄▄▄▄ ▚"GRAY"─────────────│"RESET,
    GRAY"│────────────"YELLOW"▞       ▚"GRAY"────────────│"RESET,
    GRAY"│───────────"YELLOW"▞▄▚     ▞▄▚"GRAY"───────────│"RESET,
    GRAY"│──────────"YELLOW"▞  █     ▞  ▚"GRAY"──────────│"RESET,
    GRAY"│──────────"YELLOW"█   ▚▄▄▄▞   ▞"GRAY"──────────│"RESET,
    GRAY"│───────────"YELLOW"▚▄▄▞   ▚▄▄▞"GRAY"───────────│"RESET,
    GRAY"│──────────────"YELLOW""GRAY"───────────────────│"RESET,
    NULL
  };
  static const char* arch_art[]={
    GRAY"│─────────────"BLUE"  ▟▙  "GRAY"──────────────│"RESET,
    GRAY"│────────────"BLUE"  ▟██▙  "GRAY"─────────────│"RESET,
    GRAY"│───────────"BLUE"  ▟████▙  "GRAY"────────────│"RESET,
    GRAY"│──────────"BLUE"  ▟██████▙  "GRAY"───────────│"RESET,
    GRAY"│─────────"BLUE"  ▟████████▙  "GRAY"──────────│"RESET,
    GRAY"│────────"BLUE"  ▟██████████▙  "GRAY"─────────│"RESET,
    GRAY"│───────"BLUE"  ▟████████████▙  "GRAY"────────│"RESET,
    GRAY"│──────"BLUE"  ▟██████████████▙  "GRAY"───────│"RESET,
    GRAY"│─────"BLUE"  ▟██████▀▔▔▀██████▙  "GRAY"──────│"RESET,
    GRAY"│────"BLUE"  ▟██████▌    ▐██████▙  "GRAY"─────│"RESET,
    GRAY"│───"BLUE"  ▟████▀          ▀████▙  "GRAY"────│"RESET,
    GRAY"│──"BLUE"  ▐█▀                  ▀█▌  "GRAY"───│"RESET,
    NULL
  };
  static const char* gentoo_art[]={
    GRAY"│───────────"MAGENTA"          "GRAY"────────────│"RESET,
    GRAY"│─────────"MAGENTA"  ▄███████▄▖  "GRAY"──────────│"RESET,
    GRAY"│────────"MAGENTA"  ▟██████████▙  "GRAY"─────────│"RESET,
    GRAY"│───────"MAGENTA"  ▐█████▀  ████▙  "GRAY"────────│"RESET,
    GRAY"│────────"MAGENTA"  ▀████▖ ▄█████▙  "GRAY"───────│"RESET,
    GRAY"│──────────"MAGENTA"  ▀███████████  "GRAY"───────│"RESET,
    GRAY"│───────────"MAGENTA"  ▄█████████▘  "GRAY"───────│"RESET,
    GRAY"│─────────"MAGENTA"  ▄█████████▀  "GRAY"─────────│"RESET,
    GRAY"│───────"MAGENTA"  ▄█████████▀  "GRAY"───────────│"RESET,
    GRAY"│──────"MAGENTA"  ▐████████▀  "GRAY"─────────────│"RESET,
    GRAY"│───────"MAGENTA"  ▀████▀▘  "GRAY"───────────────│"RESET,
    GRAY"│────────"MAGENTA"        "GRAY"─────────────────│"RESET,
    NULL
  };
  if (!strcmp(distro, "gentoo")) { return gentoo_art; }
  if (!strcmp(distro, "arch")) { return arch_art; }
  else { return default_art; }
}

int main(int argc, char* argv[]) {
  setenv("NO_AT_BRIDGE", "1", 1);

  char distro[MAX_OUTPUT], kernel[MAX_OUTPUT], uptime[MAX_OUTPUT];
  char pkgs[MAX_OUTPUT], wm[MAX_OUTPUT], term[MAX_OUTPUT];
  char shell[MAX_OUTPUT], cpu[MAX_OUTPUT], gpu[MAX_OUTPUT];
  char hostname[MAX_OUTPUT];

  // Get all information
  getdist(distro);
  getkernel(kernel);
  getuptime(uptime);
  getpkgs(pkgs);
  getwm(wm);
  getterm(term);
  getshell(shell);
  getcpu(cpu);
  getgpu(gpu);
  gethostname_wrapper(hostname);

  // Format output strings
  char line_distro[MAX_LINE], line_kernel[MAX_LINE], line_uptime[MAX_LINE];
  char line_pkgs[MAX_LINE], line_wm[MAX_LINE], line_term[MAX_LINE];
  char line_shell[MAX_LINE], line_cpu[MAX_LINE], line_gpu[MAX_LINE];

  snprintf(line_distro, sizeof(line_distro), "distro:       %s ", distro);
  snprintf(line_kernel, sizeof(line_kernel), "kernel:       %s ", kernel);
  snprintf(line_uptime, sizeof(line_uptime), "uptime:       %s ", uptime);
  snprintf(line_pkgs, sizeof(line_pkgs), "packages:     %s ", pkgs);
  snprintf(line_wm, sizeof(line_wm), "wm:           %s ", wm);
  snprintf(line_term, sizeof(line_term), "terminal:     %s ", term);
  snprintf(line_shell, sizeof(line_shell), "shell:        %s ", shell);
  snprintf(line_cpu, sizeof(line_cpu), "cpu:          %s ", cpu);
  snprintf(line_gpu, sizeof(line_gpu), "gpu:          %s ", gpu);

  // Find max length for proper box formatting
  size_t max_len = 0;
  size_t lens[] = {
    strlen(line_distro), strlen(line_kernel), strlen(line_uptime),
    strlen(line_pkgs), strlen(line_wm), strlen(line_term),
    strlen(line_shell), strlen(line_cpu), strlen(line_gpu)
  };

  for (int i = 0; i < 9; i++) {
    if (lens[i] > max_len) max_len = lens[i];
  }

  // Print the output
  // Print each line with proper padding
  char* lines[] = {line_distro, line_kernel, line_uptime, line_pkgs, 
    line_wm, line_term, line_shell, line_cpu, line_gpu};
  char* colors[][2] = {
    {LRED, RED}, {LGREEN, GREEN}, {LYELLOW, YELLOW}, {LBLUE, BLUE},
    {LMAGENTA, MAGENTA}, {LCYAN, CYAN}, {WHITE, LGRAY}, {GRAY, DGRAY},
    {BLACK, BLACK}
  };
  const char** art = getart(distro);

  if (argc < 2 || argv[1] == NULL) {
    printf("%s┌─────────────────────────────────┐%s ┌", GRAY, RESET);
    for (size_t i = 0; i <= max_len; i++) printf("─");
    printf("┐ ┌────┐\n");

    for (int i = 0; i < 9; i++) {
      printf("%s │ %-*s│ │ %s█%s%s█%s │\n", 
             art[i],
             (int)max_len, lines[i], colors[i][0], RESET, colors[i][1], RESET);
    }
    printf("%s ├", art[9]);
    for (size_t j = 0; j <= max_len; j++) printf("─");
    printf("┤ └────┘\n");

    printf("%s │ %-*s│\n", art[10], (int)max_len, hostname);

    printf("%s └", art[11]);
    for (size_t i = 0; i <= max_len; i++) printf("─");
    printf("┘\n");

    printf("%s└─────────────────────────────────┘%s\n", GRAY, RESET);
  }

  else if (!strcmp(argv[1], "-v")) {
    printf("%s┌─────────────────────────────────┐%s ┌────┐\n", GRAY, RESET);
    for (int i = 0; i < 9; i++) {
      printf("%s │ %s█%s%s█%s │\n", art[i], colors[i][0], RESET, colors[i][1], RESET);
    }
    for (int i = 9; i < 10; i++) {
      printf("%s └────┘\n", art[i]);
    }
    for (int i = 10; art[i] != NULL; i++) {
      printf("%s\n", art[i]);
    }

    printf("%s└─────────────────────────────────┘%s\n", GRAY, RESET);

    printf("┌");
    for (size_t i = 0; i <= max_len; i++) printf("─");
    printf("┐\n");

    for (int i = 0; i < 9; i++) {
      printf("│ %-*s│\n", 
             (int)max_len, lines[i]);
    }
    printf("├");
    for (size_t j = 0; j <= max_len; j++) printf("─");
    printf("┤\n");

    printf("│ %-*s│\n", (int)max_len, hostname);

    printf("└");
    for (size_t i = 0; i <= max_len; i++) printf("─");
    printf("┘\n");
  }

  else if (!strcmp(argv[1], "-q")) {
    printf("%s┌─────────────────────────────────┐%s ┌────┐\n", GRAY, RESET);
    for (int i = 0; i < 9; i++) {
      printf("%s │ %s█%s%s█%s │\n", art[i], colors[i][0], RESET, colors[i][1], RESET);
    }
    for (int i = 9; i < 10; i++) {
      printf("%s └────┘\n", art[i]);
    }
    for (int i = 10; art[i] != NULL; i++) {
      printf("%s\n", art[i]);
    }

    printf("%s└─────────────────────────────────┘%s\n", GRAY, RESET);
  }

  else if (!strcmp(argv[1], "-s")) {
    printf("┌");
    for (size_t i = 0; i <= max_len; i++) printf("─");
    printf("┐ ┌────┐\n");

    for (int i = 0; i < 9; i++) {
      printf("│ %-*s│ │ %s█%s%s█%s │\n", 
             (int)max_len, lines[i], colors[i][0], RESET, colors[i][1], RESET);
      if (i == 8) {  // After the 9th line, print the bottom border
        printf("├");
        for (size_t j = 0; j <= max_len; j++) printf("─");
        printf("┤ └────┘\n");
      }
    }
    printf("│ %-*s│\n", (int)max_len, hostname);
    printf("└");
    for (size_t i = 0; i <= max_len; i++) printf("─");
    printf("┘\n");
  }

  return 0;
}
