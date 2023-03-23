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
#include "galois/Galois.h"
#include "galois/Timer.h"

namespace agile::workflow1 {

struct Args_t { uint64_t delta; uint64_t oid; };

// Update the global ids on this locale and spawn updateIDS on next locale.
// void updateIDS(Handle & handle, const Args_t & args) {
//   auto GlobalIDS = GlobalIDType::GetPtr((GlobalIDOID) args.oid);
//   auto updateLambda = [] (const uint64_t & key, Vertex & value, const uint64_t & delta) {value.id += delta;};

//   my_map->ForEachEntry(updateLambda, args.delta);
// }


// Fill in Vertices ... insert {key, value.edges, value.type} at index value.id
// void moveVertex(Handle & handle, const uint64_t & key, Vertex & value, uint64_t & verticesOID) {
//   auto Vertices = VertexType::GetPtr((VertexOID) verticesOID);
//   Vertices->AsyncInsertAt(handle, value.id, Vertex(key, value.edges, value.type));
// }


// // Fill in dst global id and fire-and-forget insert
// void Dst_(Handle & handle, const uint64_t & i, Vertex & dstV, uint64_t & ndx, Edge & edge, uint64_t & XEdgesOID) {
//   auto XEdges = XEdgeType::GetPtr((XEdgeOID) XEdgesOID);

//   edge.dst_glbid = dstV.id;
//   XEdges->AsyncInsertAt(handle, ndx, edge);
  
// };


// Move edges from Edges to XEdges
// void moveEdges(Handle & handle, const uint64_t & src_id, std::vector<Edge> & edges,
//      uint64_t & globalIDSOID, uint64_t & verticesOID, uint64_t & vertexEdgesOID) {
//   auto Vertices  = VertexType::GetPtr((VertexOID) verticesOID);
//   auto GlobalIDS = GlobalIDType::GetPtr((GlobalIDOID) globalIDSOID);
//   auto VertexEdges = EdgeType::GetPtr((EdgeOID) vertexEdgesOID);

//   Vertex srcVertex;
//   GlobalIDS->Lookup(src_id, &srcVertex);                                    // lookup global id for src vertex

//   // Map global id to edges
//   for (auto edge : edges) {
//     edge.src_glbid = srcVertex.id;

//     Vertex dstVertex;
//     GlobalIDS->Lookup(edge.dst, &dstVertex);
//     edge.dst_glbid = dstVertex.id;
//     VertexEdges->BufferedAsyncInsert(handle, srcVertex.id, edge);
//   }
  
//   // uint64_t ndx = Vertices->At(srcVertex.id).start;                           // start index for src vertex edges

//   // for (auto edge : edges) {                                                  // for each edge of src vertex
//   //   edge.src_glbid = srcVertex.id;
//   //   GlobalIDS->AsyncApply(handle, edge.dst, Dst_, ndx, edge, vertexEdgesOID);     // ... send edge to dst vertex and forget
//   //   ndx ++;                                                                  // ... increment edge index
//   // } 
// }



/********** CREATE COMPRESSED EDGE ARRAY AND VERTEX ARRAY **********/
void CSR(Graph_t &graph, uint64_t num_vertices, uint64_t num_edges) {
  printf("Start building CSR\n");
  galois::Timer timer;
  timer.start();
  // ***** convert local ids to global ids *****/
  EdgeType & Edges = *((EdgeType *) graph["Edges"]);
  GlobalIDType & GlobalIDS = *((GlobalIDType *) graph["GlobalIDS"]);

  // no need to updateIDS on single node
  // Args_t my_args = {0, graph["GlobalIDS"]};
  // updateIDS(my_args);

  // ***** allocate space for Vertices Array *****/
  // auto XEdges = XEdgeType::Create(num_edges, Edge());
  VertexType * Vertices = new VertexType(num_vertices + 1, Vertex());  // Vertices, key is vertex id (start from 0)
  graph["Vertices"] = (uint64_t) (Vertices);

  // Vertex to edges mapping, key is vertex id (start from 0)
  // We need this so we could lookup edges by vertex id
  EdgeType * VertexEdges = new EdgeType(num_edges);  
  graph["VertexEdges"] = (uint64_t) (VertexEdges);

  // XEdges->FillPtrs();
  // shad::rt::waitForCompletion(handle);
  // GlobalIDS->AsyncForEachEntry(handle, moveVertex, graph["Vertices"]);
  // waitForCompletion(handle);

  // ***** copy vertices from GlobalIDS to Vertices *****/
  printf("copy vertices 1!\n");
  galois::do_all(
    galois::iterate(GlobalIDS),
    [Vertices](const GlobalIDType::value_type& p) {
      (*Vertices)[p.second.id] = Vertex(p.first, p.second.edges, p.second.type);
    }
  );
  printf("copy vertices 2!\n");

  // Note: Since now we construct CSR by galois, vertex.start will not be filled anymore
  // exclusiveScanVertices<Vertex>(graph["Vertices"]);     // convert # edges to start location

  // ***** copy edges from Edges to VertexEdges *****/
  // Edges->AsyncForEachEntry(handle, moveEdges, GlobalIDSOID, graph["Vertices"], graph["VertexEdges"]);
  // waitForCompletion(handle);
  galois::do_all(
    galois::iterate(Edges),
    [Vertices, VertexEdges, GlobalIDS](EdgeType::value_type& p) {
      auto src_node = GlobalIDS.find(p.first)->second;
      auto dst_node = GlobalIDS.find(p.second.dst)->second;
      p.second.src_glbid = src_node.id;
      p.second.dst_glbid = dst_node.id;
      VertexEdges->insert({src_node.id, p.second});
    }
  );

  printf("Build CSR!\n");

  // ***** Create CSR *****/
  // Edges->AsyncForEachEntry(handle, moveEdges, GlobalIDSOID, graph["Vertices"], graph["XEdges"]);
  // waitForCompletion(handle);
  auto edgeNum_func = [VertexEdges](size_t n) {
    return VertexEdges->count(n);
  };

  auto edgeDst_func = [VertexEdges](size_t n) {
    std::vector<size_t> dst(VertexEdges->count(n));
    auto range = VertexEdges->equal_range(n);
    for (auto it = range.first; it != range.second; ++it) {
        dst.push_back(it->second.dst_glbid);
    }
    return dst.data();
  };

  auto edgeData_func = [VertexEdges](size_t n) {
    std::vector<Edge> dst(VertexEdges->count(n));
    auto range = VertexEdges->equal_range(n);
    for (auto it = range.first; it != range.second; ++it) {
        dst.push_back(it->second);
    }
    return dst.data();
  };

  graph["CSR"] = (uint64_t) new CSR_t(true, num_vertices, num_edges, edgeNum_func, edgeDst_func, edgeData_func);
  timer.stop();
  printf("Time for CSR = %lf\n", (double) timer.get_usec() / 1000000);
}

}
