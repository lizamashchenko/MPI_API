#include "src/myMPI.h"

int main(int argc, char** argv) {
    MyMPI mpi(argc, argv);
    int rank = mpi.get_rank();
    int size = mpi.get_size();

    if (rank == 0) { // Producer
        int data = 42;
        mpi.MPI_Barrier();
        mpi.MPI_Send(data);
    } else if (rank == 1) { // Consumer
        mpi.MPI_Barrier();
        mpi.MPI_Recv();
    }

    return 0;
}