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

#ifndef GNNGALOIS_H
#define GNNGALOIS_H

#include <cstdint>
#include <memory>
#include <vector>

#include "Galois/Galois.h"

#include "agile/workflow1/graph.h"
#include "agile/workflow1/main.h"
#include "agile/workflow1/wmd.h"

#include "torch/script.h"
#include "torch/torch.h"

namespace agile::workflow1 {

template <typename Dataset> struct TrainingState {
public:
  //using ArrayOID = typename shad::Array<uint64_t>::ObjectID;
  using ArrayOID = typename Galois::LargeArray<uint64_t>;
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

template <typename Dataset> class SetUpTrainingContext {
  //using ArrayOID = typename shad::Array<uint64_t>::ObjectID;
  using ArrayOID = typename Galois::LargeArray<uint64_t>;

  const int64_t trainingSetSize = 500;
  const int64_t testSetSize = 500;
  const size_t batchSize = 100;

  char modelFileName_[256];
  VertexOID _verticesOID;
  XEdgeOID _edgesOID;
  ArrayOID _featuresOID;
  //ArrayOID _reducerArrayOID;

public:
  SetUpTrainingContext(const VertexOID &VertexArrayID,
                       const XEdgeOID &EdgeArrayOID,
                       const ArrayOID &FeaturesArrayID,
                       std::string modelFileName)
      : _verticesOID(VertexArrayID), _edgesOID(EdgeArrayOID),
        _featuresOID(FeaturesArrayID) {
    if (modelFileName.size() > 256)
      throw "Filename too long";

    std::strcpy(modelFileName_, modelFileName.c_str());
  }

  void operator()(TrainingState<Dataset> &TS) {
    // Load Module
    TS.Module = torch::jit::load(modelFileName_);
    // Load Dataset
    TS.DataSet = Dataset(_verticesOID, _edgesOID, _featuresOID);

    size_t numVertices = TS.DataSet.size().value();
    // Partition in Training/Test Set
    using namespace torch::indexing;
    auto options = torch::TensorOptions().dtype(torch::kBool);

    // Create DataLoader
    uint32_t thisLocality = static_cast<uint32_t>(shad::rt::thisLocality());
    auto train_sampler = torch::data::samplers::DistributedRandomSampler(
        trainingSetSize, shad::rt::numLocalities(), thisLocality, false);
    auto test_sampler = torch::data::samplers::DistributedRandomSampler(
        testSetSize, shad::rt::numLocalities(), thisLocality, false);
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

    TS.ReducerLocalPtrOID = _reducerArrayOID;
  }
};

struct InplaceFunctor {
  void *operator()() {
    auto ptr = shad::Array<uint64_t>::GetPtr(oid_);
    uint64_t address = ptr->At(static_cast<uint32_t>(shad::rt::thisLocality()));
    return reinterpret_cast<void *>(address);
  }

  shad::Array<uint64_t>::ObjectID oid_;
};

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

template <typename TrainingState, typename TrainingStateItr>
void vcReduceGradients(TrainingStateItr B, TrainingStateItr E) {
  auto start = std::chrono::high_resolution_clock::now();
  int64_t numRanks = shad::rt::numLocalities();

  std::map<at::ScalarType, MPI_Datatype> torchToMPITypes = {
      {at::kByte, MPI_UNSIGNED_CHAR},
      {at::kChar, MPI_CHAR},
      {at::kDouble, MPI_DOUBLE},
      {at::kFloat, MPI_FLOAT},
      {at::kInt, MPI_INT},
      {at::kLong, MPI_LONG},
      {at::kShort, MPI_SHORT},
  };

  InplaceFunctor F{(*B).get().ReducerLocalPtrOID};
  for (int i = 0; i < (*B).get().Module.parameters().size(); ++i) {
    shad::for_each(
        shad::distributed_parallel_tag{}, B, E, [=](TrainingState &TS) {
          auto itr = TS.Module.parameters().begin();
          for (int j = 0; j < i; ++j)
            ++itr;
          auto localPtrs = shad::Array<uint64_t>::GetPtr(TS.ReducerLocalPtrOID);
          localPtrs->InsertAt(
              static_cast<uint32_t>(shad::rt::thisLocality()),
              reinterpret_cast<uint64_t>((*itr).mutable_grad().data_ptr()));
        });

    auto itr2 = (*B).get().Module.parameters().begin();
    for (int j = 0; j < i; ++j)
      ++itr2;
    auto reducer = shad::MPIReducer<InplaceFunctor, InplaceFunctor>::Create(
        torchToMPITypes.at((*itr2).mutable_grad().scalar_type()), F, F);

    reducer->AllReduce(MPI_SUM, (*itr2).mutable_grad().numel());

    shad::for_each(shad::distributed_parallel_tag{}, B, E,
                   [=](TrainingState &TS){
                     auto itr = TS.Module.parameters().begin();
                     for (int j = 0; j < i; ++j)
                       ++itr;
                     (*itr).mutable_grad().data() =
                       (*itr).mutable_grad().data() / numRanks;
                   });
  }
  auto end = std::chrono::high_resolution_clock::now();
  std::cout  << " Reduction Time (s) : "
            << std::chrono::duration_cast<std::chrono::duration<double>>(end -
                                                                         start)
                   .count()
            << std::endl;
}

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

typename shad::Array<
    agile::workflow1::TrainingState<VertexClassificationWMDDataset>>::ObjectID
GNN(uint64_t &num_edges, uint64_t &num_vertices, Graph_t &graph,
    std::string modelFileName);

} // namespace agile::workflow1

#endif
