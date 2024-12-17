#include "src/myMPI.h"

int main(int argc, char** argv) {
    MyMPI mpi(argc, argv);
    int rank = mpi.get_rank();
    int size = mpi.get_size();
    std::cout << "rank: " << rank << " size: " << size << std::endl;
    if (rank == 0) { // Producer
        std::cout << "in rank 0" << std::endl;

        int data = 42;
        mpi.MPI_Barrier();
        std::cout << "pass barrier" << std::endl;
        mpi.MPI_Send(data);
    } else if (rank == 1) { // Consumer
        std::cout << "in rank 1" << std::endl;
        mpi.MPI_Barrier();
        std::cout << "pass barrier" << std::endl;
        mpi.MPI_Recv();
    }

    return 0;
}