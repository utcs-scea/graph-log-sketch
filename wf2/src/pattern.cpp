// SPDX-License-Identifier: BSD-2-Clause
// Copyright (c) 2023. University of Texas at Austin. All rights reserved.

#include "pattern.hpp"

namespace wf2 {
std::pair<double, double> nyc_loc(40.67, -73.94);
galois::DynamicBitSet bitset_sync;
galois::DynamicBitSet bitset_imp_ee_people;
galois::DynamicBitSet bitset_sync_imp_ee_people;
galois::DynamicBitSet bitset_imp_forum_events;
galois::DynamicBitSet bitset_sync_imp_forum_events;
galois::DynamicBitSet bitset_imp_publications;
galois::DynamicBitSet bitset_sync_imp_publications;
galois::DynamicBitSet bitset_sync_ammo_seller;
galois::DynamicBitSet bitset_ammo_seller;
galois::DynamicBitSet bitset_forum_event_2a;
galois::DynamicBitSet bitset_sync_forum_event_2a;
galois::DynamicBitSet bitset_forum_event_2b;
galois::DynamicBitSet bitset_sync_forum_event_2b;
galois::DynamicBitSet bitset_fe_jihad_topic;
galois::DynamicBitSet bitset_sync_fe_jihad_topic;
galois::DynamicBitSet bitset_forum_1;
galois::DynamicBitSet bitset_sync_forum_1;

GALOIS_SYNC_STRUCTURE_BITSET(sync);
GALOIS_SYNC_STRUCTURE_BITSET(sync_imp_ee_people);
GALOIS_SYNC_STRUCTURE_BITSET(sync_imp_forum_events);
GALOIS_SYNC_STRUCTURE_BITSET(sync_imp_publications);
GALOIS_SYNC_STRUCTURE_BITSET(sync_ammo_seller);
GALOIS_SYNC_STRUCTURE_BITSET(sync_forum_event_2a);
GALOIS_SYNC_STRUCTURE_BITSET(sync_forum_event_2b);
GALOIS_SYNC_STRUCTURE_BITSET(sync_fe_jihad_topic);
GALOIS_SYNC_STRUCTURE_BITSET(sync_forum_1);
GALOIS_SYNC_STRUCTURE_BITSET(forum_1);

WF2_SYNC_STRUCTURE_MIRROR_TO_MASTER(imp_ee_people);
WF2_SYNC_STRUCTURE_MIRROR_TO_MASTER(imp_publications);
WF2_SYNC_STRUCTURE_MASTER_TO_MIRROR(imp_publications);
WF2_SYNC_STRUCTURE_MASTER_TO_MIRROR(ammo_seller);
WF2_SYNC_STRUCTURE_MASTER_TO_MIRROR(forum_event_2a);
WF2_SYNC_STRUCTURE_MASTER_TO_MIRROR(forum_event_2b);
WF2_SYNC_STRUCTURE_MASTER_TO_MIRROR(fe_jihad_topic);
WF2_SYNC_STRUCTURE_MASTER_TO_MIRROR(imp_ee_people);
WF2_SYNC_STRUCTURE_MASTER_TO_MIRROR(forum_1);
WF2_SYNC_STRUCTURE_MIRROR_TO_MASTER(imp_forum_events);

bool proximity_to_nyc(const wf2::TopicVertex& A) {
  double lon_miles = 0.91 * std::abs(A.lon - nyc_loc.second);
  double lat_miles = 1.15 * std::abs(A.lat - nyc_loc.first);
  double distance  = std::sqrt(lon_miles * lon_miles + lat_miles * lat_miles);
  return distance <= 30.0;
}

/*
 * Get NYC Node information and propagate it across nodes
 * Organize nodes by type
 */
void PreProcessGraph(Graph& g_ref, galois::runtime::NetworkInterface& net,
                     galois::InsertBag<uint64_t>& persons,
                     galois::InsertBag<uint64_t>& forumevents,
                     galois::InsertBag<uint64_t>& forums,
                     galois::InsertBag<uint64_t>& topics,
                     galois::InsertBag<uint64_t>& publications) {
  bool is_nyc_host = false;

  galois::do_all(
      galois::iterate(g_ref.masterNodesRange().begin(),
                      g_ref.masterNodesRange().end()),
      [&](const wf2::GlobalNodeID& lNode) {
        auto& node           = g_ref.getData(lNode);
        const uint64_t gNode = g_ref.getGID(lNode);

        switch (node.type) {
        case agile::workflow1::TYPES::PERSON:
          persons.emplace(lNode);
          break;
        case agile::workflow1::TYPES::FORUM:
          forums.emplace(lNode);
          break;
        case agile::workflow1::TYPES::FORUMEVENT:
          forumevents.emplace(lNode);
          break;
        case agile::workflow1::TYPES::PUBLICATION:
          publications.emplace(lNode);
          break;
        case agile::workflow1::TYPES::TOPIC:
          topics.emplace(lNode);
          break;
        }
      },
      galois::steal());
}

void SPPersonPurchases(Graph& g_ref, galois::InsertBag<uint64_t>& persons,
                       galois::InsertBag<uint64_t>& interesting_persons) {
  /*
      Find people who have purchased a bath bomb, a pressure cooker and an
     electronic item
  */
  galois::do_all(
      galois::iterate(persons.begin(), persons.end()),
      [&](const wf2::GlobalNodeID& lNode) {
        auto& node           = g_ref.getData(lNode);
        const uint64_t gNode = g_ref.getGID(lNode);

        std::set<uint64_t> buyers;
        bool purchased_bath_bomb       = false;
        bool purchased_pressure_cooker = false;
        bool purchased_electronics     = false;
        for (auto e : g_ref.edges(lNode)) {
          auto& edge_node = g_ref.getData(g_ref.getEdgeDst(e));
          auto& edge_data = g_ref.getEdgeData(e);

          if (edge_data.type == agile::workflow1::TYPES::SALE &&
              edge_node.type == agile::workflow1::TYPES::PERSON) {
            if (edge_data.e.sale.product == 185785)
              buyers.insert(edge_data.e.sale.buyer);
          }

          if (edge_data.type == agile::workflow1::TYPES::PURCHASE) {
            if (edge_data.e.purchase.product == 2869238) {
              // bath bomb
              purchased_bath_bomb = true;
            } else if (edge_data.e.purchase.product == 271997) {
              // pressure cooker
              purchased_pressure_cooker = true;
            } else if (edge_data.e.purchase.product == 11650) {
              // electronics
              purchased_electronics = true;
            }
          }
        }
        // Find people who have sold ammunition to multiple unique people
        if (buyers.size() > 1) {
          bitset_sync_ammo_seller.set(lNode);
          bitset_ammo_seller.set(lNode);
        }
        if (purchased_bath_bomb && purchased_pressure_cooker &&
            purchased_electronics) {
          interesting_persons.emplace(lNode);
        }
      },
      galois::steal());
}

void SPEESale(Graph& g_ref, galois::InsertBag<uint64_t>& interesting_persons) {
  galois::do_all(
      galois::iterate(interesting_persons.begin(), interesting_persons.end()),
      [&](const wf2::GlobalNodeID& lNode) {
        auto& node           = g_ref.getData(lNode);
        const uint64_t gNode = g_ref.getGID(lNode);

        for (auto e : g_ref.edges(lNode)) {
          auto& edge_node = g_ref.getData(g_ref.getEdgeDst(e));
          auto& edge_data = g_ref.getEdgeData(e);

          if (edge_data.type == agile::workflow1::TYPES::PURCHASE &&
              edge_node.type == agile::workflow1::TYPES::PERSON) {
            if (edge_data.e.sale.product == 11650) {
              bitset_sync_imp_ee_people.set(g_ref.getEdgeDst(e));
              bitset_imp_ee_people.set(g_ref.getEdgeDst(e));
            }
          }

          if (edge_node.type == agile::workflow1::TYPES::FORUMEVENT) {
            bitset_sync_imp_forum_events.set(g_ref.getEdgeDst(e));
            bitset_imp_forum_events.set(g_ref.getEdgeDst(e));
          }
        }
      });
}

void SPEEPublication(Graph& g_ref, galois::InsertBag<uint64_t>& persons) {
  galois::do_all(galois::iterate(persons.begin(), persons.end()),
                 [&](const wf2::GlobalNodeID& lNode) {
                   if (bitset_imp_ee_people.test(lNode)) {
                     auto& node           = g_ref.getData(lNode);
                     const uint64_t gNode = g_ref.getGID(lNode);

                     for (auto e : g_ref.edges(lNode)) {
                       auto& edge_node = g_ref.getData(g_ref.getEdgeDst(e));
                       auto& edge_data = g_ref.getEdgeData(e);

                       if (edge_data.type == agile::workflow1::TYPES::AUTHOR &&
                           edge_node.type ==
                               agile::workflow1::TYPES::PUBLICATION) {
                         bitset_sync_imp_publications.set(g_ref.getEdgeDst(e));
                         bitset_imp_publications.set(g_ref.getEdgeDst(e));
                       }
                     }
                   }
                 });
}

void SPEETopic(Graph& g_ref, galois::InsertBag<uint64_t>& publications) {
  galois::do_all(galois::iterate(publications.begin(), publications.end()),
                 [&](const wf2::GlobalNodeID& lNode) {
                   if (bitset_imp_publications.test(lNode)) {
                     auto& node           = g_ref.getData(lNode);
                     const uint64_t gNode = g_ref.getGID(lNode);

                     bool is_close_to_nyc = false;
                     bool is_ee_topic     = false;

                     for (auto e : g_ref.edges(lNode)) {
                       auto& edge_node = g_ref.getData(g_ref.getEdgeDst(e));
                       auto& edge_data = g_ref.getEdgeData(e);

                       if (edge_data.type ==
                               agile::workflow1::TYPES::HASTOPIC &&
                           edge_node.type == agile::workflow1::TYPES::TOPIC) {
                         if (edge_node.getToken() == 43035) {
                           is_ee_topic = true;
                         }
                       }
                       if (edge_data.type == agile::workflow1::TYPES::HASORG &&
                           edge_node.type == agile::workflow1::TYPES::TOPIC) {
                         if (proximity_to_nyc(edge_node.v.topic)) {
                           is_close_to_nyc = true;
                         }
                       }
                     }

                     if (is_close_to_nyc && is_ee_topic) {
                       bitset_sync_imp_publications.set(lNode);
                     } else {
                       bitset_imp_publications.reset(lNode);
                     }
                   }
                 });
}

void SPEEPublicationReverse(Graph& g_ref,
                            galois::InsertBag<uint64_t>& persons) {
  galois::do_all(
      galois::iterate(persons.begin(), persons.end()),
      [&](const wf2::GlobalNodeID& lNode) {
        auto& node = g_ref.getData(lNode);
        if (bitset_imp_ee_people.test(lNode)) {
          const uint64_t gNode = g_ref.getGID(lNode);

          for (auto e : g_ref.edges(lNode)) {
            auto& edge_node = g_ref.getData(g_ref.getEdgeDst(e));
            auto& edge_data = g_ref.getEdgeData(e);

            if (edge_data.type == agile::workflow1::TYPES::AUTHOR &&
                edge_node.type == agile::workflow1::TYPES::PUBLICATION) {
              if (bitset_imp_publications.test(g_ref.getEdgeDst(e))) {
                bitset_sync_imp_ee_people.set(lNode);
              } else {
                bitset_imp_ee_people.reset(lNode);
              }
            }
          }
        }
      });
}

void SPTwoABFeTopic(Graph& g_ref, galois::InsertBag<uint64_t>& forum_events) {
  galois::do_all(galois::iterate(forum_events.begin(), forum_events.end()),
                 [&](const wf2::GlobalNodeID& lNode) {
                   auto& node           = g_ref.getData(lNode);
                   const uint64_t gNode = g_ref.getGID(lNode);

                   bool topic_2A_1  = false;
                   bool topic_2A_2  = false;
                   bool topic_2B_1  = false;
                   bool topic_2B_2  = false;
                   bool topic_2B_3  = false;
                   bool jihad_topic = false;

                   for (auto e : g_ref.edges(lNode)) {
                     auto& edge_node = g_ref.getData(g_ref.getEdgeDst(e));
                     auto& edge_data = g_ref.getEdgeData(e);

                     if (edge_node.type == agile::workflow1::TYPES::TOPIC) {
                       if (edge_node.getToken() == 69871376)
                         topic_2A_1 = true;
                       if (edge_node.getToken() == 1049632)
                         topic_2A_2 = true;
                       if (edge_node.getToken() == 771572)
                         topic_2B_1 = true;
                       if (edge_node.getToken() == 179057)
                         topic_2B_2 = true;
                       if (edge_node.getToken() == 127197)
                         topic_2B_3 = true;
                       if (edge_node.getToken() == 44311)
                         jihad_topic = true;
                     }
                   }

                   if (topic_2A_1 && topic_2A_2) {
                     bitset_sync_forum_event_2a.set(lNode);
                     bitset_forum_event_2a.set(lNode);
                   }
                   if (topic_2B_1 && topic_2B_2 && topic_2B_3) {
                     bitset_sync_forum_event_2b.set(lNode);
                     bitset_forum_event_2b.set(lNode);
                   }
                   if (jihad_topic) {
                     bitset_sync_fe_jihad_topic.set(lNode);
                     bitset_fe_jihad_topic.set(lNode);
                   }
                 });
}

void SPTwoABForumMinTime(
    Graph& g_ref, galois::InsertBag<uint64_t>& forums,
    std::vector<std::pair<uint64_t, time_t>>& forum_min_time_vec) {
  galois::do_all(
      galois::iterate(forums.begin(), forums.end()),
      [&](const wf2::GlobalNodeID& lNode) {
        auto& node           = g_ref.getData(lNode);
        const uint64_t gNode = g_ref.getGID(lNode);

        time_t min_time       = shad::data_types::kNullValue<time_t>;
        bool has_2a_event     = false;
        int jihad_topic_count = 0;
        bool has_nyc_topic    = false;

        for (auto e : g_ref.edges(lNode)) {
          auto& edge_node = g_ref.getData(g_ref.getEdgeDst(e));
          auto& edge_data = g_ref.getEdgeData(e);
          if (edge_node.type == agile::workflow1::TYPES::FORUMEVENT) {
            if (bitset_forum_event_2b.test(g_ref.getEdgeDst(e)))
              min_time = min_time == 0
                             ? edge_node.v.forum_event.date
                             : std::min(min_time, edge_node.v.forum_event.date);
            has_2a_event =
                has_2a_event | bitset_forum_event_2a.test(g_ref.getEdgeDst(e));
            jihad_topic_count +=
                bitset_fe_jihad_topic.test(g_ref.getEdgeDst(e)) ? 1 : 0;
          }
          if (edge_node.type == agile::workflow1::TYPES::TOPIC) {
            if (edge_node.getToken() == 60)
              has_nyc_topic = true;
          }
        }
        if (has_nyc_topic && jihad_topic_count > 1 && has_2a_event) {
          forum_min_time_vec.push_back(
              std::make_pair(node.getToken(), min_time));
        }
      });
}

void BroadcastForumInfo(
    Graph& g_ref, galois::runtime::NetworkInterface& net,
    std::vector<std::pair<uint64_t, time_t>>& forum_min_time_vec,
    std::map<uint64_t, time_t>& forum_min_time) {

  for (int i = 0; i < net.Num; i++) {
    if (i == net.ID)
      continue;
    galois::runtime::SendBuffer lhsSendBuffer;
    galois::runtime::gSerialize(lhsSendBuffer, forum_min_time_vec);
    net.sendTagged(i, 0, std::move(lhsSendBuffer));
  }

  for (int i = 0; i < (net.Num - 1); i++) {
    decltype(net.recieveTagged(0)) p;
    do {
      p = net.recieveTagged(0);
    } while (!p);
    std::vector<std::pair<uint64_t, time_t>> recv_forum_info;
    galois::runtime::gDeserialize(p->second, recv_forum_info);
    forum_min_time_vec.insert(forum_min_time_vec.end(), recv_forum_info.begin(),
                              recv_forum_info.end());
  }
  std::copy(forum_min_time_vec.begin(), forum_min_time_vec.end(),
            std::inserter(forum_min_time, forum_min_time.begin()));
}

void FinalPatternMatch(Graph& g_ref,
                       galois::InsertBag<uint64_t>& interesting_persons,
                       std::map<uint64_t, time_t>& forum_min_time) {
  galois::do_all(
      galois::iterate(interesting_persons.begin(), interesting_persons.end()),
      [&](const wf2::GlobalNodeID& lNode) {
        auto& node           = g_ref.getData(lNode);
        const uint64_t gNode = g_ref.getGID(lNode);

        bool ESP         = false;
        time_t latest_BB = 0, latest_PC = 0, latest_AMO = 0;

        for (auto e : g_ref.edges(lNode)) {
          auto& edge_node = g_ref.getData(g_ref.getEdgeDst(e));
          auto& edge_data = g_ref.getEdgeData(e);
          if (edge_data.type == agile::workflow1::TYPES::PURCHASE) {
            if (edge_data.e.purchase.product == 2869238) {
              // bath bomb
              latest_BB = std::max(latest_BB, edge_data.e.purchase.date);
            } else if (edge_data.e.purchase.product == 271997) {
              // pressure cooker
              latest_PC = std::max(latest_PC, edge_data.e.purchase.date);
            } else if (edge_data.e.purchase.product == 185785) {
              // ammunition
              if (edge_data.e.purchase.date > latest_AMO &&
                  bitset_ammo_seller.test(g_ref.getEdgeDst(e))) {
                latest_AMO = edge_data.e.purchase.date;
              }
            } else if (edge_data.e.purchase.product == 11650) {
              // electronics
              if (!ESP)
                ESP = bitset_imp_ee_people.test(g_ref.getEdgeDst(e));
            }
          }
        }

        time_t trans_date =
            std::min(std::min(latest_BB, latest_PC), latest_AMO);

        if (ESP && trans_date != 0) {
          for (auto e : g_ref.edges(lNode)) {
            auto& edge_node = g_ref.getData(g_ref.getEdgeDst(e));
            auto& edge_data = g_ref.getEdgeData(e);
            if (edge_data.type == agile::workflow1::TYPES::AUTHOR &&
                edge_node.type == agile::workflow1::TYPES::FORUMEVENT) {
              auto x = forum_min_time.find(edge_node.v.forum_event.forum);
              if (x != forum_min_time.end() && x->second != 0 &&
                  x->second < trans_date)
                std::cout << "Found a person: " << node.getToken() << std::endl;
            }
          }
        }
      });
}

void MatchPattern(Graph& g_ref, galois::runtime::NetworkInterface& net) {
  galois::InsertBag<uint64_t> interesting_persons;
  galois::InsertBag<uint64_t> persons;
  galois::InsertBag<uint64_t> forumevents;
  galois::InsertBag<uint64_t> forums;
  galois::InsertBag<uint64_t> topics;
  galois::InsertBag<uint64_t> publications;

  std::vector<std::pair<uint64_t, time_t>> forum_min_time_vec;
  std::map<uint64_t, time_t> forum_min_time;

  double phase_start = 0;
  double phase_end   = 0;
  double algo_start  = 0;
  double algo_end    = 0;

  /*
      TODO - is there a better way to have the global id accessible from the
     node?
  */
  galois::do_all(galois::iterate(g_ref.allNodesRange().begin(),
                                 g_ref.allNodesRange().end()),
                 [&](const wf2::GlobalNodeID& lNode) {
                   auto& node = g_ref.getData(lNode);
                   node.set_id(g_ref.getGID(lNode));
                 });

  wf2::PreProcessGraph(g_ref, net, persons, forumevents, forums, topics,
                       publications);

  bitset_sync.resize(g_ref.size());
  bitset_imp_ee_people.resize(g_ref.size());
  bitset_sync_imp_ee_people.resize(g_ref.size());
  bitset_imp_forum_events.resize(g_ref.size());
  bitset_sync_imp_forum_events.resize(g_ref.size());
  bitset_imp_publications.resize(g_ref.size());
  bitset_sync_imp_publications.resize(g_ref.size());
  bitset_sync_ammo_seller.resize(g_ref.size());
  bitset_ammo_seller.resize(g_ref.size());
  bitset_forum_event_2a.resize(g_ref.size());
  bitset_sync_forum_event_2a.resize(g_ref.size());
  bitset_forum_event_2b.resize(g_ref.size());
  bitset_sync_forum_event_2b.resize(g_ref.size());
  bitset_fe_jihad_topic.resize(g_ref.size());
  bitset_sync_fe_jihad_topic.resize(g_ref.size());
  bitset_forum_1.resize(g_ref.size());
  bitset_sync_forum_1.resize(g_ref.size());

  bitset_sync.reset();
  bitset_imp_ee_people.reset();
  bitset_sync_imp_ee_people.reset();
  bitset_imp_forum_events.reset();
  bitset_sync_imp_forum_events.reset();
  bitset_imp_publications.reset();
  bitset_sync_imp_publications.reset();
  bitset_sync_ammo_seller.reset();
  bitset_ammo_seller.reset();
  bitset_forum_event_2a.reset();
  bitset_sync_forum_event_2a.reset();
  bitset_forum_event_2b.reset();
  bitset_sync_forum_event_2b.reset();
  bitset_fe_jihad_topic.reset();
  bitset_sync_fe_jihad_topic.reset();
  bitset_forum_1.reset();
  bitset_sync_forum_1.reset();

  if (BENCH) {
    galois::runtime::getHostBarrier().wait();
    phase_start = MPI_Wtime();
  }

  SPPersonPurchases(g_ref, persons, interesting_persons);
  sync_substrate->sync<writeSource, readDestination,
                       SyncMasterMirror_ammo_seller, Bitset_sync_ammo_seller>(
      "Sync_ammo_seller");

  /* EE Sub-pattern */
  SPEESale(g_ref, interesting_persons);
  sync_substrate
      ->sync<writeDestination, readSource, SyncMirrorMaster_imp_ee_people,
             Bitset_sync_imp_ee_people>("Sync_ee_people");
  SPEEPublication(g_ref, persons);
  sync_substrate
      ->sync<writeDestination, readSource, SyncMirrorMaster_imp_publications,
             Bitset_sync_imp_publications>("Sync_publications");
  bitset_sync_imp_publications.reset();
  SPEETopic(g_ref, publications);
  sync_substrate
      ->sync<writeSource, readDestination, SyncMasterMirror_imp_publications,
             Bitset_sync_imp_publications>("Sync_publications_reverse");
  bitset_sync_imp_ee_people.reset();
  SPEEPublicationReverse(g_ref, persons);
  sync_substrate
      ->sync<writeSource, readDestination, SyncMasterMirror_imp_ee_people,
             Bitset_sync_imp_ee_people>("Sync_publications_reverse");

  /* Forum Events 2A 2B */
  SPTwoABFeTopic(g_ref, forumevents);
  sync_substrate
      ->sync<writeSource, readDestination, SyncMasterMirror_forum_event_2a,
             Bitset_sync_forum_event_2a>("Sync_publications_reverse");
  sync_substrate
      ->sync<writeSource, readDestination, SyncMasterMirror_forum_event_2b,
             Bitset_sync_forum_event_2b>("Sync_publications_reverse");
  sync_substrate
      ->sync<writeSource, readDestination, SyncMasterMirror_fe_jihad_topic,
             Bitset_sync_fe_jihad_topic>("Sync_publications_reverse");

  /* Forum 1 Subpattern*/
  SPTwoABForumMinTime(g_ref, forums, forum_min_time_vec);

  BroadcastForumInfo(g_ref, net, forum_min_time_vec, forum_min_time);

  FinalPatternMatch(g_ref, interesting_persons, forum_min_time);

  sync_substrate
      ->sync<writeDestination, readSource, SyncMirrorMaster_imp_forum_events,
             Bitset_sync_imp_forum_events>("Sync_forum_events");

  if (BENCH) {
    galois::runtime::getHostBarrier().wait();
    phase_end = MPI_Wtime();
    std::cout << net.ID << " Total Time " << phase_end - phase_start << "\n";
  }
}
} // namespace wf2
