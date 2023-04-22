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

    // create shared memory
    shm_fd = shm_open("/shared", O_CREAT | O_RDWR, 0666);
    //ftruncate(shm_fd, SHM_SIZE);
    shm_ptr = mmap(NULL, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (shm_ptr == MAP_FAILED) {
        perror("mmap");
        exit(EXIT_FAILURE);
    }
    int* shm_arr = (int*) shm_ptr;

    // create semaphore
    sem_students = sem_open("/students", O_CREAT, 0666, 1);
    sem_librarian = sem_open("/librarian", O_CREAT, 0666, 0);
    sem_cont = sem_open("/cont", O_CREAT, 0666, 1);

    if (sem_students == SEM_FAILED || sem_librarian == SEM_FAILED || sem_cont == SEM_FAILED) {
        perror("sem_open");
        exit(EXIT_FAILURE);
    }

    sleep(1);

    sem_wait(sem_students);
    int student_index = shm_arr[K+2];
    shm_arr[K+2]++;

    // read from shared memory to array
    for (int i = 0; i < M * N * K; i++) {
        array[i] = shm_arr[i+K+3];
    }

    sem_post(sem_students);

    while (1) {
        sem_wait(sem_students);
        int* shm_array = (int*) shm_ptr;
        if (shm_array[K+1] >= M * N) {
            sem_post(sem_students);
            break;
        }
        int bookshelf_index = shm_array[K+1];
        shm_array[K+1]++;
        printf("Student %d took bookshelf %d\n", student_index, bookshelf_index);
        sem_post(sem_students);
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
        sleep(5);

        sem_wait(sem_students);

        printf("Student %d put books: ", student_index);
        for (int j = 0; j < K; j++) {
            shm_array[j] = books[j];
            printf("%d ", books[j]);
        }
        printf("\n");

        //printf("Student %d woke up librarian", i);
        sem_wait(sem_cont);
        sem_post(sem_librarian);

        usleep(10000);
        sem_wait(sem_librarian);
        usleep(100);
        sem_post(sem_cont);

        sem_post(sem_students);
    }

    printf("Student %d finished\n", student_index);
    sem_wait(sem_students);
    int* shm_array = (int*) shm_ptr;
    shm_array[K+2]--;
    if (shm_array[K+2] == 0) {
        sem_post(sem_librarian);
        sem_post(sem_cont);
    }
    sem_post(sem_students);

    free(array);
    return 0;
}