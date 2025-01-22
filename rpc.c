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
    int desc_ready, rc = 0;
    char response[5];
    if (argc != 3)
    {
        printf("Usage: %s <filename> <number_of_processes>\n", argv[0]);
        return 1;
    }

    /* Simple semaphore used here is actually a set of 1                  */
    int semaphoreId = -1;

    semaphoreId = semget(IPC_PRIVATE, 1, S_IRUSR | S_IWUSR);
    if (semaphoreId < 0)
    {
        printf("semget failed, err=%d\n", errno);
        exit(1);
    }
    /* Set the semaphore (#0 in the set) count to 1. Simulate a mutex */
    rc = semctl(semaphoreId, 0, SETVAL, (int)1);

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
                // Sem
                int rc;
                struct sembuf lockOperation = {0, -i - 1, 0};
                struct sembuf unlockOperation = {0, ((i + 2) % (N + 1)) == 0 ? 1 : (i + 2) % (N + 1), 0};
                // fprintf(stderr, "C%d --> %d\n", i + 1, semctl(semaphoreId, 0, GETVAL));

                read(pipes1[i][0], message, sizeof(message));
                fprintf(stderr, "C%d: %s, semval=%d\n", i + 1, message, semctl(semaphoreId, 0, GETVAL));

                rc = semop(semaphoreId, &lockOperation, 1);
                if (rc < 0)
                {
                    perror("  semop failed");
                    break;
                }
                // fprintf(stderr, "C%d --> %d\n", i + 1, semctl(semaphoreId, 0, GETVAL));

                // Write to file
                char process_name[12];
                snprintf(process_name, sizeof(process_name), "C%d", i + 1);
                write_message(filename, process_name, getpid());
                char process_num[12];
                snprintf(process_num, sizeof(process_num), "%d", ((i + 2) % (N + 1)) == 0 ? 1 : (i + 2) % (N + 1));
                write_message(filename, process_num, getpid());
                //sleep(0.1);
                rc = semop(semaphoreId, &unlockOperation, 1);
                // fprintf(stderr, "C%d --> %d\n", i + 1, semctl(semaphoreId, 0, GETVAL));

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
    /* Simple lock operation. 0=which-semaphore, -1=decrement, 0=noflags  */
    // struct sembuf lockOperation = { 0, -1, 0};
    /* Simple unlock operation. 0=which-semaphore, 1=increment, 0=noflags */
    // struct sembuf unlockOperation = { 0, 1, 0};

    // Add select() to see if children are done
    // Create a set of file descriptors to watch
    // Send messages to children
    char message[100];
    for (int i = 0; i < N; i++)
    {
        snprintf(message, sizeof(message),
                 "Hello child, I am your father and I call you: C%d", i + 1);
        write(pipes1[i][1], message, strlen(message));
        close(pipes1[i][0]);
        // char response[5];
        // read(pipes2[i][0], response, sizeof(response));
        // close(pipes2[i][0]);
        close(pipes2[i][1]);
    }

    fd_set read_set, working_set;
    struct timeval timeout;
    int max_fd = sizeof(pipes2);

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

    // Set timeout
    timeout.tv_sec = 60;
    timeout.tv_usec = 0;

    do
    {
        memcpy(&working_set, &read_set, sizeof(read_set));

        printf("Waiting on select()...\n");
        /* https://www.ibm.com/docs/en/ztpf/2024?topic=apis-select-monitor-read-write-exception-status */
        // Wait for any pipe to be ready
        int ret = select(max_fd + 1, &working_set, NULL, NULL, &timeout);
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

        for (int i = 0; i <= max_fd && desc_ready > 0; i++)
        {
            if (FD_ISSET(pipes2[i][0], &working_set))
            {
                desc_ready--;
                do
                {
                    ssize_t rc = read(pipes2[i][0], response, sizeof(response));
                    if (rc < 0)
                    {
                        fprintf(stderr, "read error on pipe %d: %s\n", i, strerror(errno));
                        break;
                    }
                    ssize_t len = rc;
                    snprintf(message, sizeof(message),
                             "Hello child, I am your father and I call you: C%d", i + 1);
                    rc = write(pipes1[i][1], message, strlen(message));
                    if (rc <= 0)
                    {
                        perror("write");
                        break;
                    }
                    break;
                } while (1);
            }
        }

    } while (1);
    // Wait for all children
    for (int i = 0; i < N; i++)
    {
        waitpid(child_pids[i], NULL, 0);
    }

    printf("Messages have been written to %s\n", filename);
    return 0;
}