// Copyright 2010 The Go Authors. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

// Represents JSON data structure using native Go types: booleans, floats,
// strings, arrays, and maps.

package json

import (
	"container/vector"
	"os"
	"reflect"
	"runtime"
	"strconv"
	"strings"
	"unicode"
	"utf16"
	"utf8"
)

// Unmarshal parses the JSON-encoded data and stores the result
// in the value pointed to by v.
//
// Unmarshal traverses the value v recursively.
// If an encountered value implements the Unmarshaler interface,
// Unmarshal calls its UnmarshalJSON method with a well-formed
// JSON encoding.
//
// Otherwise, Unmarshal uses the inverse of the encodings that
// Marshal uses, allocating maps, slices, and pointers as necessary,
// with the following additional rules:
//
// To unmarshal a JSON value into a nil interface value, the
// type stored in the interface value is one of:
//
//	bool, for JSON booleans
//	float64, for JSON numbers
//	string, for JSON strings
//	[]interface{}, for JSON arrays
//	map[string]interface{}, for JSON objects
//	nil for JSON null
//
// If a JSON value is not appropriate for a given target type,
// or if a JSON number overflows the target type, Unmarshal
// skips that field and completes the unmarshalling as best it can.
// If no more serious errors are encountered, Unmarshal returns
// an UnmarshalTypeError describing the earliest such error.
//
func Unmarshal(data []byte, v interface{}) os.Error {
	d := new(decodeState).init(data)

	// Quick check for well-formedness.
	// Avoids filling out half a data structure
	// before discovering a JSON syntax error.
	err := checkValid(data, &d.scan)
	if err != nil {
		return err
	}

	return d.unmarshal(v)
}

// Unmarshaler is the interface implemented by objects
// that can unmarshal a JSON description of themselves.
// The input can be assumed to be a valid JSON object
// encoding.  UnmarshalJSON must copy the JSON data
// if it wishes to retain the data after returning.
type Unmarshaler interface {
	UnmarshalJSON([]byte) os.Error
}


// An UnmarshalTypeError describes a JSON value that was
// not appropriate for a value of a specific Go type.
type UnmarshalTypeError struct {
	Value string       // description of JSON value - "bool", "array", "number -5"
	Type  reflect.Type // type of Go value it could not be assigned to
}

func (e *UnmarshalTypeError) String() string {
	return "json: cannot unmarshal " + e.Value + " into Go value of type " + e.Type.String()
}

// An UnmarshalFieldError describes a JSON object key that
// led to an unexported (and therefore unwritable) struct field.
type UnmarshalFieldError struct {
	Key   string
	Type  *reflect.StructType
	Field reflect.StructField
}

func (e *UnmarshalFieldError) String() string {
	return "json: cannot unmarshal object key " + strconv.Quote(e.Key) + " into unexported field " + e.Field.Name + " of type " + e.Type.String()
}

// An InvalidUnmarshalError describes an invalid argument passed to Unmarshal.
// (The argument to Unmarshal must be a non-nil pointer.)
type InvalidUnmarshalError struct {
	Type reflect.Type
}

func (e *InvalidUnmarshalError) String() string {
	if e.Type == nil {
		return "json: Unmarshal(nil)"
	}

	if _, ok := e.Type.(*reflect.PtrType); !ok {
		return "json: Unmarshal(non-pointer " + e.Type.String() + ")"
	}
	return "json: Unmarshal(nil " + e.Type.String() + ")"
}

func (d *decodeState) unmarshal(v interface{}) (err os.Error) {
	defer func() {
		if r := recover(); r != nil {
			if _, ok := r.(runtime.Error); ok {
				panic(r)
			}
			err = r.(os.Error)
		}
	}()

	rv := reflect.NewValue(v)
	pv, ok := rv.(*reflect.PtrValue)
	if !ok || pv.IsNil() {
		return &InvalidUnmarshalError{reflect.Typeof(v)}
	}

	d.scan.reset()
	// We decode rv not pv.Elem because the Unmarshaler interface
	// test must be applied at the top level of the value.
	d.value(rv)
	return d.savedError
}

