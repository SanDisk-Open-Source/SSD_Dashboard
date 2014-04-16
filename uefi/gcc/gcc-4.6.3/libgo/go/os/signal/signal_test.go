// Copyright 2009 The Go Authors. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

package signal

import (
	"syscall"
	"testing"
)

func TestSignal(t *testing.T) {
	// Send this process a SIGHUP.
	syscall.Syscall(syscall.SYS_KILL, uintptr(syscall.Getpid()), syscall.SIGHUP, 0)

	if sig := (<-Incoming).(UnixSignal); sig != SIGHUP {
		t.Errorf("signal was %v, want %v", sig, SIGHUP)
	}
}
