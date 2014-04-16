// Copyright 2009 The Go Authors. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

// Binary to decimal floating point conversion.
// Algorithm:
//   1) store mantissa in multiprecision decimal
//   2) shift decimal by exponent
//   3) read digits out & format

package strconv

import "math"

// TODO: move elsewhere?
type floatInfo struct {
	mantbits uint
	expbits  uint
	bias     int
}

var float32info = floatInfo{23, 8, -127}
var float64info = floatInfo{52, 11, -1023}

// Ftoa32 converts the 32-bit floating-point number f to a string,
// according to the format fmt and precision prec.
//
// The format fmt is one of
// 'b' (-ddddp±ddd, a binary exponent),
// 'e' (-d.dddde±dd, a decimal exponent),
// 'E' (-d.ddddE±dd, a decimal exponent),
// 'f' (-ddd.dddd, no exponent),
// 'g' ('e' for large exponents, 'f' otherwise), or
// 'G' ('E' for large exponents, 'f' otherwise).
//
// The precision prec controls the number of digits
// (excluding the exponent) printed by the 'e', 'E', 'f', 'g', and 'G' formats.
// For 'e', 'E', and 'f' it is the number of digits after the decimal point.
// For 'g' and 'G' it is the total number of digits.
// The special precision -1 uses the smallest number of digits
// necessary such that Atof32 will return f exactly.
//
// Ftoa32(f) is not the same as Ftoa64(float32(f)),
// because correct rounding and the number of digits
// needed to identify f depend on the precision of the representation.
func Ftoa32(f float32, fmt byte, prec int) string {
	return genericFtoa(uint64(math.Float32bits(f)), fmt, prec, &float32info)
}

// Ftoa64 is like Ftoa32 but converts a 64-bit floating-point number.
func Ftoa64(f float64, fmt byte, prec int) string {
	return genericFtoa(math.Float64bits(f), fmt, prec, &float64info)
}

// FtoaN converts the 64-bit floating-point number f to a string,
// according to the format fmt and precision prec, but it rounds the
// result assuming that it was obtained from a floating-point value
// of n bits (32 or 64).
func FtoaN(f float64, fmt byte, prec int, n int) string {
	if n == 32 {
		return Ftoa32(float32(f), fmt, prec)
	}
	return Ftoa64(f, fmt, prec)
}

func genericFtoa(bits uint64, fmt byte, prec int, flt *floatInfo) string {
	neg := bits>>flt.expbits>>flt.mantbits != 0
	exp := int(bits>>flt.mantbits) & (1<<flt.expbits - 1)
	mant := bits & (uint64(1)<<flt.mantbits - 1)

	switch exp {
	case 1<<flt.expbits - 1:
		// Inf, NaN
		if mant != 0 {
			return "NaN"
		}
		if neg {
			return "-Inf"
		}
		return "+Inf"

	case 0:
		// denormalized
		exp++

	default:
		// add implicit top bit
		mant |= uint64(1) << flt.mantbits
	}
	exp += flt.bias

	// Pick off easy binary format.
	if fmt == 'b' {
		return fmtB(neg, mant, exp, flt)
	}

	// Create exact decimal representation.
	// The shift is exp - flt.mantbits because mant is a 1-bit integer
	// followed by a flt.mantbits fraction, and we are treating it as
	// a 1+flt.mantbits-bit integer.
	d := newDecimal(mant).Shift(exp - int(flt.mantbits))

	// Round appropriately.
	// Negative precision means "only as much as needed to be exact."
	shortest := false
	if prec < 0 {
		shortest = true
		roundShortest(d, mant, exp, flt)
		switch fmt {
		case 'e', 'E':
			prec = d.nd - 1
		case 'f':
			prec = max(d.nd-d.dp, 0)
		case 'g', 'G':
			prec = d.nd
		}
	} else {
		switch fmt {
		case 'e', 'E':
			d.Round(prec + 1)
		case 'f':
			d.Round(d.dp + prec)
		case 'g', 'G':
			if prec == 0 {
				prec = 1
			}
			d.Round(prec)
		}
	}

	switch fmt {
	case 'e', 'E':
		return fmtE(neg, d, prec, fmt)
	case 'f':
		return fmtF(neg, d, prec)
	case 'g', 'G':
		// trailing fractional zeros in 'e' form will be trimmed.
		eprec := prec
		if eprec > d.nd && d.nd >= d.dp {
			eprec = d.nd
		}
		// %e is used if the exponent from the conversion
		// is less than -4 or greater than or equal to the precision.
		// if precision was the shortest possible, use precision 6 for this decision.
		if shortest {
			eprec = 6
		}
		exp := d.dp - 1
		if exp < -4 || exp >= eprec {
			if prec > d.nd {
				prec = d.nd
			}
			return fmtE(neg, d, prec-1, fmt+'e'-'g')
		}
		if prec > d.dp {
			prec = d.nd
		}
		return fmtF(neg, d, max(prec-d.dp, 0))
	}

	return "%" + string(fmt)
}

