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

#include "main.h"
#include "graph.h"
#include "WMDPartitioner.h"

#include "galois/DistGalois.h"
#include "galois/graphs/GenericPartitioners.h"


using namespace agile::workflow1;

typedef galois::graphs::WMDGraph<agile::workflow1::Vertex, agile::workflow1::Edge, MiningPolicyNaive> Graph;

int main(int argc, char *argv[]) {
  printf("Begin Wf1 Galois!\n");

  galois::DistMemSys G;  // init galois memory
  auto& net = galois::runtime::getSystemNetworkInterface();

  int NUM_RUN = 1;
  int NUM_DISCARD_RUN = 0;
  unsigned long total_csr = 0;
  unsigned long total_graph = 0;
  for (int i = 0; i < NUM_DISCARD_RUN + NUM_RUN; i++) {
    // galois::Timer timer;
    // timer.start();

    // Handle handle;
    // Graph_t graph;
    std::string dataFile = argv[1];


    Graph * graph = new Graph(dataFile, net.ID, net.Num, false);
    assert(graph != nullptr);
    // EdgeType * Edges = new EdgeType(LARGE); 
    // GlobalIDType * GlobalIDS = new GlobalIDType(MEDIUM);

    // graph["Edges"] = (uint64_t) Edges;  // Vertex to Edges mapping, key is vertex encoding
    // graph["GlobalIDS"] = (uint64_t) GlobalIDS;   // Vertices, key is vertex encoding

    // RF_args_t args;
    // args.Edges_OID     = graph["Edges"];
    // args.GlobalIDS_OID = graph["GlobalIDS"];
    // memcpy(args.filename, dataFile.c_str(), dataFile.size() + 1);

    // readFile(args);

    // /********** CREATE COMPRESSED EDGE ARRAY AND VERTEX ARRAY **********/
    // if (net.ID == 0) {
    //   uint64_t num_vertices = GlobalIDS->size();
    //   uint64_t num_edges = Edges->size();

    //   timer.stop();
    //   unsigned long time_graph = timer.get_usec();

    //   timer.start();
    //   CSR(graph, num_vertices, num_edges);
    //   timer.stop();
    //   unsigned long time_csr = timer.get_usec();
      

    //   CSR_t * csr = (CSR_t *) graph["CSR"];
    //   printf("Total number of vertices = %lu\n", csr->size());
    //   printf("Total number of edges    = %lu\n", Edges->size());

    //   if (i >= NUM_DISCARD_RUN) {
    //     total_csr += time_csr;
    //     total_graph += time_graph;
    //     printf("Time for CSR = %lf\n", (double) time_csr / 1000000);
    //     printf("Time for data congestion = %lf\n", (double) time_graph / 1000000);
    //   }
    // }

    // free((EdgeType *) Edges);
    // free((GlobalIDType *) GlobalIDS);
    // free((VertexType *) graph["Vertices"]);
    // free((EdgeType *) graph["VertexEdges"]);
    // free((CSR_t *) graph["CSR"]);
  }

  // if (net.ID == 0) {
  //   printf("Avg Time for CSR = %lf\n", (double) total_csr / (NUM_RUN * 1000000));
  //   printf("Avg Time for data congestion = %lf\n", (double) total_graph / (NUM_RUN * 1000000));
  // }

  return 0;
}