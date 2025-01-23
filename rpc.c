#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/file.h>
#include <time.h>
#include <string.h>
#include <sys/select.h>
#include <sys/ipc.h>
#include <errno.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <signal.h>
#include <stdatomic.h>
#include <sys/mman.h>

typedef struct
{
    atomic_int counter;
    atomic_int shutdown;
} SharedState;

volatile sig_atomic_t shutdown_requested = 0;

void handle_signal(int sig)
{
    shutdown_requested = 1;
}

void write_message(const char *filename, const char *process_name, pid_t pid)
{
    FILE *file = fopen(filename, "a");
    if (!file)
    {
        perror("fopen");
        return;
    }

    // Lock the file
    int fd = fileno(file);
    flock(fd, LOCK_EX);

    // Get current time
    // time_t now = time(NULL);
    // struct tm* tm_info = localtime(&now);
    // char time_str[20];
    // strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", tm_info);

    // Write message
    fprintf(file, "%d --> %s\n", pid, process_name);

    // Unlock and close
    flock(fd, LOCK_UN);
    fclose(file);
}

int main(int argc, char *argv[])
{
    int desc_ready, max_rd, work_rd, rc, turn = 0;
    char response[5];
    if (argc != 3)
    {
        printf("Usage: %s <filename> <number_of_processes>\n", argv[0]);
        return 1;
    }

    const char *filename = argv[1];
    int N = atoi(argv[2]);

    // Set up shared memory
    SharedState *state = mmap(NULL, sizeof(SharedState),
                              PROT_READ | PROT_WRITE,
                              MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    atomic_init(&state->counter, 0);
    atomic_init(&state->shutdown, 0);

    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    // Array to store child PIDs and pipes
    pid_t child_pids[N];
    int pipes1[N][2];
    int pipes2[N][2];

    // Create pipes
    for (int i = 0; i < N; i++)
    {
        if (pipe(pipes1[i]) == -1 || pipe(pipes2[i]) == -1)
        {
            perror("pipe");
            return 1;
        }
    }

    // Clear the file at start
    FILE *file = fopen(filename, "w");
    if (file)
        fclose(file);

    // Father writes first
    write_message(filename, "F", getpid());

    // Create children
    for (int i = 0; i < N; i++)
    {
        pid_t pid = fork();

        if (pid < 0)
        {
            perror("fork");
            return 1;
        }
        else if (pid == 0)
        {
            // Child process
            // Close all pipes except our own
            for (int j = 0; j < N; j++)
            {
                if (j != i)
                {
                    close(pipes1[j][0]);
                    close(pipes1[j][1]);
                    close(pipes2[j][0]);
                    close(pipes2[j][1]);
                }
            }
            close(pipes1[i][1]); // Close write end
            close(pipes2[i][0]); // Close read end
                                 // Read message from parent
            char process_name[12];
            snprintf(process_name, sizeof(process_name), "C%d", i + 1);

            while (1)
            {
                char message[100];
                ssize_t bytes = read(pipes1[i][0], message, sizeof(message));
                if (bytes <= 0)
                    break;

                int expected = i;
                do
                {
                    if (atomic_load(&state->counter) == i)
                    {
                        write_message(filename, process_name, getpid());
                        atomic_store(&state->counter, (i + 1) % N);
                        break;
                    }
                    usleep(1000);
                } while (1);
                sleep(1);
                write(pipes2[i][1], "done", 5);
            }

            // Wait for all children to complete cycle
            // while (!atomic_load(&state->shutdown) &&
            //        atomic_load(&state->counter) != 0)
            // {
            //     usleep(1000);
            //  }

            close(pipes1[i][0]);
            close(pipes2[i][1]);
            exit(0);
        }
        else
        {
            // Parent stores child PID
            child_pids[i] = pid;
            close(pipes1[i][0]); // Close read end of pipes1
            close(pipes2[i][1]); // Close write end of pipes2
        }
    }

    // Add select() to see if children are done
    // Create a set of file descriptors to watch
    // Send messages to children

    sleep(5);
    char message[100];
    // for (int i = 0; i < N; i++)
    //{
    //     snprintf(message, sizeof(message),
    //              "Hello child, I am your father and I call you: C%d", i + 1);
    //   write(pipes1[i][1], message, strlen(message));
    //   close(pipes1[i][0]);
    // char response[5];
    // read(pipes2[i][0], response, sizeof(response));
    // close(pipes2[i][0]);
    //   close(pipes2[i][1]);
    // }

    fd_set read_set, working_set;
    struct timeval timeout;
    int max_fd = 0;

    // Clear the set
    FD_ZERO(&read_set);
    FD_ZERO(&working_set);

    // Add all pipe read ends to the set
    for (int i = 0; i < N; i++)
    {
        FD_SET(pipes2[i][0], &read_set);
        if (pipes2[i][0] > max_fd)
        {
            max_fd = pipes2[i][0];
        }
    }
    int fin;
    // Set timeout
    timeout.tv_sec = 60;
    timeout.tv_usec = 0;

    // Main work loop
    while (!shutdown_requested)
    {
        // Send messages to all children
        for (int i = 0; i < N; i++)
        {
            snprintf(message, sizeof(message),
                     "Hello child, I am your father and I call you: C%d", i + 1);
            printf("Hello child, I am your father and I call you: C%d\n", i + 1);

            write(pipes1[i][1], message, strlen(message));
        };

        // Wait for all responses

        max_rd = N;
        work_rd = max_rd;
        int responses = 0;
        while (responses < N && !shutdown_requested)
        {
            FD_ZERO(&working_set);
            memcpy(&working_set, &read_set, sizeof(read_set));
            timeout.tv_sec = 60;
            timeout.tv_usec = 0;
            printf("Waiting on select()...\n");
            /* https://www.ibm.com/docs/en/ztpf/2024?topic=apis-select-monitor-read-write-exception-status */
            // Wait for any pipe to be ready
            int ret = select(max_fd + 1, &working_set, NULL, NULL, &timeout);
            /**********************************************************/
            /* Check to see if the select call failed.                */
            /**********************************************************/
            if (ret < 0)
            {
                if (errno == EINTR)
                    continue;
                perror("select");
                break;
            }

            /**********************************************************/
            /* Check to see if the 3 minute time out expired.         */
            /**********************************************************/
            if (ret == 0)
            {
                printf("  select() timed out.  End program.\n");
                break;
            }

            desc_ready = ret;
            // fprintf(stderr, "%d\n", desc_ready);
            // fprintf(stderr, "%d\n", max_fd);

            for (int i = 0; i < N && desc_ready > 0; i++)
            {
                if (FD_ISSET(pipes2[i][0], &working_set))
                {
                    desc_ready--;
                    ssize_t rc = read(pipes2[i][0], response, sizeof(response));
                    if (rc < 0)
                    {
                        fprintf(stderr, "read error on pipe %d: %s\n", i, strerror(errno));
                        break;
                    }
                    //    write(pipes1[i][1], message, strlen(message));

                    // FD_CLR(pipes2[i][0], &working_set);
                    // max_rd--;
                    //  fprintf(stderr, "%d\n", max_rd);
                    //  printf("read pipes if...\n");
                    responses++;
                    ret--;
                }
                // printf("read pipes loop...\n");
            }
            // printf("after read pipe loop...\n");
        }
    }
    // Graceful shutdown
    printf("\nInitiating shutdown...\n");
    fflush(stdout); // Force flush output

    // Close communication channels
    for (int i = 0; i < N; i++)
    {
        close(pipes1[i][1]);
        close(pipes2[i][0]);
    }

    // Wait for children to exit
    int active_children = N;
    while (active_children > 0)
    {
        pid_t pid = waitpid(-1, NULL, WNOHANG);
        if (pid > 0)
        {
            active_children--;
            printf("Child %d exited\n", pid);
            fflush(stdout); // Flush after each message
        }
        usleep(100000);
    }
    munmap(state, sizeof(SharedState));

    printf("All children exited. Clean shutdown complete.\n");
    return 0;
}