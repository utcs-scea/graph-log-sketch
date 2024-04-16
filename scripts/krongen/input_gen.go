package main

import "flag"
import "strconv"
import "github.com/MichaelTJones/pcg"
import "github.com/vmunoz82/shuffle"
import "github.com/emer/empi/mpi"
import "os"
import "fmt"
import "strings"
import "runtime"


func roundFunction(v, key shuffle.FeistelWord) shuffle.FeistelWord {
  return (v * 941083987) ^ (key >> (v & 7) * 104729)
}

var channel_size uint64;

type PCG_Rec struct {
  p   *pcg.PCG32;
  loc uint64;
};

type KronInput struct {
  scale       uint8;
  edge_factor uint64;
  ab          uint64;
  c_norm      uint64;
  a_norm      uint64;
};

var vert_fiestel *shuffle.Feistel;
var edge_fiestel *shuffle.Feistel;

func vert_to_perm_vert(n uint64, v_id uint64) uint64 {
  perm_v_id, err := shuffle.RandomIndex(shuffle.FeistelWord(v_id), shuffle.FeistelWord(n), vert_fiestel);
  must(err == nil, err);
  return uint64(perm_v_id);
}

func perm_edge_to_edge(m uint64, p_id uint64) uint64 {
  e_id, err := shuffle.GetIndex(shuffle.FeistelWord(p_id), shuffle.FeistelWord(m), edge_fiestel);
  must(err == nil, err);
  return uint64(e_id);
}

func g5k_noweight(k KronInput, p *PCG_Rec, e_id uint64) (uint64, uint64) {
  var r int64;
  r = int64(2* uint64(k.scale) * e_id) - int64(p.loc);
  if r >= 0 {
    p.p = p.p.Advance(uint64(r));
  } else {
    p.p = p.p.Retreat(uint64(-1 *r));
  }

  p.loc = 2*uint64(k.scale)* (e_id + 1) ;


  var src uint64;
  var dst uint64;
  src = 0;
  dst = 0;

  var ib  uint8;
  for ib = 0; ib < k.scale; ib++ {
    ii_bit := uint64(p.p.Random()) > k.ab;
    var jj_thresh uint64;
    if ii_bit{
      jj_thresh = k.c_norm;
    } else{
      jj_thresh = k.a_norm;
    }
    jj_bit := uint64(p.p.Random()) > jj_thresh;

    src = src << 1;
    dst = dst << 1;
    if ii_bit { src += 1; }
    if jj_bit { dst += 1; }
  }

  return src,dst
}

func must(check bool, err error) {
  if(!check) {
    mpi.Finalize();
    panic(err);
  }
}

func musts(check bool, err string) {
  if(!check) {
    mpi.Finalize();
    panic(err);
  }
}

func getInputScale() uint8 {
  raw_s, err := strconv.ParseUint(flag.Arg(0), 0, 8);

  must(err == nil, err);

  musts(raw_s < 64, "The scale argument must be less than 64");

  return uint8(raw_s);
}

func getInputOutputFile(myrank uint64) *os.File {
  raw_s := flag.Arg(1);
  ret, err := os.OpenFile(raw_s + "-" + strconv.FormatUint(myrank, 10) + ".el", os.O_WRONLY|os.O_CREATE|os.O_TRUNC, 0664);
  must(err == nil, err);
  return ret;
}

type Edge struct {
  src uint64;
  dst uint64;
}

type InputParams struct {
  //Graph Size
  scale       uint8;
  tot_edges   uint64;
  edge_factor uint64;
  num_verts   uint64;

  //Output File
  outFile     *os.File;

  //Performance Tuning
  buffer_size uint64;
  go_routines uint64;

  //Kronecker Randomization
  seedA       uint8;
  seedB       uint8;
  seedC       uint8;

  //PCG Randomization
  state       uint64;
  seq         uint64;

  //Feistel Randomization
  fvert_keys  []shuffle.FeistelWord;
  fedge_keys  []shuffle.FeistelWord;
};

