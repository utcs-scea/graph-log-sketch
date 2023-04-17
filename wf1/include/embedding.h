#ifndef EMBEDDING_H_
#define EMBEDDING_H_

#include <string>
#include "gnn.h"
#include "ego_graph.h"
// #include "graph.h"
// #include "main.h"
// #include "wmd.h"

namespace agile::workflow1 {
    void GNN( const CSR_t *graph,
    std::string modelFileName);

} // namespace agile::workflow1

#endif  // EMBEDDING_H_
