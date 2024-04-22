package main

import (
	"flag"
	"fmt"
	"math/rand"
	"os"
	"runtime"
	"sync"
	"syscall"
	"unsafe"

	"github.com/vmunoz82/shuffle"
)

/* Function for each round, could be anything don't need to be reversible */
func roundFunction(v, key shuffle.FeistelWord) shuffle.FeistelWord {
	return (v * 941083987) ^ (key >> (v & 7) * 104729)
}

// main function
func main() {
	// parse CLI options
	threads := flag.Int("threads", runtime.NumCPU(), "number of threads")
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

	// mmap input file
	fin, err := os.Open(input_file)
	if err != nil {
		fmt.Printf("Error opening file %s: %v\n", input_file, err)
		return
	}
	defer fin.Close()

	fout, err := os.OpenFile(output_file, os.O_RDWR|os.O_CREATE, 0644)
	if err != nil {
		fmt.Printf("Error opening file %s: %v\n", output_file, err)
		return
	}

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

	fout.Truncate(int64(sizeBytes))

	data, err := syscall.Mmap(int(fin.Fd()), 0, int(sizeBytes), syscall.PROT_READ, syscall.MAP_PRIVATE|syscall.MAP_NORESERVE)
	if err != nil {
		fmt.Printf("Error mmaping file %s: %v\n", input_file, err)
		return
	}
	if uintptr(unsafe.Pointer(&data[0]))%uintptr(pageSize) != 0 {
		fmt.Println("mmap returned a non-page-aligned address: %p", uintptr(unsafe.Pointer(&data[0])))
		return
	}
	// madv willneed
	err = syscall.Madvise(data, syscall.MADV_WILLNEED)
	if err != nil {
		fmt.Printf("Error madvise: %v\n", err)
		return
	}

	// mmap output file
	output, err := syscall.Mmap(int(fout.Fd()), 0, int(sizeBytes), syscall.PROT_WRITE, syscall.MAP_SHARED)
	if err != nil {
		fmt.Printf("Error mmaping file %s: %v\n", output_file, err)
		return
	}

	num_edges := sizeBytes / 16

	rand.Seed(*rseed)
	// keys should be an array of 4 random uint64s
	keys := []shuffle.FeistelWord{shuffle.FeistelWord(rand.Uint64()), shuffle.FeistelWord(rand.Uint64()), shuffle.FeistelWord(rand.Uint64()), shuffle.FeistelWord(rand.Uint64())}

	var wg sync.WaitGroup
	// spawn a goroutine for every page, since each page can fault
	// todo (meyer): should we limit the number of concurrent goroutines?
	for i := uint64(0); i < num_edges; i += uint64(pageSize / 16) {
		wg.Add(1)
		go func(i uint64) {
			defer wg.Done()
			fn := shuffle.NewFeistel(keys, roundFunction)
			for j := i; j < i+uint64(pageSize/16) && j < num_edges; j++ {
				src, _ := shuffle.GetIndex(shuffle.FeistelWord(j), shuffle.FeistelWord(num_edges), fn)
				// move 16 bytes from data to output
				copy(output[j*16:], data[src*16:src*16+16])
			}
		}(i)
	}
	wg.Wait()

	err = syscall.Munmap(data)
	if err != nil {
		fmt.Printf("Error unmapping file %s: %v\n", input_file, err)
		return
	}

	err = syscall.Munmap(output)
	if err != nil {
		fmt.Printf("Error unmapping file %s: %v\n", output_file, err)
		return
	}
}
