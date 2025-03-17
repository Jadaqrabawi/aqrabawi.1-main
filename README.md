
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

# Operating System Simulation

This project simulates an operating system coordinating worker processes using a shared clock, message queues, and round-robin scheduling.

## Components

- **oss (Simulator)**
  - Maintains a simulated system clock in shared memory.
  - Launches worker processes (up to a total `-n` with a concurrent limit `-s`).
  - Communicates with workers via message queues in a round-robin fashion.
  - Logs process events (launch, messaging, termination) to the terminal and a log file.

- **worker**
  - Attaches to the shared clock.
  - Determines its termination time based on command-line parameters.
  - Waits for a message from `oss`, prints status (clock time, iteration, termination time), and responds with its state.

## Files

- `oss.c`: Main simulator.
- `worker.c`: Worker process.
- `Makefile`: Build instructions.

## Requirements

- Unix-like OS (Linux, macOS, etc.)
- GCC
- Basic knowledge of using `make`

## Compilation

```bash
make
```

## Usage

```bash
./oss -n <totalProcs> -s <simulLimit> -t <childTimeLimit> -i <launchIntervalMs> -f <logfile>
```

**Example:**

```bash
./oss -n 20 -s 5 -t 7 -i 100 -f oss.log
```

## Program Flow

1. **Initialization:**  
   Setup shared memory and message queue; clock starts at `0 s, 0 ns`.

2. **Worker Launch:**  
   Fork and exec worker processes using the provided parameters.

3. **Messaging:**  
   `oss` sends round-robin messages to workers, which check the clock, print status, and reply. Workers notify `oss` upon termination.

4. **Logging:**  
   Process events and a process table (with PID, start time, and message count) are logged.

5. **Termination:**  
   Ends after all workers finish or after 60 seconds; shared memory and queues are cleaned up.

## Cleanup

```bash
make clean
```

---
