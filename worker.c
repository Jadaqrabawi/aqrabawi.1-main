#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/msg.h>
#include <signal.h>
#include <string.h>
#include <errno.h>
#include <stdbool.h>  // Added for bool support

#define SHMKEY 9876
#define MSGKEY 1234
#define ONE_BILLION 1000000000ULL

typedef struct {
    long mtype;
    int mtext;
} Message;

int shmid;
int *shmClock;
int msqid;

void cleanup() {
    if (shmClock != (void *) -1) {
        shmdt(shmClock);
    }
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <seconds> <nanoseconds>\n", argv[0]);
        exit(1);
    }

    int targetSeconds = atoi(argv[1]);
    int targetNano = atoi(argv[2]);

    shmid = shmget(SHMKEY, 2 * sizeof(int), 0666);
    if (shmid == -1) {
        perror("worker: shmget");
        exit(1);
    }
    shmClock = (int *) shmat(shmid, NULL, 0);
    if (shmClock == (int *) -1) {
        perror("worker: shmat");
        exit(1);
    }

    msqid = msgget(MSGKEY, 0666);
    if (msqid == -1) {
        perror("worker: msgget");
        cleanup();
        exit(1);
    }

    int termSeconds = shmClock[0] + targetSeconds;
    int termNano = shmClock[1] + targetNano;
    if (termNano >= ONE_BILLION) {
        termSeconds += termNano / ONE_BILLION;
        termNano %= ONE_BILLION;
    }

    printf("WORKER PID:%d PPID:%d SysClockS: %d SysclockNano: %d TermTimeS: %d TermTimeNano: %d --Just Starting\n",
           getpid(), getppid(), shmClock[0], shmClock[1], termSeconds, termNano);

    int iterations = 0;
    bool done = false;
    while (!done) {
        Message msg;
        if (msgrcv(msqid, &msg, sizeof(msg.mtext), getpid(), 0) == -1) {
            perror("worker: msgrcv");
            cleanup();
            exit(1);
        }

        iterations++;
        printf("WORKER PID:%d PPID:%d SysClockS: %d SysclockNano: %d TermTimeS: %d TermTimeNano: %d --%d iterations have passed since starting\n",
               getpid(), getppid(), shmClock[0], shmClock[1], termSeconds, termNano, iterations);

        // Check if the termination time has passed
        if (shmClock[0] > termSeconds || (shmClock[0] == termSeconds && shmClock[1] > termNano)) {
            printf("WORKER PID:%d PPID:%d SysClockS: %d SysclockNano: %d TermTimeS: %d TermTimeNano: %d --Terminating after %d iterations.\n",
                   getpid(), getppid(), shmClock[0], shmClock[1], termSeconds, termNano, iterations);
            done = true;
            msg.mtext = 0;
        } else {
            msg.mtext = 1;
        }

        msg.mtype = getppid();
        if (msgsnd(msqid, &msg, sizeof(msg.mtext), 0) == -1) {
            perror("worker: msgsnd");
            cleanup();
            exit(1);
        }
    }

    cleanup();
    return 0;
}
