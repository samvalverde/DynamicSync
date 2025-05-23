#include <stdio.h>
#include <stdlib.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <unistd.h>
#include <time.h>

#define SHM_KEY 0x1234
#define SEM_KEY 0x5678
#define BITACORA_FILE "bitacora.txt"
#define MAX_LINEAS 1000

typedef struct {
    int lineas[MAX_LINEAS];
    int total_lineas;
} Memoria;

union semun {
    int val;
    struct semid_ds *buf;
    unsigned short *array;
};

void down(int semid, int semnum) {
    struct sembuf op = {semnum, -1, 0};
    semop(semid, &op, 1);
}

void up(int semid, int semnum) {
    struct sembuf op = {semnum, 1, 0};
    semop(semid, &op, 1);
}

void escribir_bitacora(int semid, const char* mensaje) {
    down(semid, 1);
    FILE *log = fopen(BITACORA_FILE, "a");
    if (log) {
        time_t now = time(NULL);
        fprintf(log, "[%s] %s\n", ctime(&now), mensaje);
        fclose(log);
    }
    up(semid, 1);
}

int main() {
    int shmid = shmget(SHM_KEY, sizeof(Memoria), 0666);
    if (shmid < 0) {
        perror("Finalizador: no se pudo acceder a la memoria compartida");
        exit(EXIT_FAILURE);
    }

    Memoria *mem = (Memoria *)shmat(shmid, NULL, 0);
    if (mem == (void *) -1) {
        perror("Finalizador: error al mapear memoria compartida");
        exit(EXIT_FAILURE);
    }

    int semid = semget(SEM_KEY, 2, 0666);
    if (semid < 0) {
        perror("Finalizador: no se pudo acceder a los semáforos");
        shmdt(mem);
        exit(EXIT_FAILURE);
    }

    down(semid, 0);
    int liberadas = 0;
    for (int i = 0; i < mem->total_lineas; ++i) {
        if (mem->lineas[i] != 0) {
            mem->lineas[i] = 0;
            liberadas++;
        }
    }
    up(semid, 0);

    char msg[128];
    sprintf(msg, "Finalizador: liberó %d líneas de memoria.", liberadas);
    escribir_bitacora(semid, msg);

    shmdt(mem);
    shmctl(shmid, IPC_RMID, NULL);
    semctl(semid, 0, IPC_RMID);
    escribir_bitacora(semid, "Finalizador: recursos eliminados. Sistema terminado.");

    printf("Sistema finalizado correctamente.\n");
    return 0;
}