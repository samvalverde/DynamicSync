// inicializador.c
#include <stdio.h>
#include <stdlib.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <string.h>
#include <fcntl.h>
#include <time.h>
#include <errno.h>

#define SHM_KEY 0x1234
#define SEM_KEY 0x5678
#define BITACORA_FILE "bitacora.txt"
#define MAX_LINEAS 1000 // límite máximo de líneas posibles

/*Correr codigo y bitacora
gcc -o inicializador inicializador.c
./inicializador
tail -f bitacora.txt
*/

// Estructura de memoria: cada línea puede ser 0 (libre) o un PID (>0)
typedef struct {
    int lineas[MAX_LINEAS];
    int total_lineas;
} Memoria;

// Para inicializar semáforo
union semun {
    int val;
    struct semid_ds *buf;
    unsigned short *array;
};

int main() {
    int n_lineas;

    printf("Ingrese la cantidad de líneas de memoria a simular (1-%d): ", MAX_LINEAS);
    scanf("%d", &n_lineas);

    if (n_lineas <= 0 || n_lineas > MAX_LINEAS) {
        fprintf(stderr, "Cantidad inválida de líneas.\n");
        exit(EXIT_FAILURE);
    }

    // Crear memoria compartida
    int shmid = shmget(SHM_KEY, sizeof(Memoria), IPC_CREAT | 0666);
    if (shmid < 0) {
        perror("Error al crear memoria compartida");
        exit(EXIT_FAILURE);
    }

    Memoria *mem = (Memoria *)shmat(shmid, NULL, 0);
    if (mem == (void *) -1) {
        perror("Error al mapear memoria compartida");
        shmctl(shmid, IPC_RMID, NULL);
        exit(EXIT_FAILURE);
    }

    // Inicializar la memoria
    mem->total_lineas = n_lineas;
    for (int i = 0; i < MAX_LINEAS; ++i) {
        mem->lineas[i] = 0;  // 0 significa libre
    }

    // Crear semáforos (2: uno para memoria, uno para bitácora)
    int semid = semget(SEM_KEY, 2, IPC_CREAT | 0666);
    if (semid < 0) {
        perror("Error al crear semáforos");
        shmdt(mem);
        shmctl(shmid, IPC_RMID, NULL);
        exit(EXIT_FAILURE);
    }

    union semun arg;
    arg.val = 1; // inicializar ambos en 1 (disponible)

    if (semctl(semid, 0, SETVAL, arg) == -1) {
        perror("Error al inicializar semáforo de memoria");
        exit(EXIT_FAILURE);
        goto cleanup;
    }
    if (semctl(semid, 1, SETVAL, arg) == -1) {
        perror("Error al inicializar semáforo de bitácora");
        exit(EXIT_FAILURE);
        goto cleanup;
    }

    // Crear/limpiar archivo de bitácora
    FILE *log = fopen(BITACORA_FILE, "w");
    if (!log) {
        perror("Error al crear bitácora");
        exit(EXIT_FAILURE);
        goto cleanup;
    }
    if(n_lineas == 1)
        fprintf(log, "BITÁCORA DE EVENTOS - INICIALIZADO CON %d LINEA %s\n", n_lineas, ctime(&(time_t){time(NULL)}));
    else
        fprintf(log, "BITÁCORA DE EVENTOS - INICIALIZADO CON %d LINEAS %s\n", n_lineas, ctime(&(time_t){time(NULL)}));
    fclose(log);

    printf("Inicialización completada.\n");
    printf("  > Memoria compartida creada con ID: %d\n", shmid);
    printf("  > Semáforos creados con ID: %d\n", semid);
    printf("  > Archivo de bitácora listo: %s\n", BITACORA_FILE);

    // Desconectarse y terminar
    shmdt(mem);
    //printf("Inicialización completada. Memoria y semáforos listos.\n");

    return 0;

    cleanup:
    shmdt(mem);
    shmctl(shmid, IPC_RMID, NULL);
    semctl(semid, 0, IPC_RMID);
    exit(EXIT_FAILURE);
}
