// Copyright 2009 The Go Authors.  All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

package xml

import (
	"bytes"
	"fmt"
	"io"
	"os"
	"reflect"
	"strconv"
	"strings"
	"unicode"
	"utf8"
)

// BUG(rsc): Mapping between XML elements and data structures is inherently flawed:
// an XML element is an order-dependent collection of anonymous
// values, while a data structure is an order-independent collection
// of named values.
// See package json for a textual representation more suitable
// to data structures.

// Unmarshal parses an XML element from r and uses the
// reflect library to fill in an arbitrary struct, slice, or string
// pointed at by val.  Well-formed data that does not fit
// into val is discarded.
//
// For example, given these definitions:
//
//	type Email struct {
//		Where string "attr"
//		Addr  string
//	}
//
//	type Result struct {
//		XMLName xml.Name "result"
//		Name	string
//		Phone	string
//		Email	[]Email
//		Groups  []string "group>value"
//	}
//
//	result := Result{Name: "name", Phone: "phone", Email: nil}
//
// unmarshalling the XML input
//
//	<result>
//		<email where="home">
//			<addr>gre@example.com</addr>
//		</email>
//		<email where='work'>
//			<addr>gre@work.com</addr>
//		</email>
//		<name>Grace R. Emlin</name>
// 		<group>
// 			<value>Friends</value>
// 			<value>Squash</value>
// 		</group>
//		<address>123 Main Street</address>
//	</result>
//
// via Unmarshal(r, &result) is equivalent to assigning
//
//	r = Result{xml.Name{"", "result"},
//		"Grace R. Emlin", // name
//		"phone",	  // no phone given
//		[]Email{
//			Email{"home", "gre@example.com"},
//			Email{"work", "gre@work.com"},
//		},
//		[]string{"Friends", "Squash"},
//	}
//
// Note that the field r.Phone has not been modified and
// that the XML <address> element was discarded. Also, the field
// Groups was assigned considering the element path provided in the
// field tag.
//
// Because Unmarshal uses the reflect package, it can only
// assign to upper case fields.  Unmarshal uses a case-insensitive
// comparison to match XML element names to struct field names.
//
// Unmarshal maps an XML element to a struct using the following rules:
//
//   * If the struct has a field of type []byte or string with tag "innerxml",
//      Unmarshal accumulates the raw XML nested inside the element
//      in that field.  The rest of the rules still apply.
//
//   * If the struct has a field named XMLName of type xml.Name,
//      Unmarshal records the element name in that field.
//
//   * If the XMLName field has an associated tag string of the form
//      "tag" or "namespace-URL tag", the XML element must have
//      the given tag (and, optionally, name space) or else Unmarshal
//      returns an error.
//
//   * If the XML element has an attribute whose name matches a
//      struct field of type string with tag "attr", Unmarshal records
//      the attribute value in that field.
//
//   * If the XML element contains character data, that data is
//      accumulated in the first struct field that has tag "chardata".
//      The struct field may have type []byte or string.
//      If there is no such field, the character data is discarded.
//
//   * If the XML element contains a sub-element whose name matches
//      the prefix of a struct field tag formatted as "a>b>c", unmarshal
//      will descend into the XML structure looking for elements with the
//      given names, and will map the innermost elements to that struct field.
//      A struct field tag starting with ">" is equivalent to one starting
//      with the field name followed by ">".
//
//   * If the XML element contains a sub-element whose name
//      matches a struct field whose tag is neither "attr" nor "chardata",
//      Unmarshal maps the sub-element to that struct field.
//      Otherwise, if the struct has a field named Any, unmarshal
//      maps the sub-element to that struct field.
//
// Unmarshal maps an XML element to a string or []byte by saving the
// concatenation of that element's character data in the string or []byte.
//
// Unmarshal maps an XML element to a slice by extending the length
// of the slice and mapping the element to the newly created value.
//
// Unmarshal maps an XML element to a bool by setting it to the boolean
// value represented by the string.
//
// Unmarshal maps an XML element to an integer or floating-point
// field by setting the field to the result of interpreting the string
// value in decimal.  There is no check for overflow.
//
// Unmarshal maps an XML element to an xml.Name by recording the
// element name.
//
// Unmarshal maps an XML element to a pointer by setting the pointer
// to a freshly allocated value and then mapping the element to that value.
//
func Unmarshal(r io.Reader, val interface{}) os.Error {
	v, ok := reflect.NewValue(val).(*reflect.PtrValue)
	if !ok {
		return os.NewError("non-pointer passed to Unmarshal")
	}
	p := NewParser(r)
	elem := v.Elem()
	err := p.unmarshal(elem, nil)
	if err != nil {
		return err
	}
	return nil
}

