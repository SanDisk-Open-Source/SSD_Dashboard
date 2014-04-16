// Copyright 2009 The Go Authors. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

// The syslog package provides a simple interface to
// the system log service. It can send messages to the
// syslog daemon using UNIX domain sockets, UDP, or
// TCP connections.
package syslog

import (
	"fmt"
	"log"
	"net"
	"os"
)

type Priority int

const (
	// From /usr/include/sys/syslog.h.
	// These are the same on Linux, BSD, and OS X.
	LOG_EMERG Priority = iota
	LOG_ALERT
	LOG_CRIT
	LOG_ERR
	LOG_WARNING
	LOG_NOTICE
	LOG_INFO
	LOG_DEBUG
)

// A Writer is a connection to a syslog server.
type Writer struct {
	priority Priority
	prefix   string
	conn     serverConn
}

type serverConn interface {
	writeBytes(p Priority, prefix string, b []byte) (int, os.Error)
	writeString(p Priority, prefix string, s string) (int, os.Error)
	close() os.Error
}

type netConn struct {
	conn net.Conn
}

// New establishes a new connection to the system log daemon.
// Each write to the returned writer sends a log message with
// the given priority and prefix.
func New(priority Priority, prefix string) (w *Writer, err os.Error) {
	return Dial("", "", priority, prefix)
}

// Dial establishes a connection to a log daemon by connecting
// to address raddr on the network net.
// Each write to the returned writer sends a log message with
// the given priority and prefix.
func Dial(network, raddr string, priority Priority, prefix string) (w *Writer, err os.Error) {
	if prefix == "" {
		prefix = os.Args[0]
	}
	var conn serverConn
	if network == "" {
		conn, err = unixSyslog()
	} else {
		var c net.Conn
		c, err = net.Dial(network, "", raddr)
		conn = netConn{c}
	}
	return &Writer{priority, prefix, conn}, err
}

// Write sends a log message to the syslog daemon.
func (w *Writer) Write(b []byte) (int, os.Error) {
	if w.priority > LOG_DEBUG || w.priority < LOG_EMERG {
		return 0, os.EINVAL
	}
	return w.conn.writeBytes(w.priority, w.prefix, b)
}

func (w *Writer) writeString(p Priority, s string) (int, os.Error) {
	return w.conn.writeString(p, w.prefix, s)
}

func (w *Writer) Close() os.Error { return w.conn.close() }

// Emerg logs a message using the LOG_EMERG priority.
func (w *Writer) Emerg(m string) (err os.Error) {
	_, err = w.writeString(LOG_EMERG, m)
	return err
}
// Crit logs a message using the LOG_CRIT priority.
func (w *Writer) Crit(m string) (err os.Error) {
	_, err = w.writeString(LOG_CRIT, m)
	return err
}
// ERR logs a message using the LOG_ERR priority.
func (w *Writer) Err(m string) (err os.Error) {
	_, err = w.writeString(LOG_ERR, m)
	return err
}

// Warning logs a message using the LOG_WARNING priority.
func (w *Writer) Warning(m string) (err os.Error) {
	_, err = w.writeString(LOG_WARNING, m)
	return err
}

// Notice logs a message using the LOG_NOTICE priority.
func (w *Writer) Notice(m string) (err os.Error) {
	_, err = w.writeString(LOG_NOTICE, m)
	return err
}
// Info logs a message using the LOG_INFO priority.
func (w *Writer) Info(m string) (err os.Error) {
	_, err = w.writeString(LOG_INFO, m)
	return err
}
// Debug logs a message using the LOG_DEBUG priority.
func (w *Writer) Debug(m string) (err os.Error) {
	_, err = w.writeString(LOG_DEBUG, m)
	return err
}

func (n netConn) writeBytes(p Priority, prefix string, b []byte) (int, os.Error) {
	return fmt.Fprintf(n.conn, "<%d>%s: %s\n", p, prefix, b)
}

func (n netConn) writeString(p Priority, prefix string, s string) (int, os.Error) {
	return fmt.Fprintf(n.conn, "<%d>%s: %s\n", p, prefix, s)
}

func (n netConn) close() os.Error {
	return n.conn.Close()
}

// NewLogger provides an object that implements the full log.Logger interface,
// but sends messages to Syslog instead; flag is passed as is to Logger;
// priority will be used for all messages sent using this interface.
// All messages are logged with priority p.
func NewLogger(p Priority, flag int) *log.Logger {
	s, err := New(p, "")
	if err != nil {
		return nil
	}
	return log.New(s, "", flag)
}
