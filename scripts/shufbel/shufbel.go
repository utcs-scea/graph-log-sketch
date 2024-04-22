package main

import (
	"flag"
	"fmt"
	"io"
	"math/rand"
	"os"
	"runtime"
	"syscall"

	"github.com/vmunoz82/shuffle"
)

// from https://github.com/bramp/dsector/blob/e96a7734bb3f/input/io.go#L41
func ReadFullAt(r io.ReaderAt, buf []byte, off int64) (n int, err error) {
	min := len(buf)
	for n < min && err == nil {
		var nn int
		nn, err = r.ReadAt(buf[n:], off)
		n += nn
		off += int64(nn)
	}
	if n >= min {
		err = nil
	} else if n > 0 && err == io.EOF {
		err = io.ErrUnexpectedEOF
	}
	return
}

/* Function for each round, could be anything don't need to be reversible */
func roundFunction(v, key shuffle.FeistelWord) shuffle.FeistelWord {
	return (v * 941083987) ^ (key >> (v & 7) * 104729)
}

// main function
func main() {
	// parse CLI options
	threads := flag.Int("threads", runtime.NumCPU(), "number of threads")
	routines := flag.Int("routines", 8*runtime.NumCPU(), "number of goroutines")
	big := flag.Bool("big", false, "don't attempt to memory-map the input file")
	rseed := flag.Int64("rseed", 0, "random seed")
	flag.Parse()

	// limit go runtime threads
	runtime.GOMAXPROCS(*threads)
	pageSize := syscall.Getpagesize()

	files := flag.Args()
	if len(files) != 2 {
		fmt.Println("Usage: shufbel [options] input output ...")
		flag.PrintDefaults()
		return
	}
	input_file := files[0]
	output_file := files[1]

	// Open input file.
	fin, err := os.Open(input_file)
	if err != nil {
		fmt.Printf("Error opening file %s: %v\n", input_file, err)
		return
	}
	defer fin.Close()

	// Compute file size.
	fi, err := fin.Stat()
	if err != nil {
		fmt.Printf("Error getting file info: %v\n", err)
	}
	sizeBytes := uint64(fi.Size())
	if sizeBytes == 0 {
		fmt.Printf("Error: file size is 0: %s\n", input_file)
		return
	}
	if sizeBytes%16 != 0 {
		fmt.Printf("Error: file size is not divisible by 16")
		return
	}
	num_edges := sizeBytes / 16

	var readFromInput func([]byte, int64)
	if *big {
		readFromInput = func(buf []byte, off int64) {
			_, err := ReadFullAt(fin, buf, off)
			if err != nil {
				panic("Error reading input file")
			}
		}
	} else {
		// Memory-map the input file.
		data, err := syscall.Mmap(int(fin.Fd()), 0, int(sizeBytes), syscall.PROT_READ, syscall.MAP_PRIVATE|syscall.MAP_POPULATE|syscall.MAP_NONBLOCK)
		if err != nil {
			fmt.Printf("Error memory-mapping file %s: %v\n", input_file, err)
			return
		}
		defer syscall.Munmap(data)

		readFromInput = func(buf []byte, off int64) {
			copy(buf, data[off:off+int64(len(buf))])
		}
	}

	// Open output file.
	fout, err := os.OpenFile(output_file, os.O_RDWR|os.O_CREATE, 0644)
	if err != nil {
		fmt.Printf("Error opening file %s: %v\n", output_file, err)
		return
	}
	fout.Truncate(int64(sizeBytes))

	// keys should be an array of 4 random uint64s
	rand.Seed(*rseed)
	var keys []shuffle.FeistelWord
	for i := 0; i < 4; i++ {
		keys = append(keys, shuffle.FeistelWord(rand.Uint64()))
	}

	// limit the in-flight goroutines with a ticketing system
	limiter := make(chan struct{}, *routines)
	for i := 0; i < *routines; i++ {
		limiter <- struct{}{}
	}

	// Spawn one goroutine per page.
	for i := uint64(0); i < num_edges; i += uint64(pageSize / 16) {
		// take a ticket
		<-limiter
		go func(i uint64) {
			output := make([]byte, pageSize)
			fn := shuffle.NewFeistel(keys, roundFunction)
			for j := i; j < i+uint64(pageSize/16) && j < num_edges; j++ {
				src, _ := shuffle.GetIndex(shuffle.FeistelWord(j), shuffle.FeistelWord(num_edges), fn)
				// move 16 bytes from data to output
				readFromInput(output[(j-i)*16:(j-i+1)*16], int64(src*16))
			}
			_, err := fout.WriteAt(output, int64(i*16))
			if err != nil {
				panic("Error writing output file")
			}
			// release the ticket
			limiter <- struct{}{}
		}(i)
	}
	// wait for all tickets to be returned (i.e. all routines to be finish)
	for i := 0; i < *routines; i++ {
		<-limiter
	}
}
