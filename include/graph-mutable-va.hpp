#ifndef _GRAPH_MUTABLE_VA_HPP_
#define _GRAPH_MUTABLE_VA_HPP_
#include <cassert>
#include <cstdint>
#include <stdlib.h>
#include <fstream>
#include <iostream>
#include <cerrno>
#include <cstring>
#include <vector>
#include <sys/mman.h>
//#include <utility>
//#include <ranges>
//#include <concepts>

using namespace std;

enum VertexState : uint16_t
{
  UNLK = 0x0 << 0,
  LOCK = 0x1 << 0,
  TOMB = 0x1 << 1,
  UMAX = 0x1 << 2
};

constexpr uint64_t mask (uint8_t mask, uint8_t shift) {return mask << shift ;}
constexpr uint64_t lower(uint8_t num)                 {return (1 << num) - 1;}

//Pack things in the same order of VertexState
template<typename T>
struct __attribute__ ((packed))
  PackedVal
{
  VertexState get_vertex_state(uint64_t v)  const {return (VertexState) (v >> 48);}
  uint64_t    get_raw_value   (uint64_t v)  const {return v & lower(48);}
  uint16_t    get_flags_unlock(uint16_t f)  const {return f & (lower(15) << 1);}
  uint16_t    get_flags_untomb(uint16_t f)  const {return f & (lower(14) << 2 | 0x1);}

  volatile uint16_t flags: 16;
           uint64_t value: 48;

  PackedVal(T t): flags(get_vertex_state((uint64_t) t)), value(get_raw_value((uint64_t) t)) {}

  inline VertexState try_lock()
  {
    uint16_t f = __atomic_load_2(this, __ATOMIC_RELAXED);
    bool b = false;
    if(!(f & LOCK))
      b = __atomic_compare_exchange_2(this, &f, f | LOCK, true, __ATOMIC_ACQUIRE, __ATOMIC_RELAXED);
    return (VertexState) ((b ? UNLK : LOCK) | get_flags_unlock(f));
  }

  //Make an explicit function that returns tombstone and locks
  inline bool lock()
  {
    uint64_t ret;
    VertexState s;
    do
    {
      s = this->try_lock();
    } while(s & LOCK);
    return !(s & TOMB);
  }

  inline void unlock()
  {
    uint64_t f = flags;
    __atomic_store_2(this, f & (~LOCK), __ATOMIC_RELEASE);
  }

  inline void  set_value(T p)  { if((uint64_t) p == UINT64_MAX) { flags |= UMAX;} else {value = get_raw_value(p);} }

  inline T     get_value()     { return (flags & UMAX) ? (T) UINT64_MAX : (T) value; }

  inline void  unset_tomb()    { flags = flags & (~TOMB); }

  inline void  set_tomb()      { flags = flags | TOMB; }

  inline bool  is_tomb()       { return flags & TOMB; }

  inline bool  atomic_is_tomb() { return __atomic_load_2(this, __ATOMIC_RELAXED) & TOMB; }
};

template<bool parallel>
struct EdgeEntry
{
  PackedVal<uint64_t> dest;
  //false mean that the value is TOMBSTONED
  inline bool      lock()        { if(parallel) return dest.lock(); else return !dest.is_tomb(); }

  inline void      unlock()      { if(parallel) dest.unlock(); }

  inline uint64_t  get_dest()    { return dest.get_value(); }

  inline void      unset_tomb()  { dest.unset_tomb(); }

  inline void      set_tomb()    { dest.set_tomb(); }

  inline bool      is_tomb()     { return dest.is_tomb(); }
  inline bool atomic_is_tomb()   { if(parallel) return dest.atomic_is_tomb(); else return dest.is_tomb(); }

  inline void set_dest(uint64_t d)
  {
    dest.set_value(d);
  }

};

template<bool parallel, typename NodeTy>
struct NodeEntry
{
  PackedVal<uint64_t> start;
            uint64_t  stop;
            struct    empty{};
            [[no_unique_address]]
  std::conditional_t<std::is_same_v<NodeTy, void>, empty, NodeTy> val;
  //false mean that the value is TOMBSTONED
  inline bool lock()           { if(parallel) return start.lock(); else return !start.is_tomb(); }

  inline void unlock()         { if(parallel) start.unlock(); }

