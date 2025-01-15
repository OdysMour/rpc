#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/file.h>
#include <time.h>

void write_message(const char* filename, const char* process_name, pid_t pid) {
    FILE* file = fopen(filename, "a");
    if (!file) {
        perror("fopen");
        return;
    }

    // Lock the file
    int fd = fileno(file);
    flock(fd, LOCK_EX);

    // Get current time
    time_t now = time(NULL);
    struct tm* tm_info = localtime(&now);
    char time_str[20];
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", tm_info);

    // Write message
    fprintf(file, "[%s] %s (PID: %d) wrote this message\n", time_str, process_name, pid);

    // Unlock and close
    flock(fd, LOCK_UN);
    fclose(file);
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        printf("Usage: %s <filename> <number_of_processes>\n", argv[0]);
        return 1;
    }

    const char* filename = argv[1];
    int N = atoi(argv[2]);

    // Clear the file at start
    FILE* file = fopen(filename, "w");
    if (file) fclose(file);

    // Create pipes for synchronization
    int pipes[N][2];
    for (int i = 0; i < N; i++) {
        if (pipe(pipes[i]) == -1) {
            perror("pipe");
            return 1;
        }
    }

    // Father writes first
    write_message(filename, "F", getpid());

    // Create children in order
    for (int i = 1; i <= N; i++) {
        pid_t pid = fork();
        
        if (pid < 0) {
            perror("fork");
            return 1;
        }
        else if (pid == 0) {
            // Child process
            char process_name[10];
            snprintf(process_name, sizeof(process_name), "C%d", i);
            
            // Wait for signal from previous child (or father for C1)
            //if (i > 1) {
                char buf;
                read(pipes[i-1][0], &buf, 1);
           // }
            
            write_message(filename, process_name, getpid());
            
            // Signal next child
            if (i < N) {
                char buf = 'x';
                write(pipes[i][1], &buf, 1);
            }

            return 0;
        }
    }

    // Father signals first child
    char buf = 'x';
    write(pipes[0][1], &buf, 1);

    // Wait for all children
    for (int i = 0; i < N; i++) {
        wait(NULL);
    }

    // Close all pipes
    for (int i = 0; i < N; i++) {
        close(pipes[i][0]);
        close(pipes[i][1]);
    }

    printf("Messages have been written to %s\n", filename);
    return 0;
}