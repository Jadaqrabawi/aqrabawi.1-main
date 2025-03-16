/*
 * oss.c
 * Author: aqrabwi, 13/02/2025 (modified)
 * Description: Launches worker processes using a simulated system clock in shared memory.
 *              Maintains a process table and launches workers based on command-line parameters.
 *              Alternates sending messages to workers in round-robin order.
 *
 * Usage: oss [-h] [-n totalProcs] [-s simulLimit] [-t childTimeLimit] [-i launchIntervalMs] [-f logfile]
 *   -n totalProcs        Total number of worker processes to launch (default: 20)
 *   -s simulLimit        Maximum number of workers running concurrently (default: 5)
 *   -t childTimeLimit    Upper bound (in seconds) for a worker's run time (default: 5)
 *   -i launchIntervalMs  Interval (in simulated milliseconds) between launches (default: 100)
 *   -f logfile           Log file to write oss output to
 */

 #include <stdio.h>
 #include <stdlib.h>
 #include <unistd.h>
 #include <sys/types.h>
 #include <sys/ipc.h>
 #include <sys/shm.h>
 #include <sys/msg.h>
 #include <sys/wait.h>
 #include <signal.h>
 #include <time.h>
 #include <string.h>
 #include <errno.h>
 #include <stdbool.h>
 #include <getopt.h>
 
 #define SHMKEY 9876
 #define MSGKEY 1234
 
 #define MAX_CHILDREN 20
 #define ONE_BILLION 1000000000ULL
 
 #define DEFAULT_TOTAL_PROCS 20
 #define DEFAULT_SIMUL_LIMIT 5
 #define DEFAULT_CHILD_TIME_LIMIT 5
 #define DEFAULT_LAUNCH_INTERVAL_MS 100
 
 typedef struct {
     int occupied;
     pid_t pid;
     int startSeconds;
     int startNano;
     int messagesSent;
 } PCB;
 
 PCB processTable[MAX_CHILDREN];
 
 int shmid;
 int *shmClock;
 int msqid;
 
 int totalProcs = DEFAULT_TOTAL_PROCS;
 int simulLimit = DEFAULT_SIMUL_LIMIT;
 int childTimeLimit = DEFAULT_CHILD_TIME_LIMIT;
 int launchIntervalMs = DEFAULT_LAUNCH_INTERVAL_MS;
 char logFile[256] = "oss.log";
 
 volatile sig_atomic_t terminateFlag = 0;
 
 typedef struct {
     long mtype;
     int mtext;
 } Message;
 
 int totalMessagesSent = 0; // New counter for messages sent
 
 void cleanup(int signum) {
     if (shmClock != (void *) -1) {
         shmdt(shmClock);
     }
     shmctl(shmid, IPC_RMID, NULL);
     msgctl(msqid, IPC_RMID, NULL);
     kill(0, SIGTERM);
     exit(1);
 }
 
 void alarmHandler(int signum) {
     printf("Real time limit reached. Terminating oss and all children.\n");
     cleanup(signum);
 }
 
 void incrementClock(int runningCount) {
     int nanoIncrement = 250000000 / runningCount;
     shmClock[1] += nanoIncrement;
     if (shmClock[1] >= ONE_BILLION) {
         shmClock[0] += shmClock[1] / ONE_BILLION;
         shmClock[1] %= ONE_BILLION;
     }
 }
 
 void displayTime(FILE *logFilePtr) {
     fprintf(logFilePtr, "OSS PID: %d | SysClock: %d s, %d ns\n", getpid(), shmClock[0], shmClock[1]);
     fprintf(logFilePtr, "Process Table:\n");
     fprintf(logFilePtr, "Entry  Occupied  PID     StartSec  StartNano  MessagesSent\n");
     for (int i = 0; i < MAX_CHILDREN; i++) {
         fprintf(logFilePtr, "%-6d %-9d %-7d %-9d %-9d %-13d\n", i, processTable[i].occupied, processTable[i].pid,
                 processTable[i].startSeconds, processTable[i].startNano, processTable[i].messagesSent);
     }
     fprintf(logFilePtr, "\n");
     fflush(logFilePtr);
 }
 
 int main(int argc, char *argv[]) {
     int opt;
     while ((opt = getopt(argc, argv, "hn:s:t:i:f:")) != -1) {
         switch (opt) {
             case 'h':
                 printf("Usage: %s [-n totalProcs] [-s simulLimit] [-t childTimeLimit] [-i launchIntervalMs] [-f logfile]\n", argv[0]);
                 exit(0);
             case 'n':
                 totalProcs = atoi(optarg);
                 break;
             case 's':
                 simulLimit = atoi(optarg);
                 break;
             case 't':
                 childTimeLimit = atoi(optarg);
                 break;
             case 'i':
                 launchIntervalMs = atoi(optarg);
                 break;
             case 'f':
                 strncpy(logFile, optarg, sizeof(logFile) - 1);
                 break;
             default:
                 fprintf(stderr, "Unknown option: %c\n", opt);
                 exit(1);
         }
     }
 
     signal(SIGINT, cleanup);
     signal(SIGALRM, alarmHandler);
     alarm(60);
 
     shmid = shmget(SHMKEY, 2 * sizeof(int), IPC_CREAT | 0666);
     if (shmid == -1) {
         perror("oss: shmget");
         exit(1);
     }
     shmClock = (int *) shmat(shmid, NULL, 0);
     if (shmClock == (int *) -1) {
         perror("oss: shmat");
         exit(1);
     }
     shmClock[0] = 0;
     shmClock[1] = 0;
 
     for (int i = 0; i < MAX_CHILDREN; i++) {
         processTable[i].occupied = 0;
     }
 
     msqid = msgget(MSGKEY, IPC_CREAT | 0666);
     if (msqid == -1) {
         perror("oss: msgget");
         cleanup(0);
     }
 
     FILE *logFilePtr = fopen(logFile, "w");
     if (!logFilePtr) {
         perror("oss: fopen");
         cleanup(0);
     }
 
     int launchedCount = 0;
     int runningCount = 0;
     unsigned long long lastLaunchTime = 0;
     unsigned long long lastTableDisplayTime = 0;
     int roundRobinIndex = 0; // For round-robin messaging
 
     while (launchedCount < totalProcs || runningCount > 0) {
         // Increment clock based on current running children (if none, use 1)
         incrementClock(runningCount == 0 ? 1 : runningCount);
 
         unsigned long long currentSimTime = ((unsigned long long) shmClock[0]) * ONE_BILLION + shmClock[1];
 
         // Display process table every simulated half-second (500,000,000 ns)
         if (currentSimTime - lastTableDisplayTime >= 500000000ULL) {
             displayTime(logFilePtr);
             lastTableDisplayTime = currentSimTime;
         }
 
         // Check for terminated children (non-blocking)
         int status;
         pid_t pidTerm = waitpid(-1, &status, WNOHANG);
         if (pidTerm > 0) {
             for (int i = 0; i < MAX_CHILDREN; i++) {
                 if (processTable[i].occupied && processTable[i].pid == pidTerm) {
                     processTable[i].occupied = 0;
                     runningCount--;
                     fprintf(logFilePtr, "Child PID %d terminated.\n", pidTerm);
                     fflush(logFilePtr);
                     break;
                 }
             }
         }
 
         // Possibly launch a new child if allowed
         if (launchedCount < totalProcs && runningCount < simulLimit &&
             (currentSimTime - lastLaunchTime) >= ((unsigned long long) launchIntervalMs) * 1000000) {
 
             int slot = -1;
             for (int i = 0; i < MAX_CHILDREN; i++) {
                 if (!processTable[i].occupied) {
                     slot = i;
                     break;
                 }
             }
             if (slot != -1) {
                 int randSec = (rand() % childTimeLimit) + 1;
                 int randNano = rand() % ONE_BILLION;
 
                 pid_t pid = fork();
                 if (pid < 0) {
                     perror("oss: fork");
                     cleanup(0);
                 } else if (pid == 0) {
                     char secArg[16], nanoArg[16];
                     sprintf(secArg, "%d", randSec);
                     sprintf(nanoArg, "%d", randNano);
                     execl("./worker", "worker", secArg, nanoArg, (char *) NULL);
                     perror("oss: execl");
                     exit(1);
                 } else {
                     processTable[slot].occupied = 1;
                     processTable[slot].pid = pid;
                     processTable[slot].startSeconds = shmClock[0];
                     processTable[slot].startNano = shmClock[1];
                     processTable[slot].messagesSent = 0;
                     launchedCount++;
                     runningCount++;
                     lastLaunchTime = currentSimTime;
                     fprintf(logFilePtr, "Launched worker PID %d at simulated time %d s, %d ns.\n",
                             pid, shmClock[0], shmClock[1]);
                     displayTime(logFilePtr);
                 }
             }
         }
 
         // Round-robin: send a message to one active child and wait for its reply
         if (runningCount > 0) {
             int startIndex = roundRobinIndex;
             bool found = false;
             // Find the next occupied entry
             do {
                 if (processTable[roundRobinIndex].occupied) {
                     found = true;
                     break;
                 }
                 roundRobinIndex = (roundRobinIndex + 1) % MAX_CHILDREN;
             } while (roundRobinIndex != startIndex);
 
             if (found) {
                 Message msg;
                 msg.mtype = processTable[roundRobinIndex].pid;
                 msg.mtext = 1;
                 if (msgsnd(msqid, &msg, sizeof(msg.mtext), 0) == -1) {
                     perror("oss: msgsnd");
                     cleanup(0);
                 }
                 totalMessagesSent++;
                 processTable[roundRobinIndex].messagesSent++;
                 fprintf(logFilePtr, "OSS: Sending message to worker at index %d PID %d at time %d:%d\n",
                         roundRobinIndex, processTable[roundRobinIndex].pid, shmClock[0], shmClock[1]);
                 fflush(logFilePtr);
 
                 if (msgrcv(msqid, &msg, sizeof(msg.mtext), getpid(), 0) == -1) {
                     perror("oss: msgrcv");
                     cleanup(0);
                 }
                 fprintf(logFilePtr, "OSS: Received message from worker at index %d PID %d at time %d:%d\n",
                         roundRobinIndex, processTable[roundRobinIndex].pid, shmClock[0], shmClock[1]);
                 fflush(logFilePtr);
 
                 if (msg.mtext == 0) {
                     fprintf(logFilePtr, "OSS: Worker at index %d PID %d is planning to terminate.\n",
                             roundRobinIndex, processTable[roundRobinIndex].pid);
                     waitpid(processTable[roundRobinIndex].pid, NULL, 0);
                     processTable[roundRobinIndex].occupied = 0;
                     runningCount--;
                 }
                 roundRobinIndex = (roundRobinIndex + 1) % MAX_CHILDREN;
             }
         }
     }
 
     fprintf(logFilePtr, "Summary: Total processes launched: %d, Total messages sent: %d\n",
             launchedCount, totalMessagesSent);
     fclose(logFilePtr);
 
     shmdt(shmClock);
     shmctl(shmid, IPC_RMID, NULL);
     msgctl(msqid, IPC_RMID, NULL);
 
     return 0;
 }
 