// decodeState represents the state while decoding a JSON value.
type decodeState struct {
	data       []byte
	off        int // read offset in data
	scan       scanner
	nextscan   scanner // for calls to nextValue
	savedError os.Error
}

// errPhase is used for errors that should not happen unless
// there is a bug in the JSON decoder or something is editing
// the data slice while the decoder executes.
var errPhase = os.NewError("JSON decoder out of sync - data changing underfoot?")

func (d *decodeState) init(data []byte) *decodeState {
	d.data = data
	d.off = 0
	d.savedError = nil
	return d
}

// error aborts the decoding by panicking with err.
func (d *decodeState) error(err os.Error) {
	panic(err)
}

// saveError saves the first err it is called with,
// for reporting at the end of the unmarshal.
func (d *decodeState) saveError(err os.Error) {
	if d.savedError == nil {
		d.savedError = err
	}
}

// next cuts off and returns the next full JSON value in d.data[d.off:].
// The next value is known to be an object or array, not a literal.
func (d *decodeState) next() []byte {
	c := d.data[d.off]
	item, rest, err := nextValue(d.data[d.off:], &d.nextscan)
	if err != nil {
		d.error(err)
	}
	d.off = len(d.data) - len(rest)

	// Our scanner has seen the opening brace/bracket
	// and thinks we're still in the middle of the object.
	// invent a closing brace/bracket to get it out.
	if c == '{' {
		d.scan.step(&d.scan, '}')
	} else {
		d.scan.step(&d.scan, ']')
	}

	return item
}

// scanWhile processes bytes in d.data[d.off:] until it
// receives a scan code not equal to op.
// It updates d.off and returns the new scan code.
func (d *decodeState) scanWhile(op int) int {
	var newOp int
	for {
		if d.off >= len(d.data) {
			newOp = d.scan.eof()
			d.off = len(d.data) + 1 // mark processed EOF with len+1
		} else {
			c := int(d.data[d.off])
			d.off++
			newOp = d.scan.step(&d.scan, c)
		}
		if newOp != op {
			break
		}
	}
	return newOp
}

// value decodes a JSON value from d.data[d.off:] into the value.
// it updates d.off to point past the decoded value.
func (d *decodeState) value(v reflect.Value) {
	if v == nil {
		_, rest, err := nextValue(d.data[d.off:], &d.nextscan)
		if err != nil {
			d.error(err)
		}
		d.off = len(d.data) - len(rest)

		// d.scan thinks we're still at the beginning of the item.
		// Feed in an empty string - the shortest, simplest value -
		// so that it knows we got to the end of the value.
		if d.scan.step == stateRedo {
			panic("redo")
		}
		d.scan.step(&d.scan, '"')
		d.scan.step(&d.scan, '"')
		return
	}

	switch op := d.scanWhile(scanSkipSpace); op {
	default:
		d.error(errPhase)

	case scanBeginArray:
		d.array(v)

	case scanBeginObject:
		d.object(v)

	case scanBeginLiteral:
		d.literal(v)
	}
}

