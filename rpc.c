#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/file.h>
#include <time.h>
#include <string.h>

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
    //time_t now = time(NULL);
    //struct tm* tm_info = localtime(&now);
    //char time_str[20];
    //strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", tm_info);

    // Write message
    fprintf(file, "%d --> %s\n", pid, process_name);

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

    // Array to store child PIDs and pipes
    pid_t child_pids[N];
    int pipes1[N][2];
    int pipes2[N][2];

    // Create pipes
    for (int i = 0; i < N; i++) {
        if (pipe(pipes1[i]) == -1 || pipe(pipes2[i]) == -1) {
            perror("pipe");
            return 1;
        }
    }

    // Clear the file at start
    FILE* file = fopen(filename, "w");
    if (file) fclose(file);

    // Father writes first
    write_message(filename, "F", getpid());

    // Create children
    for (int i = 0; i < N; i++) {
        pid_t pid = fork();
        
        if (pid < 0) {
            perror("fork");
            return 1;
        }
        else if (pid == 0) {
            // Child process
            // Close all pipes except our own
            for (int j = 0; j < N; j++) {
                if (j != i) {
                    close(pipes1[j][0]);
                    close(pipes1[j][1]);
                    close(pipes2[j][0]);
                    close(pipes2[j][1]);
                }
            }
            close(pipes1[i][1]);  // Close write end
            close(pipes2[i][0]);  // Close read end
            // Read message from parent
            char message[100];
            read(pipes1[i][0], message, sizeof(message));
            
            // Write to file
            char process_name[10];
            snprintf(process_name, sizeof(process_name), "C%d", i+1);
            write_message(filename, process_name, getpid());
            
            // Print the requested format
            //FILE* f = fopen(filename, "a");
            //if (f) {
            //    fprintf(f, "%d --> %s\n", getpid(), process_name);
            //    fclose(f);
            //}

            // Send response to parent
            char response[] = "done";
            write(pipes2[i][1], response, sizeof(response));

            close(pipes1[i][0]);
            close(pipes2[i][1]);
            return 0;
        }
        else {
            // Parent stores child PID
            child_pids[i] = pid;
        }
    }

    // Send messages to children
    for (int i = 0; i < N; i++) {
        char message[100];
        snprintf(message, sizeof(message), 
                "Hello child, I am your father and I call you: C%d", i+1);
        write(pipes1[i][1], message, strlen(message)+1);
        close(pipes1[i][0]);
        close(pipes1[i][1]);
        char response[5];
        read(pipes2[i][0], response, sizeof(response));
        close(pipes2[i][0]);
        close(pipes2[i][1]);
    }

    // Wait for responses
    //for (int i = 0; i < N; i++) {
    //    char response[5];
    //    read(pipes2[i][0], response, sizeof(response));
    //    close(pipes2[i][0]);
    //    close(pipes2[i][1]);
    //}

    // Wait for all children
    for (int i = 0; i < N; i++) {
        waitpid(child_pids[i], NULL, 0);
    }

    printf("Messages have been written to %s\n", filename);
    return 0;
}