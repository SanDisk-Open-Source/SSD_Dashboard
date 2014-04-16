// Copyright 2009 The Go Authors. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

// This package partially implements the TLS 1.1 protocol, as specified in RFC 4346.
package tls

import (
	"crypto/rsa"
	"crypto/x509"
	"encoding/pem"
	"io/ioutil"
	"net"
	"os"
	"strings"
)

// Server returns a new TLS server side connection
// using conn as the underlying transport.
// The configuration config must be non-nil and must have
// at least one certificate.
func Server(conn net.Conn, config *Config) *Conn {
	return &Conn{conn: conn, config: config}
}

// Client returns a new TLS client side connection
// using conn as the underlying transport.
// Client interprets a nil configuration as equivalent to
// the zero configuration; see the documentation of Config
// for the defaults.
func Client(conn net.Conn, config *Config) *Conn {
	return &Conn{conn: conn, config: config, isClient: true}
}

// A Listener implements a network listener (net.Listener) for TLS connections.
type Listener struct {
	listener net.Listener
	config   *Config
}

// Accept waits for and returns the next incoming TLS connection.
// The returned connection c is a *tls.Conn.
func (l *Listener) Accept() (c net.Conn, err os.Error) {
	c, err = l.listener.Accept()
	if err != nil {
		return
	}
	c = Server(c, l.config)
	return
}

// Close closes the listener.
func (l *Listener) Close() os.Error { return l.listener.Close() }

// Addr returns the listener's network address.
func (l *Listener) Addr() net.Addr { return l.listener.Addr() }

// NewListener creates a Listener which accepts connections from an inner
// Listener and wraps each connection with Server.
// The configuration config must be non-nil and must have
// at least one certificate.
func NewListener(listener net.Listener, config *Config) (l *Listener) {
	l = new(Listener)
	l.listener = listener
	l.config = config
	return
}

// Listen creates a TLS listener accepting connections on the
// given network address using net.Listen.
// The configuration config must be non-nil and must have
// at least one certificate.
func Listen(network, laddr string, config *Config) (*Listener, os.Error) {
	if config == nil || len(config.Certificates) == 0 {
		return nil, os.NewError("tls.Listen: no certificates in configuration")
	}
	l, err := net.Listen(network, laddr)
	if err != nil {
		return nil, err
	}
	return NewListener(l, config), nil
}

// Dial connects to the given network address using net.Dial
// and then initiates a TLS handshake, returning the resulting
// TLS connection.
// Dial interprets a nil configuration as equivalent to
// the zero configuration; see the documentation of Config
// for the defaults.
func Dial(network, laddr, raddr string, config *Config) (*Conn, os.Error) {
	c, err := net.Dial(network, laddr, raddr)
	if err != nil {
		return nil, err
	}

	colonPos := strings.LastIndex(raddr, ":")
	if colonPos == -1 {
		colonPos = len(raddr)
	}
	hostname := raddr[:colonPos]

	if config == nil {
		config = defaultConfig()
	}
	if config.ServerName != "" {
		// Make a copy to avoid polluting argument or default.
		c := *config
		c.ServerName = hostname
		config = &c
	}
	conn := Client(c, config)
	if err = conn.Handshake(); err != nil {
		c.Close()
		return nil, err
	}
	return conn, nil
}

// LoadX509KeyPair reads and parses a public/private key pair from a pair of
// files. The files must contain PEM encoded data.
func LoadX509KeyPair(certFile string, keyFile string) (cert Certificate, err os.Error) {
	certPEMBlock, err := ioutil.ReadFile(certFile)
	if err != nil {
		return
	}

	certDERBlock, _ := pem.Decode(certPEMBlock)
	if certDERBlock == nil {
		err = os.ErrorString("crypto/tls: failed to parse certificate PEM data")
		return
	}

	cert.Certificate = [][]byte{certDERBlock.Bytes}

	keyPEMBlock, err := ioutil.ReadFile(keyFile)
	if err != nil {
		return
	}

	keyDERBlock, _ := pem.Decode(keyPEMBlock)
	if keyDERBlock == nil {
		err = os.ErrorString("crypto/tls: failed to parse key PEM data")
		return
	}

	key, err := x509.ParsePKCS1PrivateKey(keyDERBlock.Bytes)
	if err != nil {
		err = os.ErrorString("crypto/tls: failed to parse key")
		return
	}

	cert.PrivateKey = key

	// We don't need to parse the public key for TLS, but we so do anyway
	// to check that it looks sane and matches the private key.
	x509Cert, err := x509.ParseCertificate(certDERBlock.Bytes)
	if err != nil {
		return
	}

	if x509Cert.PublicKeyAlgorithm != x509.RSA || x509Cert.PublicKey.(*rsa.PublicKey).N.Cmp(key.PublicKey.N) != 0 {
		err = os.ErrorString("crypto/tls: private key does not match public key")
		return
	}

	return
}