// indirect walks down v allocating pointers as needed,
// until it gets to a non-pointer.
// if it encounters an Unmarshaler, indirect stops and returns that.
// if wantptr is true, indirect stops at the last pointer.
func (d *decodeState) indirect(v reflect.Value, wantptr bool) (Unmarshaler, reflect.Value) {
	for {
		var isUnmarshaler bool
		if v.Type().NumMethod() > 0 {
			// Remember that this is an unmarshaler,
			// but wait to return it until after allocating
			// the pointer (if necessary).
			_, isUnmarshaler = v.Interface().(Unmarshaler)
		}

		if iv, ok := v.(*reflect.InterfaceValue); ok && !iv.IsNil() {
			v = iv.Elem()
			continue
		}
		pv, ok := v.(*reflect.PtrValue)
		if !ok {
			break
		}
		_, isptrptr := pv.Elem().(*reflect.PtrValue)
		if !isptrptr && wantptr && !isUnmarshaler {
			return nil, pv
		}
		if pv.IsNil() {
			pv.PointTo(reflect.MakeZero(pv.Type().(*reflect.PtrType).Elem()))
		}
		if isUnmarshaler {
			// Using v.Interface().(Unmarshaler)
			// here means that we have to use a pointer
			// as the struct field.  We cannot use a value inside
			// a pointer to a struct, because in that case
			// v.Interface() is the value (x.f) not the pointer (&x.f).
			// This is an unfortunate consequence of reflect.
			// An alternative would be to look up the
			// UnmarshalJSON method and return a FuncValue.
			return v.Interface().(Unmarshaler), nil
		}
		v = pv.Elem()
	}
	return nil, v
}

// array consumes an array from d.data[d.off-1:], decoding into the value v.
// the first byte of the array ('[') has been read already.
func (d *decodeState) array(v reflect.Value) {
	// Check for unmarshaler.
	unmarshaler, pv := d.indirect(v, false)
	if unmarshaler != nil {
		d.off--
		err := unmarshaler.UnmarshalJSON(d.next())
		if err != nil {
			d.error(err)
		}
		return
	}
	v = pv

	// Decoding into nil interface?  Switch to non-reflect code.
	iv, ok := v.(*reflect.InterfaceValue)
	if ok {
		iv.Set(reflect.NewValue(d.arrayInterface()))
		return
	}

	// Check type of target.
	av, ok := v.(reflect.ArrayOrSliceValue)
	if !ok {
		d.saveError(&UnmarshalTypeError{"array", v.Type()})
		d.off--
		d.next()
		return
	}

	sv, _ := v.(*reflect.SliceValue)

	i := 0
	for {
		// Look ahead for ] - can only happen on first iteration.
		op := d.scanWhile(scanSkipSpace)
		if op == scanEndArray {
			break
		}

		// Back up so d.value can have the byte we just read.
		d.off--
		d.scan.undo(op)

		// Get element of array, growing if necessary.
		if i >= av.Cap() && sv != nil {
			newcap := sv.Cap() + sv.Cap()/2
			if newcap < 4 {
				newcap = 4
			}
			newv := reflect.MakeSlice(sv.Type().(*reflect.SliceType), sv.Len(), newcap)
			reflect.Copy(newv, sv)
			sv.Set(newv)
		}
		if i >= av.Len() && sv != nil {
			// Must be slice; gave up on array during i >= av.Cap().
			sv.SetLen(i + 1)
		}

		// Decode into element.
		if i < av.Len() {
			d.value(av.Elem(i))
		} else {
			// Ran out of fixed array: skip.
			d.value(nil)
		}
		i++

		// Next token must be , or ].
		op = d.scanWhile(scanSkipSpace)
		if op == scanEndArray {
			break
		}
		if op != scanArrayValue {
			d.error(errPhase)
		}
	}
	if i < av.Len() {
		if sv == nil {
			// Array.  Zero the rest.
			z := reflect.MakeZero(av.Type().(*reflect.ArrayType).Elem())
			for ; i < av.Len(); i++ {
				av.Elem(i).SetValue(z)
			}
		} else {
			sv.SetLen(i)
		}
	}
}

// matchName returns true if key should be written to a field named name.
func matchName(key, name string) bool {
	return strings.ToLower(key) == strings.ToLower(name)
}

