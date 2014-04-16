// Copyright 2009 The Go Authors. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.
package syslog

import (
	"io"
	"log"
	"net"
	"testing"
)

var serverAddr string

func runSyslog(c net.PacketConn, done chan<- string) {
	var buf [4096]byte
	var rcvd string = ""
	for {
		n, _, err := c.ReadFrom(buf[0:])
		if err != nil || n == 0 {
			break
		}
		rcvd += string(buf[0:n])
	}
	done <- rcvd
}

func startServer(done chan<- string) {
	c, e := net.ListenPacket("udp", "127.0.0.1:0")
	if e != nil {
		log.Exitf("net.ListenPacket failed udp :0 %v", e)
	}
	serverAddr = c.LocalAddr().String()
	c.SetReadTimeout(100e6) // 100ms
	go runSyslog(c, done)
}

func TestNew(t *testing.T) {
	s, err := New(LOG_INFO, "")
	if err != nil {
		t.Fatalf("New() failed: %s", err)
	}
	// Don't send any messages.
	s.Close()
}

func TestNewLogger(t *testing.T) {
	f := NewLogger(LOG_INFO, 0)
	if f == nil {
		t.Error("NewLogger() failed")
	}
}

func TestDial(t *testing.T) {
	l, err := Dial("", "", LOG_ERR, "syslog_test")
	if err != nil {
		t.Fatalf("Dial() failed: %s", err)
	}
	l.Close()
}

func TestUDPDial(t *testing.T) {
	done := make(chan string)
	startServer(done)
	l, err := Dial("udp", serverAddr, LOG_INFO, "syslog_test")
	if err != nil {
		t.Fatalf("syslog.Dial() failed: %s", err)
	}
	msg := "udp test"
	l.Info(msg)
	expected := "<6>syslog_test: udp test\n"
	rcvd := <-done
	if rcvd != expected {
		t.Fatalf("s.Info() = '%q', but wanted '%q'", rcvd, expected)
	}
}

func TestWrite(t *testing.T) {
	done := make(chan string)
	startServer(done)
	l, err := Dial("udp", serverAddr, LOG_ERR, "syslog_test")
	if err != nil {
		t.Fatalf("syslog.Dial() failed: %s", err)
	}
	msg := "write test"
	_, err = io.WriteString(l, msg)
	if err != nil {
		t.Fatalf("WriteString() failed: %s", err)
	}
	expected := "<3>syslog_test: write test\n"
	rcvd := <-done
	if rcvd != expected {
		t.Fatalf("s.Info() = '%q', but wanted '%q'", rcvd, expected)
	}
}
