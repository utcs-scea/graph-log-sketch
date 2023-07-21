#include <iostream>
#include <sstream>
#include <fstream>
#include <mpi.h>
#include <random>

struct RMAT_args_t
{
  uint64_t seed;
  uint64_t scale;
  double A;
  double B;
  double C;
  double D;
};

void RMAT(const RMAT_args_t & args, uint64_t ndx, std::ofstream& out)
{
  std::mt19937_64 gen64(args.seed + ndx);
  uint64_t max_rand = gen64.max();
  uint64_t src = 0, dst = 0;

  for (uint64_t i = 0; i < args.scale; i ++) {
    double rand = (double) gen64() / (double) max_rand;
    if      (rand <= args.A)                            { src = (src << 1) + 0; dst = (dst << 1) + 0; }
    else if (rand <= args.A + args.B)                   { src = (src << 1) + 0; dst = (dst << 1) + 1; }
    else if (rand <= args.A + args.B + args.C)          { src = (src << 1) + 1; dst = (dst << 1) + 0; }
    else if (rand <= args.A + args.B + args.C + args.D) { src = (src << 1) + 1; dst = (dst << 1) + 1; }
  }

  uint64_t num_v = ((uint64_t)1) << args.scale;

  if (src != dst) {                          // do not include self edges
    if (src > dst) std::swap(src, dst);     // make src less than dst
    out << (src % num_v) << ", " << (dst % num_v) << std::endl;
  }
}

int main(int argc, char** argv)
{

  MPI_Init(NULL, NULL);

  int nprocs_raw;
  int myrank_raw;

  MPI_Comm_size(MPI_COMM_WORLD, &nprocs_raw);
  MPI_Comm_rank(MPI_COMM_WORLD, &myrank_raw);

  if(nprocs_raw < 0 || myrank_raw < 0)
  {
    std::cout << "Something went wrong with MPI Initialization" << std::endl;
    return 2;
  }

  uint64_t nprocs = (uint64_t) nprocs_raw;
  uint64_t myrank = (uint64_t) myrank_raw;

  if (argc != 8)
  {
    if(myrank == 0) printf("Usage: <seed> <scale> <edge ratio> <A> <B> <C> <outfile-tag>\n");
    return 1;
  }

  std::stringstream ofn;
  ofn << argv[7] << "-" << myrank << ".el";
  std::cout << ofn.str() << std::endl;
  std::ofstream ofs(ofn.str(), std::ofstream::out);

  uint64_t seed = std::stoll(argv[1]);
  uint64_t scale = std::stoll(argv[2]);
  uint64_t num_vertices = (uint64_t) 1 << scale;
  uint64_t tot_edges = num_vertices * std::stoll(argv[3]);

  uint64_t num_big_ranks = tot_edges % nprocs;
  uint64_t num_edges = (myrank < num_big_ranks) ? tot_edges/nprocs + 1 : tot_edges/nprocs;
  uint64_t num_bumps = num_big_ranks * (tot_edges/nprocs + 1) + (myrank - num_big_ranks) *(tot_edges/nprocs);


  double A = std::stod(argv[4]) / 100.0;
  double B = std::stod(argv[5]) / 100.0;
  double C = std::stod(argv[6]) / 100.0;
  double D = 1.0 - A - B - C;

  if ( (A <= 0.0) || (B <= 0.0) || (C <= 0.0) || (A + B + C >= 1.0) )
     {printf("sector probablities must be greater than 0.0 and sum to 1.0\n"); return 1;}

  RMAT_args_t rmat_args = {seed + myrank, scale, A, B, C, D};

  //generate the random edges and write them out
  double t02;
  MPI_Barrier(MPI_COMM_WORLD);
  double t01 = MPI_Wtime();
  MPI_Barrier(MPI_COMM_WORLD);
  for(uint64_t i = 0; i < num_edges; i++)
  {
    RMAT(rmat_args, i + num_bumps, ofs);
  }

  MPI_Barrier(MPI_COMM_WORLD);
  t02 = MPI_Wtime();

  if(myrank == 0) std::cerr << "Graph creation and writeout time:" << "\t" << t02 - t01 << std::endl;

}
