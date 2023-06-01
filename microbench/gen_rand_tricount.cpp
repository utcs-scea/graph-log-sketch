#include <iostream>
#include <sstream>
#include <fstream>
#include <mpi.h>
#include "galois/Galois.h"
#include "galois/DistGalois.h"

struct RMAT_args_t
{
  uint64_t seed;
  uint64_t scale;
  double A;
  double B;
  double C;
  double D;
};

void RMAT(const RMAT_args_t & args, uint64_t ndx, std::pair<uint64_t, uint64_t>* edges)
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
    edges[ndx] = std::pair<uint64_t, uint64_t>(src % num_v, dst % num_v);
  }
}

int main(int argc, char** argv)
{
  galois::DistMemSys G;

  auto& net = galois::runtime::getSystemNetworkInterface();
  auto myrank = net.ID;
  auto nprocs = net.Num;

  if (argc != 9)
  {
    if(myrank == 0) printf("Usage: <nthreads> <seed> <scale> <edge ratio> <A> <B> <C> <outfile-tag>\n");
    return 1;
  }

  std::stringstream ofn;
  ofn << argv[8] << "-" << myrank << ".el";
  std::cout << ofn.str() << std::endl;
  std::ofstream ofs(ofn.str(), std::ofstream::out);

  galois::setActiveThreads(std::stoll(argv[1]));
  uint64_t seed = std::stoll(argv[2]);
  uint64_t scale = std::stoll(argv[3]);
  uint64_t num_vertices = (uint64_t) 1 << scale;
  uint64_t tot_edges = num_vertices * std::stoll(argv[4]);

  uint64_t num_big_ranks = tot_edges % nprocs;
  uint64_t num_edges = (myrank < num_big_ranks) ? tot_edges/nprocs + 1 : tot_edges/nprocs;
  uint64_t num_bumps = num_big_ranks * (tot_edges/nprocs + 1) + (myrank - num_big_ranks) *(tot_edges/nprocs);


  using namespace galois;

  double A = std::stod(argv[5]) / 100.0;
  double B = std::stod(argv[6]) / 100.0;
  double C = std::stod(argv[7]) / 100.0;
  double D = 1.0 - A - B - C;

  if ( (A <= 0.0) || (B <= 0.0) || (C <= 0.0) || (A + B + C >= 1.0) )
     {printf("sector probablitiies must be greater than 0.0 and sum to 1.0\n"); return 1;}

  RMAT_args_t rmat_args = {seed + myrank, scale, A, B, C, D};

  std::pair<uint64_t, uint64_t>* edges = new std::pair<uint64_t, uint64_t>[num_edges];

  //generate the random edges
  double t02;
  galois::runtime::getHostBarrier().wait();
  double t01 = MPI_Wtime();
  galois::runtime::getHostBarrier().wait();
  galois::do_all(galois::iterate((uint64_t)0, num_edges),
      [&](uint64_t i)
      {
        RMAT(rmat_args, i + num_bumps, edges);
      }, galois::steal());
  galois::runtime::getHostBarrier().wait();
  t02 = MPI_Wtime();

  if(myrank == 0) std::cerr << "Graph creation time:" << "\t" << t02 - t01 << std::endl;

  for(uint64_t k = 0; k < num_edges; k++)
  {
      const std::pair<uint64_t, uint64_t>& p = edges[k];
      ofs << p.first << "\t" << p.second << std::endl;
  }

  delete[] edges;
}
