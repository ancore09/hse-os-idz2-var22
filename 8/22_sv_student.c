#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <time.h>
#include <sys/wait.h>

#define SHM_SIZE 1024

int shmid, semid1, semid2, semid3;
key_t key1, key2, key3;
int *shm;
struct sembuf sem_wait = {0, -1, SEM_UNDO};
struct sembuf sem_signal = {0, 1, SEM_UNDO};

void terminate(int num) {
    shmdt(shm);
    shmctl(shmid, IPC_RMID, 0);
    semctl(semid1, 0, IPC_RMID, 0);
    semctl(semid2, 0, IPC_RMID, 0);
    semctl(semid3, 0, IPC_RMID, 0);
    exit(EXIT_FAILURE);
}

int main(int argc, char *argv[]) {
    key1 = 1234;
    key2 = 5678;
    key3 = 9012;

    struct sigaction act;
    act.sa_handler = terminate;
    sigaction(SIGINT, &act, NULL);
    sigaction(SIGTERM, &act, NULL);

    int M = atoi(argv[1]);
    int N = atoi(argv[2]);
    int K = atoi(argv[3]);

    int* array = malloc(M * N * K * sizeof(int));

    if ((shmid = shmget(key1, SHM_SIZE, IPC_CREAT | 0666)) < 0) {
        perror("shmget");
        exit(EXIT_FAILURE);
    }

    if ((shm = shmat(shmid, NULL, 0)) == (int *) -1) {
        perror("shmat");
        exit(1);
    }

    if ((semid1 = semget(key1, 1, IPC_CREAT | 0666)) == -1) {
        perror("semget");
        exit(1);
    }

    if ((semid2 = semget(key2, 1, IPC_CREAT | 0666)) == -1) {
        perror("semget");
        exit(1);
    }

    if ((semid3 = semget(key3, 1, IPC_CREAT | 0666)) == -1) {
        perror("semget");
        exit(1);
    }

    if (semctl(semid1, 0, SETVAL, 1) == -1) {
        perror("semctl");
        exit(1);
    }

    if (semctl(semid2, 0, SETVAL, 0) == -1) {
        perror("semctl");
        exit(1);
    }

    if (semctl(semid3, 0, SETVAL, 1) == -1) {
        perror("semctl");
        exit(1);
    }

    sleep(1);

    semop(semid1, &sem_wait, 1);
    int* shm_array = (int*) shm;
    int student_index = shm_array[K+2];
    shm_array[K+2]++;

    // read from shared memory to array
    for (int i = 0; i < M * N * K; i++) {
        array[i] = shm_array[i+K+3];
    }
    semop(semid1, &sem_signal, 1);

    while (1) {
        semop(semid1, &sem_wait, 1);
        int* shm_array = (int*) shm;
        if (shm_array[K+1] >= M * N) {
            semop(semid1, &sem_signal, 1);
            break;
        }
        int bookshelf_index = shm_array[K+1];
        shm_array[K+1]++;
        printf("Student %d took bookshelf %d\n", student_index, bookshelf_index);
        semop(semid1, &sem_signal, 1);
        usleep(1);

        int books[K];
        int book_index = bookshelf_index * K;
        for (int j = 0; j < K; j++) {
            books[j] = array[book_index + j];
        }
        for (int j = 0; j < K; j++) {
            for (int k = j + 1; k < K; k++) {
                if (books[j] > books[k]) {
                    int tmp = books[j];
                    books[j] = books[k];
                    books[k] = tmp;
                }
            }
        }
        sleep(10);

        semop(semid1, &sem_wait, 1);

        printf("Student %d put books: ", student_index);
        for (int j = 0; j < K; j++) {
            shm_array[j] = books[j];
            printf("%d ", books[j]);
        }
        printf("\n");

        //printf("Student %d woke up librarian", i);
        semop(semid3, &sem_wait, 1);
        semop(semid2, &sem_signal, 1);

        usleep(10000);
        semop(semid2, &sem_wait, 1);
        usleep(100);
        semop(semid3, &sem_signal, 1);

        semop(semid1, &sem_signal, 1);
    }
    printf("Student %d finished\n", student_index);
    semop(semid1, &sem_signal, 1);
    shm_array[K+2]--;
    if (shm_array[K+2] == 0) {
        semop(semid2, &sem_signal, 1);
        semop(semid3, &sem_signal, 1);
    }
    semop(semid1, &sem_signal, 1);
    
    free(array);
    return 0;
}