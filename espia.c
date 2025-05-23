#include <stdio.h>
#include <stdlib.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <string.h>
#include <unistd.h>

#define SHM_KEY 0x1234
#define SEM_KEY 0x5678
#define MAX_LINEAS 1000

typedef struct {
    int lineas[MAX_LINEAS];
    int total_lineas;
} Memoria;

void down(int semid, int semnum) {
    struct sembuf op = {semnum, -1, 0};
    semop(semid, &op, 1);
}

void up(int semid, int semnum) {
    struct sembuf op = {semnum, 1, 0};
    semop(semid, &op, 1);
}

int ya_registrado(int pid, int *lista, int total) {
    for (int i = 0; i < total; i++)
        if (lista[i] == pid) return 1;
    return 0;
}

int main() {
    int shmid = shmget(SHM_KEY, sizeof(Memoria), 0666);
    if (shmid < 0) {
        perror("No se pudo acceder a la memoria compartida");
        exit(EXIT_FAILURE);
    }

    Memoria *mem = (Memoria *)shmat(shmid, NULL, 0);
    if (mem == (void *) -1) {
        perror("Error al mapear memoria compartida");
        exit(EXIT_FAILURE);
    }

    int semid = semget(SEM_KEY, 2, 0666);
    if (semid < 0) {
        perror("No se pudo acceder a los semáforos");
        shmdt(mem);
        exit(EXIT_FAILURE);
    }

    down(semid, 0);

    printf("Estado de la memoria (%d líneas):\n", mem->total_lineas);
    int pids[MAX_LINEAS] = {0};
    int total_pids = 0;

    for (int i = 0; i < mem->total_lineas; ++i) {
        int pid = mem->lineas[i];
        if (pid == 0) {
            printf("Línea %3d: Libre\n", i);
        } else {
            printf("Línea %3d: Ocupada por PID %d\n", i, pid);
            if (!ya_registrado(pid, pids, total_pids)) {
                pids[total_pids++] = pid;
            }
        }
    }

    printf("\nResumen de procesos activos (PID):\n");
    for (int i = 0; i < total_pids; ++i) {
        printf("  - PID lógico activo: %d\n", pids[i]);
    }

    up(semid, 0);
    shmdt(mem);
    return 0;
}
