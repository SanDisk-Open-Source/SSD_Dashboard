// Copyright 2009 The Go Authors. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

package strconv_test

import (
	"os"
	"reflect"
	. "strconv"
	"testing"
)

type atofTest struct {
	in  string
	out string
	err os.Error
}

var atoftests = []atofTest{
	{"", "0", os.EINVAL},
	{"1", "1", nil},
	{"+1", "1", nil},
	{"1x", "0", os.EINVAL},
	{"1.1.", "0", os.EINVAL},
	{"1e23", "1e+23", nil},
	{"1E23", "1e+23", nil},
	{"100000000000000000000000", "1e+23", nil},
	{"1e-100", "1e-100", nil},
	{"123456700", "1.234567e+08", nil},
	{"99999999999999974834176", "9.999999999999997e+22", nil},
	{"100000000000000000000001", "1.0000000000000001e+23", nil},
	{"100000000000000008388608", "1.0000000000000001e+23", nil},
	{"100000000000000016777215", "1.0000000000000001e+23", nil},
	{"100000000000000016777216", "1.0000000000000003e+23", nil},
	{"-1", "-1", nil},
	{"-0", "-0", nil},
	{"1e-20", "1e-20", nil},
	{"625e-3", "0.625", nil},

	// NaNs
	{"nan", "NaN", nil},
	{"NaN", "NaN", nil},
	{"NAN", "NaN", nil},

	// Infs
	{"inf", "+Inf", nil},
	{"-Inf", "-Inf", nil},
	{"+INF", "+Inf", nil},

	// largest float64
	{"1.7976931348623157e308", "1.7976931348623157e+308", nil},
	{"-1.7976931348623157e308", "-1.7976931348623157e+308", nil},
	// next float64 - too large
	{"1.7976931348623159e308", "+Inf", os.ERANGE},
	{"-1.7976931348623159e308", "-Inf", os.ERANGE},
	// the border is ...158079
	// borderline - okay
	{"1.7976931348623158e308", "1.7976931348623157e+308", nil},
	{"-1.7976931348623158e308", "-1.7976931348623157e+308", nil},
	// borderline - too large
	{"1.797693134862315808e308", "+Inf", os.ERANGE},
	{"-1.797693134862315808e308", "-Inf", os.ERANGE},

	// a little too large
	{"1e308", "1e+308", nil},
	{"2e308", "+Inf", os.ERANGE},
	{"1e309", "+Inf", os.ERANGE},

	// way too large
	{"1e310", "+Inf", os.ERANGE},
	{"-1e310", "-Inf", os.ERANGE},
	{"1e400", "+Inf", os.ERANGE},
	{"-1e400", "-Inf", os.ERANGE},
	{"1e400000", "+Inf", os.ERANGE},
	{"-1e400000", "-Inf", os.ERANGE},

	// denormalized
	{"1e-305", "1e-305", nil},
	{"1e-306", "1e-306", nil},
	{"1e-307", "1e-307", nil},
	{"1e-308", "1e-308", nil},
	{"1e-309", "1e-309", nil},
	{"1e-310", "1e-310", nil},
	{"1e-322", "1e-322", nil},
	// smallest denormal
	{"5e-324", "5e-324", nil},
	{"4e-324", "5e-324", nil},
	{"3e-324", "5e-324", nil},
	// too small
	{"2e-324", "0", nil},
	// way too small
	{"1e-350", "0", nil},
	{"1e-400000", "0", nil},

	// try to overflow exponent
	{"1e-4294967296", "0", nil},
	{"1e+4294967296", "+Inf", os.ERANGE},
	{"1e-18446744073709551616", "0", nil},
	{"1e+18446744073709551616", "+Inf", os.ERANGE},

	// Parse errors
	{"1e", "0", os.EINVAL},
	{"1e-", "0", os.EINVAL},
	{".e-1", "0", os.EINVAL},
}

func init() {
	// The atof routines return NumErrors wrapping
	// the error and the string.  Convert the table above.
	for i := range atoftests {
		test := &atoftests[i]
		if test.err != nil {
			test.err = &NumError{test.in, test.err}
		}
	}
}

func testAtof(t *testing.T, opt bool) {
	oldopt := SetOptimize(opt)
	for i := 0; i < len(atoftests); i++ {
		test := &atoftests[i]
		out, err := Atof64(test.in)
		outs := Ftoa64(out, 'g', -1)
		if outs != test.out || !reflect.DeepEqual(err, test.err) {
			t.Errorf("Atof64(%v) = %v, %v want %v, %v",
				test.in, out, err, test.out, test.err)
		}

		out, err = AtofN(test.in, 64)
		outs = FtoaN(out, 'g', -1, 64)
		if outs != test.out || !reflect.DeepEqual(err, test.err) {
			t.Errorf("AtofN(%v, 64) = %v, %v want %v, %v",
				test.in, out, err, test.out, test.err)
		}

		if float64(float32(out)) == out {
			out32, err := Atof32(test.in)
			outs := Ftoa32(out32, 'g', -1)
			if outs != test.out || !reflect.DeepEqual(err, test.err) {
				t.Errorf("Atof32(%v) = %v, %v want %v, %v  # %v",
					test.in, out32, err, test.out, test.err, out)
			}

			out, err := AtofN(test.in, 32)
			out32 = float32(out)
			outs = FtoaN(float64(out32), 'g', -1, 32)
			if outs != test.out || !reflect.DeepEqual(err, test.err) {
				t.Errorf("AtofN(%v, 32) = %v, %v want %v, %v  # %v",
					test.in, out32, err, test.out, test.err, out)
			}
		}
	}
	SetOptimize(oldopt)
}

func TestAtof(t *testing.T) { testAtof(t, true) }

func TestAtofSlow(t *testing.T) { testAtof(t, false) }

func BenchmarkAtof64Decimal(b *testing.B) {
	for i := 0; i < b.N; i++ {
		Atof64("33909")
	}
}

func BenchmarkAtof64Float(b *testing.B) {
	for i := 0; i < b.N; i++ {
		Atof64("339.7784")
	}
}

func BenchmarkAtof64FloatExp(b *testing.B) {
	for i := 0; i < b.N; i++ {
		Atof64("-5.09e75")
	}
}

func BenchmarkAtof64Big(b *testing.B) {
	for i := 0; i < b.N; i++ {
		Atof64("123456789123456789123456789")
	}
}
