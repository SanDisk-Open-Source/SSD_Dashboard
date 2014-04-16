// Copyright 2010 The Go Authors.  All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

package ioutil

import (
	"os"
	"strconv"
)

// Random number state, accessed without lock; racy but harmless.
// We generate random temporary file names so that there's a good
// chance the file doesn't exist yet - keeps the number of tries in
// TempFile to a minimum.
var rand uint32

func reseed() uint32 {
	sec, nsec, _ := os.Time()
	return uint32(sec*1e9 + nsec + int64(os.Getpid()))
}

func nextSuffix() string {
	r := rand
	if r == 0 {
		r = reseed()
	}
	r = r*1664525 + 1013904223 // constants from Numerical Recipes
	rand = r
	return strconv.Itoa(int(1e9 + r%1e9))[1:]
}

// TempFile creates a new temporary file in the directory dir
// with a name beginning with prefix, opens the file for reading
// and writing, and returns the resulting *os.File.
// If dir is the empty string, TempFile uses the default directory
// for temporary files (see os.TempDir).
// Multiple programs calling TempFile simultaneously
// will not choose the same file.  The caller can use f.Name()
// to find the name of the file.  It is the caller's responsibility to
// remove the file when no longer needed.
func TempFile(dir, prefix string) (f *os.File, err os.Error) {
	if dir == "" {
		dir = os.TempDir()
	}

	nconflict := 0
	for i := 0; i < 10000; i++ {
		name := dir + "/" + prefix + nextSuffix()
		f, err = os.Open(name, os.O_RDWR|os.O_CREATE|os.O_EXCL, 0600)
		if pe, ok := err.(*os.PathError); ok && pe.Error == os.EEXIST {
			if nconflict++; nconflict > 10 {
				rand = reseed()
			}
			continue
		}
		break
	}
	return
}
