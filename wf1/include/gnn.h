#ifndef GNN_H_
#define GNN_H_

#include <cstdint>
#include <memory>
#include <vector>

#include "galois/Galois.h"

#include "graph.h"
#include "main.h"
#include "wmd.h"

#include "torch/script.h"
#include "torch/torch.h"

namespace agile::workflow1 {

template <typename Dataset> struct TrainingState {
public:
  //using ArrayOID = typename shad::Array<uint64_t>::ObjectID;
  //using ArrayOID = typename Galois::LargeArray<uint64_t>;
  using sampler_type = torch::data::samplers::DistributedRandomSampler;
  using dataset_type = torch::data::datasets::MapDataset<
      Dataset, torch::data::transforms::Stack<typename Dataset::Data>>;
  using data_loader_type =
      torch::data::StatelessDataLoader<dataset_type, sampler_type>;

  TrainingState() = default;
  TrainingState(const TrainingState &) = default;
  TrainingState(TrainingState &&) = default;

  TrainingState &operator=(const TrainingState &) = default;
  TrainingState &operator=(TrainingState &&) = default;

  torch::jit::script::Module Module;
  Dataset DataSet;
  std::shared_ptr<data_loader_type> TrainDataLoader{nullptr};
  std::shared_ptr<data_loader_type> TestDataLoader{nullptr};
  std::shared_ptr<torch::optim::Adam> Adam{nullptr};
  std::vector<torch::jit::IValue> Inputs;
  //ArrayOID ReducerLocalPtrOID{ArrayOID::kNullID};
};

template <typename Dataset, typename Graph> class SetUpTrainingContext {
  //using ArrayOID = typename shad::Array<uint64_t>::ObjectID;
  //using ArrayOID = typename Galois::LargeArray<uint64_t>;

  const int64_t trainingSetSize = 500;
  const int64_t testSetSize = 500;
  const size_t batchSize = 100;

  char modelFileName_[256];
  const Graph* _g;
  // VertexOID _verticesOID;
  // XEdgeOID _edgesOID;
  // ArrayOID _featuresOID;
  //ArrayOID _reducerArrayOID;

public:
  SetUpTrainingContext(const Graph* g,
                       std::string modelFileName)
   {
    _g = g;
    if (modelFileName.size() > 256)
      throw "Filename too long";

    std::strcpy(modelFileName_, modelFileName.c_str());
  }

  void operator()(TrainingState<Dataset> &TS) {
    // Load Module
    TS.Module = torch::jit::load(modelFileName_);
    // Load Dataset
    TS.DataSet = Dataset(_g);

    size_t numVertices = TS.DataSet.size().value();
    // Partition in Training/Test Set
    using namespace torch::indexing;
    auto options = torch::TensorOptions().dtype(torch::kBool);

    // Create DataLoader
    //uint32_t thisLocality = static_cast<uint32_t>(shad::rt::thisLocality());
    // auto train_sampler = torch::data::samplers::DistributedRandomSampler(
    //     trainingSetSize, shad::rt::numLocalities(), thisLocality, false);
    // auto test_sampler = torch::data::samplers::DistributedRandomSampler(
    //     testSetSize, shad::rt::numLocalities(), thisLocality, false);

    // auto train_sampler = torch::data::samplers::DistributedRandomSampler(
    //     trainingSetSize, 1, thisLocality, false);
    // auto test_sampler = torch::data::samplers::DistributedRandomSampler(
    //     testSetSize, shad::rt::numLocalities(), thisLocality, false);

    auto train_sampler = torch::data::samplers::DistributedRandomSampler(
        trainingSetSize, 1, 1, false);
    auto test_sampler = torch::data::samplers::DistributedRandomSampler(
        testSetSize, 1, 1, false);

    auto stackedDataSet = TS.DataSet.map(
        torch::data::transforms::Stack<typename Dataset::Data>());
    TS.TrainDataLoader =
        torch::data::make_data_loader(stackedDataSet, train_sampler, batchSize);
    TS.TestDataLoader =
        torch::data::make_data_loader(stackedDataSet, test_sampler, batchSize);

    // Create Optimizer
    std::vector<at::Tensor> parameters;
    for (const auto &params : TS.Module.parameters()) {
      parameters.push_back(params);
    }

    const double learningRate = 0.01;
    const int numEpochs = 200;
    TS.Adam = std::make_unique<torch::optim::Adam>(
        parameters, torch::optim::AdamOptions(learningRate).weight_decay(5e-4));

    // Set inputs
    TS.Inputs.resize(2);

    //TS.ReducerLocalPtrOID = _reducerArrayOID;
  }
};

// struct InplaceFunctor {
//   void *operator()() {
//     auto ptr = shad::Array<uint64_t>::GetPtr(oid_);
//     uint64_t address = ptr->At(static_cast<uint32_t>(shad::rt::thisLocality()));
//     return reinterpret_cast<void *>(address);
//   }

//   shad::Array<uint64_t>::ObjectID oid_;
// };

template <typename TrainingState> void vcTrainLoop(TrainingState &TS) {
  size_t train_correct = 0;
  size_t train_size = 0;

  for (auto &batch : *TS.TrainDataLoader) {
    TS.Inputs[0] = batch.Features;
    TS.Inputs[1] = batch.EdgeIndex;
    train_size += batch.Features.size(0);
    auto groundTruth = batch.Labels;

    TS.Module.train();
    auto output = TS.Module.forward(TS.Inputs).toTensor();

    auto loss = torch::nn::functional::nll_loss(
        output.index({batch.Mask}), groundTruth.index({batch.Mask}));

    loss.backward();
    TS.Module.eval();
    auto prediction = std::get<1>(output.max(1));
    auto equal = prediction.eq(groundTruth);
    train_correct += equal.index({batch.Mask}).sum().template item<int64_t>();
  }
  std::cout << " Train Accuracy: "
            << static_cast<float>(train_correct) / train_size << std::endl;
}

// template <typename TrainingState, typename TrainingStateItr>
// void vcReduceGradients(TrainingStateItr B, TrainingStateItr E) {
//   auto start = std::chrono::high_resolution_clock::now();
//   int64_t numRanks = shad::rt::numLocalities();

//   std::map<at::ScalarType, MPI_Datatype> torchToMPITypes = {
//       {at::kByte, MPI_UNSIGNED_CHAR},
//       {at::kChar, MPI_CHAR},
//       {at::kDouble, MPI_DOUBLE},
//       {at::kFloat, MPI_FLOAT},
//       {at::kInt, MPI_INT},
//       {at::kLong, MPI_LONG},
//       {at::kShort, MPI_SHORT},
//   };

//   InplaceFunctor F{(*B).get().ReducerLocalPtrOID};
//   for (int i = 0; i < (*B).get().Module.parameters().size(); ++i) {
//     shad::for_each(
//         shad::distributed_parallel_tag{}, B, E, [=](TrainingState &TS) {
//           auto itr = TS.Module.parameters().begin();
//           for (int j = 0; j < i; ++j)
//             ++itr;
//           auto localPtrs = shad::Array<uint64_t>::GetPtr(TS.ReducerLocalPtrOID);
//           localPtrs->InsertAt(
//               static_cast<uint32_t>(shad::rt::thisLocality()),
//               reinterpret_cast<uint64_t>((*itr).mutable_grad().data_ptr()));
//         });

//     auto itr2 = (*B).get().Module.parameters().begin();
//     for (int j = 0; j < i; ++j)
//       ++itr2;
//     auto reducer = shad::MPIReducer<InplaceFunctor, InplaceFunctor>::Create(
//         torchToMPITypes.at((*itr2).mutable_grad().scalar_type()), F, F);

//     reducer->AllReduce(MPI_SUM, (*itr2).mutable_grad().numel());

//     shad::for_each(shad::distributed_parallel_tag{}, B, E,
//                    [=](TrainingState &TS){
//                      auto itr = TS.Module.parameters().begin();
//                      for (int j = 0; j < i; ++j)
//                        ++itr;
//                      (*itr).mutable_grad().data() =
//                        (*itr).mutable_grad().data() / numRanks;
//                    });
//   }
//   auto end = std::chrono::high_resolution_clock::now();
//   std::cout  << " Reduction Time (s) : "
//             << std::chrono::duration_cast<std::chrono::duration<double>>(end -
//                                                                          start)
//                    .count()
//             << std::endl;
// }

template <typename TrainingState>
void vcBackPropAndEvaluationLoop(TrainingState &TS) {
  size_t test_correct = 0;
  size_t test_size = 0;
  TS.Adam->step();
  TS.Adam->zero_grad();

  for (auto &batch : *TS.TestDataLoader) {
    test_size += batch.Features.size(0);
    TS.Module.eval();
    TS.Inputs[0] = batch.Features;
    TS.Inputs[1] = batch.EdgeIndex;

    auto groundTruth = batch.Labels;
    auto output = TS.Module.forward(TS.Inputs).toTensor();
    auto prediction = std::get<1>(output.max(1));
    auto equal = prediction.eq(groundTruth);
    test_correct += equal.index({batch.Mask}).sum().template item<int64_t>();
  }

  std::cout << "Test Accuracy: " << static_cast<float>(test_correct) / test_size
            << std::endl;
}

// typename shad::Array<
//     agile::workflow1::TrainingState<VertexClassificationWMDDataset>>::ObjectID
// GNN(uint64_t &num_edges, uint64_t &num_vertices, Graph_t &graph,
//     std::string modelFileName);

} // namespace agile::workflow1

#endif
