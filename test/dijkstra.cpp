#include "graph-mutable-va.hpp"

#include <queue>
#include <unordered_set>
#include <thread>
#include <system_error>
#include <iostream>

using namespace std;

typedef pair<uint64_t, uint64_t> nodeDist;

constexpr uint64_t T_NUM = 8;

volatile uint64_t bar = T_NUM;
uint64_t* p_bar =  (uint64_t*) &bar;

uint32_t x = 1, y = 4, z = 7, w = 13;

uint32_t simple_rand(void) {
  uint32_t t = x;
  t ^= t << 11;
  t ^= t >> 8;
  x = y;
  y = z;
  z = w;
  w ^= w >> 19;
  w ^= t;
  return w;
}

//assume for now every edge has length 1.
uint64_t shortest_length_dijkstras(uint64_t src, uint64_t dest, Graph* g)
{
  priority_queue<nodeDist, vector<nodeDist>, greater<nodeDist> > pq;
  unordered_set <uint64_t> visited_nodes;
  auto dist = 0;
  auto curr_node = g->get_node(src);
  visited_nodes.insert(src);
  curr_node->lock();
  uint64_t curr = src;
  while(curr != dest)
  {
    auto start = curr_node->start.get_value();
    auto stop  = curr_node->stop;
    for(uint64_t e = start; e < stop; e++)
    {
      //lock all edges
      g->get_edge(e)->lock();
    }
    //unlock the current node (should increase parrallelism
    curr_node->unlock();
    for(uint64_t e = start; e < stop; e++)
    {
      auto edge         = g->get_edge(e);
      auto edge_t       = edge->is_tomb();
      auto next         = edge->get_dest();
      auto next_node    = g->get_node(next);
      if(!edge_t)
      {
        //Check if we can lock this node
        if(visited_nodes.find(next) == visited_nodes.end())
        {
          auto next_node_t  = !next_node->lock();
          if(next_node_t)
          {
            edge->set_tomb();
            next_node->unlock();
          }
          else
          {
            visited_nodes.insert(next);
            //If we push we hold the lock
            pq.push(make_pair(dist+1, next));
          }
        }
      }
      edge->unlock();
    }
    if(pq.empty()) return UINT64_MAX;
    dist = pq.top().first;
    curr = pq.top().second;
    curr_node = g->get_node(curr);
    pq.pop();
  }
  assert(curr == dest);
  g->get_node(curr)->unlock();
  //Unlock Everything on the queue
  while(!pq.empty())
  {
    auto t = pq.top().second;
    pq.pop();
    g->get_node(t)->unlock();
  }
  return dist;
}

int main()
{
  Graph* g = new Graph();
  //Build psuedo-random graph
  //TODO make this better
  uint64_t nodes = simple_rand() + 10;
  for(uint64_t i = 0; i < nodes; i++)
  {
    g->ingestNode();
    uint64_t edges = (simple_rand() + 10) % nodes;
    uint64_t* dst = (uint64_t*) malloc(sizeof(uint64_t) * edges);
    for(uint64_t j = 0; j < edges; j++)
    {
      dst[j] = j;
    }
    g->ingestEdges(edges, i, dst);
    free(dst);
  }

  for(int i = 0; i < 100; i++)
  {
    std::vector<std::thread> threads;
    uint64_t src = simple_rand() % nodes;
    uint64_t dst = simple_rand() % nodes;

    auto f = [src, dst, g, p_bar](){
      __atomic_add_fetch_8(p_bar, -1, __ATOMIC_RELAXED);
      while(__atomic_load_8(p_bar, __ATOMIC_ACQUIRE));
      uint64_t dist = shortest_length_dijkstras(src, dst, g);
      printf("The distance was\t%lu\n", dist);
    };
//    f();
    for(uint64_t t = 0; t < T_NUM; t++)
    {
      threads.push_back(std::thread(f));
    }
    for(auto& t : threads)
    {
      t.join();
    }
    bar = T_NUM;
  }
  //Build psuedo-random stuff to ingest
  //spawn threads to ingest
  //do some dijsktras
}
