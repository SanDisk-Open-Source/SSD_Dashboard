// Copyright 2010 The Go Authors.  All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

// Windows cryptographically secure pseudorandom number
// generator.

package rand

import (
	"os"
	"sync"
	"syscall"
)

// Implemented by using Windows CryptoAPI 2.0.

func init() { Reader = &rngReader{} }

// A rngReader satisfies reads by reading from the Windows CryptGenRandom API.
type rngReader struct {
	prov uint32
	mu   sync.Mutex
}

func (r *rngReader) Read(b []byte) (n int, err os.Error) {
	r.mu.Lock()
	if r.prov == 0 {
		const provType = syscall.PROV_RSA_FULL
		const flags = syscall.CRYPT_VERIFYCONTEXT | syscall.CRYPT_SILENT
		ok, errno := syscall.CryptAcquireContext(&r.prov, nil, nil, provType, flags)
		if !ok {
			r.mu.Unlock()
			return 0, os.NewSyscallError("CryptAcquireContext", errno)
		}
	}
	r.mu.Unlock()
	ok, errno := syscall.CryptGenRandom(r.prov, uint32(len(b)), &b[0])
	if !ok {
		return 0, os.NewSyscallError("CryptGenRandom", errno)
	}
	return len(b), nil
}