// Round d (= mant * 2^exp) to the shortest number of digits
// that will let the original floating point value be precisely
// reconstructed.  Size is original floating point size (64 or 32).
func roundShortest(d *decimal, mant uint64, exp int, flt *floatInfo) {
	// If mantissa is zero, the number is zero; stop now.
	if mant == 0 {
		d.nd = 0
		return
	}

	// TODO(rsc): Unless exp == minexp, if the number of digits in d
	// is less than 17, it seems likely that it would be
	// the shortest possible number already.  So maybe we can
	// bail out without doing the extra multiprecision math here.

	// Compute upper and lower such that any decimal number
	// between upper and lower (possibly inclusive)
	// will round to the original floating point number.

	// d = mant << (exp - mantbits)
	// Next highest floating point number is mant+1 << exp-mantbits.
	// Our upper bound is halfway inbetween, mant*2+1 << exp-mantbits-1.
	upper := newDecimal(mant*2 + 1).Shift(exp - int(flt.mantbits) - 1)

	// d = mant << (exp - mantbits)
	// Next lowest floating point number is mant-1 << exp-mantbits,
	// unless mant-1 drops the significant bit and exp is not the minimum exp,
	// in which case the next lowest is mant*2-1 << exp-mantbits-1.
	// Either way, call it mantlo << explo-mantbits.
	// Our lower bound is halfway inbetween, mantlo*2+1 << explo-mantbits-1.
	minexp := flt.bias + 1 // minimum possible exponent
	var mantlo uint64
	var explo int
	if mant > 1<<flt.mantbits || exp == minexp {
		mantlo = mant - 1
		explo = exp
	} else {
		mantlo = mant*2 - 1
		explo = exp - 1
	}
	lower := newDecimal(mantlo*2 + 1).Shift(explo - int(flt.mantbits) - 1)

	// The upper and lower bounds are possible outputs only if
	// the original mantissa is even, so that IEEE round-to-even
	// would round to the original mantissa and not the neighbors.
	inclusive := mant%2 == 0

	// Now we can figure out the minimum number of digits required.
	// Walk along until d has distinguished itself from upper and lower.
	for i := 0; i < d.nd; i++ {
		var l, m, u byte // lower, middle, upper digits
		if i < lower.nd {
			l = lower.d[i]
		} else {
			l = '0'
		}
		m = d.d[i]
		if i < upper.nd {
			u = upper.d[i]
		} else {
			u = '0'
		}

		// Okay to round down (truncate) if lower has a different digit
		// or if lower is inclusive and is exactly the result of rounding down.
		okdown := l != m || (inclusive && l == m && i+1 == lower.nd)

		// Okay to round up if upper has a different digit and
		// either upper is inclusive or upper is bigger than the result of rounding up.
		okup := m != u && (inclusive || i+1 < upper.nd)

		// If it's okay to do either, then round to the nearest one.
		// If it's okay to do only one, do it.
		switch {
		case okdown && okup:
			d.Round(i + 1)
			return
		case okdown:
			d.RoundDown(i + 1)
			return
		case okup:
			d.RoundUp(i + 1)
			return
		}
	}
}

// %e: -d.ddddde±dd
func fmtE(neg bool, d *decimal, prec int, fmt byte) string {
	buf := make([]byte, 3+max(prec, 0)+30) // "-0." + prec digits + exp
	w := 0                                 // write index

	// sign
	if neg {
		buf[w] = '-'
		w++
	}

	// first digit
	if d.nd == 0 {
		buf[w] = '0'
	} else {
		buf[w] = d.d[0]
	}
	w++

	// .moredigits
	if prec > 0 {
		buf[w] = '.'
		w++
		for i := 0; i < prec; i++ {
			if 1+i < d.nd {
				buf[w] = d.d[1+i]
			} else {
				buf[w] = '0'
			}
			w++
		}
	}

	// e±
	buf[w] = fmt
	w++
	exp := d.dp - 1
	if d.nd == 0 { // special case: 0 has exponent 0
		exp = 0
	}
	if exp < 0 {
		buf[w] = '-'
		exp = -exp
	} else {
		buf[w] = '+'
	}
	w++

	// dddd
	// count digits
	n := 0
	for e := exp; e > 0; e /= 10 {
		n++
	}
	// leading zeros
	for i := n; i < 2; i++ {
		buf[w] = '0'
		w++
	}
	// digits
	w += n
	n = 0
	for e := exp; e > 0; e /= 10 {
		n++
		buf[w-n] = byte(e%10 + '0')
	}

	return string(buf[0:w])
}

// %f: -ddddddd.ddddd
func fmtF(neg bool, d *decimal, prec int) string {
	buf := make([]byte, 1+max(d.dp, 1)+1+max(prec, 0))
	w := 0

	// sign
	if neg {
		buf[w] = '-'
		w++
	}

	// integer, padded with zeros as needed.
	if d.dp > 0 {
		var i int
		for i = 0; i < d.dp && i < d.nd; i++ {
			buf[w] = d.d[i]
			w++
		}
		for ; i < d.dp; i++ {
			buf[w] = '0'
			w++
		}
	} else {
		buf[w] = '0'
		w++
	}

	// fraction
	if prec > 0 {
		buf[w] = '.'
		w++
		for i := 0; i < prec; i++ {
			if d.dp+i < 0 || d.dp+i >= d.nd {
				buf[w] = '0'
			} else {
				buf[w] = d.d[d.dp+i]
			}
			w++
		}
	}

	return string(buf[0:w])
}

// %b: -ddddddddp+ddd
func fmtB(neg bool, mant uint64, exp int, flt *floatInfo) string {
	var buf [50]byte
	w := len(buf)
	exp -= int(flt.mantbits)
	esign := byte('+')
	if exp < 0 {
		esign = '-'
		exp = -exp
	}
	n := 0
	for exp > 0 || n < 1 {
		n++
		w--
		buf[w] = byte(exp%10 + '0')
		exp /= 10
	}
	w--
	buf[w] = esign
	w--
	buf[w] = 'p'
	n = 0
	for mant > 0 || n < 1 {
		n++
		w--
		buf[w] = byte(mant%10 + '0')
		mant /= 10
	}
	if neg {
		w--
		buf[w] = '-'
	}
	return string(buf[w:])
}

func max(a, b int) int {
	if a > b {
		return a
	}
	return b
}
