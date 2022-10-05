#ifndef _GRAPH_MUTABLE_VA_H_
#define _GRAPH_MUTABLE_VA_H_
#include <cassert>
#include <cstdint>
#include <stdlib.h>
using namespace std;

enum VertexState : uint16_t
{
  UNLK = 0x0 << 0,
  LOCK = 0x1 << 0,
  TOMB = 0x1 << 1
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

  VertexState try_lock()
  {
    uint16_t f = __atomic_load_2(this, __ATOMIC_RELAXED);
    bool b = false;
    if(!(f & LOCK))
      b = __atomic_compare_exchange_2(this, &f, f | LOCK, true, __ATOMIC_ACQUIRE, __ATOMIC_RELAXED);
    return (VertexState) ((b ? UNLK : LOCK) | get_flags_unlock(f));
  }

  //Make an explicit function that returns tombstone and locks
  bool lock()
  {
    uint64_t ret;
    VertexState s;
    do
    {
      s = this->try_lock();
    } while(s & LOCK);
    return !(s & TOMB);
  }

  void unlock()
  {
    uint64_t f = flags;
    __atomic_store_2(this, f & (~LOCK), __ATOMIC_RELEASE);
  }

  void  set_value(T p)  { value = get_raw_value(p); }

  T     get_value()     { return (T) value; }

  void  unset_tomb()    { flags = flags & (~TOMB); }

  void  set_tomb()      { flags = flags | TOMB; }

  bool  is_tomb()       { return flags & TOMB; }

  bool  atomic_is_tomb() { return __atomic_load_2(this, __ATOMIC_RELAXED) & TOMB; }
};

// Make it complicated for tombstone checking now
struct NodeEntry;

struct EdgeEntry
{
  PackedVal<uint64_t> dest;
  //false mean that the value is TOMBSTONED
  bool      lock()        { return dest.lock(); }

  void      unlock()      { dest.unlock(); }

  uint64_t  get_dest()    { return dest.get_value(); }

  void      unset_tomb()  { dest.unset_tomb(); }

  void      set_tomb()    { dest.set_tomb(); }

  bool      is_tomb()     { return dest.is_tomb(); }
  bool atomic_is_tomb()   { return dest.atomic_is_tomb(); }

  void set_dest(uint64_t d)
  {
    assert(!(d >> 48));
    dest.set_value(d);
  }

};

struct NodeEntry
{
  PackedVal<uint64_t> start;
            uint64_t  stop;
  //false mean that the value is TOMBSTONED
  bool lock()           { return start.lock(); }

  void unlock()         { start.unlock(); }

  bool is_tomb()        { return start.is_tomb(); }

  bool atomic_is_tomb() { return start.atomic_is_tomb(); }

};

class Graph
{
  PackedVal<uint64_t> num_nodes = 0;
  //Cache pad this location
  PackedVal<uint64_t> edge_end = 0;
  NodeEntry* nodes;
  EdgeEntry* edges;
  uint64_t          no_overflow_average(uint64_t n, uint64_t m) const { return (n / 2) + (m / 2) + (((n % 2) + (m % 2)) / 2);}

  //TODO deal with memory allocation
public:
  Graph(): num_nodes(0), edge_end(0), nodes((NodeEntry*)malloc(1<<29)), edges((EdgeEntry*)malloc(1<<29)) {}


  inline EdgeEntry* get_edge(uint64_t e)
  {
    if(edge_end.get_value() > e) return edges + e;
    edge_end.lock();
    auto ee = edge_end.get_value();
    edge_end.unlock();
    if(ee > e)
    {
      return edges + e;
    }
    return nullptr;
  }

  inline NodeEntry* get_node(uint64_t n)
  {
    if(num_nodes.get_value() > n) return nodes + n;
    num_nodes.lock();
    auto nn = num_nodes.get_value();
    num_nodes.unlock();
    if(nn > n)
    {
      return nodes + n;
    }
    return nullptr;
  }

  //THESE ARE NOT THREAD SAFE, IF YOU WANT THEM TO BE SAFE TODO
  inline uint64_t   get_num_nodes() {return num_nodes.get_value();}
  inline uint64_t   get_edge_end() {return edge_end  .get_value();}
private:

