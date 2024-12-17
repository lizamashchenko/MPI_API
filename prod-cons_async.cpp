#include "src/myMPI.h"
#include <chrono>
#include <thread>

int main(int argc, char** argv) {
    MyMPI mpi(argc, argv);
    int rank = mpi.get_rank();
    int size = mpi.get_size();

    if (rank == 0) { // Producer
        int data = 42;
        mpi.MPI_Barrier();  // Synchronize
        mpi.MPI_Isend(data, 1);  // Send data asynchronously to Rank 1

        // Wait for the operation to complete
        while (!mpi.MPI_Test(1)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));  // Poll every 100ms
        }
        std::cout << "Rank 0: Asynchronous send complete." << std::endl;
    } else if (rank == 1) { // Consumer
        std::cout << "in consumer" << std::endl;
        mpi.MPI_Barrier();  // Synchronize
        std::cout << "pass barrier consumer" << std::endl;

        mpi.MPI_Irecv(0);  // Receive data asynchronously from Rank 0

        // Wait for the operation to complete
        while (!mpi.MPI_Test(0)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));  // Poll every 100ms
        }
        std::cout << "Rank 1: Asynchronous receive complete." << std::endl;
    }

    return 0;
}
