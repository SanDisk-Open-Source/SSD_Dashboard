// Copyright 2010 The Go Authors.  All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

// Package pprof writes runtime profiling data in the format expected
// by the pprof visualization tool.
// For more information about pprof, see
// http://code.google.com/p/google-perftools/.
package pprof

import (
	"bufio"
	"fmt"
	"io"
	"os"
	"runtime"
)

// WriteHeapProfile writes a pprof-formatted heap profile to w.
// If a write to w returns an error, WriteHeapProfile returns that error.
// Otherwise, WriteHeapProfile returns nil.
func WriteHeapProfile(w io.Writer) os.Error {
	// Find out how many records there are (MemProfile(nil, false)),
	// allocate that many records, and get the data.
	// There's a race—more records might be added between
	// the two calls—so allocate a few extra records for safety
	// and also try again if we're very unlucky.
	// The loop should only execute one iteration in the common case.
	var p []runtime.MemProfileRecord
	n, ok := runtime.MemProfile(nil, false)
	for {
		// Allocate room for a slightly bigger profile,
		// in case a few more entries have been added
		// since the call to MemProfile.
		p = make([]runtime.MemProfileRecord, n+50)
		n, ok = runtime.MemProfile(p, false)
		if ok {
			p = p[0:n]
			break
		}
		// Profile grew; try again.
	}

	var total runtime.MemProfileRecord
	for i := range p {
		r := &p[i]
		total.AllocBytes += r.AllocBytes
		total.AllocObjects += r.AllocObjects
		total.FreeBytes += r.FreeBytes
		total.FreeObjects += r.FreeObjects
	}

	// Technically the rate is MemProfileRate not 2*MemProfileRate,
	// but early versions of the C++ heap profiler reported 2*MemProfileRate,
	// so that's what pprof has come to expect.
	b := bufio.NewWriter(w)
	fmt.Fprintf(b, "heap profile: %d: %d [%d: %d] @ heap/%d\n",
		total.InUseObjects(), total.InUseBytes(),
		total.AllocObjects, total.AllocBytes,
		2*runtime.MemProfileRate)

	for i := range p {
		r := &p[i]
		fmt.Fprintf(b, "%d: %d [%d: %d] @",
			r.InUseObjects(), r.InUseBytes(),
			r.AllocObjects, r.AllocBytes)
		for _, pc := range r.Stack() {
			fmt.Fprintf(b, " %#x", pc)
		}
		fmt.Fprintf(b, "\n")
	}

	// Print memstats information too.
	// Pprof will ignore, but useful for people.
	s := &runtime.MemStats
	fmt.Fprintf(b, "\n# runtime.MemStats\n")
	fmt.Fprintf(b, "# Alloc = %d\n", s.Alloc)
	fmt.Fprintf(b, "# TotalAlloc = %d\n", s.TotalAlloc)
	fmt.Fprintf(b, "# Sys = %d\n", s.Sys)
	fmt.Fprintf(b, "# Lookups = %d\n", s.Lookups)
	fmt.Fprintf(b, "# Mallocs = %d\n", s.Mallocs)

	fmt.Fprintf(b, "# HeapAlloc = %d\n", s.HeapAlloc)
	fmt.Fprintf(b, "# HeapSys = %d\n", s.HeapSys)
	fmt.Fprintf(b, "# HeapIdle = %d\n", s.HeapIdle)
	fmt.Fprintf(b, "# HeapInuse = %d\n", s.HeapInuse)

	fmt.Fprintf(b, "# Stack = %d / %d\n", s.StackInuse, s.StackSys)
	fmt.Fprintf(b, "# MSpan = %d / %d\n", s.MSpanInuse, s.MSpanSys)
	fmt.Fprintf(b, "# MCache = %d / %d\n", s.MCacheInuse, s.MCacheSys)
	fmt.Fprintf(b, "# MHeapMapSys = %d\n", s.MHeapMapSys)
	fmt.Fprintf(b, "# BuckHashSys = %d\n", s.BuckHashSys)

	fmt.Fprintf(b, "# NextGC = %d\n", s.NextGC)
	fmt.Fprintf(b, "# PauseNs = %d\n", s.PauseNs)
	fmt.Fprintf(b, "# NumGC = %d\n", s.NumGC)
	fmt.Fprintf(b, "# EnableGC = %v\n", s.EnableGC)
	fmt.Fprintf(b, "# DebugGC = %v\n", s.DebugGC)

	fmt.Fprintf(b, "# BySize = Size * (Active = Mallocs - Frees)\n")
	fmt.Fprintf(b, "# (Excluding large blocks.)\n")
	for _, t := range s.BySize {
		if t.Mallocs > 0 {
			fmt.Fprintf(b, "#   %d * (%d = %d - %d)\n", t.Size, t.Mallocs-t.Frees, t.Mallocs, t.Frees)
		}
	}
	return b.Flush()
}
