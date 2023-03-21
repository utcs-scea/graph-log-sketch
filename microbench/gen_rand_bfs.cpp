
#define DETERMINISTIC
#define BOTTOMUPTIME
#include <iostream>
#include <mpi.h>
#include <omp.h>
#ifndef __STDC_CONSTANT_MACROS
#define __STDC_CONSTANT_MACROS
#endif
#ifndef __STDC_LIMIT_MACROS
#define __STDC_LIMIT_MACROS
#endif
#include <stdint.h>

#define EDGEFACTOR 16

double cblas_alltoalltime;
double cblas_allgathertime;
double cblas_mergeconttime;
double cblas_transvectime;
double cblas_localspmvtime;
double cblas_ewisemulttime;

double bottomup_sendrecv;
double bottomup_allgather;
double bottomup_total;
double bottomup_convert;

double bu_local;
double bu_update;
double bu_rotate;
int cblas_splits;

#include "CombBLAS/CombBLAS.h"

int main(int argc, char** argv)
{
  using namespace combblas;
  cblas_splits = omp_get_max_threads();

  int nprocs, myrank;
  int provided;

  MPI_Init_thread(&argc, &argv, MPI_THREAD_FUNNELED, &provided);

  MPI_Comm_size(MPI_COMM_WORLD,&nprocs);
  MPI_Comm_rank(MPI_COMM_WORLD,&myrank);

  if(argc < 2)
  {
    if(myrank == 0)
    {
			cout << "Usage:   ./gen-rand-bfs <Scale>" << endl;
			cout << "Example: ./gen-rand-bfs 25" << endl;
    }
    MPI_Finalize();
    return -1;
  }

  unsigned scale = static_cast<unsigned>(atoi(argv[1]));
  if(myrank == 0)
  {
    std::cerr << "Forcing scale to : " << scale << endl;
  }

  double initiator[4] = {.57, .19, .19, .05};

  double t02;
  MPI_Barrier(MPI_COMM_WORLD);
  double t01 = MPI_Wtime();
  MPI_Barrier(MPI_COMM_WORLD);
  DistEdgeList<int64_t> * DEL = new DistEdgeList<int64_t>();
  DEL->GenGraph500Data(initiator, scale, EDGEFACTOR, true, true);
  MPI_Barrier(MPI_COMM_WORLD);
  t02 = MPI_Wtime();

  if(myrank == 0)
  {
    std::cerr << "Generation took " << t02-t01 << " seconds" << endl;
  }

  combblas::packed_edge* pedges = DEL->getPackedEdges();
  for(int64_t i = 0; i < DEL->getNumLocalEdges(); i++)
  {
    std::cout << get_v0_from_edge(&pedges[i]) << "\t" << get_v1_from_edge(&pedges[i]) << std::endl;
  }
  MPI_Finalize();
}