  inline bool is_tomb()        { return start.is_tomb(); }

  inline bool atomic_is_tomb() { if(parallel) return start.atomic_is_tomb(); else return start.is_tomb(); }

};

template<bool parallel = true, bool ehp = false, typename NodeTy = void>
class Graph
{
  PackedVal<uint64_t> num_nodes = 0;
  //Cache pad this location
  PackedVal<uint64_t> edge_end = 0;
  NodeEntry<parallel,NodeTy>* nodes;
  EdgeEntry<parallel>* edges;
  uint64_t          no_overflow_average(uint64_t n, uint64_t m) const { return (n / 2) + (m / 2) + (((n % 2) + (m % 2)) / 2);}

  //TODO deal with memory allocation
public:
  Graph():  num_nodes(0), edge_end(0),
    nodes((NodeEntry<parallel,NodeTy>*) (ehp ? mmap(NULL, 1<<30, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON | MAP_HUGETLB | (30 << MAP_HUGE_SHIFT), -1, 0): malloc(1<<30))),
    edges((EdgeEntry<parallel>*) (ehp ? mmap(NULL, 1<<30, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON | MAP_HUGETLB | (30 << MAP_HUGE_SHIFT), -1, 0): malloc(1<<30)))
  {
    assert(nodes != MAP_FAILED);
    assert(edges != MAP_FAILED);
  }
  ~Graph()
  {
    if(ehp) { munmap(nodes, 1<<30); munmap(edges, 1<<30); }
    else    { free(nodes); free(edges); }
  }

  inline EdgeEntry<parallel>* get_edge(uint64_t e)
  {
    if(edge_end.get_value() > e) return edges + e;
    if(parallel)
    {
      edge_end.lock();
      auto ee = edge_end.get_value();
      edge_end.unlock();
      if(ee > e)
      {
        return edges + e;
      }
    }
    return nullptr;
  }

  inline EdgeEntry<parallel>* get_edge_unsafe(uint64_t e) { return this->edges + e; }

  inline NodeEntry<parallel,NodeTy>* get_node(uint64_t n)
  {
    if(num_nodes.get_value() > n) return nodes + n;
    if(parallel)
    {
      num_nodes.lock();
      auto nn = num_nodes.get_value();
      num_nodes.unlock();
      if(nn > n)
      {
        return nodes + n;
      }
    }
    return nullptr;
  }

  inline NodeEntry<parallel,NodeTy>* get_node_unsafe(uint64_t n) { return this->nodes + n; }

  //THESE ARE NOT THREAD SAFE, IF YOU WANT THEM TO BE SAFE TODO
  inline uint64_t   get_num_nodes() {return num_nodes.get_value();}
  inline uint64_t   get_edge_end()  {return edge_end .get_value();}
private:

