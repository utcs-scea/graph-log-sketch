#include "embedding.h"

namespace agile::workflow1 { 
  void GNN(CSR_t * graph, std::string modelFileName) {
  
  gen_2_hop_features<CSR_t, (size_t)30>(graph);
  TrainingState<VertexClassificationWMDDataset> initState;
  SetUpTrainingContext<VertexClassificationWMDDataset, CSR_t> setup(
      graph, modelFileName);
  setup(initState);
  std::cout << "Initialized Training State" << std::endl;

  const size_t numEpochs = 200;
  for (size_t epoch = 0; epoch < numEpochs; ++epoch) {
    auto start = std::chrono::high_resolution_clock::now();
    agile::workflow1::vcTrainLoop<
                       TrainingState<VertexClassificationWMDDataset>>(initState);

    agile::workflow1::vcBackPropAndEvaluationLoop<
                       TrainingState<VertexClassificationWMDDataset>>(initState);
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
}