// object consumes an object from d.data[d.off-1:], decoding into the value v.
// the first byte of the object ('{') has been read already.
func (d *decodeState) object(v reflect.Value) {
	// Check for unmarshaler.
	unmarshaler, pv := d.indirect(v, false)
	if unmarshaler != nil {
		d.off--
		err := unmarshaler.UnmarshalJSON(d.next())
		if err != nil {
			d.error(err)
		}
		return
	}
	v = pv

	// Decoding into nil interface?  Switch to non-reflect code.
	iv, ok := v.(*reflect.InterfaceValue)
	if ok {
		iv.Set(reflect.NewValue(d.objectInterface()))
		return
	}

	// Check type of target: struct or map[string]T
	var (
		mv *reflect.MapValue
		sv *reflect.StructValue
	)
	switch v := v.(type) {
	case *reflect.MapValue:
		// map must have string type
		t := v.Type().(*reflect.MapType)
		if t.Key() != reflect.Typeof("") {
			d.saveError(&UnmarshalTypeError{"object", v.Type()})
			break
		}
		mv = v
		if mv.IsNil() {
			mv.SetValue(reflect.MakeMap(t))
		}
	case *reflect.StructValue:
		sv = v
	default:
		d.saveError(&UnmarshalTypeError{"object", v.Type()})
	}

	if mv == nil && sv == nil {
		d.off--
		d.next() // skip over { } in input
		return
	}

	for {
		// Read opening " of string key or closing }.
		op := d.scanWhile(scanSkipSpace)
		if op == scanEndObject {
			// closing } - can only happen on first iteration.
			break
		}
		if op != scanBeginLiteral {
			d.error(errPhase)
		}

		// Read string key.
		start := d.off - 1
		op = d.scanWhile(scanContinue)
		item := d.data[start : d.off-1]
		key, ok := unquote(item)
		if !ok {
			d.error(errPhase)
		}

		// Figure out field corresponding to key.
		var subv reflect.Value
		if mv != nil {
			subv = reflect.MakeZero(mv.Type().(*reflect.MapType).Elem())
		} else {
			var f reflect.StructField
			var ok bool
			// First try for field with that tag.
			st := sv.Type().(*reflect.StructType)
			for i := 0; i < sv.NumField(); i++ {
				f = st.Field(i)
				if f.Tag == key {
					ok = true
					break
				}
			}
			if !ok {
				// Second, exact match.
				f, ok = st.FieldByName(key)
			}
			if !ok {
				// Third, case-insensitive match.
				f, ok = st.FieldByNameFunc(func(s string) bool { return matchName(key, s) })
			}

			// Extract value; name must be exported.
			if ok {
				if f.PkgPath != "" {
					d.saveError(&UnmarshalFieldError{key, st, f})
				} else {
					subv = sv.FieldByIndex(f.Index)
				}
			}
		}

		// Read : before value.
		if op == scanSkipSpace {
			op = d.scanWhile(scanSkipSpace)
		}
		if op != scanObjectKey {
			d.error(errPhase)
		}

		// Read value.
		d.value(subv)

		// Write value back to map;
		// if using struct, subv points into struct already.
		if mv != nil {
			mv.SetElem(reflect.NewValue(key), subv)
		}

		// Next token must be , or }.
		op = d.scanWhile(scanSkipSpace)
		if op == scanEndObject {
			break
		}
		if op != scanObjectValue {
			d.error(errPhase)
		}
	}
}

