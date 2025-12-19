# ConcurrentRequestOrchestratorAPI
This project is a simulation of a multithreaded resource orchestrator (modeled as a print service) to visualize traffic and concurrency control. It implements a multi-stage producer-consumer pattern involving multiple producers, multiple consumers, and the management of shared, finite resources.

- See React frontend [here](https://github.com/joshoti/ConcurrentRequestOrchestrator)

## Architecture
![Architecture diagram](docs/Architecture%20diagram.png)

<details><summary><i>Click to see architecture explanation</i></summary>
The service operates as a pipeline with distinct roles handled by different threads:

1. **Job Arrival:** The **Job Receiver** thread acts as the first producer. It simulates the arrival of new print jobs and places them into the `Job Queue`.
2. **Resource Provisioning:** In parallel, the **Paper Refill** thread acts as a second producer. On signal, it replenishes the `Paper Supply` in the requesting printer when it lacks sufficient paper to process the job at the front of the `Job Queue`.
3. **Job Servicing:** The two **Server** threads (printers) act as the final consumers. They concurrently pull jobs from the `Job Queue` and simulate the "printing" process, after which the job is complete.</details>

## Local Environment Setup
To compile and run terminal version, execute the below command in the terminal window
```sh
make -s bin/cli && ./bin/cli
```

To see list of configurable options, execute the below command in the terminal window
```sh
make -s bin/cli && ./bin/cli -help
```