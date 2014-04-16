// Copyright 2009 The Go Authors. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

package tls

import (
	"rand"
	"reflect"
	"testing"
	"testing/quick"
)

var tests = []interface{}{
	&clientHelloMsg{},
	&serverHelloMsg{},

	&certificateMsg{},
	&certificateRequestMsg{},
	&certificateVerifyMsg{},
	&certificateStatusMsg{},
	&clientKeyExchangeMsg{},
	&finishedMsg{},
	&nextProtoMsg{},
}

type testMessage interface {
	marshal() []byte
	unmarshal([]byte) bool
}

func TestMarshalUnmarshal(t *testing.T) {
	rand := rand.New(rand.NewSource(0))
	for i, iface := range tests {
		ty := reflect.NewValue(iface).Type()

		for j := 0; j < 100; j++ {
			v, ok := quick.Value(ty, rand)
			if !ok {
				t.Errorf("#%d: failed to create value", i)
				break
			}

			m1 := v.Interface().(testMessage)
			marshaled := m1.marshal()
			m2 := iface.(testMessage)
			if !m2.unmarshal(marshaled) {
				t.Errorf("#%d failed to unmarshal %#v %x", i, m1, marshaled)
				break
			}
			m2.marshal() // to fill any marshal cache in the message

			if !reflect.DeepEqual(m1, m2) {
				t.Errorf("#%d got:%#v want:%#v %x", i, m2, m1, marshaled)
				break
			}

			if i >= 2 {
				// The first two message types (ClientHello and
				// ServerHello) are allowed to have parsable
				// prefixes because the extension data is
				// optional.
				for j := 0; j < len(marshaled); j++ {
					if m2.unmarshal(marshaled[0:j]) {
						t.Errorf("#%d unmarshaled a prefix of length %d of %#v", i, j, m1)
						break
					}
				}
			}
		}
	}
}

func TestFuzz(t *testing.T) {
	rand := rand.New(rand.NewSource(0))
	for _, iface := range tests {
		m := iface.(testMessage)

		for j := 0; j < 1000; j++ {
			len := rand.Intn(100)
			bytes := randomBytes(len, rand)
			// This just looks for crashes due to bounds errors etc.
			m.unmarshal(bytes)
		}
	}
}

func randomBytes(n int, rand *rand.Rand) []byte {
	r := make([]byte, n)
	for i := 0; i < n; i++ {
		r[i] = byte(rand.Int31())
	}
	return r
}

func randomString(n int, rand *rand.Rand) string {
	b := randomBytes(n, rand)
	return string(b)
}

func (*clientHelloMsg) Generate(rand *rand.Rand, size int) reflect.Value {
	m := &clientHelloMsg{}
	m.vers = uint16(rand.Intn(65536))
	m.random = randomBytes(32, rand)
	m.sessionId = randomBytes(rand.Intn(32), rand)
	m.cipherSuites = make([]uint16, rand.Intn(63)+1)
	for i := 0; i < len(m.cipherSuites); i++ {
		m.cipherSuites[i] = uint16(rand.Int31())
	}
	m.compressionMethods = randomBytes(rand.Intn(63)+1, rand)
	if rand.Intn(10) > 5 {
		m.nextProtoNeg = true
	}
	if rand.Intn(10) > 5 {
		m.serverName = randomString(rand.Intn(255), rand)
	}
	m.ocspStapling = rand.Intn(10) > 5
	m.supportedPoints = randomBytes(rand.Intn(5)+1, rand)
	m.supportedCurves = make([]uint16, rand.Intn(5)+1)
	for i, _ := range m.supportedCurves {
		m.supportedCurves[i] = uint16(rand.Intn(30000))
	}

	return reflect.NewValue(m)
}

func (*serverHelloMsg) Generate(rand *rand.Rand, size int) reflect.Value {
	m := &serverHelloMsg{}
	m.vers = uint16(rand.Intn(65536))
	m.random = randomBytes(32, rand)
	m.sessionId = randomBytes(rand.Intn(32), rand)
	m.cipherSuite = uint16(rand.Int31())
	m.compressionMethod = uint8(rand.Intn(256))

	if rand.Intn(10) > 5 {
		m.nextProtoNeg = true

		n := rand.Intn(10)
		m.nextProtos = make([]string, n)
		for i := 0; i < n; i++ {
			m.nextProtos[i] = randomString(20, rand)
		}
	}

	return reflect.NewValue(m)
}

func (*certificateMsg) Generate(rand *rand.Rand, size int) reflect.Value {
	m := &certificateMsg{}
	numCerts := rand.Intn(20)
	m.certificates = make([][]byte, numCerts)
	for i := 0; i < numCerts; i++ {
		m.certificates[i] = randomBytes(rand.Intn(10)+1, rand)
	}
	return reflect.NewValue(m)
}

func (*certificateRequestMsg) Generate(rand *rand.Rand, size int) reflect.Value {
	m := &certificateRequestMsg{}
	m.certificateTypes = randomBytes(rand.Intn(5)+1, rand)
	numCAs := rand.Intn(100)
	m.certificateAuthorities = make([][]byte, numCAs)
	for i := 0; i < numCAs; i++ {
		m.certificateAuthorities[i] = randomBytes(rand.Intn(15)+1, rand)
	}
	return reflect.NewValue(m)
}

func (*certificateVerifyMsg) Generate(rand *rand.Rand, size int) reflect.Value {
	m := &certificateVerifyMsg{}
	m.signature = randomBytes(rand.Intn(15)+1, rand)
	return reflect.NewValue(m)
}

func (*certificateStatusMsg) Generate(rand *rand.Rand, size int) reflect.Value {
	m := &certificateStatusMsg{}
	if rand.Intn(10) > 5 {
		m.statusType = statusTypeOCSP
		m.response = randomBytes(rand.Intn(10)+1, rand)
	} else {
		m.statusType = 42
	}
	return reflect.NewValue(m)
}

func (*clientKeyExchangeMsg) Generate(rand *rand.Rand, size int) reflect.Value {
	m := &clientKeyExchangeMsg{}
	m.ciphertext = randomBytes(rand.Intn(1000)+1, rand)
	return reflect.NewValue(m)
}

func (*finishedMsg) Generate(rand *rand.Rand, size int) reflect.Value {
	m := &finishedMsg{}
	m.verifyData = randomBytes(12, rand)
	return reflect.NewValue(m)
}

func (*nextProtoMsg) Generate(rand *rand.Rand, size int) reflect.Value {
	m := &nextProtoMsg{}
	m.proto = randomString(rand.Intn(255), rand)
	return reflect.NewValue(m)
}
