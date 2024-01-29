#define CACHE_LINE 64

template <typename NodeTy, typename NodeIt>
struct NodeLogEntry {
  NodeIt edges;
  NodeTy store;
};

template <typename EdgeTy, typename NodeId>
struct EdgeLogEntry {
  NodeId sourc;
  NodeId desti;
  EdgeTy store;
};

template <typename T>
struct LogEntry {
  volatile uint8_t esv;
  volatile T t;
};
template <typename T>
struct Log {
  volatile uint8_t esv;
  LogEntry<T>* t;
};

template <typename NodeTy, typename EdgeTy, typename NodeIt>
struct NodeVectorEntry {
  NodeIt it;
  NodeTy ob;
  Log<EdgeTy>* l;
}

template <typename NodeTy, typename EdgeTy, typename NodeIt>
struct NodeVector {
  Log<NodeLogEntry<NodeTy, NodeIt>>* l;
  NodeVectorEntry<NodeTy, EdgeTy, NodeIt>* nv;
}

template <typename T>
struct CacheIndirectLogElement {
  T t;
  Log<T>* log;
}

template <typename T>
struct AlignedLogArray {

}

template <typename NodeStruct, typename EdgeStruct>
struct Graph {
}