// literal consumes a literal from d.data[d.off-1:], decoding into the value v.
// The first byte of the literal has been read already
// (that's how the caller knows it's a literal).
func (d *decodeState) literal(v reflect.Value) {
	// All bytes inside literal return scanContinue op code.
	start := d.off - 1
	op := d.scanWhile(scanContinue)

	// Scan read one byte too far; back up.
	d.off--
	d.scan.undo(op)
	item := d.data[start:d.off]

	// Check for unmarshaler.
	wantptr := item[0] == 'n' // null
	unmarshaler, pv := d.indirect(v, wantptr)
	if unmarshaler != nil {
		err := unmarshaler.UnmarshalJSON(item)
		if err != nil {
			d.error(err)
		}
		return
	}
	v = pv

	switch c := item[0]; c {
	case 'n': // null
		switch v.(type) {
		default:
			d.saveError(&UnmarshalTypeError{"null", v.Type()})
		case *reflect.InterfaceValue, *reflect.PtrValue, *reflect.MapValue:
			v.SetValue(nil)
		}

	case 't', 'f': // true, false
		value := c == 't'
		switch v := v.(type) {
		default:
			d.saveError(&UnmarshalTypeError{"bool", v.Type()})
		case *reflect.BoolValue:
			v.Set(value)
		case *reflect.InterfaceValue:
			v.Set(reflect.NewValue(value))
		}

	case '"': // string
		s, ok := unquote(item)
		if !ok {
			d.error(errPhase)
		}
		switch v := v.(type) {
		default:
			d.saveError(&UnmarshalTypeError{"string", v.Type()})
		case *reflect.StringValue:
			v.Set(s)
		case *reflect.InterfaceValue:
			v.Set(reflect.NewValue(s))
		}

	default: // number
		if c != '-' && (c < '0' || c > '9') {
			d.error(errPhase)
		}
		s := string(item)
		switch v := v.(type) {
		default:
			d.error(&UnmarshalTypeError{"number", v.Type()})
		case *reflect.InterfaceValue:
			n, err := strconv.Atof64(s)
			if err != nil {
				d.saveError(&UnmarshalTypeError{"number " + s, v.Type()})
				break
			}
			v.Set(reflect.NewValue(n))

		case *reflect.IntValue:
			n, err := strconv.Atoi64(s)
			if err != nil || v.Overflow(n) {
				d.saveError(&UnmarshalTypeError{"number " + s, v.Type()})
				break
			}
			v.Set(n)

		case *reflect.UintValue:
			n, err := strconv.Atoui64(s)
			if err != nil || v.Overflow(n) {
				d.saveError(&UnmarshalTypeError{"number " + s, v.Type()})
				break
			}
			v.Set(n)

		case *reflect.FloatValue:
			n, err := strconv.AtofN(s, v.Type().Bits())
			if err != nil || v.Overflow(n) {
				d.saveError(&UnmarshalTypeError{"number " + s, v.Type()})
				break
			}
			v.Set(n)
		}
	}
}

// The xxxInterface routines build up a value to be stored
// in an empty interface.  They are not strictly necessary,
// but they avoid the weight of reflection in this common case.

// valueInterface is like value but returns interface{}
func (d *decodeState) valueInterface() interface{} {
	switch d.scanWhile(scanSkipSpace) {
	default:
		d.error(errPhase)
	case scanBeginArray:
		return d.arrayInterface()
	case scanBeginObject:
		return d.objectInterface()
	case scanBeginLiteral:
		return d.literalInterface()
	}
	panic("unreachable")
}

// arrayInterface is like array but returns []interface{}.
func (d *decodeState) arrayInterface() []interface{} {
	var v vector.Vector
	for {
		// Look ahead for ] - can only happen on first iteration.
		op := d.scanWhile(scanSkipSpace)
		if op == scanEndArray {
			break
		}

		// Back up so d.value can have the byte we just read.
		d.off--
		d.scan.undo(op)

		v.Push(d.valueInterface())

		// Next token must be , or ].
		op = d.scanWhile(scanSkipSpace)
		if op == scanEndArray {
			break
		}
		if op != scanArrayValue {
			d.error(errPhase)
		}
	}
	return v
}

// objectInterface is like object but returns map[string]interface{}.
func (d *decodeState) objectInterface() map[string]interface{} {
	m := make(map[string]interface{})
	for {
		// Read opening " of string key or closing }.
		op := d.scanWhile(scanSkipSpace)
		if op == scanEndObject {
			// closing } - can only happen on first iteration.
			break
		}
		if op != scanBeginLiteral {
			d.error(errPhase)
		}

		// Read string key.
		start := d.off - 1
		op = d.scanWhile(scanContinue)
		item := d.data[start : d.off-1]
		key, ok := unquote(item)
		if !ok {
			d.error(errPhase)
		}

		// Read : before value.
		if op == scanSkipSpace {
			op = d.scanWhile(scanSkipSpace)
		}
		if op != scanObjectKey {
			d.error(errPhase)
		}

		// Read value.
		m[key] = d.valueInterface()

		// Next token must be , or }.
		op = d.scanWhile(scanSkipSpace)
		if op == scanEndObject {
			break
		}
		if op != scanObjectValue {
			d.error(errPhase)
		}
	}
	return m
}


