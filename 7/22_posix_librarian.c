#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <semaphore.h>
#include <time.h>

#define SHM_SIZE 1024

sem_t *sem_students;
sem_t *sem_librarian;
sem_t *sem_cont;

int shm_fd;
void *shm_ptr;

void terminate(int num) {
    sem_close(sem_students);
    sem_close(sem_librarian);
    sem_close(sem_cont);
    sem_unlink("/students");
    sem_unlink("/librarian");
    sem_unlink("/cont");
    shm_unlink("/shared");
    munmap(shm_ptr, SHM_SIZE);
    close(shm_fd);

    exit(EXIT_FAILURE);
}

int main(int argc, char *argv[]) {
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

    // create shared memory
    shm_fd = shm_open("/shared", O_CREAT | O_RDWR, 0666);
    ftruncate(shm_fd, SHM_SIZE);
    shm_ptr = mmap(NULL, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (shm_ptr == MAP_FAILED) {
        perror("mmap");
        exit(EXIT_FAILURE);
    }


    int* shm_arr = (int*) shm_ptr;
    shm_arr[K+1] = 0;
    shm_arr[K+2] = 0;

    

    // create semaphore
    sem_students = sem_open("/students", O_CREAT, 0666, 1);
    sem_librarian = sem_open("/librarian", O_CREAT, 0666, 0);
    sem_cont = sem_open("/cont", O_CREAT, 0666, 1);

    if (sem_students == SEM_FAILED || sem_librarian == SEM_FAILED || sem_cont == SEM_FAILED) {
        perror("sem_open");
        exit(EXIT_FAILURE);
    }

    sleep(1);

    // write array to shared memory
    sem_wait(sem_students);
    for (int i = 0; i < M * N * K; i++) {
        shm_arr[i+K+3] = array[i];
    }
    sem_post(sem_students);

    int last_bookshelf = 0;
    int result[M * N * K];

    while (1) {
        sem_wait(sem_librarian);
        if (last_bookshelf >= M * N) {
            sem_post(sem_librarian);
            break;
        }
        int* shm_array = (int*) shm_ptr;
        printf("Librarian took books: ");
        for (int i = 0; i < K; i++) {
            printf("%d ", shm_array[i]);
            result[last_bookshelf * K + i] = shm_array[i];
        }
        printf("\n");
        last_bookshelf++;
        sem_post(sem_librarian);
        sem_wait(sem_cont);
        usleep(10);
        sem_post(sem_cont);
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

    sem_close(sem_students);
    sem_close(sem_librarian);
    sem_close(sem_cont);
    sem_unlink("/students");
    sem_unlink("/librarian");
    sem_unlink("/cont");
    shm_unlink("/shared");
    munmap(shm_ptr, SHM_SIZE);
    close(shm_fd);
    free(array);
    return 0;
}