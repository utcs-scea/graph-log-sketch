//===------------------------------------------------------------*- C++ -*-===//
//
//                            The AGILE Workflows
//
//===----------------------------------------------------------------------===//
// ** Pre-Copyright Notice
//
// This computer software was prepared by Battelle Memorial Institute,
// hereinafter the Contractor, under Contract No. DE-AC05-76RL01830 with the
// Department of Energy (DOE). All rights in the computer software are reserved
// by DOE on behalf of the United States Government and the Contractor as
// provided in the Contract. You are authorized to use this computer software
// for Governmental purposes but it is not to be released or distributed to the
// public. NEITHER THE GOVERNMENT NOR THE CONTRACTOR MAKES ANY WARRANTY, EXPRESS
// OR IMPLIED, OR ASSUMES ANY LIABILITY FOR THE USE OF THIS SOFTWARE. This
// notice including this sentence must appear on any copies of this computer
// software.
//
// ** Disclaimer Notice
//
// This material was prepared as an account of work sponsored by an agency of
// the United States Government. Neither the United States Government nor the
// United States Department of Energy, nor Battelle, nor any of their employees,
// nor any jurisdiction or organization that has cooperated in the development
// of these materials, makes any warranty, express or implied, or assumes any
// legal liability or responsibility for the accuracy, completeness, or
// usefulness or any information, apparatus, product, software, or process
// disclosed, or represents that its use would not infringe privately owned
// rights. Reference herein to any specific commercial product, process, or
// service by trade name, trademark, manufacturer, or otherwise does not
// necessarily constitute or imply its endorsement, recommendation, or favoring
// by the United States Government or any agency thereof, or Battelle Memorial
// Institute. The views and opinions of authors expressed herein do not
// necessarily state or reflect those of the United States Government or any
// agency thereof.
//
//                    PACIFIC NORTHWEST NATIONAL LABORATORY
//                                 operated by
//                                   BATTELLE
//                                   for the
//                      UNITED STATES DEPARTMENT OF ENERGY
//                       under Contract DE-AC05-76RL01830
//===----------------------------------------------------------------------===//

#include "pakman.hpp"

#include "fasta.hpp"
#include "llvm/Support/CommandLine.h"

/////////////////////////////////////////

#include "galois/Galois.h"
#include "galois/gstl.h"
#include "galois/Reduction.h"
#include "galois/Timer.h"
#include "galois/graphs/MorphGraph.h"
#include "galois/graphs/TypeTraits.h"
#include <graph_benchmark_style.hpp>

#include <iostream>
#include <deque>
#include <type_traits>
#include <queue>

#include <benchmark.hpp>

namespace cll = llvm::cl;

static const char* name = "PaKman";

static const cll::opt<std::uint64_t> num_threads("t", cll::desc("<num threads>"), cll::init(1));

static const cll::opt<std::string> input_file(
  "fasta-file", cll::desc("<input fasta file>"), cll::Required);
static const cll::opt<std::uint64_t> kmer_length("k", cll::desc("<kmer length>"), cll::init(32));
static const cll::opt<std::uint64_t> coverage("c", cll::desc("<coverage>"), cll::init(100));
static const cll::opt<std::uint64_t> min_length_count("min-length", cll::desc("<minimum length count>"), cll::init(21));
static const cll::opt<std::uint64_t> node_threshold("node-threshold", cll::desc("<compact graph until threshold number of nodes reached>"), cll::init(100000));

int main(int argc, char *argv[]) {
  cll::ParseCommandLineOptions(argc, argv);
  galois::SharedMemSys G;
  galois::setActiveThreads(num_threads);

  std::uint64_t mn_length = kmer_length - 1;
  fasta::ingest(input_file, mn_length, min_length_count);
}