// An UnmarshalError represents an error in the unmarshalling process.
type UnmarshalError string

func (e UnmarshalError) String() string { return string(e) }

// A TagPathError represents an error in the unmarshalling process
// caused by the use of field tags with conflicting paths.
type TagPathError struct {
	Struct       reflect.Type
	Field1, Tag1 string
	Field2, Tag2 string
}

func (e *TagPathError) String() string {
	return fmt.Sprintf("%s field %q with tag %q conflicts with field %q with tag %q", e.Struct, e.Field1, e.Tag1, e.Field2, e.Tag2)
}

// The Parser's Unmarshal method is like xml.Unmarshal
// except that it can be passed a pointer to the initial start element,
// useful when a client reads some raw XML tokens itself
// but also defers to Unmarshal for some elements.
// Passing a nil start element indicates that Unmarshal should
// read the token stream to find the start element.
func (p *Parser) Unmarshal(val interface{}, start *StartElement) os.Error {
	v, ok := reflect.NewValue(val).(*reflect.PtrValue)
	if !ok {
		return os.NewError("non-pointer passed to Unmarshal")
	}
	return p.unmarshal(v.Elem(), start)
}

// fieldName strips invalid characters from an XML name
// to create a valid Go struct name.  It also converts the
// name to lower case letters.
func fieldName(original string) string {

	var i int
	//remove leading underscores
	for i = 0; i < len(original) && original[i] == '_'; i++ {
	}

	return strings.Map(
		func(x int) int {
			if x == '_' || unicode.IsDigit(x) || unicode.IsLetter(x) {
				return unicode.ToLower(x)
			}
			return -1
		},
		original[i:])
}

