package main

import (
	"bufio"
	"encoding/binary"
	"encoding/csv"
	"flag"
	"fmt"
	"io"
	"os"
	"sort"
	"strconv"
	"sync"
	"sync/atomic"
	"unsafe"
)

type ConvertType int64

const (
	el2bel ConvertType = iota
	sbel2gr2
	bel2bdf
	bel2vdl
	vdl2hdl
	sbel2col
	sbel2dst
)

var convert = map[string]ConvertType{
	"el2bel":   el2bel,
	"sbel2gr2": sbel2gr2,
	"bel2bdf":  bel2bdf,
	"bel2vdl":  bel2vdl,
	"vdl2hdl":  vdl2hdl,
	"sbel2col": sbel2col,
	"sbel2dst": sbel2dst,
}

type InputParams struct {
	srcFile   *os.File
	dstFile   *os.File
	convType  ConvertType
	numNodes  uint64
	numVhosts uint64
	numHosts  uint64
	comma     rune
	comment   rune
	routines  uint64
}

func musts(b bool, str string) {
	if !b {
		panic(str)
	}
}

func must(err error) {
	if err != nil {
		panic(err)
	}
}

func max(a, b uint64) uint64 {
	if a <= b {
		return a
	}
	return b
}

const sizeofu64 uint64 = uint64(unsafe.Sizeof(uint64(0)))

func writeU64At(outputFile *os.File, off uint64, val uint64) {
	var u64buf [sizeofu64]byte
	binary.LittleEndian.PutUint64(u64buf[0:], val)
	num, err := outputFile.WriteAt(u64buf[0:], int64(off))
	must(err)
	musts(uint64(num) == sizeofu64, "The size of what was printed was constructed "+fmt.Sprint(num)+" it should have been "+fmt.Sprint(sizeofu64))
}

func goWriteU64AtWG(outputFile *os.File, off uint64, val uint64, wg *sync.WaitGroup) {
	wg.Add(1)
	go func() {
		defer wg.Done()
		writeU64At(outputFile, off, val)
	}()
}

func readU64At(inputFile *os.File, off uint64) uint64 {
	var u64buf [sizeofu64]byte
	num, err := inputFile.ReadAt(u64buf[0:], int64(off))
	must(err)
	musts(uint64(num) == sizeofu64, "Read the wrong number of things")
	ret := binary.LittleEndian.Uint64(u64buf[0:])
	return ret
}

type u64WriteRecord struct {
	pos uint64
	val uint64
}

func vdl2hdlConvert(inputFile *os.File, outputFile *os.File, vhosts uint64, hosts uint64) {
	var vhostDist []uint64 = make([]uint64, vhosts)
	var hostDist []uint64 = make([]uint64, hosts)
	scanner := bufio.NewScanner(inputFile)
	var currVhost uint64 = 0
	for scanner.Scan() {
		line := scanner.Text()
		i, err := strconv.ParseUint(line, 10, 64)
		must(err)
		vhostDist[currVhost] += i
		currVhost = (currVhost + 1) % vhosts
	}
	sort.Slice(vhostDist, func(a, b int) bool {
		return vhostDist[a] > vhostDist[b]
	})
	for _, v := range vhostDist {
		sort.Slice(hostDist, func(a, b int) bool {
			return hostDist[a] < hostDist[b]
		})

		hostDist[0] += v
	}

	for _, v := range hostDist {
		fmt.Fprintln(outputFile, v)
	}
}