/*

#include "agile/workflow3/main.h"
#include "agile/workflow3/graph.h"

namespace shad {
  using namespace agile::workflow3;

int main(int argc, char *argv[]) {
  if (argc != 6) {
     printf("Command parameters: <file name> <kmer_length> ");
     printf("<coverage> <min counts> <node threshold> \n");
     exit(-1);
  }

  double time1 = my_timer();
  std::string filename = argv[1];
  uint64_t min_counts  = std::stoull(argv[4]);
  uint64_t node_threshold = std::stoull(argv[5]);

// CREATE DATA STRUCTURES AND ARGS
  rt:: Handle handle;
  auto KMap = KMapType::Create(LARGE);                     // distinct kmer hashmap
  auto MNMap = MNMapType::Create(LARGE);                   // macro node multimap
  auto WireMap = WireMapType::Create(LARGE);               // wire multimap
  auto ModifiedNodes = ModifiedMapType::Create(LARGE);     // modified nodes multimap
  auto ProcessedNodes = IntSet::Create(LARGE);             // set of processed macro nodes
  auto BucketCounts = IntArray::Create(min_counts, 0);     // array to count kmers appearing [1..min_count] times

  BucketCounts->FillPtrs();

  Args_t args;
  args.KMap_OID = (uint64_t) (KMap->GetGlobalID());
  args.MNMap_OID = (uint64_t) (MNMap->GetGlobalID());
  args.WireMap_OID = (uint64_t) (WireMap->GetGlobalID());
  args.ModifiedNodes_OID = (uint64_t) (ModifiedNodes->GetGlobalID());
  args.ProcessedNodes_OID = (uint64_t) (ProcessedNodes->GetGlobalID());
  args.BucketCounts_OID = (uint64_t) (BucketCounts->GetGlobalID());

  args.mnLength   = std::stoull(argv[2]) - 1;
  args.coverage   = std::stoull(argv[3]);
  args.min_counts = std::stoull(argv[4]);
  memcpy(args.filename, filename.c_str(), filename.size() + 1);

  printf("Reading file\n");
// READ FASTA FILE AND CONSTRUCT KMER HASH MAP
  shad::rt::asyncExecuteOnAll(handle, readFASTA, args);                  // read FASTA file
  rt::waitForCompletion(handle);
  KMap->WaitForBufferedInsert();

  printf("Time to read FASTA file = %lf\n", my_timer() - time1);
  printf("Number of distinct k-mer entries = %lu\n", KMap->Size());

// CONSTRUCT MACRO NODES
  time1 = my_timer();

  shad::rt::asyncExecuteOnAll(handle, BucketCounts_, args);     // count number kmers appearing [1..min_count] times 
  rt::waitForCompletion(handle);

  args.min_index = 0;
  uint64_t min_count = ULLONG_MAX;

  for (uint64_t i = 1; i < min_counts; ++ i) {                  // compute ndx with minimum number of appearances
    uint64_t count = BucketCounts->At(i);
    if (count < min_count) {args.min_index = i; min_count = count;}
  }

  // construct a macro node for each kmer that appears > min_index times in KMap
  shad::rt::asyncExecuteOnAll(handle, ConstructMacroNodes, args);
  rt::waitForCompletion(handle);
  MNMap->WaitForBufferedInsert();

  KMap->Clear();                                                      // can delete KMap
  MNMap->AsyncForEachEntry(handle, Finish_MN_WireMaps, args);         // push MNMap terminals and WireMap defaults

  rt::waitForCompletion(handle);
  MNMap->WaitForBufferedInsert();
  WireMap->WaitForBufferedInsert();

  MNMap->AsyncForEachEntry(handle, WireMacroNodes, args);             // wire macro nodes
  rt::waitForCompletion(handle);
  WireMap->WaitForBufferedInsert();

  printf("Time to construct and wire macro nodes = %lf\n", my_timer() - time1);
  printf("Kmers appearing less than %lu times have been removed\n", args.min_index);

// CONSTRUCT CONTIGS
  uint64_t num_iterations = 0;
  uint64_t num_macro_nodes = MNMap->NumberKeys();
  printf("Initial number of macro nodes: %7lu\n", num_macro_nodes);

  time1 = my_timer();

  while (num_macro_nodes >= node_threshold) {
    ModifiedNodes->Clear();                                                 // clear multimap of modified node
    ProcessedNodes->Clear();                                                // clear list of processed nodes

    MNMap->AsyncForEachEntry(handle, ProcessMacroNode, args);               // process macro nodes
    rt::waitForCompletion(handle);

    ProcessedNodes->AsyncForEachElement(handle, DeleteMacroNode, args);     // delete processed macro nodes
    rt::waitForCompletion(handle);

    ModifiedNodes->AsyncForEachEntry(handle, ModifyMacroNode, args);        // modify macro nodes
    rt::waitForCompletion(handle);
    WireMap->WaitForBufferedInsert();

    ModifiedNodes->AsyncForEachKey(handle, RewireMacroNode, args);          // rewire macro nodes
    rt::waitForCompletion(handle);
    WireMap->WaitForBufferedInsert();

    printf("Iteration: %2lu\n", num_iterations);
    printf("     Number of modified nodes : %7lu\n", ModifiedNodes->NumberKeys());
    printf("     Number of processed nodes: %7lu\n", ProcessedNodes->Size());

    num_iterations ++;
    num_macro_nodes = MNMap->NumberKeys();
    printf("     Number of macro nodes    : %7lu\n", num_macro_nodes);
  }

  printf("Time to compress graph %lu = %lf\n", my_timer() - time1);
  time1 = my_timer();

// PRINT CONTIGS 
  MNMap->ForEachEntry(ProcessContigs, args);

  rt::waitForCompletion(handle);
  printf("Time to print contigs = %lf\n", my_timer() - time1);
  return 0;
}

}

*/
