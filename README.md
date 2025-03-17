
# Simulated System Clock & Worker Process Coordination (aqrabawi.1-main)

## Quick Start

1. **Download or Clone the Project**  
   Clone the repository using:
   ```bash
   git clone https://github.com/Jadaqrabawi/aqrabawi.1-main.git
   ```
   Or download and extract the project archive.

2. **Navigate to the Project Directory**
   ```bash
   cd aqrabawi.1-main-main
   ```

3. **Clean the Project (Optional)**
   ```bash
   make clean
   ```

4. **Compile the Project**
   ```bash
   make all
   ```

5. **Run the Project**
   ```bash
./oss -n 20 -s 5 -t 7 -i 100 -f oss.log
   ```
## Overview

This project demonstrates an operating system simulation where a parent process (`oss`) maintains a simulated system clock and coordinates multiple worker processes (`worker`). The parent process uses shared memory and message queues to communicate with the worker processes, ensuring tight coordination by sending messages in a round-robin manner.

## Project Overview

- **oss (Operating System Simulator):**
  - Maintains a simulated system clock stored in shared memory.
  - Launches worker processes up to a specified total (`-n`) and limits the number of concurrent workers (`-s`).
  - Uses a message queue to send and receive messages from workers.
  - Implements a round-robin mechanism to coordinate messaging with active workers.
  - Logs detailed process activity, including process table information and messaging events, to both the terminal and a log file.

- **worker:**
  - Attaches to the shared memory clock and calculates its own termination time based on the provided command-line arguments.
  - Waits for a message from `oss` before checking the clock.
  - Periodically prints its status (including simulated clock time, iteration count, and termination time).
  - Sends a message back to `oss` indicating whether it is still running or planning to terminate.

## Files Included

- **oss.c**: The main program simulating the operating system. It launches and coordinates worker processes.
- **worker.c**: The worker process that periodically checks the simulated clock and communicates with `oss`.
- **Makefile**: A makefile to compile the project.

## Prerequisites

- A Unix-like operating system (Linux, macOS, etc.)
- GCC (GNU Compiler Collection)
- Basic familiarity with compiling C programs using `make`

## Compilation

1. Open a terminal and navigate to the project directory containing `oss.c`, `worker.c`, and the `Makefile`.
2. Run the following command to compile both executables:
   ```bash
   make
   ```
   This will compile the source files and generate the `oss` and `worker` executables.

## Running the Program

The `oss` program accepts the following command-line options:

```
Usage: ./oss [-h] [-n totalProcs] [-s simulLimit] [-t childTimeLimit] [-i launchIntervalMs] [-f logfile]
```

- **-h**: Display help/usage information.
- **-n totalProcs**: Total number of worker processes to launch (e.g., `20`).
- **-s simulLimit**: Maximum number of workers running concurrently (e.g., `5`).
- **-t childTimeLimit**: Upper bound (in seconds) for a worker's run time (e.g., `7`).
- **-i launchIntervalMs**: Interval in simulated milliseconds between launching new workers (e.g., `100`).
- **-f logfile**: The log file to which `oss` will write its output (e.g., `oss.log`).

### Example Command

```bash
./oss -n 20 -s 5 -t 7 -i 100 -f oss.log
```

This command will launch a total of 20 worker processes, with no more than 5 running concurrently. Each worker will have a random lifetime up to 7 seconds, and new workers are launched at simulated 100 millisecond intervals. All OSS output is logged in `oss.log`.

## Program Flow

1. **Initialization:**
   - `oss` sets up shared memory for the simulated clock and a message queue.
   - The simulated clock starts at `0 s, 0 ns`.

2. **Worker Process Launch:**
   - `oss` forks and execs `worker` processes based on the given parameters.
   - Each worker calculates its termination time using the system clock and its command-line parameters.

3. **Message Coordination:**
   - `oss` sends messages in round-robin order to active worker processes.
   - Each worker, upon receiving a message, checks the clock, prints its current status, and then sends a response.
   - When a worker’s termination time is reached, it informs `oss` and terminates.

4. **Logging and Process Table:**
   - `oss` logs each event including process launches, messaging events, and process terminations.
   - The process table (which includes details like PID, start time, and messages sent) is displayed at regular intervals.

5. **Termination:**
   - The simulation ends either after all workers have terminated or after 60 real-life seconds.
   - On termination, `oss` cleans up the shared memory and message queue.

## Cleanup

To remove the compiled executables and log file, run:

```bash
make clean
```

## Additional Notes

- The simulated clock is incremented by `250 ms` divided by the number of current active workers, ensuring that the clock progresses proportionally.
- The log file provides detailed traceability, which can be useful for debugging or analyzing the sequence of events.