  void edge_placement(uint64_t start, uint64_t stop, uint64_t num_orig,
                      uint64_t num_new, uint64_t* dest,
                     uint64_t place_start, uint64_t place_stop)
  {
    //This is a special case of sorting so we will do it in here
    //If you must swap places with the head of the buffer
    uint64_t  place_curr      = place_start;
    //Iterators for original and new
    uint64_t  orig_curr       = start;
    uint64_t  dest_curr       = 0;
    //Iterators for swapped values
    uint64_t* swap_arr        = nullptr;
    uint64_t  swap_start      = 0;
    uint64_t  swap_stop       = 0;



    uint64_t edges_left = num_orig + num_new;
    //deal with duplicates properly
    while(edges_left)
    {
      assert(swap_stop >= swap_start);
      while(orig_curr < stop && get_edge(orig_curr)->is_tomb()) orig_curr++;

      uint64_t orig_curr_dest = (orig_curr < stop) ? get_edge(orig_curr)->get_dest() : UINT64_MAX;
      uint64_t dest_curr_dest = (dest_curr < num_new) ? dest[dest_curr] : UINT64_MAX;
      uint64_t swap_curr_dest = (swap_stop - swap_start) ? swap_arr[swap_start] : UINT64_MAX;
      assert((swap_stop - swap_start) ? swap_curr_dest < orig_curr_dest : true);

      //Check if we should swap
      if(orig_curr < stop && place_curr == orig_curr
          && (orig_curr_dest > dest_curr_dest || orig_curr_dest > swap_curr_dest))
      {
        if(!swap_arr) swap_arr = (uint64_t*) malloc(sizeof(uint64_t) * num_orig);
        swap_arr[swap_stop] = orig_curr_dest;
        orig_curr++;
        swap_stop++;
      }

      //Now place data

      if(swap_curr_dest < dest_curr_dest)
      {
        get_edge(place_curr)->set_dest(swap_curr_dest);
        swap_start++;
      }
      //This takes care of the equal case of dest_curr_dest == swap_curr_dest
      else if(dest_curr_dest < orig_curr_dest)
      {
        get_edge(place_curr)->set_dest(dest_curr_dest);
        dest_curr++;
        if(dest_curr_dest == swap_curr_dest) {swap_start++; edges_left--;}
      }
      else if(orig_curr_dest < dest_curr_dest)
      {
        get_edge(place_curr)->set_dest(orig_curr_dest);
        orig_curr++;
      }
      //orig_curr_dest == dest_curr_dest
      else
      {
        assert(orig_curr_dest == dest_curr_dest);
        get_edge(place_curr)->set_dest(orig_curr_dest);
        orig_curr++;
        dest_curr++;
        edges_left--;
      }

      get_edge(place_curr)->unset_tomb();
      place_curr++;
      edges_left--;
    }

    if(swap_arr) free(swap_arr);

    while(place_curr < place_stop)
    {
      get_edge(place_curr)->set_tomb();
      get_edge(place_curr)->set_dest(UINT64_MAX);
      place_curr++;
    }
  }

  //Key observation is tombstoned values are still sorted
  //Returns a LOCKED EdgeEntry
  EdgeEntry<parallel>* binarySearchAcquire(uint64_t find, uint64_t start, uint64_t stop, EdgeEntry<parallel>* e_list)
  {
    if(start == stop) return nullptr;
    uint64_t curr = no_overflow_average(start, stop);
    auto e = &e_list[curr];
    e->lock();
    auto ev = e->get_dest();
    if(find == ev)
    {
      return e;
    }
    else
    {
      // start inclusive and stop exclusive
      uint64_t new_start = (find < ev) ? start : curr + 1;
      uint64_t new_stop  = (find < ev) ? curr  : stop;

      e->unlock();
      return binarySearchAcquire(find, new_start, new_stop, e_list);
    }
  }

public:
  // Add assertions for duplicate edges
  // Change values from pointers to offsets.
  // Assume dst contains no repeats and is sorted.
  // reformulate graph
  void ingestEdges(uint64_t num_new_edges, uint64_t src, uint64_t* dst)
  {
    auto ns = get_node(src);
    if (ns->lock())
    {
      uint64_t  start = ns->start.get_value();
      uint64_t  stop  = ns->stop;
      assert (start <= stop);

      int64_t   diff    = stop - start;
      uint64_t num_orig = 0;

      //Lock all edges
      //Calculate the original valid edges
      for(int64_t i = 0; i < diff; i++)
      {
        auto e          = get_edge(start + i);
        bool e_valid    = e->lock();
        if(e_valid)
        {
          bool dest_tomb  = get_node(e->get_dest())->atomic_is_tomb();
          if(dest_tomb) e->set_tomb();
          else num_orig++;
        }
      }

      //Check if we can put all edges in place
      if(num_orig + num_new_edges <= diff)
      {
        edge_placement(start, stop, num_orig, num_new_edges, dst, start, stop);
      }
      else
      {
        if(parallel) this->edge_end.lock();
        //Check if you you are at the end.
        uint64_t new_start = (stop == this->edge_end.get_value()) ? start : this->edge_end.get_value();
        uint64_t new_stop  = new_start + num_orig + num_new_edges;

        this->edge_end.set_value(new_stop);
        if(parallel) this->edge_end.unlock();

        edge_placement(start, stop, num_orig, num_new_edges, dst, new_start, new_stop);

        ns->start.set_value(new_start);
        ns->stop  = new_stop;
      }

      for(int64_t i = 0; i < diff; i++)
      {
        get_edge(start + i)->unlock();
      }
    }
    ns->unlock();
  }

