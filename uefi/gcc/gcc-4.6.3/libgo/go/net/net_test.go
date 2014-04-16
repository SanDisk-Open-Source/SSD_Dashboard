// Copyright 2009 The Go Authors. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

package net

import (
	"flag"
	"regexp"
	"runtime"
	"testing"
)

var runErrorTest = flag.Bool("run_error_test", false, "let TestDialError check for dns errors")

type DialErrorTest struct {
	Net     string
	Laddr   string
	Raddr   string
	Pattern string
}

var dialErrorTests = []DialErrorTest{
	{
		"datakit", "", "mh/astro/r70",
		"dial datakit mh/astro/r70: unknown network datakit",
	},
	{
		"tcp", "", "127.0.0.1:☺",
		"dial tcp 127.0.0.1:☺: unknown port tcp/☺",
	},
	{
		"tcp", "", "no-such-name.google.com.:80",
		"dial tcp no-such-name.google.com.:80: lookup no-such-name.google.com.( on .*)?: no (.*)",
	},
	{
		"tcp", "", "no-such-name.no-such-top-level-domain.:80",
		"dial tcp no-such-name.no-such-top-level-domain.:80: lookup no-such-name.no-such-top-level-domain.( on .*)?: no (.*)",
	},
	{
		"tcp", "", "no-such-name:80",
		`dial tcp no-such-name:80: lookup no-such-name\.(.*\.)?( on .*)?: no (.*)`,
	},
	{
		"tcp", "", "mh/astro/r70:http",
		"dial tcp mh/astro/r70:http: lookup mh/astro/r70: invalid domain name",
	},
	{
		"unix", "", "/etc/file-not-found",
		"dial unix /etc/file-not-found: [nN]o such file or directory",
	},
	{
		"unix", "", "/etc/",
		"dial unix /etc/: ([pP]ermission denied|[sS]ocket operation on non-socket|[cC]onnection refused)",
	},
	{
		"unixpacket", "", "/etc/file-not-found",
		"dial unixpacket /etc/file-not-found: no such file or directory",
	},
	{
		"unixpacket", "", "/etc/",
		"dial unixpacket /etc/: (permission denied|socket operation on non-socket|connection refused)",
	},
}

func TestDialError(t *testing.T) {
	if !*runErrorTest {
		t.Logf("test disabled; use --run_error_test to enable")
		return
	}
	for i, tt := range dialErrorTests {
		c, e := Dial(tt.Net, tt.Laddr, tt.Raddr)
		if c != nil {
			c.Close()
		}
		if e == nil {
			t.Errorf("#%d: nil error, want match for %#q", i, tt.Pattern)
			continue
		}
		s := e.String()
		match, _ := regexp.MatchString(tt.Pattern, s)
		if !match {
			t.Errorf("#%d: %q, want match for %#q", i, s, tt.Pattern)
		}
	}
}

var revAddrTests = []struct {
	Addr      string
	Reverse   string
	ErrPrefix string
}{
	{"1.2.3.4", "4.3.2.1.in-addr.arpa.", ""},
	{"245.110.36.114", "114.36.110.245.in-addr.arpa.", ""},
	{"::ffff:12.34.56.78", "78.56.34.12.in-addr.arpa.", ""},
	{"::1", "1.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.ip6.arpa.", ""},
	{"1::", "0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.1.0.0.0.ip6.arpa.", ""},
	{"1234:567::89a:bcde", "e.d.c.b.a.9.8.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.7.6.5.0.4.3.2.1.ip6.arpa.", ""},
	{"1234:567:fefe:bcbc:adad:9e4a:89a:bcde", "e.d.c.b.a.9.8.0.a.4.e.9.d.a.d.a.c.b.c.b.e.f.e.f.7.6.5.0.4.3.2.1.ip6.arpa.", ""},
	{"1.2.3", "", "unrecognized address"},
	{"1.2.3.4.5", "", "unrecognized address"},
	{"1234:567:bcbca::89a:bcde", "", "unrecognized address"},
	{"1234:567::bcbc:adad::89a:bcde", "", "unrecognized address"},
}

func TestReverseAddress(t *testing.T) {
	if runtime.GOOS == "windows" {
		return
	}
	for i, tt := range revAddrTests {
		a, e := reverseaddr(tt.Addr)
		if len(tt.ErrPrefix) > 0 && e == nil {
			t.Errorf("#%d: expected %q, got <nil> (error)", i, tt.ErrPrefix)
			continue
		}
		if len(tt.ErrPrefix) == 0 && e != nil {
			t.Errorf("#%d: expected <nil>, got %q (error)", i, e)
		}
		if e != nil && e.(*DNSError).Error != tt.ErrPrefix {
			t.Errorf("#%d: expected %q, got %q (mismatched error)", i, tt.ErrPrefix, e.(*DNSError).Error)
		}
		if a != tt.Reverse {
			t.Errorf("#%d: expected %q, got %q (reverse address)", i, tt.Reverse, a)
		}
	}
}