// literalInterface is like literal but returns an interface value.
func (d *decodeState) literalInterface() interface{} {
	// All bytes inside literal return scanContinue op code.
	start := d.off - 1
	op := d.scanWhile(scanContinue)

	// Scan read one byte too far; back up.
	d.off--
	d.scan.undo(op)
	item := d.data[start:d.off]

	switch c := item[0]; c {
	case 'n': // null
		return nil

	case 't', 'f': // true, false
		return c == 't'

	case '"': // string
		s, ok := unquote(item)
		if !ok {
			d.error(errPhase)
		}
		return s

	default: // number
		if c != '-' && (c < '0' || c > '9') {
			d.error(errPhase)
		}
		n, err := strconv.Atof64(string(item))
		if err != nil {
			d.saveError(&UnmarshalTypeError{"number " + string(item), reflect.Typeof(0.0)})
		}
		return n
	}
	panic("unreachable")
}

// getu4 decodes \uXXXX from the beginning of s, returning the hex value,
// or it returns -1.
func getu4(s []byte) int {
	if len(s) < 6 || s[0] != '\\' || s[1] != 'u' {
		return -1
	}
	rune, err := strconv.Btoui64(string(s[2:6]), 16)
	if err != nil {
		return -1
	}
	return int(rune)
}

// unquote converts a quoted JSON string literal s into an actual string t.
// The rules are different than for Go, so cannot use strconv.Unquote.
func unquote(s []byte) (t string, ok bool) {
	if len(s) < 2 || s[0] != '"' || s[len(s)-1] != '"' {
		return
	}
	b := make([]byte, len(s)+2*utf8.UTFMax)
	w := 0
	for r := 1; r < len(s)-1; {
		// Out of room?  Can only happen if s is full of
		// malformed UTF-8 and we're replacing each
		// byte with RuneError.
		if w >= len(b)-2*utf8.UTFMax {
			nb := make([]byte, (len(b)+utf8.UTFMax)*2)
			copy(nb, b[0:w])
			b = nb
		}
		switch c := s[r]; {
		case c == '\\':
			r++
			if r >= len(s)-1 {
				return
			}
			switch s[r] {
			default:
				return
			case '"', '\\', '/', '\'':
				b[w] = s[r]
				r++
				w++
			case 'b':
				b[w] = '\b'
				r++
				w++
			case 'f':
				b[w] = '\f'
				r++
				w++
			case 'n':
				b[w] = '\n'
				r++
				w++
			case 'r':
				b[w] = '\r'
				r++
				w++
			case 't':
				b[w] = '\t'
				r++
				w++
			case 'u':
				r--
				rune := getu4(s[r:])
				if rune < 0 {
					return
				}
				r += 6
				if utf16.IsSurrogate(rune) {
					rune1 := getu4(s[r:])
					if dec := utf16.DecodeRune(rune, rune1); dec != unicode.ReplacementChar {
						// A valid pair; consume.
						r += 6
						w += utf8.EncodeRune(b[w:], dec)
						break
					}
					// Invalid surrogate; fall back to replacement rune.
					rune = unicode.ReplacementChar
				}
				w += utf8.EncodeRune(b[w:], rune)
			}

		// Quote, control characters are invalid.
		case c == '"', c < ' ':
			return

		// ASCII
		case c < utf8.RuneSelf:
			b[w] = c
			r++
			w++

		// Coerce to well-formed UTF-8.
		default:
			rune, size := utf8.DecodeRune(s[r:])
			r += size
			w += utf8.EncodeRune(b[w:], rune)
		}
	}
	return string(b[0:w]), true
}
