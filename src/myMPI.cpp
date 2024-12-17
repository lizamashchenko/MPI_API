#include "myMPI.h"

#include <fstream>
#include <thread>

MyMPI::MyMPI(int argc, char** argv) {
    if (argc < 3) {
        throw std::runtime_error("Insufficient arguments provided to MyMPI.");
    }

    rank = std::stoi(argv[1]);
    std::string config_file = argv[2];

    std::ifstream file(config_file);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open configuration file.");
    }

    file >> config.mode >> config.process_count;

    world_size = config.process_count;

    if (config.mode == 0) {
        file >> config.shared_mem_name;

        // Initialize semaphores
        sem_barrier = sem_open(SEM_BARRIER_NAME, O_CREAT, 0644, 0);
        sem_empty = sem_open(SEM_EMPTY_NAME, O_CREAT, 0644, 1);
        sem_full = sem_open(SEM_FULL_NAME, O_CREAT, 0644, 0);

        if (sem_barrier == SEM_FAILED || sem_empty == SEM_FAILED || sem_full == SEM_FAILED) {
            perror("Semaphore initialization failed");
            exit(1);
        }

        // Initialize shared mem
        int shm_fd = shm_open(config.shared_mem_name.c_str(), O_CREAT | O_RDWR, 0644);
        if (shm_fd == -1) {
            perror("Shared memory failed");
            exit(1);
        }
        ftruncate(shm_fd, SHM_SIZE);
        shared_data = static_cast<int *>(mmap(nullptr, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0));

        // Initialize barrier
        barrier_count_fd = shm_open("/shm_barrier_count", O_CREAT | O_RDWR, 0644);
        ftruncate(barrier_count_fd, sizeof(int));
        barrier_count = static_cast<int *>(mmap(nullptr, sizeof(int), PROT_READ | PROT_WRITE, MAP_SHARED, barrier_count_fd, 0));
        if (shared_data == MAP_FAILED || barrier_count == MAP_FAILED) {
            perror("Memory mapping failed");
            exit(1);
        }

        if (rank == 0) {
            *barrier_count = 0;
        }
    } else {
        // network
    }


}

void MyMPI::MPI_Barrier() {
    static sem_t *barrier_lock = sem_open("/sem_barrier_lock", O_CREAT, 0644, 1);

    sem_wait(barrier_lock);
    (*barrier_count)++;
    sem_post(barrier_lock);

    while (*barrier_count < world_size) {
        usleep(100);
    }

    sem_post(sem_barrier);
}

void MyMPI::MPI_Send(int data) {
    sem_wait(sem_empty);
    *shared_data = data;
    std::cout << "Rank " << rank << " sent: " << data << std::endl;
    sem_post(sem_full);
}

int MyMPI::MPI_Recv() {
    sem_wait(sem_full);
    int data = *shared_data;
    std::cout << "Rank " << rank << " received: " << data << std::endl;
    sem_post(sem_empty);
    return data;
}


void MyMPI::MPI_Isend(int data, int dest) {
    std::thread send_thread([this, data, dest]() {
        sem_wait(sem_empty);
        *shared_data = data;
        std::cout << "Rank " << rank << " asynchronously sent: " << data << " to Rank " << dest << std::endl;
        sem_post(sem_full);
        operation_status[dest] = true;  // Операція завершена
    });
    send_thread.detach();
}

int MyMPI::MPI_Irecv(int src) {
    std::thread recv_thread([this, src]() {
        sem_wait(sem_full);
        int data = *shared_data;
        std::cout << "Rank " << rank << " asynchronously received: " << data << " from Rank " << src << std::endl;
        sem_post(sem_empty);
        operation_status[src] = true;  // Операція завершена
    });
    recv_thread.detach();
    return -1;  // Для асинхронного виклику, результат ще не доступний
}

bool MyMPI::MPI_Test(int request) {
    auto status = operation_status.find(request);
    if (status != operation_status.end()) {
        return status->second; // Повертає true, якщо операція завершена
    }
    return false;
}



void MyMPI::MPI_Finalize() {
    // Clear shared mem
    munmap(shared_data, SHM_SIZE);
    munmap(barrier_count, sizeof(int));
    close(barrier_count_fd);

    // Close semaphores
    sem_close(sem_barrier);
    sem_close(sem_empty);
    sem_close(sem_full);

    if (rank == 0) {
        shm_unlink(config.shared_mem_name.c_str());
        shm_unlink("/shm_barrier_count");
        sem_unlink(SEM_BARRIER_NAME);
        sem_unlink(SEM_EMPTY_NAME);
        sem_unlink(SEM_FULL_NAME);
    }
}

int MyMPI::get_size() {
    return world_size;
}

int MyMPI::get_rank() {
    return rank;
}

MyMPI::~MyMPI() {
    MPI_Finalize();
}