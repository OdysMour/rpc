#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>

int main() {
    int N;
    printf("Εισάγετε τον αριθμό των παιδικών διεργασιών: ");
    scanf("%d", &N);

    for (int i = 1; i <= N; i++) {
        pid_t pid = fork();
        
        if (pid < 0) {
            // Σφάλμα κατά τη δημιουργία διεργασίας
            perror("fork");
            return 1;
        }
        else if (pid == 0) {
            // Παιδική διεργασία
            printf("Παιδική διεργασία C%d (PID: %d) δημιουργήθηκε\n", i, getpid());
            sleep(2);  // Προσομοίωση εργασίας
            printf("Παιδική διεργασία C%d (PID: %d) τερματίζεται\n", i, getpid());
            return 0;
        }
    }

    // Γονική διεργασία
    printf("Γονική διεργασία F (PID: %d)\n", getpid());
    
    // Αναμονή για όλες τις παιδικές διεργασίες
    for (int i = 0; i < N; i++) {
        wait(NULL);
    }
    
    printf("Όλες οι παιδικές διεργασίες ολοκληρώθηκαν\n");
    return 0;
}