func bel2vdlConvert(inputFile *os.File, outputFile *os.File, vhosts uint64, routines uint64) {
	var vhostDist []uint64 = make([]uint64, vhosts)
	var wg sync.WaitGroup

	finfo, err := inputFile.Stat()
	must(err)
	sz := uint64(finfo.Size())
	sz -= (sz % sizeofu64)

	var indexes = sz / (2 * sizeofu64)

	var chunkSize uint64 = indexes / routines
	var remaining uint64 = indexes % routines
	if routines == 0 {
		routines = indexes
	}

	for i := uint64(0); i < routines; i++ {
		var howManyAdds uint64

		//Load balance
		if remaining > i {
			howManyAdds = i
		} else {
			howManyAdds = remaining
		}
		var startInd uint64 = chunkSize*i + howManyAdds
		var endInd uint64 = startInd + chunkSize
		if remaining > i {
			endInd += 1
		}
		wg.Add(1)
		go func(srcFile *os.File, vhostDist []uint64, startInd uint64, endInd uint64) {
			defer wg.Done()
			for i := startInd; i < endInd; i++ {
				//Compute vhost location
				curr := readU64At(srcFile, i*2*sizeofu64)
				atomic.AddUint64(&vhostDist[curr%vhosts], 1)
			}
		}(inputFile, vhostDist, startInd, endInd)
	}
	wg.Wait()
	for i := uint64(0); i < vhosts; i++ {
		fmt.Fprintln(outputFile, vhostDist[i])
	}
}

func el2belConvert(inputFile *os.File, outputFile *os.File, comma rune, comment rune, routines uint64) {
	reader := csv.NewReader(inputFile)
	reader.ReuseRecord = true
	reader.FieldsPerRecord = 2
	reader.Comment = comment
	reader.Comma = comma
	var wg sync.WaitGroup
	var index uint64 = 0
	var err error

	writeChan := make(chan u64WriteRecord, routines*1024)
	for i := uint64(0); i < routines; i++ {
		wg.Add(1)
		go func() {
			defer wg.Done()
			for vals := range writeChan {
				writeU64At(outputFile, vals.pos, vals.val)
			}
		}()
	}

	var numVerts uint64 = 0

	for record, err := reader.Read(); err == nil; record, err = reader.Read() {
		src, err := strconv.ParseUint(record[0], 10, 64)
		must(err)
		dst, err := strconv.ParseUint(record[1], 10, 64)
		must(err)
		numVerts = max(max(src, dst), numVerts)
		writeChan <- u64WriteRecord{index * 2 * sizeofu64, src}
		writeChan <- u64WriteRecord{index*2*sizeofu64 + sizeofu64, dst}
		index++
	}
	if err != io.EOF {
		must(err)
	}
	numVerts += 1
	fmt.Println("The number of Vertices is Projected to be:", numVerts)
	close(writeChan)
	wg.Wait()
}

func sbel2gr2Convert(inputFile *os.File, outputFile *os.File, routines uint64) {
	var err error
	var wg sync.WaitGroup
	finfo, err := inputFile.Stat()
	must(err)
	sz := uint64(finfo.Size())
	sz -= (sz % sizeofu64)

	writeChan := make(chan u64WriteRecord, routines*1024)
	for i := uint64(0); i < routines; i++ {
		wg.Add(1)
		go func() {
			defer wg.Done()
			for vals := range writeChan {
				writeU64At(outputFile, vals.pos, vals.val)
			}
		}()
	}

	//gr Version
	writeChan <- u64WriteRecord{uint64(0), uint64(2)}

	//edgeDataSz
	writeChan <- u64WriteRecord{uint64(sizeofu64), uint64(0)}

	//Get the largest vertexid
	numNodes := readU64At(inputFile, sz-2*sizeofu64)
	numNodes += 1
	writeChan <- u64WriteRecord{2 * sizeofu64, numNodes}

	//Get the number of edges
	numEdges := uint64(sz) / uint64(2*sizeofu64)
	writeChan <- u64WriteRecord{3 * sizeofu64, numEdges}

	//Write edges
	const nodeBase uint64 = 4 * sizeofu64
	var edgeBase uint64 = nodeBase + sizeofu64*numNodes

	var lastSrc uint64 = 0
	var tempSrc uint64 = 0

	for i := uint64(0); i < numEdges; i++ {
		//Read the src
		tempSrc = readU64At(inputFile, i*2*sizeofu64)

		//See if you should write to src array
		if tempSrc != lastSrc {
			writeChan <- u64WriteRecord{nodeBase + i*sizeofu64, i}
			lastSrc = tempSrc
		}

		//Now Write to edge array
		dst := readU64At(inputFile, i*2*sizeofu64+sizeofu64)
		writeChan <- u64WriteRecord{edgeBase + i*sizeofu64, dst}
	}
	close(writeChan)
	wg.Wait()
}

