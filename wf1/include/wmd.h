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

#ifndef WMD_H
#define WMD_H

#include <cstdint>

#include "graph.h"
#include "ego_graph.h"
//#include "shad/data_structures/array.h"
#include "torch/torch.h"

constexpr int NUM_FEATURES  = 30;

namespace agile::workflow1 {
template <typename EdgeIndexType = torch::Tensor,
          typename FeaturesType = torch::Tensor,
          typename LabelsType = torch::Tensor,
          typename MaskType = torch::Tensor>
struct WMDData {
  using edge_index_type = EdgeIndexType;
  using features_type = FeaturesType;
  using labels_type = LabelsType;
  using mask_type = MaskType;

  WMDData() = default;
  WMDData(edge_index_type ei, features_type fs, labels_type ls, mask_type mask)
      : EdgeIndex(std::move(ei)), Features(std::move(fs)),
        Labels(std::move(ls)), Mask(std::move(mask)) {}

  edge_index_type EdgeIndex;
  features_type Features;
  labels_type Labels;
  mask_type Mask;
};
} // namespace agile::workflow1

namespace torch::data::transforms {
template <>
struct Stack<agile::workflow1::WMDData<>>
    : public Collation<agile::workflow1::WMDData<>> {
  agile::workflow1::WMDData<>
  apply_batch(std::vector<agile::workflow1::WMDData<>> examples) override {
    int64_t offset = 0;

    std::vector<torch::Tensor> ei, fs, ls, ms;

    for (size_t i = 0; i < examples.size(); ++i) {
      ei.push_back(examples[i].EdgeIndex.add(offset));
      fs.push_back(examples[i].Features);
      ls.push_back(examples[i].Labels);
      ms.push_back(examples[i].Mask);

      offset += examples[i].Features.size(0);
    }

    return {torch::cat(ei, 1).to(torch::kLong), torch::cat(fs), torch::cat(ls),
            torch::cat(ms)};
  }
};
} // namespace torch::data::transforms

namespace agile::workflow1 {
class WMDDataset {

protected:
  CSR_t * _g;

public:
  using Data = WMDData<>;

  WMDDataset()
       {_g = nullptr;}

  WMDDataset(CSR_t *g)
     {_g = g;}
};


class VertexClassificationWMDDataset
    : public torch::data::Dataset<VertexClassificationWMDDataset, WMDData<>>,
      public WMDDataset {
public:
  VertexClassificationWMDDataset() : WMDDataset() {}

  VertexClassificationWMDDataset(CSR_t *g)
      : WMDDataset(g) {}


  //! Returns the i-th data point from the dataset.
  //!
  //! The data point returned contains:
  //!  + the ego-graph of the idx vertex;
  //!  + the feature vector of each of the vertices in the ego-graph;
  //!  + the labels for each of the vertices in the ego-graphs;
  //!  + a mask selecting which vertex to use in training;
  //!
  //! Each data point is constructed on the fly by querying the CSR
  //! representation that is built at the beginning of the workflow.
  WMDData<> get(size_t idx) override {
    uint64_t root = idx;
    auto bool_tensor = torch::TensorOptions().dtype(torch::kBool);
    auto options = torch::TensorOptions().dtype(torch::kLong);

    auto [graph, vertex_set] = _build_ego_graph<CSR_t, Vertex, Edge>(*_g, root, *((&root) + 1));
    int64_t num_vertices = vertex_set.size();

    // create type and feature vector
    std::vector<int64_t> vertexTypes(num_vertices);
    std::vector<int64_t> featureVectors(num_vertices * NUM_FEATURES);

    //shad::rt::Handle handle;
    //auto Features = _featuresOID;
    
    for (auto itr = vertex_set.begin(); itr != vertex_set.end(); ++itr) {
      int64_t glbID = (*itr).first;
      std::array<uint64_t, 30> Features{};
      std::array<uint64_t, 15> arr_1_hop{0};
      std::array<uint64_t, 15> arr_2_hop{0};
      // auto arr_1_hop = _g->getData(glbID).arr_1_hop;
      // auto arr_2_hop = _g->getData(glbID).arr_2_hop;
      std::copy(arr_1_hop.begin(), arr_1_hop.end(), Features.begin());
      std::copy(arr_2_hop.begin(), arr_2_hop.end(), Features.begin() + arr_1_hop.size());

      int64_t localID = (*itr).second.id;
      int64_t type = (int64_t)(*itr).second.type;

      vertexTypes[localID] = type;
      auto ptr = (uint64_t *)(featureVectors.data() + localID * NUM_FEATURES);
      std::copy(Features.begin(), Features.end(), ptr);
      // Features->AsyncGetElements(handle, ptr, glbID * NUM_FEATURES,
      //                            NUM_FEATURES);
    }

    // The vertex tensor stores a bitmask representing vertices to be used
    // as part of the training process. We are using roughly 75% of the
    // ego-graph.
    auto vertex = torch::zeros(num_vertices, bool_tensor);
    auto indices = torch::randint(0, num_vertices,
                                  {num_vertices - num_vertices / 4}, options);

    vertex.index_put_({0}, true);       // set root's bit to true
    vertex.index_put_({indices}, true); // set choosen vertices' bits to true
    std::vector<float> floatFeatures(featureVectors.begin(),
                                     featureVectors.end());

    //shad::rt::waitForCompletion(handle);

    // The features tensor stores the two hop features of the ego-graph vertices
    auto features =
        torch::from_blob(floatFeatures.data(), {num_vertices, NUM_FEATURES},
                         torch::TensorOptions().dtype(torch::kFloat))
            .clone();

    // The labels tensor stores the type of the ego-graph vertices
    auto labels =
        torch::from_blob(vertexTypes.data(), {num_vertices}, options).clone();
    return {graph, features, labels, vertex};
  }

  torch::optional<size_t> size() const override {
    //return VertexType::GetPtr(_verticesOID)->Size() - 1;
    return _g->size();
  }
};

} // namespace agile::workflow1

#endif