func setupCommandLine(ip *InputParams, myrank uint64) {

  //Graph Flags
  flag.Uint64Var(&ip.edge_factor, "edge_factor", 16, "The number of edges per vertex (default 16, max 2^scale)");

  //Kronecker Flags
  raw_seedA := flag.Uint64("seedA", 57, "The seed for A (100 - sA - sB - sC = sD)");
  raw_seedB := flag.Uint64("seedB", 19, "The seed for B");
  raw_seedC := flag.Uint64("seedC", 19, "The seed for C");

  //Performance Flags
  flag.Uint64Var(&ip.buffer_size, "buffer_size", 10_000, "The buffer_size for the communication channel (default 10_000)");
  threads := flag.Int("threads", 1, "The number of threads that exist");
  runtime.GOMAXPROCS(*threads);
  flag.Uint64Var(&ip.go_routines, "routines", 1, "The number of routines")

  //PCG Flags
  flag.Uint64Var(&ip.state, "pcg_state", 0xAAD3342859, "Set the state for pcg randomization");
  flag.Uint64Var(&ip.seq  , "pcg_seq"  , 0X4537397DDE, "Set the sequence for pcg randomization");

  //Feistel Flags

    // Gotten from the vmunoz82/shuffle github.com
  var vkeys = []shuffle.FeistelWord{
        0xA45CF355C3B1CD88,
        0x8B9271CC2FC9365A,
        0x33CD458F23C816B1,
        0xC026F9D152DE23A9};

  var ekeys = []shuffle.FeistelWord{
        0x8B9271CC2FC9365A,
        0xA45CF355C3B1CD88,
        0xC026F9D152DE23A9,
        0x33CD458F23C816B1};

  feist_vert_raw := flag.String("fvert_key", "", "The key for vertices shuffle.")
  feist_edge_raw := flag.String("fedge_key", "", "The key for vertices shuffle.")

  if *feist_vert_raw == "" {ip.fvert_keys = vkeys;} else{
    feist_vert_strings := strings.Split(*feist_vert_raw, ",");
    ip.fvert_keys = make([]shuffle.FeistelWord, len(feist_vert_strings));
    for i, str := range feist_vert_strings {
      ret, err := strconv.ParseUint(str, 0, 64);
      must(err == nil, err);
      ip.fvert_keys[i] = shuffle.FeistelWord(ret);
    }
  }
  if *feist_vert_raw == "" {ip.fedge_keys = ekeys;} else{
    feist_edge_strings := strings.Split(*feist_edge_raw, ",");
    ip.fedge_keys = make([]shuffle.FeistelWord, len(feist_edge_strings));
    for i, str := range feist_edge_strings {
      ret, err := strconv.ParseUint(str, 0, 64);
      must(err == nil, err);
      ip.fedge_keys[i] = shuffle.FeistelWord(ret);
    }
  }

  flag.Parse();

  //The one scale argument parsing
  musts(flag.NArg() == 2, "You used " + strconv.Itoa(flag.NArg()) + " arguments, when there should be 2 arguments: ./input_gen <SCALE> <out-stub>");

  ip.scale = getInputScale();
  var n = uint64(1) << ip.scale;

  ip.num_verts = n;

  ip.outFile = getInputOutputFile(myrank);

  //Check that Edge Factor is large enough
  musts(ip.edge_factor < n, "Edge factor must be less than 2^SCALE");

  //Check that the seeds are valid
  musts(*raw_seedA < 100 && *raw_seedB < 100 && *raw_seedC < 100,
        "sA, sB, sC must be < 100");

  ip.seedA = uint8(*raw_seedA);
  ip.seedB = uint8(*raw_seedB);
  ip.seedC = uint8(*raw_seedC);

  var seedABC uint16;
  seedABC = uint16(ip.seedA) + uint16(ip.seedB) + uint16(ip.seedC);
  musts(seedABC < 100, "sD must be positive");

  ip.tot_edges = n * ip.edge_factor;
  musts(ip.tot_edges/n == ip.edge_factor, "Total Edges overflows");

}

func main() {

  mpi.Init();
  musts(mpi.IsOn(), "MPI is not on");
  raw_myrank := mpi.WorldRank();
  raw_nprocs := mpi.WorldSize();
  if raw_myrank < 0 || raw_nprocs <= 0 {
    panic("MPI did not initialize correctly")
  }

  myrank := uint64(raw_myrank);
  nprocs := uint64(raw_nprocs);

  // Input parameters
  var ip InputParams;
  setupCommandLine(&ip, myrank);
  defer ip.outFile.Close();


  //Create the KronInput
  ki := KronInput{ip.scale,
                  ip.edge_factor,
                  uint64((float64(ip.seedA + ip.seedB)/100) * float64(uint64(1) <<32)),
                  uint64((float64(ip.seedC)/float64(100 - (ip.seedA + ip.seedB))) * float64(uint64(1)<<32)),
                  uint64((float64(ip.seedA)/float64(ip.seedA + ip.seedB)) * float64(uint64(1)<<32))};

  pr := PCG_Rec{pcg.NewPCG32(), 0};

  pr.p = pr.p.Seed(ip.state, ip.seq);

  //Create the Fiestel Function inputs
  vert_fiestel = shuffle.NewFeistel(ip.fvert_keys, roundFunction)
  edge_fiestel = shuffle.NewFeistel(ip.fedge_keys, roundFunction)

  //Compute what myrank should do
  num_big_ranks := ip.tot_edges % nprocs;

  var num_edges uint64;
  if myrank < num_big_ranks {
    num_edges = ip.tot_edges/nprocs + 1
  } else {
    num_edges = ip.tot_edges/nprocs;
  }

  var edges_low uint64;

  if(myrank < num_big_ranks) {
    edges_low = myrank * (ip.tot_edges/nprocs + 1);
  } else {
    edges_low =  num_big_ranks * (ip.tot_edges/nprocs + 1) + (myrank - num_big_ranks) * (ip.tot_edges/nprocs);
  }

  ch := make(chan Edge, ip.buffer_size);

  var compute = func(edges_low uint64, edges_high uint64){

    var i uint64;
    for i = edges_low; i < edges_high; i++ {
      e_id := perm_edge_to_edge(ip.tot_edges, i);
      p_src, p_dst := g5k_noweight(ki, &pr, e_id);
      src := vert_to_perm_vert(ip.num_verts, p_src);
      dst := vert_to_perm_vert(ip.num_verts, p_dst);
      ch <- Edge{src, dst};
    }

    close(ch);
  }

  //TODO enable multiple routines
  go compute(edges_low, edges_low + num_edges);

  for edge := range ch {
    ip.outFile.WriteString(fmt.Sprintf("%d %d\n", edge.src, edge.dst));
  }

  mpi.Finalize();
}
