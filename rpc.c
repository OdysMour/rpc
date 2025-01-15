#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
<<<<<<< HEAD
#include <sys/file.h>
#include <time.h>
#include <string.h>

typedef struct {
    int child_id;
    char message[100];
} child_message;
=======
>>>>>>> parent of bb8ecb0 (first)

int main() {
    int N;
    printf("Εισάγετε τον αριθμό των παιδικών διεργασιών: ");
    scanf("%d", &N);

<<<<<<< HEAD
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

    // Single pipe for all communication
    int pipefd[2];
    if (pipe(pipefd) == -1) {
        perror("pipe");
        return 1;
    }

    // Array to store child PIDs
    pid_t child_pids[N];

    // Clear the file at start
    FILE* file = fopen(filename, "w");
    if (file) fclose(file);

    // Father writes first
    write_message(filename, "F", getpid());

    // Create children
    for (int i = 0; i < N; i++) {
=======
    for (int i = 1; i <= N; i++) {
>>>>>>> parent of bb8ecb0 (first)
        pid_t pid = fork();
        
        if (pid < 0) {
            // Σφάλμα κατά τη δημιουργία διεργασίας
            perror("fork");
            return 1;
        }
        else if (pid == 0) {
<<<<<<< HEAD
            // Child process
            close(pipefd[1]);  // Close write end
            
            child_message msg;
            while (1) {
                // Read message from parent
                if (read(pipefd[0], &msg, sizeof(msg)) > 0) {
                    if (msg.child_id == i) {  // Message is for this child
                        // Write to file
                        char process_name[10];
                        snprintf(process_name, sizeof(process_name), "C%d", i+1);
                        write_message(filename, process_name, getpid());
                        
                        // Send response
                        char response[] = "done";
                        write(pipefd[1], response, sizeof(response));
                        break;
                    }
                }
            }
            close(pipefd[0]);
            close(pipefd[1]);  // Close write end
=======
            // Παιδική διεργασία
            printf("Παιδική διεργασία C%d (PID: %d) δημιουργήθηκε\n", i, getpid());
            sleep(2);  // Προσομοίωση εργασίας
            printf("Παιδική διεργασία C%d (PID: %d) τερματίζεται\n", i, getpid());
>>>>>>> parent of bb8ecb0 (first)
            return 0;
        }
        else {
            // Parent stores child PID
            child_pids[i] = pid;
        }
    }

<<<<<<< HEAD
    // Parent closes read end (will reopen later)

    // Send messages to children
    for (int i = 0; i < N; i++) {
        child_message msg;
        msg.child_id = i;
        snprintf(msg.message, sizeof(msg.message), 
                "Hello child, I am your father and I call you: C%d", i+1);
        write(pipefd[1], &msg, sizeof(msg));
    }

    // Wait for responses
    for (int i = 0; i < N; i++) {
        char response[5];
        read(pipefd[0], response, sizeof(response));
    }

    // Wait for all children
=======
    // Γονική διεργασία
    printf("Γονική διεργασία F (PID: %d)\n", getpid());
    
    // Αναμονή για όλες τις παιδικές διεργασίες
>>>>>>> parent of bb8ecb0 (first)
    for (int i = 0; i < N; i++) {
        waitpid(child_pids[i], NULL, 0);
    }
<<<<<<< HEAD

    close(pipefd[0]);
    close(pipefd[1]);

    printf("Messages have been written to %s\n", filename);
=======
    
    printf("Όλες οι παιδικές διεργασίες ολοκληρώθηκαν\n");
>>>>>>> parent of bb8ecb0 (first)
    return 0;
}