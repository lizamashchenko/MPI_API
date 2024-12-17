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

        std::cout << config.shared_mem_name << std::endl;

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
            std::cout << "Rank 0 nullify barrier count" << std::endl;
            *barrier_count = 0;
        }
    } else if (config.mode == 1) {
        std::cout << "Process ID: " << rank << std::endl;
        if (rank == 0) {
            *barrier_count = 0;
        }
        for (int i = 0; i < world_size; ++i) {
            std::string ip;
            file >> ip;
            config.ip_addresses.push_back(ip);
        }

        std::cout << "done with IP " << config.ip_addresses.size() << std::endl;


        init_network(argc, argv);
    }
}




// SYNCRONIZATION

void MyMPI::MPI_Barrier() {
    if(config.mode == 0) {

        if (rank == 0) {
            sem_unlink("/sem_barrier_lock");
        }
        std::cout << "sem open" << std::endl;

        static sem_t *barrier_lock = sem_open("/sem_barrier_lock",  O_CREAT, 0644, 1);

        if (barrier_lock == SEM_FAILED) {
            std::cerr << "Rank " << rank << " failed to open semaphore: " << strerror(errno) << std::endl;
            exit(EXIT_FAILURE);
        }

        int lock_val = -1;
        sem_getvalue(barrier_lock, &lock_val);
        std::cout << "waiting for sem " << lock_val << std::endl;

        sem_wait(barrier_lock);
        (*barrier_count)++;
        std::cout << "Rank " << rank << " barrier count incremented: " << *barrier_count << std::endl;

        sem_post(barrier_lock);

        std::cout << "Barrier size " << *barrier_count << " World size " << world_size << std::endl;


        while (*barrier_count < world_size) {
            std::cout << "Rank " << rank << " Waiting for barrier..." << std::endl;
            std::cout << "Barrier size " << *barrier_count << " World size " << world_size << std::endl;
            usleep(100);
        }

        sem_post(sem_barrier);
    } else if (config.mode == 1) {
        std::cout << "Rank " << rank << " entering barrier" << std::endl;

        if (rank == 0) {
            for (int i = 0; i < world_size - 1; ++i) {
                if (send(client_sockets[i], "BARRIER", 8, 0) < 0) {
                    perror("Barrier send failed");
                }
            }
            for (int i = 0; i < world_size - 1; ++i) {
                char buffer[8];
                if (recv(client_sockets[i], buffer, sizeof(buffer), 0) < 0) {
                    perror("Barrier receive failed");
                }
                std::cout << "Rank 0 received barrier acknowledgment from Rank " << i + 1 << std::endl;
            }
        } else {
            char buffer[8];
            if (recv(socket_fd, buffer, sizeof(buffer), 0) < 0) {
                perror("Barrier receive failed");
            }
            std::cout << "Rank " << rank << " received barrier signal from Rank 0" << std::endl;

            if (send(socket_fd, "BARRIER", 8, 0) < 0) {
                perror("Barrier send failed");
            }
        }

        std::cout << "Rank " << rank << " exiting barrier" << std::endl;
    }
}




// SYNC

void MyMPI::MPI_Send(int data) {
    if(config.mode == 0) {
        sem_wait(sem_empty);
        *shared_data = data;
        std::cout << "Rank " << rank << " sent: " << data << std::endl;
        sem_post(sem_full);
    }
    else if (config.mode == 1) {
        std::cout << "Rank " << rank << " sending data: " << data << " to Rank 0" << std::endl;
        if (rank == 0) {
            for (int i = 0; i < world_size - 1; ++i) {
                if (send(client_sockets[i], &data, sizeof(data), 0) < 0) {
                    perror("Send failed");
                }
            }
        } else {
            if (send(socket_fd, &data, sizeof(data), 0) < 0) {
                perror("Send failed");
            }
        }
    }
}