  void edge_placement(uint64_t start, uint64_t stop, uint64_t num_orig,
                      uint64_t num_new, uint64_t* dst,
                      uint64_t place_start, uint64_t place_stop)
  {
    //This is a special case of sorting so we will do it in here
    //If you must swap places with the head of the buffer
    uint64_t place_curr         = place_start;
    //Iterators for original and new
    uint64_t read_orig_curr     = start;
    uint64_t dst_curr           = 0;
    //Iterators for swapped values
    uint64_t dst_swapped_start  = 0;
    uint64_t dst_swapped_stop   = 0;
    //Example:
    //dest : 0 1 2 | 3 1 2 | 3 4 2
    //start: 3 4 T | 0 4 T | 0 1 T

    uint64_t edges_left = num_orig + num_new;
    //deal with duplicates properly
    while(edges_left)
    {
      assert(dst_swapped_stop <= dst_curr);

      while(read_orig_curr < stop && get_edge(read_orig_curr)->is_tomb()) read_orig_curr++;

      //Check if the original values swapped into the dst array
      uint64_t* orig_curr_ref = &read_orig_curr;
      uint64_t orig_curr_edge_dst = UINT64_MAX;
      //Choose where the original edge comes from
      if (dst_swapped_stop - dst_swapped_start)
      {
        orig_curr_edge_dst = dst[dst_swapped_start];
        orig_curr_ref      = &dst_swapped_start;
      }
      else if(read_orig_curr < stop)
      {
        orig_curr_edge_dst = get_edge(read_orig_curr)->get_dest();
      }

      uint64_t dst_curr_edge_dst  = (dst_curr < num_new) ? dst[dst_curr] : UINT64_MAX;

      //One of the above two must be valid otherwise we shouldn't be in this loop anymore
      if(orig_curr_edge_dst < dst_curr_edge_dst)
      {
        get_edge(place_curr)->set_dest(orig_curr_edge_dst);
        (*orig_curr_ref)++;
      }
      else if(orig_curr_edge_dst > dst_curr_edge_dst)
      {
        //swap_case
        if(place_curr == read_orig_curr)
        {
          dst[dst_swapped_stop] = get_edge(read_orig_curr)->get_dest();
          dst_swapped_stop++;
          read_orig_curr++;
        }
        get_edge(place_curr)->set_dest(dst_curr_edge_dst);
        dst_curr ++;
      }
      //If they are equal then increment both counters
      else
      {
        get_edge(place_curr)->set_dest(orig_curr_edge_dst);
        (*orig_curr_ref)++;
        dst_curr ++;
        edges_left--;
      }
      get_edge(place_curr)->unset_tomb();
      place_curr++;
      edges_left--;
    }

    while(place_curr < place_stop)
    {
      get_edge(place_curr)->set_tomb();
      get_edge(place_curr)->set_dest(UINT64_MAX);
      place_curr++;
    }
  }

  //Key observation is tombstoned values are still sorted
  //Returns a LOCKED EdgeEntry
  EdgeEntry* binarySearchAcquire(uint64_t find, uint64_t start, uint64_t stop, EdgeEntry* e_list)
  {
    if(start == stop) return nullptr;
    uint64_t curr = no_overflow_average(start, stop);
    EdgeEntry* e = &e_list[curr];
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
  // TODO implement readFile
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
        bool dest_tomb  = get_node(e->get_dest())->atomic_is_tomb();
        if(dest_tomb) e->set_tomb();
        else if (e_valid) num_orig++;
      }

      //Check if we can put all edges in place
      if(num_orig + num_new_edges <= diff)
      {
        edge_placement(start, stop, num_orig, num_new_edges, dst, start, stop);
      }
      else
      {
        this->edge_end.lock();
        //Check if you you are at the end.
        uint64_t new_start = (stop == this->edge_end.get_value()) ? start : this->edge_end.get_value();
        uint64_t new_stop  = new_start + num_orig + num_new_edges;

        this->edge_end.set_value(new_stop);
        this->edge_end.unlock();

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
    this->num_nodes.lock();

    auto ret = this->num_nodes.get_value();
    this->num_nodes.set_value(ret + 1);
    auto nr = get_node(ret);
    nr->start.set_value(0);
    nr->stop = 0;

    this->num_nodes.unlock();

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
    NodeEntry* n = get_node(src);
    n->lock();

    uint64_t start = n->start.get_value();
    uint64_t stop  = n->stop;

    EdgeEntry* e = binarySearchAcquire(dest, start, stop, edges);
    if(e)
    {
      e->set_tomb();
      e->unlock();
    }

    n->unlock();
  }

};

#endif
