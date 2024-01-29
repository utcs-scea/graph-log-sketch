
enum VertexState {
  FREE,
  NODE,
  TOMB, // Waiting on Value
  DEAD  // Has some Value
}

// One in the top means invalid
template <typename T>
struct __attribute__((packed)) PackedPointer {
  volatile uint64_t point;
  PackedPointer(T* t) : point((uint64_t)t) {}
  bool invalidate() {
    uint64_t t = __atomic_load_8(&point, __ATOMIC_RELAXED);
    if (t >> 63)
      return false;
    return __atomic_compare_exchange_8(&point, &t, 1 << 63 | t, true,
                                       __ATOMIC_RELAXED, ATOMIC_RELAXED);
  }
  bool change(T* n, T&* o, int memory_model = __ATOMIC_RELAXED) {
    uint64_t o = __atomic_load_8(&point, __ATOMIC_RELAXED);
    if (o >> 63)
      return false;
    return __atomic_compare_exchange_8(&point, &o, 0xEFFF & (uint64_t)n, true,
                                       memory_model, memory_model);
  }
  T* getPointer(VertexState& ready) {
    uint64_t v = __atomic_load_8(&point, __ATOMIC_RELAXED);
    if (v >> 63) {
      if (v & 0xEFFF)
        ready = DEAD;
      else
        ready = TOMB;
    } else {
      if (v)
        ready = NODE;
      else
        ready = FREE;
    }
    return (T*)(v & 0xEFFF);
  }
  bool isInvalid() { return (__atomic_load_8(&point, __ATOMIC_RELAXED) >> 63); }
  bool isAlloc() { return !(__atomic_load_8(&point, __ATOMIC_RELAXED)); }

}

template <typename NodeTy, typename EdgeTy>
struct EdgeEntry {
  PackedPointer<Vertex<NodeTy, EdgeTy>> dest;
  volatile uint64_t ref_count = 1;
  T t;
};

//
template <typename NodeTy, typename EdgeTy>
struct NodeEntry {
  PackedPointer<EdgeEntry<EdgeTy>> star;
  PackedPointer<EdgeEntry<EdgeTy>> stop;
  volatile uint64_t ref_count = 1;
  NodeTy t;
};

template <typename NodeTy, typename EdgeTy>
struct DeadNodeEntry {
  PackedPointer<NodeEntry<NodeTy>> dead;
};

template <typename NodeTy, typename EdgeTy>
union Vertex {
  NodeEntry<NodeTy, EdgeTy> node;
  DeadNodeEntry<NodeTy, EdgeTy> dode;
};

struct Bitmap {
  volatile uint64_t* pointer;
};

// Invalid here means all fullup
template <typename T>
struct Heap {
  PackedPointer<Heap<T>>* next;
  Bitmap bitm;
  T* blob;
};

template <typename NodeTy, typename EdgeTy>
struct Graph {
  volatile uint64_t node_count = 0;
  // For now do not prealloc (necessary for speed imho)
  // Vertex<NodeTy, EdgeTy>* vertex_heap;
  // EdgeEntry<NodeTy, EdgeTy>* edge_heap;
  volatile uint64_t len = 0;
  volatile Vertex<NodeTy, EdgeTy>* vertex_orig;
  volatile Vertex<NodeTy, EdgeTy>* vertex_log;

  // Traverses tombstoned items in order to generate a local version of the
  // Node.
  NodeEntry<NodeTy*, EdgeTy> acquire_node_read(uint64_t node_id) {
    // This part might be racy only matters when aggressive transitions are
    // enabled
    auto vo     = vertex_orig;
    auto vl     = vertex_log;
    auto length = __atomic_load_8(len, __ATOMIC_ACQUIRE);

    Vertex<NodeTy, EdgeTy>* next;
    if (node_id < length) {
      next = &vo[node_id];
    } else {
      next = &vl[node_id - length];
    }
    Vertex<NodeTy, EdgeTy>* curr;
    VertexState node;
    do {
      curr = next;
      node = TOMB;
      while (node == TOMB)
        next = curr->dode.getPointer(node);
    } while (node == DEAD) return NodeEntry{
        next, curr->node.stop.getPointer(node), 1, &curr->node.t};
  }

  // Assumption edges are unique and at most added once
  void ingestNewEdge(uint64_t src, uint64_t dest, EdgeTy e) {
    // Mark original entry as being written use stop's DEAD for this
    // Get the NodeEntry<NodeTy*, EdgeTy> locally
    // Construct new NodeEntry<NodeTy, EdgeTy> pointing to deep copy of edge
    // objects Mark the whole entry
  }

  void ingestNewNode(EdgeEntry<T>* e);
  void ingestNewNode(NodeTy n, EdgeEntry<T>* e);
}