// Unmarshal a single XML element into val.
func (p *Parser) unmarshal(val reflect.Value, start *StartElement) os.Error {
	// Find start element if we need it.
	if start == nil {
		for {
			tok, err := p.Token()
			if err != nil {
				return err
			}
			if t, ok := tok.(StartElement); ok {
				start = &t
				break
			}
		}
	}

	if pv, ok := val.(*reflect.PtrValue); ok {
		if pv.Get() == 0 {
			zv := reflect.MakeZero(pv.Type().(*reflect.PtrType).Elem())
			pv.PointTo(zv)
			val = zv
		} else {
			val = pv.Elem()
		}
	}

	var (
		data         []byte
		saveData     reflect.Value
		comment      []byte
		saveComment  reflect.Value
		saveXML      reflect.Value
		saveXMLIndex int
		saveXMLData  []byte
		sv           *reflect.StructValue
		styp         *reflect.StructType
		fieldPaths   map[string]pathInfo
	)

	switch v := val.(type) {
	default:
		return os.ErrorString("unknown type " + v.Type().String())

	case *reflect.SliceValue:
		typ := v.Type().(*reflect.SliceType)
		if typ.Elem().Kind() == reflect.Uint8 {
			// []byte
			saveData = v
			break
		}

		// Slice of element values.
		// Grow slice.
		n := v.Len()
		if n >= v.Cap() {
			ncap := 2 * n
			if ncap < 4 {
				ncap = 4
			}
			new := reflect.MakeSlice(typ, n, ncap)
			reflect.Copy(new, v)
			v.Set(new)
		}
		v.SetLen(n + 1)

		// Recur to read element into slice.
		if err := p.unmarshal(v.Elem(n), start); err != nil {
			v.SetLen(n)
			return err
		}
		return nil

	case *reflect.BoolValue, *reflect.FloatValue, *reflect.IntValue, *reflect.UintValue, *reflect.StringValue:
		saveData = v

	case *reflect.StructValue:
		if _, ok := v.Interface().(Name); ok {
			v.Set(reflect.NewValue(start.Name).(*reflect.StructValue))
			break
		}

		sv = v
		typ := sv.Type().(*reflect.StructType)
		styp = typ
		// Assign name.
		if f, ok := typ.FieldByName("XMLName"); ok {
			// Validate element name.
			if f.Tag != "" {
				tag := f.Tag
				ns := ""
				i := strings.LastIndex(tag, " ")
				if i >= 0 {
					ns, tag = tag[0:i], tag[i+1:]
				}
				if tag != start.Name.Local {
					return UnmarshalError("expected element type <" + tag + "> but have <" + start.Name.Local + ">")
				}
				if ns != "" && ns != start.Name.Space {
					e := "expected element <" + tag + "> in name space " + ns + " but have "
					if start.Name.Space == "" {
						e += "no name space"
					} else {
						e += start.Name.Space
					}
					return UnmarshalError(e)
				}
			}

			// Save
			v := sv.FieldByIndex(f.Index)
			if _, ok := v.Interface().(Name); !ok {
				return UnmarshalError(sv.Type().String() + " field XMLName does not have type xml.Name")
			}
			v.(*reflect.StructValue).Set(reflect.NewValue(start.Name).(*reflect.StructValue))
		}

		// Assign attributes.
		// Also, determine whether we need to save character data or comments.
		for i, n := 0, typ.NumField(); i < n; i++ {
			f := typ.Field(i)
			switch f.Tag {
			case "attr":
				strv, ok := sv.FieldByIndex(f.Index).(*reflect.StringValue)
				if !ok {
					return UnmarshalError(sv.Type().String() + " field " + f.Name + " has attr tag but is not type string")
				}
				// Look for attribute.
				val := ""
				k := strings.ToLower(f.Name)
				for _, a := range start.Attr {
					if fieldName(a.Name.Local) == k {
						val = a.Value
						break
					}
				}
				strv.Set(val)

			case "comment":
				if saveComment == nil {
					saveComment = sv.FieldByIndex(f.Index)
				}

			case "chardata":
				if saveData == nil {
					saveData = sv.FieldByIndex(f.Index)
				}

			case "innerxml":
				if saveXML == nil {
					saveXML = sv.FieldByIndex(f.Index)
					if p.saved == nil {
						saveXMLIndex = 0
						p.saved = new(bytes.Buffer)
					} else {
						saveXMLIndex = p.savedOffset()
					}
				}

			default:
				if strings.Contains(f.Tag, ">") {
					if fieldPaths == nil {
						fieldPaths = make(map[string]pathInfo)
					}
					path := strings.ToLower(f.Tag)
					if strings.HasPrefix(f.Tag, ">") {
						path = strings.ToLower(f.Name) + path
					}
					if strings.HasSuffix(f.Tag, ">") {
						path = path[:len(path)-1]
					}
					err := addFieldPath(sv, fieldPaths, path, f.Index)
					if err != nil {
						return err
					}
				}
			}
		}
	}

	// Find end element.
	// Process sub-elements along the way.
Loop:
	for {
		var savedOffset int
		if saveXML != nil {
			savedOffset = p.savedOffset()
		}
		tok, err := p.Token()
		if err != nil {
			return err
		}
		switch t := tok.(type) {
		case StartElement:
			// Sub-element.
			// Look up by tag name.
			if sv != nil {
				k := fieldName(t.Name.Local)

				if fieldPaths != nil {
					if _, found := fieldPaths[k]; found {
						if err := p.unmarshalPaths(sv, fieldPaths, k, &t); err != nil {
							return err
						}
						continue Loop
					}
				}

				match := func(s string) bool {
					// check if the name matches ignoring case
					if strings.ToLower(s) != k {
						return false
					}
					// now check that it's public
					c, _ := utf8.DecodeRuneInString(s)
					return unicode.IsUpper(c)
				}

				f, found := styp.FieldByNameFunc(match)
				if !found { // fall back to mop-up field named "Any"
					f, found = styp.FieldByName("Any")
				}
				if found {
					if err := p.unmarshal(sv.FieldByIndex(f.Index), &t); err != nil {
						return err
					}
					continue Loop
				}
			}
			// Not saving sub-element but still have to skip over it.
			if err := p.Skip(); err != nil {
				return err
			}

		case EndElement:
			if saveXML != nil {
				saveXMLData = p.saved.Bytes()[saveXMLIndex:savedOffset]
				if saveXMLIndex == 0 {
					p.saved = nil
				}
			}
			break Loop

		case CharData:
			if saveData != nil {
				data = append(data, t...)
			}

		case Comment:
			if saveComment != nil {
				comment = append(comment, t...)
			}
		}
	}

	var err os.Error
	// Helper functions for integer and unsigned integer conversions
	var itmp int64
	getInt64 := func() bool {
		itmp, err = strconv.Atoi64(string(data))
		// TODO: should check sizes
		return err == nil
	}
	var utmp uint64
	getUint64 := func() bool {
		utmp, err = strconv.Atoui64(string(data))
		// TODO: check for overflow?
		return err == nil
	}
	var ftmp float64
	getFloat64 := func() bool {
		ftmp, err = strconv.Atof64(string(data))
		// TODO: check for overflow?
		return err == nil
	}

	// Save accumulated data and comments
	switch t := saveData.(type) {
	case nil:
		// Probably a comment, handled below
	default:
		return os.ErrorString("cannot happen: unknown type " + t.Type().String())
	case *reflect.IntValue:
		if !getInt64() {
			return err
		}
		t.Set(itmp)
	case *reflect.UintValue:
		if !getUint64() {
			return err
		}
		t.Set(utmp)
	case *reflect.FloatValue:
		if !getFloat64() {
			return err
		}
		t.Set(ftmp)
	case *reflect.BoolValue:
		value, err := strconv.Atob(strings.TrimSpace(string(data)))
		if err != nil {
			return err
		}
		t.Set(value)
	case *reflect.StringValue:
		t.Set(string(data))
	case *reflect.SliceValue:
		t.Set(reflect.NewValue(data).(*reflect.SliceValue))
	}

	switch t := saveComment.(type) {
	case *reflect.StringValue:
		t.Set(string(comment))
	case *reflect.SliceValue:
		t.Set(reflect.NewValue(comment).(*reflect.SliceValue))
	}

	switch t := saveXML.(type) {
	case *reflect.StringValue:
		t.Set(string(saveXMLData))
	case *reflect.SliceValue:
		t.Set(reflect.NewValue(saveXMLData).(*reflect.SliceValue))
	}

	return nil
}