  uint64_t ingestNode()
  {
    if(parallel) this->num_nodes.lock();

    auto ret = this->num_nodes.get_value();
    this->num_nodes.set_value(ret + 1);
    auto nr = get_node(ret);
    nr->start.set_value(0);
    nr->stop = 0;

    if(parallel) this->num_nodes.unlock();

    return ret;
  }

  //returns the start
  uint64_t ingestNodes(uint64_t n, uint64_t& end)
  {
    if(n == 0) return UINT64_MAX;
    if(parallel) this->num_nodes.lock();

    auto ret = this->num_nodes.get_value();
    this->num_nodes.set_value(ret + n);
    for(auto r = ret; r < ret + n; r++)
    {
      auto nr = get_node(r);
      nr->start.set_value(0);
      nr->stop = 0;
    }

    if(parallel) this->num_nodes.unlock();

    end = ret + n;
    return ret;
  }

  void deleteNode(uint64_t src)
  {
    auto ns = get_node(src);
    if(ns->lock())
    {
      auto start  = ns->start.get_value();
      auto stop   = ns->stop;
      assert (start <= stop);

      for(uint64_t i = 0; i < stop - start; i++)
      {
        auto e = get_edge(start + i);
        e->lock();
        e->set_tomb();
        e->unlock();
      }

      ns->start.set_tomb();
    }
    ns->unlock();
  }

  void deleteEdge(uint64_t src, uint64_t dest)
  {
    auto n = get_node(src);
    n->lock();

    uint64_t start = n->start.get_value();
    uint64_t stop  = n->stop;

    auto e = binarySearchAcquire(dest, start, stop, edges);
    if(e)
    {
      e->set_tomb();
      e->unlock();
    }

    n->unlock();
  }

  uint64_t ingestEdgeList(uint64_t num_nodes, const std::vector<uint64_t>& srcs, const std::vector<uint64_t>& dests)
  {
    assert(srcs.size() == dests.size());
    uint64_t end;
    uint64_t start = this->ingestNodes(num_nodes, *&end);
    for(int i = 0; i < srcs.size(); i++)
    {
      auto src = srcs.at(i) + start;
      auto dest = dests.at(i) + start;
      assert(src < end);
      assert(dest < end);
      this->ingestEdges(1, src, &dest);
    }
    return start;
  }

  uint64_t ingestEdgeList(uint64_t num_nodes, const std::vector<std::pair<uint64_t, uint64_t>>& edges)
  {
    uint64_t end;
    uint64_t start = this->ingestNodes(num_nodes, *&end);
    for(const auto& [s,d]  : edges)
    {
      auto src  = s + start;
      auto dest = d + start;
      assert(src  < end);
      assert(dest < end);
      this->ingestEdges(1, src, &dest);
    }
    return start;
  }

  /* Not supported in g++9
  template<typename T>
  requires std::ranges::forward_range<T> && convertible_to<std::ranges::range_value_t<T>, std::pair<uint64_t, uint64_t>>
  uint64_t ingestEdgeList(uint64_t num_nodes, const T& x)
  {
    uint64_t end;
    uint64_t start = this->ingestNodes(numNodes, *&end);
    auto it = std::ranges::begin(x);
    auto end = std::ranges::end(x);
    for(; it != end; ++it)
    {
      std::pair<uint64_t, uint64_t> edge = *it;
      auto src = edge.fst + start;
      auto dest= edge.snd + start;
      assert(src < end);
      assert(dest < end);
      this->ingestEdge(1, src, &dest);
    }
  }
  */

  void ingestSubGraphFromELFile(const std::string& filename)
  {
    std::ifstream graphFile(filename.c_str());
    if (!graphFile.is_open())
    {
      std::cerr << "UNABLE TO open graphFile: " << filename
                << "\terrno: " << errno
                << "\terrstr: " << std::strerror(errno)
                << std::endl;

      exit(-2);
    }
    uint64_t numNodes;
    uint64_t useless;
    graphFile >> numNodes;
    graphFile >> useless;
    uint64_t end;
    uint64_t start = this->ingestNodes(numNodes, *&end);
    uint64_t dest;
    uint64_t src;
    //Using a subgraph
    while(graphFile >> src && graphFile >> dest)
    {
      src += start;
      dest += start;
      this->ingestEdges(1, src, &dest);
    }
    graphFile.close();
  }

};

#endif