func calculateDegrees(nodeID uint64, sz uint64, srcFile *os.File, dstFile *os.File, wg *sync.WaitGroup) {
	var count uint64 = 0
	for i := uint64(0); i < sz; i += 2 * sizeofu64 {
		src := readU64At(srcFile, i)
		if src == nodeID {
			count++
		}
	}
	goWriteU64AtWG(dstFile, nodeID*sizeofu64, count, wg)
}

func bel2bdfConvert(srcFile *os.File, dstFile *os.File, numNodes uint64, routines uint64) {
	var err error
	finfo, err := srcFile.Stat()
	must(err)
	sz := uint64(finfo.Size())
	sz -= (sz % sizeofu64)

	var chanWG sync.WaitGroup
	writeChan := make(chan u64WriteRecord, routines*1024)
	for i := uint64(0); i < routines; i++ {
		chanWG.Add(1)
		go func() {
			defer chanWG.Done()
			for vals := range writeChan {
				writeU64At(dstFile, vals.pos, vals.val)
			}
		}()
	}

	var wg sync.WaitGroup
	for i := uint64(0); i < numNodes; i += routines {
		for j := i; j < i+routines && j < numNodes; j++ {
			wg.Add(1)
			go func(nodeID uint64) {
				defer wg.Done()
				calculateDegrees(nodeID, sz, srcFile, dstFile, &wg)
			}(j)
		}
		wg.Wait()
	}
	wg.Wait()
	close(writeChan)
	chanWG.Wait()
}

func sbel2colConvert(srcFile *os.File, dstFile *os.File, numVerts uint64, routines uint64) {
	var err error
	var wg sync.WaitGroup
	finfo, err := srcFile.Stat()
	must(err)
	sz := uint64(finfo.Size())
	sz -= (sz % sizeofu64)

	writeChan := make(chan u64WriteRecord, routines*1024)
	for i := uint64(0); i < routines; i++ {
		wg.Add(1)
		go func() {
			defer wg.Done()
			for vals := range writeChan {
				writeU64At(dstFile, vals.pos, vals.val)
			}
		}()
	}

	//Number of valid elements
	writeChan <- u64WriteRecord{uint64(0), uint64(numVerts)}

	//Placeholder
	writeChan <- u64WriteRecord{uint64(sizeofu64), uint64(0)}

	//Get the number of edges
	numEdges := uint64(sz) / uint64(2*sizeofu64)

	var lastSrc uint64 = 0
	var tempSrc uint64 = 0

	var count uint64 = 0

	const vertexBase uint64 = 2 * sizeofu64

	for i := uint64(0); i < numEdges; i++ {
		//Read the src
		tempSrc = readU64At(srcFile, i*2*sizeofu64)

		//See if you should write to src array
		for j := lastSrc; j < tempSrc; j++ {
			writeChan <- u64WriteRecord{vertexBase + j*sizeofu64, count}
		}
		count++
	}
	close(writeChan)
	wg.Wait()
}

