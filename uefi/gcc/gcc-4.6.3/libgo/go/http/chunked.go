// Copyright 2009 The Go Authors. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

package http

import (
	"io"
	"os"
	"strconv"
)

// NewChunkedWriter returns a new writer that translates writes into HTTP
// "chunked" format before writing them to w.  Closing the returned writer
// sends the final 0-length chunk that marks the end of the stream.
func NewChunkedWriter(w io.Writer) io.WriteCloser {
	return &chunkedWriter{w}
}

// Writing to ChunkedWriter translates to writing in HTTP chunked Transfer
// Encoding wire format to the undering Wire writer.
type chunkedWriter struct {
	Wire io.Writer
}

// Write the contents of data as one chunk to Wire.
// NOTE: Note that the corresponding chunk-writing procedure in Conn.Write has
// a bug since it does not check for success of io.WriteString
func (cw *chunkedWriter) Write(data []byte) (n int, err os.Error) {

	// Don't send 0-length data. It looks like EOF for chunked encoding.
	if len(data) == 0 {
		return 0, nil
	}

	head := strconv.Itob(len(data), 16) + "\r\n"

	if _, err = io.WriteString(cw.Wire, head); err != nil {
		return 0, err
	}
	if n, err = cw.Wire.Write(data); err != nil {
		return
	}
	if n != len(data) {
		err = io.ErrShortWrite
		return
	}
	_, err = io.WriteString(cw.Wire, "\r\n")

	return
}

func (cw *chunkedWriter) Close() os.Error {
	_, err := io.WriteString(cw.Wire, "0\r\n")
	return err
}
