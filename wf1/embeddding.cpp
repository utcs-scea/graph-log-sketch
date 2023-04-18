#include "embedding.h"

void GNN(CSR_t * graph,
    std::string modelFileName) {
  //Handle handle;
  // auto Vertices = VertexType::GetPtr((VertexOID)graph["Vertices"]);
  // auto Vertices = (VertexType *) graph["Vertices"];
  
  // auto Embeddings = EmbeddingType::Create(num_vertices * NUM_FEATURES, 0);

  // Embeddings->FillPtrs();
  // graph["Embeddings"] = (uint64_t)(Embeddings->GetGlobalID());
  // Args_t args = {graph["VertexEdges"], graph["Embeddings"]};

  // Vertices->AsyncForEachInRange(handle, 0, num_vertices, OneHopFeatures, args);
  // waitForCompletion(handle);

  // Vertices->AsyncForEachInRange(handle, 0, num_vertices, TwoHopFeatures, args);
  // waitForCompletion(handle);

  //std::cout << "Embeddings created" << std::endl;

//   size_t parallelThreads = shad::rt::numLocalities();
  gen_2_hop_features<CSR_t, (size_t)30>(graph);
  TrainingState<VertexClassificationWMDDataset, CSR_t> initState;
//   auto TSs = shad::Array<TrainingState<VertexClassificationWMDDataset>>::Create(
//       parallelThreads, initState);
//   auto reducerArrayOID =
//       shad::Array<uint64_t>::Create(shad::rt::numLocalities(), 0ul)
//           ->GetGlobalID();
  SetUpTrainingContext<VertexClassificationWMDDataset, CSR_t> setup(
      graph, modelFileName);
  setup(initstate);
//   shad::for_each(shad::distributed_parallel_tag{}, TSs->begin(), TSs->end(),
//                  setup);

  std::cout << "Initialized Training State" << std::endl;

  const size_t numEpochs = 200;
  for (size_t epoch = 0; epoch < numEpochs; ++epoch) {
    auto start = std::chrono::high_resolution_clock::now();
    // shad::for_each(shad::distributed_parallel_tag{}, TSs->begin(), TSs->end(),
    //                agile::workflow1::vcTrainLoop<
    //                    TrainingState<VertexClassificationWMDDataset>>);
    agile::workflow1::vcTrainLoop<
                       TrainingState<VertexClassificationWMDDataset>>(&initState);

    // vcReduceGradients<TrainingState<VertexClassificationWMDDataset>>(
    //     TSs->begin(), TSs->end());


    // shad::for_each(shad::distributed_parallel_tag{}, TSs->begin(), TSs->end(),
    //                agile::workflow1::vcBackPropAndEvaluationLoop<
    //                    TrainingState<VertexClassificationWMDDataset>>);

    agile::workflow1::vcBackPropAndEvaluationLoop<
                       TrainingState<VertexClassificationWMDDataset>>(&initState);
    auto end = std::chrono::high_resolution_clock::now();

    std::cout <<  " Time (s) : "
              << std::chrono::duration_cast<std::chrono::duration<double>>(
                     end - start)
                     .count()
              << std::endl;
  }

  std::cout << "Model Trained" << std::endl;

  //return TSs->GetGlobalID();
}