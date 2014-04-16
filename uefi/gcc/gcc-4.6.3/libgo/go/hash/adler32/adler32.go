// Copyright 2009 The Go Authors. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

// This package implements the Adler-32 checksum.
// Defined in RFC 1950:
//	Adler-32 is composed of two sums accumulated per byte: s1 is
//	the sum of all bytes, s2 is the sum of all s1 values. Both sums
//	are done modulo 65521. s1 is initialized to 1, s2 to zero.  The
//	Adler-32 checksum is stored as s2*65536 + s1 in most-
//	significant-byte first (network) order.
package adler32

import (
	"hash"
	"os"
)

const (
	mod = 65521
)

// The size of an Adler-32 checksum in bytes.
const Size = 4

// digest represents the partial evaluation of a checksum.
type digest struct {
	// invariant: (a < mod && b < mod) || a <= b
	// invariant: a + b + 255 <= 0xffffffff
	a, b uint32
}

func (d *digest) Reset() { d.a, d.b = 1, 0 }

// New returns a new hash.Hash32 computing the Adler-32 checksum.
func New() hash.Hash32 {
	d := new(digest)
	d.Reset()
	return d
}

func (d *digest) Size() int { return Size }

// Add p to the running checksum a, b.
func update(a, b uint32, p []byte) (aa, bb uint32) {
	for i := 0; i < len(p); i++ {
		a += uint32(p[i])
		b += a
		// invariant: a <= b
		if b > (0xffffffff-255)/2 {
			a %= mod
			b %= mod
			// invariant: a < mod && b < mod
		} else {
			// invariant: a + b + 255 <= 2 * b + 255 <= 0xffffffff
		}
	}
	return a, b
}

// Return the 32-bit checksum corresponding to a, b.
func finish(a, b uint32) uint32 {
	if b >= mod {
		a %= mod
		b %= mod
	}
	return b<<16 | a
}

func (d *digest) Write(p []byte) (nn int, err os.Error) {
	d.a, d.b = update(d.a, d.b, p)
	return len(p), nil
}

func (d *digest) Sum32() uint32 { return finish(d.a, d.b) }

func (d *digest) Sum() []byte {
	p := make([]byte, 4)
	s := d.Sum32()
	p[0] = byte(s >> 24)
	p[1] = byte(s >> 16)
	p[2] = byte(s >> 8)
	p[3] = byte(s)
	return p
}

// Checksum returns the Adler-32 checksum of data.
func Checksum(data []byte) uint32 { return finish(update(1, 0, data)) }
