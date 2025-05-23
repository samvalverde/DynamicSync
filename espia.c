// espia.c FINAL - Muestra estado de memoria, procesos activos, ejecutando y bloqueados
#include <stdio.h>
#include <stdlib.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>

#define SHM_KEY 0x1234
#define SEM_KEY 0x5678
#define MAX_LINEAS 1000
#define BITACORA_FILE "bitacora.txt"

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

void listar_pids_bloqueados() {
    FILE *log = fopen(BITACORA_FILE, "r");
    if (!log) {
        printf("No se pudo abrir la bitácora.\n");
        return;
    }

    char linea[256];
    int encontrados = 0;

    printf("===== PROCESOS BLOQUEADOS (SIN ESPACIO) =====\n");
    while (fgets(linea, sizeof(linea), log)) {
        char *p = strstr(linea, "PID ");
        if (p && strstr(linea, "No encontr")) {
            int pid;
            if (sscanf(p, "PID %d", &pid) == 1) {
                printf("  - PID %d\n", pid);
                encontrados++;
            }
        }
    }

    if (encontrados == 0)
        printf("  Ninguno\n");

    fclose(log);
}

void mostrar_fragmentacion(Memoria *mem) {
    int huecos = 0;
    int total_huecos = 0;

    for (int i = 0; i < mem->total_lineas;) {
        if (mem->lineas[i] == 0) {
            int inicio = i;
            while (i < mem->total_lineas && mem->lineas[i] == 0)
                i++;
            int tam = i - inicio;
            printf("  - Hueco libre de tamaño %d (líneas %d a %d)\n", tam, inicio, i - 1);
            huecos++;
            total_huecos += tam;
        } else {
            i++;
        }
    }

    printf("Total de huecos: %d, líneas libres: %d\n", huecos, total_huecos);
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

    printf("===== ESTADO DE LA MEMORIA (%d líneas) =====\n", mem->total_lineas);
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

    up(semid, 0);

    printf("\n===== PROCESOS CON ACCESO A MEMORIA =====\n");
    if (total_pids == 0) {
        printf("  Ninguno\n");
    } else {
        for (int i = 0; i < total_pids; ++i)
            printf("  - PID %d\n", pids[i]);
    }

    printf("\n===== PROCESOS EJECUTANDO =====\n");
    if (total_pids == 0) {
        printf("  Ninguno\n");
    } else {
        for (int i = 0; i < total_pids; ++i)
            printf("  - PID %d\n", pids[i]);
    }

    printf("\n");
    listar_pids_bloqueados();

    printf("\n===== FRAGMENTACIÓN =====\n");
    mostrar_fragmentacion(mem);


    shmdt(mem);
    return 0;
}