func sbel2dstConvert(srcFile *os.File, dstFile *os.File, routines uint64) {
	var err error
	var wg sync.WaitGroup
	finfo, err := srcFile.Stat()
	must(err)
	sz := uint64(finfo.Size())
	sz -= (sz % sizeofu64)

	writeChan := make(chan u64WriteRecord, routines*1024)
	for i := uint64(0); i < routines; i++ {
		wg.Add(1)
		go func() {
			defer wg.Done()
			for vals := range writeChan {
				writeU64At(dstFile, vals.pos, vals.val)
			}
		}()
	}

	//Get the number of edges
	numEdges := uint64(sz) / uint64(2*sizeofu64)

	//Number of valid elements
	writeChan <- u64WriteRecord{uint64(0), uint64(numEdges)}

	//Placeholder
	writeChan <- u64WriteRecord{uint64(sizeofu64), uint64(0)}

	var tempDst uint64 = 0

	const edgeBase uint64 = 2 * sizeofu64

	for i := uint64(0); i < numEdges; i++ {
		//Read the src
		tempDst = readU64At(srcFile, i*2*sizeofu64+sizeofu64)
		writeChan <- u64WriteRecord{edgeBase + i*sizeofu64, tempDst}
	}
	close(writeChan)
	wg.Wait()
}

func setupCommandLine(ip *InputParams) {

	var convUsage string = "The graph conversion to do: "
	for k := range convert {
		convUsage += " " + k + ","
	}
	conv := flag.String("conv", "", convUsage)
	srcFName := flag.String("src", "", "The src file name")
	dstFName := flag.String("dst", "", "The dst file name")
	flag.Uint64Var(&ip.numNodes, "nodes", 0, "The number of nodes (only for bel2bdf, sbel2col)")
	flag.Uint64Var(&ip.numVhosts, "vhosts", 0, "The number of vhosts (only for bel2vdl)")
	flag.Uint64Var(&ip.numHosts, "hosts", 0, "The number of hosts (only for vdl2hdl)")
	comma := flag.String("comma", ",", "The delimiter for reading el files parses as string and Unquotes single rune")
	comment := flag.String("comment", "#", "The comment signifier for reading el files pasrses as string and Unquotes single rune")
	flag.Uint64Var(&ip.routines, "writers", 0, "The number of writers that are spawned")

	flag.Parse()

	musts(flag.NArg() == 0, "You should have no arguments other than flags")

	var err error
	//Conversion Type
	convType, ok := convert[*conv]
	musts(ok, "Graph conversion type invalid: "+*conv)
	ip.convType = convType

	//Files
	ip.srcFile, err = os.Open(*srcFName)
	must(err)
	ip.dstFile, err = os.OpenFile(*dstFName, os.O_CREATE|os.O_WRONLY, 0660)
	must(err)

	//number of objects error checking
	musts((ip.convType != bel2bdf && ip.convType != sbel2col) || ip.numNodes != 0, "Number of Nodes in file must not be zero")
	musts(ip.convType != bel2vdl || ip.numVhosts != 0, "Number of Virtual hosts is not nonzero")

	//delimiter error checking
	var sepRunes string

	sepRunes, err = strconv.Unquote(`"` + *comma + `"`)
	must(err)
	ip.comma = rune(sepRunes[0])

	sepRunes, err = strconv.Unquote(`"` + *comment + `"`)
	ip.comment = rune(sepRunes[0])
	must(err)

	musts(ip.routines != 0, "Writers must not be equal to zero.")

}

func main() {
	var ip InputParams
	setupCommandLine(&ip)

	switch ip.convType {
	case el2bel:
		el2belConvert(ip.srcFile, ip.dstFile, ip.comma, ip.comment, ip.routines)
	case sbel2gr2:
		sbel2gr2Convert(ip.srcFile, ip.dstFile, ip.routines)
	case bel2bdf:
		bel2bdfConvert(ip.srcFile, ip.dstFile, ip.numNodes, ip.routines)
	case bel2vdl:
		bel2vdlConvert(ip.srcFile, ip.dstFile, ip.numVhosts, ip.routines)
	case vdl2hdl:
		vdl2hdlConvert(ip.srcFile, ip.dstFile, ip.numVhosts, ip.numHosts)
	case sbel2col:
		sbel2colConvert(ip.srcFile, ip.dstFile, ip.numNodes, ip.routines)
	case sbel2dst:
		sbel2dstConvert(ip.srcFile, ip.dstFile, ip.routines)
	}
}