int MyMPI::MPI_Recv() {
    int data = 0;

    if(config.mode == 0) {
        sem_wait(sem_full);
        data = *shared_data;
        std::cout << "Rank " << rank << " received: " << data << std::endl;
        sem_post(sem_empty);
    }
    else if (config.mode == 1) {
        // Receive via socket
        std::cout << "Rank " << rank << " waiting to receive data" << std::endl;
        if (rank == 0) {
            // Server receives data from clients
            for (int i = 0; i < world_size - 1; ++i) {
                if (recv(client_sockets[i], &data, sizeof(data), 0) < 0) {
                    perror("Receive failed");
                }
                std::cout << "Rank " << rank << " received data: " << data << " from Rank " << i + 1 << std::endl;
            }
        } else {
            // Client receives data from server
            if (recv(socket_fd, &data, sizeof(data), 0) < 0) {
                perror("Receive failed");
            }
            std::cout << "Rank " << rank << " received data: " << data << std::endl;
        }
    }
    return data;
}



// ASYNC

void MyMPI::MPI_Isend(int data, int dest) {
    std::thread send_thread([this, data, dest]() {
        sem_wait(sem_empty);
        *shared_data = data;
        std::cout << "Rank " << rank << " asynchronously sent: " << data << " to Rank " << dest << std::endl;
        sem_post(sem_full);
        operation_status[dest] = true;
    });
    send_thread.detach();
}

int MyMPI::MPI_Irecv(int src) {
    std::thread recv_thread([this, src]() {
        sem_wait(sem_full);
        int data = *shared_data;
        std::cout << "Rank " << rank << " asynchronously received: " << data << " from Rank " << src << std::endl;
        sem_post(sem_empty);
        operation_status[src] = true;
    });
    recv_thread.detach();
    return -1;
}

bool MyMPI::MPI_Test(int request) {
    auto status = operation_status.find(request);
    if (status != operation_status.end()) {
        return status->second;
    }
    return false;
}


// SOCKETS

void MyMPI::init_network(int argc, char** argv) {
    if (rank == 0) {
        socket_fd = socket(AF_INET, SOCK_STREAM, 0);
        if (socket_fd == -1) {
            perror("Socket creation failed");
            exit(1);
        }

        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(5000);
        inet_pton(AF_INET, config.ip_addresses[0].c_str(), &server_addr.sin_addr);

        if (bind(socket_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
            perror("Bind failed");
            exit(1);
        }

        if (listen(socket_fd, world_size - 1) < 0) {
            perror("Listen failed");
            exit(1);
        }

        socklen_t client_len = sizeof(client_addr);
        for (int i = 1; i < world_size; ++i) {
            int client_socket = accept(socket_fd, (struct sockaddr*)&client_addr, &client_len);
            if (client_socket < 0) {
                perror("Accept failed");
                exit(1);
            }
            client_sockets.push_back(client_socket);
        }
    } else {
        socket_fd = socket(AF_INET, SOCK_STREAM, 0);
        if (socket_fd == -1) {
            perror("Socket creation failed");
            exit(1);
        }

        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(5000);
        inet_pton(AF_INET, config.ip_addresses[rank].c_str(), &server_addr.sin_addr);

        if (connect(socket_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
            perror("Connect failed");
            exit(1);
        }
    }
}





// ADDITIONAL

void MyMPI::MPI_Finalize() {
    if (config.mode == 0) {
        munmap(shared_data, SHM_SIZE);
        munmap(barrier_count, sizeof(int));
        close(barrier_count_fd);
        sem_close(sem_barrier);
        sem_close(sem_empty);
        sem_close(sem_full);
        if (rank == 0) {
            shm_unlink(config.shared_mem_name.c_str());
            sem_unlink("/sem_barrier_lock");
            sem_unlink(SEM_BARRIER_NAME);
            sem_unlink(SEM_EMPTY_NAME);
            sem_unlink(SEM_FULL_NAME);
        }
    } else if (config.mode == 1) {
        if (rank == 0) {
            for (int i = 0; i < world_size - 1; ++i) {
                close(client_sockets[i]);
            }
        }
        close(socket_fd);
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