#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/file.h>
#include <time.h>
#include <string.h>
#include <sys/select.h>
#include <sys/sem.h>
#include <sys/ipc.h>
#include <errno.h>
#include <sys/stat.h>
#include <stdlib.h>

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
            do
            {
                char message[100];
                read(pipes1[i][0], message, sizeof(message));

                // Write to file
                char process_name[12];
                snprintf(process_name, sizeof(process_name), "C%d", i + 1);
                write_message(filename, process_name, getpid());
                sleep(1);
                // Send response to parent
                char response[] = "done";
                write(pipes2[i][1], response, sizeof(response));
            } while (1);
            // close(pipes1[i][0]);
            // close(pipes2[i][1]);
            // return 0;
        }
        else
        {
            // Parent stores child PID
            child_pids[i] = pid;
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
    int max_fd;

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
    do
    {
        for (int i = 0; i < N; i++)
        {
            snprintf(message, sizeof(message),
                     "Hello child, I am your father and I call you: C%d", i + 1);
            printf("Hello child, I am your father and I call you: C%d\n", i + 1);

            write(pipes1[i][1], message, strlen(message));
        };
        FD_ZERO(&working_set);
        memcpy(&working_set, &read_set, sizeof(read_set));
        timeout.tv_sec = 60;
        timeout.tv_usec = 0;
        max_rd = N;
        work_rd = max_rd;
        do
        {

            printf("Waiting on select()...\n");
            /* https://www.ibm.com/docs/en/ztpf/2024?topic=apis-select-monitor-read-write-exception-status */
            // Wait for any pipe to be ready
            int ret = select(max_fd + 1, &working_set, NULL, NULL, NULL);
            /**********************************************************/
            /* Check to see if the select call failed.                */
            /**********************************************************/
            if (ret < 0)
            {
                perror("  select() failed");
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
            fprintf(stderr, "%d\n", desc_ready);
            fprintf(stderr, "%d\n", max_fd);

            for (int i = 0; i <= N && desc_ready > 0; i++)
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

                    FD_CLR(pipes2[i][0], &working_set);
                    max_rd--;
                    // fprintf(stderr, "%d\n", max_rd);
                    // printf("read pipes if...\n");
                }
                printf("read pipes loop...\n");
            }
            printf("after read pipe loop...\n");

        } while (max_rd > 0);
    } while (1);
    // Wait for all children
    for (int i = 0; i < N; i++)
    {
        waitpid(child_pids[i], NULL, 0);
    }

    printf("Messages have been written to %s\n", filename);
    return 0;
}