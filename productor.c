// productor.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/types.h>

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

typedef enum {
    FIRST_FIT = 1,
    BEST_FIT,
    WORST_FIT
} Algoritmo;

typedef struct {
    int pid;
    int tamanio;
    int duracion;
    Algoritmo algoritmo;
} ProcesoArgs;

// --------------------- SEMÁFORO ---------------------
void down(int semid, int semnum) {
    struct sembuf op = {semnum, -1, 0};
    semop(semid, &op, 1);
}

void up(int semid, int semnum) {
    struct sembuf op = {semnum, 1, 0};
    semop(semid, &op, 1);
}

// -------------------- BITÁCORA ----------------------
void escribir_bitacora(int semid, const char* mensaje) {
    down(semid, 1); // Semáforo de bitácora
    FILE *log = fopen(BITACORA_FILE, "a");
    if (log) {
        time_t now = time(NULL);
        char *timestamp = ctime(&now);
        timestamp[strcspn(timestamp, "\n")] = 0;  
        fprintf(log, "[%s] %s\n", timestamp, mensaje);
        fclose(log);
    }
    up(semid, 1);
}



// ----------------- BÚSQUEDA DE HUECO ----------------
int buscar_hueco(Memoria *mem, int size, Algoritmo alg) {
    int mejor_inicio = -1;
    int mejor_tam = (alg == BEST_FIT) ? MAX_LINEAS + 1 : -1;

    for (int i = 0; i < mem->total_lineas;) {
        if (mem->lineas[i] == 0) {
            int inicio = i;
            int espacio = 0;
            while (i < mem->total_lineas && mem->lineas[i] == 0) {
                espacio++;
                i++;
            }

            if (espacio >= size) {
                if (alg == FIRST_FIT)
                    return inicio;

                if ((alg == BEST_FIT && espacio < mejor_tam) ||
                    (alg == WORST_FIT && espacio > mejor_tam)) {
                    mejor_inicio = inicio;
                    mejor_tam = espacio;
                }
            }
        } else {
            i++;
        }
    }

    return mejor_inicio;
}

// ---------------------- THREAD ----------------------
void* simular_proceso(void* arg) {
    ProcesoArgs *p = (ProcesoArgs*)arg;

    int shmid = shmget(SHM_KEY, sizeof(Memoria), 0666);
    if (shmid < 0) {
        perror("No se pudo acceder a memoria compartida");
        pthread_exit(NULL);
    }

    Memoria *mem = (Memoria *)shmat(shmid, NULL, 0);
    if (mem == (void *) -1) {
        perror("Fallo al mapear memoria");
        pthread_exit(NULL);
    }

    int semid = semget(SEM_KEY, 2, 0666);
    if (semid < 0) {
        perror("No se pudo acceder a los semáforos");
        shmdt(mem);
        pthread_exit(NULL);
    }

    // Bloquear acceso a memoria
    down(semid, 0);

    int inicio = buscar_hueco(mem, p->tamanio, p->algoritmo);
    if (inicio == -1) {
        // Proceso muere si no hay espacio
        up(semid, 0);
        char msg[100];
        sprintf(msg, "PID %d - No encontró espacio (%d líneas)", p->pid, p->tamanio);
        escribir_bitacora(semid, msg);
        free(p);
        pthread_exit(NULL);
    }

    for (int i = 0; i < p->tamanio; i++) {
        mem->lineas[inicio + i] = p->pid;
    }

    char msg[100];
    sprintf(msg, "PID %d - Asignó %d líneas en [%d-%d]", p->pid, p->tamanio, inicio, inicio + p->tamanio - 1);
    escribir_bitacora(semid, msg);
    up(semid, 0); 

    sleep(p->duracion); // Simula ejecución

    // Liberar memoria
    down(semid, 0);     // Región crítica
    for (int i = 0; i < mem->total_lineas; i++) {
        if (mem->lineas[i] == p->pid)
            mem->lineas[i] = 0;
    }
    sprintf(msg, "PID %d - Liberó memoria después de %ds", p->pid, p->duracion);
    escribir_bitacora(semid, msg);
    up(semid, 0);

    shmdt(mem);
    free(p);
    pthread_exit(NULL);
}

// -------------------- MAIN --------------------------
int main() {
    srand(time(NULL));

    printf("Seleccione el algoritmo de asignación:\n");
    printf("1. First-Fit\n2. Best-Fit\n3. Worst-Fit\n> ");
    int tipo;
    scanf("%d", &tipo);
    if (tipo < 1 || tipo > 3) {
        printf("Algoritmo inválido.\n");
        return 1;
    }

    Algoritmo algoritmo = (Algoritmo)tipo;
    printf("Productor iniciado. Usando algoritmo %d\n", algoritmo);

    int contador_pid = 1;
    while (1) {
        ProcesoArgs *p = malloc(sizeof(ProcesoArgs));
        p->pid = getpid() * 1000 + contador_pid++; // Pseudo PID único
        p->tamanio = (rand() % 10) + 1; // 1-10 líneas
        //p->duracion = 20 + rand() % 41; // 20-60 segundos
        p->duracion = 5 + rand() % 6;  // duración muy corta (5–10s)
        p->algoritmo = algoritmo;

        pthread_t hilo;
        pthread_create(&hilo, NULL, simular_proceso, p);
        pthread_detach(hilo);

        //int intervalo = 30 + rand() % 31; // 30-60 segundos
        //sleep(intervalo);
        int intervalo = 1 + rand() % 2;  // Genera un proceso cada 1-2 segundos
        sleep(intervalo);
    }

    return 0;
}