type pathInfo struct {
	fieldIdx []int
	complete bool
}

// addFieldPath takes an element path such as "a>b>c" and fills the
// paths map with all paths leading to it ("a", "a>b", and "a>b>c").
// It is okay for paths to share a common, shorter prefix but not ok
// for one path to itself be a prefix of another.
func addFieldPath(sv *reflect.StructValue, paths map[string]pathInfo, path string, fieldIdx []int) os.Error {
	if info, found := paths[path]; found {
		return tagError(sv, info.fieldIdx, fieldIdx)
	}
	paths[path] = pathInfo{fieldIdx, true}
	for {
		i := strings.LastIndex(path, ">")
		if i < 0 {
			break
		}
		path = path[:i]
		if info, found := paths[path]; found {
			if info.complete {
				return tagError(sv, info.fieldIdx, fieldIdx)
			}
		} else {
			paths[path] = pathInfo{fieldIdx, false}
		}
	}
	return nil

}

func tagError(sv *reflect.StructValue, idx1 []int, idx2 []int) os.Error {
	t := sv.Type().(*reflect.StructType)
	f1 := t.FieldByIndex(idx1)
	f2 := t.FieldByIndex(idx2)
	return &TagPathError{t, f1.Name, f1.Tag, f2.Name, f2.Tag}
}

// unmarshalPaths walks down an XML structure looking for
// wanted paths, and calls unmarshal on them.
func (p *Parser) unmarshalPaths(sv *reflect.StructValue, paths map[string]pathInfo, path string, start *StartElement) os.Error {
	if info, _ := paths[path]; info.complete {
		return p.unmarshal(sv.FieldByIndex(info.fieldIdx), start)
	}
	for {
		tok, err := p.Token()
		if err != nil {
			return err
		}
		switch t := tok.(type) {
		case StartElement:
			k := path + ">" + fieldName(t.Name.Local)
			if _, found := paths[k]; found {
				if err := p.unmarshalPaths(sv, paths, k, &t); err != nil {
					return err
				}
				continue
			}
			if err := p.Skip(); err != nil {
				return err
			}
		case EndElement:
			return nil
		}
	}
	panic("unreachable")
}

// Have already read a start element.
// Read tokens until we find the end element.
// Token is taking care of making sure the
// end element matches the start element we saw.
func (p *Parser) Skip() os.Error {
	for {
		tok, err := p.Token()
		if err != nil {
			return err
		}
		switch t := tok.(type) {
		case StartElement:
			if err := p.Skip(); err != nil {
				return err
			}
		case EndElement:
			return nil
		}
	}
	panic("unreachable")
}
