#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <time.h>
#include <sys/wait.h>

#define SHM_SIZE 1024
#define ST_COUNT 3

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

    srand(time(0));
    struct sigaction act;
    act.sa_handler = terminate;
    sigaction(SIGINT, &act, NULL);
    sigaction(SIGTERM, &act, NULL);

    int M = atoi(argv[1]);
    int N = atoi(argv[2]);
    int K = atoi(argv[3]);

    int* array = malloc(M * N * K * sizeof(int));
    for (int i = 0; i < M * N * K; i++) {
        array[i] = i;
    }
    for (int i = 0; i < M * N * K; i++) {
        int j = rand() % (M * N * K);
        int tmp = array[i];
        array[i] = array[j];
        array[j] = tmp;
    }

    printf("Array: ");
    for (int i = 0; i < M * N * K; i++) {
        printf("%d ", array[i]);
    }
    printf("\n");

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
    shm_array[K+1] = 0;
    for (int i = 0; i < M * N * K; i++) {
        shm_array[i+K+3] = array[i];
    }
    semop(semid1, &sem_signal, 1);

    int last_bookshelf = 0;
    int result[M * N * K];

    while (1) {
        semop(semid2, &sem_wait, 1);
        if (last_bookshelf >= M * N) {
            semop(semid2, &sem_signal, 1);
            break;
        }
        int* shm_array = (int*) shm;
        printf("Librarian took books: ");
        for (int i = 0; i < K; i++) {
            printf("%d ", shm_array[i]);
            result[last_bookshelf * K + i] = shm_array[i];
        }
        printf("\n");
        last_bookshelf++;
        semop(semid2, &sem_signal, 1);
        semop(semid3, &sem_wait, 1);
        usleep(10);
        semop(semid3, &sem_signal, 1);
    }

    for (int i = 0; i < ST_COUNT; i++) {
        wait(NULL);
    }

    // sort result
    for (int i = 0; i < M * N * K; i++) {
        for (int j = i + 1; j < M * N * K; j++) {
            if (result[i] > result[j]) {
                int tmp = result[i];
                result[i] = result[j];
                result[j] = tmp;
            }
        }
    }

    printf("Result: ");
    for (int i = 0; i < M * N * K; i++) {
        printf("%d ", result[i]);
    }
    printf("\n");

    shmdt(shm);
    shmctl(shmid, IPC_RMID, 0);
    semctl(semid1, 0, IPC_RMID, 0);
    semctl(semid2, 0, IPC_RMID, 0);
    semctl(semid3, 0, IPC_RMID, 0);
    free(array);
    return 0;
}