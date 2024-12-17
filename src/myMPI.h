#ifndef MYMPI_H
#define MYMPI_H

// mpi_api.h
#ifndef MPI_API_H
#define MPI_API_H

#include <atomic>
#include <fcntl.h>
#include <semaphore.h>
#include <sys/mman.h>
#include <unistd.h>
#include <vector>
#include <iostream>
#include <cstring>
#include <map>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define SHM_NAME "/shm_mpi_example"     // Shared memory name
#define SEM_BARRIER_NAME "/sem_barrier" // Barrier semaphore
#define SEM_EMPTY_NAME "/sem_empty"     // Empty buffer semaphore
#define SEM_FULL_NAME "/sem_full"       // Full buffer semaphore
#define SHM_SIZE 4096           // Size of shared memory (one int)

struct Config {
    int mode;                 // 0 - shared memory, 1 - network
    int process_count;
    std::string shared_mem_name;
    std::vector<std::string> ip_addresses;
};

class MyMPI {
public:
    MyMPI(int argc, char** argv);
    ~MyMPI();
    void MPI_Finalize();

    // sync shared mem IPC
    void MPI_Barrier();
    void MPI_Send(int data);
    int MPI_Recv();

    // async IPC
    void MPI_Isend(int data, int dest);
    int MPI_Irecv(int src);
    bool MPI_Test(int request);

    // network sockets
    void init_network(int argc, char** argv);
    void send_network(int data);
    int recv_network();


    int get_size();
    int get_rank();

private:
    int rank;
    int world_size;
    int *shared_data;

    int socket_fd;
    std::vector<int> client_sockets;
    struct sockaddr_in server_addr;
    struct sockaddr_in client_addr;

    sem_t *sem_barrier;
    sem_t *sem_empty;
    sem_t *sem_full;

    int barrier_count_fd;
    int *barrier_count;

    std::map<int, std::atomic<bool>> operation_status;

    Config config;
};

#endif

#endif //MYMPI_H
