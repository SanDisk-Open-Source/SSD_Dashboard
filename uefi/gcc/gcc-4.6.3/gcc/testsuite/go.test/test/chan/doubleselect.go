// $G $D/$F.go && $L $F.$A && ./$A.out

// Copyright 2009 The Go Authors. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

// This test is designed to flush out the case where two cases of a select can
// both end up running. See http://codereview.appspot.com/180068.
package main

import (
	"flag"
	"runtime"
)

var iterations *int = flag.Int("n", 100000, "number of iterations")

// sender sends a counter to one of four different channels. If two
// cases both end up running in the same iteration, the same value will be sent
// to two different channels.
func sender(n int, c1, c2, c3, c4 chan<- int) {
	defer close(c1)
	defer close(c2)

	for i := 0; i < n; i++ {
		select {
		case c1 <- i:
		case c2 <- i:
		case c3 <- i:
		case c4 <- i:
		}
	}
}

// mux receives the values from sender and forwards them onto another channel.
// It would be simplier to just have sender's four cases all be the same
// channel, but this doesn't actually trigger the bug.
func mux(out chan<- int, in <-chan int) {
	for {
		v := <-in
		if closed(in) {
			close(out)
			break
		}
		out <- v
	}
}

// recver gets a steam of values from the four mux's and checks for duplicates.
func recver(in <-chan int) {
	seen := make(map[int]bool)

	for {
		v := <-in
		if closed(in) {
			break
		}
		if _, ok := seen[v]; ok {
			println("got duplicate value: ", v)
			panic("fail")
		}
		seen[v] = true
	}
}

func main() {
	runtime.GOMAXPROCS(2)

	c1 := make(chan int)
	c2 := make(chan int)
	c3 := make(chan int)
	c4 := make(chan int)
	cmux := make(chan int)
	go sender(*iterations, c1, c2, c3, c4)
	go mux(cmux, c1)
	go mux(cmux, c2)
	go mux(cmux, c3)
	go mux(cmux, c4)
	// We keep the recver because it might catch more bugs in the future.
	// However, the result of the bug linked to at the top is that we'll
	// end up panicing with: "throw: bad g->status in ready".
	recver(cmux)
	print("PASS\n")
}
