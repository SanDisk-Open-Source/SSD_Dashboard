// Copyright 2009 The Go Authors. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

// Package xml implements a simple XML 1.0 parser that
// understands XML name spaces.
package xml

// References:
//    Annotated XML spec: http://www.xml.com/axml/testaxml.htm
//    XML name spaces: http://www.w3.org/TR/REC-xml-names/

// TODO(rsc):
//	Test error handling.

import (
	"bufio"
	"bytes"
	"fmt"
	"io"
	"os"
	"strconv"
	"strings"
	"unicode"
	"utf8"
)

// A SyntaxError represents a syntax error in the XML input stream.
type SyntaxError struct {
	Msg  string
	Line int
}

func (e *SyntaxError) String() string {
	return "XML syntax error on line " + strconv.Itoa(e.Line) + ": " + e.Msg
}

// A Name represents an XML name (Local) annotated
// with a name space identifier (Space).
// In tokens returned by Parser.Token, the Space identifier
// is given as a canonical URL, not the short prefix used
// in the document being parsed.
type Name struct {
	Space, Local string
}

// An Attr represents an attribute in an XML element (Name=Value).
type Attr struct {
	Name  Name
	Value string
}

// A Token is an interface holding one of the token types:
// StartElement, EndElement, CharData, Comment, ProcInst, or Directive.
type Token interface{}

// A StartElement represents an XML start element.
type StartElement struct {
	Name Name
	Attr []Attr
}

func (e StartElement) Copy() StartElement {
	attrs := make([]Attr, len(e.Attr))
	copy(e.Attr, attrs)
	e.Attr = attrs
	return e
}

// An EndElement represents an XML end element.
type EndElement struct {
	Name Name
}

// A CharData represents XML character data (raw text),
// in which XML escape sequences have been replaced by
// the characters they represent.
type CharData []byte

func makeCopy(b []byte) []byte {
	b1 := make([]byte, len(b))
	copy(b1, b)
	return b1
}

func (c CharData) Copy() CharData { return CharData(makeCopy(c)) }

// A Comment represents an XML comment of the form <!--comment-->.
// The bytes do not include the <!-- and --> comment markers.
type Comment []byte

func (c Comment) Copy() Comment { return Comment(makeCopy(c)) }

// A ProcInst represents an XML processing instruction of the form <?target inst?>
type ProcInst struct {
	Target string
	Inst   []byte
}

func (p ProcInst) Copy() ProcInst {
	p.Inst = makeCopy(p.Inst)
	return p
}

// A Directive represents an XML directive of the form <!text>.
// The bytes do not include the <! and > markers.
type Directive []byte

func (d Directive) Copy() Directive { return Directive(makeCopy(d)) }

// CopyToken returns a copy of a Token.
func CopyToken(t Token) Token {
	switch v := t.(type) {
	case CharData:
		return v.Copy()
	case Comment:
		return v.Copy()
	case Directive:
		return v.Copy()
	case ProcInst:
		return v.Copy()
	case StartElement:
		return v.Copy()
	}
	return t
}

// A Parser represents an XML parser reading a particular input stream.
// The parser assumes that its input is encoded in UTF-8.
type Parser struct {
	// Strict defaults to true, enforcing the requirements
	// of the XML specification.
	// If set to false, the parser allows input containing common
	// mistakes:
	//	* If an element is missing an end tag, the parser invents
	//	  end tags as necessary to keep the return values from Token
	//	  properly balanced.
	//	* In attribute values and character data, unknown or malformed
	//	  character entities (sequences beginning with &) are left alone.
	//
	// Setting:
	//
	//	p.Strict = false;
	//	p.AutoClose = HTMLAutoClose;
	//	p.Entity = HTMLEntity
	//
	// creates a parser that can handle typical HTML.
	Strict bool

	// When Strict == false, AutoClose indicates a set of elements to
	// consider closed immediately after they are opened, regardless
	// of whether an end element is present.
	AutoClose []string

	// Entity can be used to map non-standard entity names to string replacements.
	// The parser behaves as if these standard mappings are present in the map,
	// regardless of the actual map content:
	//
	//	"lt": "<",
	//	"gt": ">",
	//	"amp": "&",
	//	"apos": "'",
	//	"quot": `"`,
	Entity map[string]string

	r         io.ReadByter
	buf       bytes.Buffer
	saved     *bytes.Buffer
	stk       *stack
	free      *stack
	needClose bool
	toClose   Name
	nextToken Token
	nextByte  int
	ns        map[string]string
	err       os.Error
	line      int
	tmp       [32]byte
}

// NewParser creates a new XML parser reading from r.
func NewParser(r io.Reader) *Parser {
	p := &Parser{
		ns:       make(map[string]string),
		nextByte: -1,
		line:     1,
		Strict:   true,
	}

	// Get efficient byte at a time reader.
	// Assume that if reader has its own
	// ReadByte, it's efficient enough.
	// Otherwise, use bufio.
	if rb, ok := r.(io.ReadByter); ok {
		p.r = rb
	} else {
		p.r = bufio.NewReader(r)
	}

	return p
}

// Token returns the next XML token in the input stream.
// At the end of the input stream, Token returns nil, os.EOF.
//
// Slices of bytes in the returned token data refer to the
// parser's internal buffer and remain valid only until the next
// call to Token.  To acquire a copy of the bytes, call CopyToken
// or the token's Copy method.
//
// Token expands self-closing elements such as <br/>
// into separate start and end elements returned by successive calls.
//
// Token guarantees that the StartElement and EndElement
// tokens it returns are properly nested and matched:
// if Token encounters an unexpected end element,
// it will return an error.
//
// Token implements XML name spaces as described by
// http://www.w3.org/TR/REC-xml-names/.  Each of the
// Name structures contained in the Token has the Space
// set to the URL identifying its name space when known.
// If Token encounters an unrecognized name space prefix,
// it uses the prefix as the Space rather than report an error.
func (p *Parser) Token() (t Token, err os.Error) {
	if p.nextToken != nil {
		t = p.nextToken
		p.nextToken = nil
	} else if t, err = p.RawToken(); err != nil {
		return
	}

	if !p.Strict {
		if t1, ok := p.autoClose(t); ok {
			p.nextToken = t
			t = t1
		}
	}
	switch t1 := t.(type) {
	case StartElement:
		// In XML name spaces, the translations listed in the
		// attributes apply to the element name and
		// to the other attribute names, so process
		// the translations first.
		for _, a := range t1.Attr {
			if a.Name.Space == "xmlns" {
				v, ok := p.ns[a.Name.Local]
				p.pushNs(a.Name.Local, v, ok)
				p.ns[a.Name.Local] = a.Value
			}
			if a.Name.Space == "" && a.Name.Local == "xmlns" {
				// Default space for untagged names
				v, ok := p.ns[""]
				p.pushNs("", v, ok)
				p.ns[""] = a.Value
			}
		}

		p.translate(&t1.Name, true)
		for i := range t1.Attr {
			p.translate(&t1.Attr[i].Name, false)
		}
		p.pushElement(t1.Name)
		t = t1

	case EndElement:
		p.translate(&t1.Name, true)
		if !p.popElement(&t1) {
			return nil, p.err
		}
		t = t1
	}
	return
}

// Apply name space translation to name n.
// The default name space (for Space=="")
// applies only to element names, not to attribute names.
func (p *Parser) translate(n *Name, isElementName bool) {
	switch {
	case n.Space == "xmlns":
		return
	case n.Space == "" && !isElementName:
		return
	case n.Space == "" && n.Local == "xmlns":
		return
	}
	if v, ok := p.ns[n.Space]; ok {
		n.Space = v
	}
}

// Parsing state - stack holds old name space translations
// and the current set of open elements.  The translations to pop when
// ending a given tag are *below* it on the stack, which is
// more work but forced on us by XML.
type stack struct {
	next *stack
	kind int
	name Name
	ok   bool
}

const (
	stkStart = iota
	stkNs
)

func (p *Parser) push(kind int) *stack {
	s := p.free
	if s != nil {
		p.free = s.next
	} else {
		s = new(stack)
	}
	s.next = p.stk
	s.kind = kind
	p.stk = s
	return s
}

func (p *Parser) pop() *stack {
	s := p.stk
	if s != nil {
		p.stk = s.next
		s.next = p.free
		p.free = s
	}
	return s
}

// Record that we are starting an element with the given name.
func (p *Parser) pushElement(name Name) {
	s := p.push(stkStart)
	s.name = name
}

// Record that we are changing the value of ns[local].
// The old value is url, ok.
func (p *Parser) pushNs(local string, url string, ok bool) {
	s := p.push(stkNs)
	s.name.Local = local
	s.name.Space = url
	s.ok = ok
}

// Creates a SyntaxError with the current line number.
func (p *Parser) syntaxError(msg string) os.Error {
	return &SyntaxError{Msg: msg, Line: p.line}
}

// Record that we are ending an element with the given name.
// The name must match the record at the top of the stack,
// which must be a pushElement record.
// After popping the element, apply any undo records from
// the stack to restore the name translations that existed
// before we saw this element.
func (p *Parser) popElement(t *EndElement) bool {
	s := p.pop()
	name := t.Name
	switch {
	case s == nil || s.kind != stkStart:
		p.err = p.syntaxError("unexpected end element </" + name.Local + ">")
		return false
	case s.name.Local != name.Local:
		if !p.Strict {
			p.needClose = true
			p.toClose = t.Name
			t.Name = s.name
			return true
		}
		p.err = p.syntaxError("element <" + s.name.Local + "> closed by </" + name.Local + ">")
		return false
	case s.name.Space != name.Space:
		p.err = p.syntaxError("element <" + s.name.Local + "> in space " + s.name.Space +
			"closed by </" + name.Local + "> in space " + name.Space)
		return false
	}

	// Pop stack until a Start is on the top, undoing the
	// translations that were associated with the element we just closed.
	for p.stk != nil && p.stk.kind != stkStart {
		s := p.pop()
		p.ns[s.name.Local] = s.name.Space, s.ok
	}

	return true
}

// If the top element on the stack is autoclosing and
// t is not the end tag, invent the end tag.
func (p *Parser) autoClose(t Token) (Token, bool) {
	if p.stk == nil || p.stk.kind != stkStart {
		return nil, false
	}
	name := strings.ToLower(p.stk.name.Local)
	for _, s := range p.AutoClose {
		if strings.ToLower(s) == name {
			// This one should be auto closed if t doesn't close it.
			et, ok := t.(EndElement)
			if !ok || et.Name.Local != name {
				return EndElement{p.stk.name}, true
			}
			break
		}
	}
	return nil, false
}


// RawToken is like Token but does not verify that
// start and end elements match and does not translate
// name space prefixes to their corresponding URLs.
func (p *Parser) RawToken() (Token, os.Error) {
	if p.err != nil {
		return nil, p.err
	}
	if p.needClose {
		// The last element we read was self-closing and
		// we returned just the StartElement half.
		// Return the EndElement half now.
		p.needClose = false
		return EndElement{p.toClose}, nil
	}

	b, ok := p.getc()
	if !ok {
		return nil, p.err
	}

	if b != '<' {
		// Text section.
		p.ungetc(b)
		data := p.text(-1, false)
		if data == nil {
			return nil, p.err
		}
		return CharData(data), nil
	}

	if b, ok = p.mustgetc(); !ok {
		return nil, p.err
	}
	switch b {
	case '/':
		// </: End element
		var name Name
		if name, ok = p.nsname(); !ok {
			if p.err == nil {
				p.err = p.syntaxError("expected element name after </")
			}
			return nil, p.err
		}
		p.space()
		if b, ok = p.mustgetc(); !ok {
			return nil, p.err
		}
		if b != '>' {
			p.err = p.syntaxError("invalid characters between </" + name.Local + " and >")
			return nil, p.err
		}
		return EndElement{name}, nil

	case '?':
		// <?: Processing instruction.
		// TODO(rsc): Should parse the <?xml declaration to make sure
		// the version is 1.0 and the encoding is UTF-8.
		var target string
		if target, ok = p.name(); !ok {
			if p.err == nil {
				p.err = p.syntaxError("expected target name after <?")
			}
			return nil, p.err
		}
		p.space()
		p.buf.Reset()
		var b0 byte
		for {
			if b, ok = p.mustgetc(); !ok {
				return nil, p.err
			}
			p.buf.WriteByte(b)
			if b0 == '?' && b == '>' {
				break
			}
			b0 = b
		}
		data := p.buf.Bytes()
		data = data[0 : len(data)-2] // chop ?>
		return ProcInst{target, data}, nil

	case '!':
		// <!: Maybe comment, maybe CDATA.
		if b, ok = p.mustgetc(); !ok {
			return nil, p.err
		}
		switch b {
		case '-': // <!-
			// Probably <!-- for a comment.
			if b, ok = p.mustgetc(); !ok {
				return nil, p.err
			}
			if b != '-' {
				p.err = p.syntaxError("invalid sequence <!- not part of <!--")
				return nil, p.err
			}
			// Look for terminator.
			p.buf.Reset()
			var b0, b1 byte
			for {
				if b, ok = p.mustgetc(); !ok {
					return nil, p.err
				}
				p.buf.WriteByte(b)
				if b0 == '-' && b1 == '-' && b == '>' {
					break
				}
				b0, b1 = b1, b
			}
			data := p.buf.Bytes()
			data = data[0 : len(data)-3] // chop -->
			return Comment(data), nil

		case '[': // <![
			// Probably <![CDATA[.
			for i := 0; i < 6; i++ {
				if b, ok = p.mustgetc(); !ok {
					return nil, p.err
				}
				if b != "CDATA["[i] {
					p.err = p.syntaxError("invalid <![ sequence")
					return nil, p.err
				}
			}
			// Have <![CDATA[.  Read text until ]]>.
			data := p.text(-1, true)
			if data == nil {
				return nil, p.err
			}
			return CharData(data), nil
		}

		// Probably a directive: <!DOCTYPE ...>, <!ENTITY ...>, etc.
		// We don't care, but accumulate for caller.
		p.buf.Reset()
		p.buf.WriteByte(b)
		for {
			if b, ok = p.mustgetc(); !ok {
				return nil, p.err
			}
			if b == '>' {
				break
			}
			p.buf.WriteByte(b)
		}
		return Directive(p.buf.Bytes()), nil
	}

	// Must be an open element like <a href="foo">
	p.ungetc(b)

	var (
		name  Name
		empty bool
		attr  []Attr
	)
	if name, ok = p.nsname(); !ok {
		if p.err == nil {
			p.err = p.syntaxError("expected element name after <")
		}
		return nil, p.err
	}

	attr = make([]Attr, 0, 4)
	for {
		p.space()
		if b, ok = p.mustgetc(); !ok {
			return nil, p.err
		}
		if b == '/' {
			empty = true
			if b, ok = p.mustgetc(); !ok {
				return nil, p.err
			}
			if b != '>' {
				p.err = p.syntaxError("expected /> in element")
				return nil, p.err
			}
			break
		}
		if b == '>' {
			break
		}
		p.ungetc(b)

		n := len(attr)
		if n >= cap(attr) {
			nattr := make([]Attr, n, 2*cap(attr))
			copy(nattr, attr)
			attr = nattr
		}
		attr = attr[0 : n+1]
		a := &attr[n]
		if a.Name, ok = p.nsname(); !ok {
			if p.err == nil {
				p.err = p.syntaxError("expected attribute name in element")
			}
			return nil, p.err
		}
		p.space()
		if b, ok = p.mustgetc(); !ok {
			return nil, p.err
		}
		if b != '=' {
			p.err = p.syntaxError("attribute name without = in element")
			return nil, p.err
		}
		p.space()
		data := p.attrval()
		if data == nil {
			return nil, p.err
		}
		a.Value = string(data)
	}

	if empty {
		p.needClose = true
		p.toClose = name
	}
	return StartElement{name, attr}, nil
}

func (p *Parser) attrval() []byte {
	b, ok := p.mustgetc()
	if !ok {
		return nil
	}
	// Handle quoted attribute values
	if b == '"' || b == '\'' {
		return p.text(int(b), false)
	}
	// Handle unquoted attribute values for strict parsers
	if p.Strict {
		p.err = p.syntaxError("unquoted or missing attribute value in element")
		return nil
	}
	// Handle unquoted attribute values for unstrict parsers
	p.ungetc(b)
	p.buf.Reset()
	for {
		b, ok = p.mustgetc()
		if !ok {
			return nil
		}
		// http://www.w3.org/TR/REC-html40/intro/sgmltut.html#h-3.2.2
		if 'a' <= b && b <= 'z' || 'A' <= b && b <= 'Z' ||
			'0' <= b && b <= '9' || b == '_' || b == ':' || b == '-' {
			p.buf.WriteByte(b)
		} else {
			p.ungetc(b)
			break
		}
	}
	return p.buf.Bytes()
}

// Skip spaces if any
func (p *Parser) space() {
	for {
		b, ok := p.getc()
		if !ok {
			return
		}
		switch b {
		case ' ', '\r', '\n', '\t':
		default:
			p.ungetc(b)
			return
		}
	}
}

// Read a single byte.
// If there is no byte to read, return ok==false
// and leave the error in p.err.
// Maintain line number.
func (p *Parser) getc() (b byte, ok bool) {
	if p.err != nil {
		return 0, false
	}
	if p.nextByte >= 0 {
		b = byte(p.nextByte)
		p.nextByte = -1
	} else {
		b, p.err = p.r.ReadByte()
		if p.err != nil {
			return 0, false
		}
		if p.saved != nil {
			p.saved.WriteByte(b)
		}
	}
	if b == '\n' {
		p.line++
	}
	return b, true
}

// Return saved offset.
// If we did ungetc (nextByte >= 0), have to back up one.
func (p *Parser) savedOffset() int {
	n := p.saved.Len()
	if p.nextByte >= 0 {
		n--
	}
	return n
}

// Must read a single byte.
// If there is no byte to read,
// set p.err to SyntaxError("unexpected EOF")
// and return ok==false
func (p *Parser) mustgetc() (b byte, ok bool) {
	if b, ok = p.getc(); !ok {
		if p.err == os.EOF {
			p.err = p.syntaxError("unexpected EOF")
		}
	}
	return
}

// Unread a single byte.
func (p *Parser) ungetc(b byte) {
	if b == '\n' {
		p.line--
	}
	p.nextByte = int(b)
}

var entity = map[string]int{
	"lt":   '<',
	"gt":   '>',
	"amp":  '&',
	"apos": '\'',
	"quot": '"',
}

// Read plain text section (XML calls it character data).
// If quote >= 0, we are in a quoted string and need to find the matching quote.
// If cdata == true, we are in a <![CDATA[ section and need to find ]]>.
// On failure return nil and leave the error in p.err.
func (p *Parser) text(quote int, cdata bool) []byte {
	var b0, b1 byte
	var trunc int
	p.buf.Reset()
Input:
	for {
		b, ok := p.getc()
		if !ok {
			if cdata {
				if p.err == os.EOF {
					p.err = p.syntaxError("unexpected EOF in CDATA section")
				}
				return nil
			}
			break Input
		}

		// <![CDATA[ section ends with ]]>.
		// It is an error for ]]> to appear in ordinary text.
		if b0 == ']' && b1 == ']' && b == '>' {
			if cdata {
				trunc = 2
				break Input
			}
			p.err = p.syntaxError("unescaped ]]> not in CDATA section")
			return nil
		}

		// Stop reading text if we see a <.
		if b == '<' && !cdata {
			if quote >= 0 {
				p.err = p.syntaxError("unescaped < inside quoted string")
				return nil
			}
			p.ungetc('<')
			break Input
		}
		if quote >= 0 && b == byte(quote) {
			break Input
		}
		if b == '&' && !cdata {
			// Read escaped character expression up to semicolon.
			// XML in all its glory allows a document to define and use
			// its own character names with <!ENTITY ...> directives.
			// Parsers are required to recognize lt, gt, amp, apos, and quot
			// even if they have not been declared.  That's all we allow.
			var i int
		CharLoop:
			for i = 0; i < len(p.tmp); i++ {
				var ok bool
				p.tmp[i], ok = p.getc()
				if !ok {
					if p.err == os.EOF {
						p.err = p.syntaxError("unexpected EOF")
					}
					return nil
				}
				c := p.tmp[i]
				if c == ';' {
					break
				}
				if 'a' <= c && c <= 'z' ||
					'A' <= c && c <= 'Z' ||
					'0' <= c && c <= '9' ||
					c == '_' || c == '#' {
					continue
				}
				p.ungetc(c)
				break
			}
			s := string(p.tmp[0:i])
			if i >= len(p.tmp) {
				if !p.Strict {
					b0, b1 = 0, 0
					p.buf.WriteByte('&')
					p.buf.Write(p.tmp[0:i])
					continue Input
				}
				p.err = p.syntaxError("character entity expression &" + s + "... too long")
				return nil
			}
			var haveText bool
			var text string
			if i >= 2 && s[0] == '#' {
				var n uint64
				var err os.Error
				if i >= 3 && s[1] == 'x' {
					n, err = strconv.Btoui64(s[2:], 16)
				} else {
					n, err = strconv.Btoui64(s[1:], 10)
				}
				if err == nil && n <= unicode.MaxRune {
					text = string(n)
					haveText = true
				}
			} else {
				if r, ok := entity[s]; ok {
					text = string(r)
					haveText = true
				} else if p.Entity != nil {
					text, haveText = p.Entity[s]
				}
			}
			if !haveText {
				if !p.Strict {
					b0, b1 = 0, 0
					p.buf.WriteByte('&')
					p.buf.Write(p.tmp[0:i])
					continue Input
				}
				p.err = p.syntaxError("invalid character entity &" + s + ";")
				return nil
			}
			p.buf.Write([]byte(text))
			b0, b1 = 0, 0
			continue Input
		}
		p.buf.WriteByte(b)
		b0, b1 = b1, b
	}
	data := p.buf.Bytes()
	data = data[0 : len(data)-trunc]

	// Inspect each rune for being a disallowed character.
	buf := data
	for len(buf) > 0 {
		r, size := utf8.DecodeRune(buf)
		if r == utf8.RuneError && size == 1 {
			p.err = p.syntaxError("invalid UTF-8")
			return nil
		}
		buf = buf[size:]
		if !isInCharacterRange(r) {
			p.err = p.syntaxError(fmt.Sprintf("illegal character code %U", r))
			return nil
		}
	}

	// Must rewrite \r and \r\n into \n.
	w := 0
	for r := 0; r < len(data); r++ {
		b := data[r]
		if b == '\r' {
			if r+1 < len(data) && data[r+1] == '\n' {
				continue
			}
			b = '\n'
		}
		data[w] = b
		w++
	}
	return data[0:w]
}

// Decide whether the given rune is in the XML Character Range, per
// the Char production of http://www.xml.com/axml/testaxml.htm,
// Section 2.2 Characters.
func isInCharacterRange(rune int) (inrange bool) {
	return rune == 0x09 ||
		rune == 0x0A ||
		rune == 0x0D ||
		rune >= 0x20 && rune <= 0xDF77 ||
		rune >= 0xE000 && rune <= 0xFFFD ||
		rune >= 0x10000 && rune <= 0x10FFFF
}

// Get name space name: name with a : stuck in the middle.
// The part before the : is the name space identifier.
func (p *Parser) nsname() (name Name, ok bool) {
	s, ok := p.name()
	if !ok {
		return
	}
	i := strings.Index(s, ":")
	if i < 0 {
		name.Local = s
	} else {
		name.Space = s[0:i]
		name.Local = s[i+1:]
	}
	return name, true
}

// Get name: /first(first|second)*/
// Do not set p.err if the name is missing (unless unexpected EOF is received):
// let the caller provide better context.
func (p *Parser) name() (s string, ok bool) {
	var b byte
	if b, ok = p.mustgetc(); !ok {
		return
	}

	// As a first approximation, we gather the bytes [A-Za-z_:.-\x80-\xFF]*
	if b < utf8.RuneSelf && !isNameByte(b) {
		p.ungetc(b)
		return "", false
	}
	p.buf.Reset()
	p.buf.WriteByte(b)
	for {
		if b, ok = p.mustgetc(); !ok {
			return
		}
		if b < utf8.RuneSelf && !isNameByte(b) {
			p.ungetc(b)
			break
		}
		p.buf.WriteByte(b)
	}

	// Then we check the characters.
	s = p.buf.String()
	for i, c := range s {
		if !unicode.Is(first, c) && (i == 0 || !unicode.Is(second, c)) {
			p.err = p.syntaxError("invalid XML name: " + s)
			return "", false
		}
	}
	return s, true
}

func isNameByte(c byte) bool {
	return 'A' <= c && c <= 'Z' ||
		'a' <= c && c <= 'z' ||
		'0' <= c && c <= '9' ||
		c == '_' || c == ':' || c == '.' || c == '-'
}

// These tables were generated by cut and paste from Appendix B of
// the XML spec at http://www.xml.com/axml/testaxml.htm
// and then reformatting.  First corresponds to (Letter | '_' | ':')
// and second corresponds to NameChar.

var first = []unicode.Range{
	{0x003A, 0x003A, 1},
	{0x0041, 0x005A, 1},
	{0x005F, 0x005F, 1},
	{0x0061, 0x007A, 1},
	{0x00C0, 0x00D6, 1},
	{0x00D8, 0x00F6, 1},
	{0x00F8, 0x00FF, 1},
	{0x0100, 0x0131, 1},
	{0x0134, 0x013E, 1},
	{0x0141, 0x0148, 1},
	{0x014A, 0x017E, 1},
	{0x0180, 0x01C3, 1},
	{0x01CD, 0x01F0, 1},
	{0x01F4, 0x01F5, 1},
	{0x01FA, 0x0217, 1},
	{0x0250, 0x02A8, 1},
	{0x02BB, 0x02C1, 1},
	{0x0386, 0x0386, 1},
	{0x0388, 0x038A, 1},
	{0x038C, 0x038C, 1},
	{0x038E, 0x03A1, 1},
	{0x03A3, 0x03CE, 1},
	{0x03D0, 0x03D6, 1},
	{0x03DA, 0x03E0, 2},
	{0x03E2, 0x03F3, 1},
	{0x0401, 0x040C, 1},
	{0x040E, 0x044F, 1},
	{0x0451, 0x045C, 1},
	{0x045E, 0x0481, 1},
	{0x0490, 0x04C4, 1},
	{0x04C7, 0x04C8, 1},
	{0x04CB, 0x04CC, 1},
	{0x04D0, 0x04EB, 1},
	{0x04EE, 0x04F5, 1},
	{0x04F8, 0x04F9, 1},
	{0x0531, 0x0556, 1},
	{0x0559, 0x0559, 1},
	{0x0561, 0x0586, 1},
	{0x05D0, 0x05EA, 1},
	{0x05F0, 0x05F2, 1},
	{0x0621, 0x063A, 1},
	{0x0641, 0x064A, 1},
	{0x0671, 0x06B7, 1},
	{0x06BA, 0x06BE, 1},
	{0x06C0, 0x06CE, 1},
	{0x06D0, 0x06D3, 1},
	{0x06D5, 0x06D5, 1},
	{0x06E5, 0x06E6, 1},
	{0x0905, 0x0939, 1},
	{0x093D, 0x093D, 1},
	{0x0958, 0x0961, 1},
	{0x0985, 0x098C, 1},
	{0x098F, 0x0990, 1},
	{0x0993, 0x09A8, 1},
	{0x09AA, 0x09B0, 1},
	{0x09B2, 0x09B2, 1},
	{0x09B6, 0x09B9, 1},
	{0x09DC, 0x09DD, 1},
	{0x09DF, 0x09E1, 1},
	{0x09F0, 0x09F1, 1},
	{0x0A05, 0x0A0A, 1},
	{0x0A0F, 0x0A10, 1},
	{0x0A13, 0x0A28, 1},
	{0x0A2A, 0x0A30, 1},
	{0x0A32, 0x0A33, 1},
	{0x0A35, 0x0A36, 1},
	{0x0A38, 0x0A39, 1},
	{0x0A59, 0x0A5C, 1},
	{0x0A5E, 0x0A5E, 1},
	{0x0A72, 0x0A74, 1},
	{0x0A85, 0x0A8B, 1},
	{0x0A8D, 0x0A8D, 1},
	{0x0A8F, 0x0A91, 1},
	{0x0A93, 0x0AA8, 1},
	{0x0AAA, 0x0AB0, 1},
	{0x0AB2, 0x0AB3, 1},
	{0x0AB5, 0x0AB9, 1},
	{0x0ABD, 0x0AE0, 0x23},
	{0x0B05, 0x0B0C, 1},
	{0x0B0F, 0x0B10, 1},
	{0x0B13, 0x0B28, 1},
	{0x0B2A, 0x0B30, 1},
	{0x0B32, 0x0B33, 1},
	{0x0B36, 0x0B39, 1},
	{0x0B3D, 0x0B3D, 1},
	{0x0B5C, 0x0B5D, 1},
	{0x0B5F, 0x0B61, 1},
	{0x0B85, 0x0B8A, 1},
	{0x0B8E, 0x0B90, 1},
	{0x0B92, 0x0B95, 1},
	{0x0B99, 0x0B9A, 1},
	{0x0B9C, 0x0B9C, 1},
	{0x0B9E, 0x0B9F, 1},
	{0x0BA3, 0x0BA4, 1},
	{0x0BA8, 0x0BAA, 1},
	{0x0BAE, 0x0BB5, 1},
	{0x0BB7, 0x0BB9, 1},
	{0x0C05, 0x0C0C, 1},
	{0x0C0E, 0x0C10, 1},
	{0x0C12, 0x0C28, 1},
	{0x0C2A, 0x0C33, 1},
	{0x0C35, 0x0C39, 1},
	{0x0C60, 0x0C61, 1},
	{0x0C85, 0x0C8C, 1},
	{0x0C8E, 0x0C90, 1},
	{0x0C92, 0x0CA8, 1},
	{0x0CAA, 0x0CB3, 1},
	{0x0CB5, 0x0CB9, 1},
	{0x0CDE, 0x0CDE, 1},
	{0x0CE0, 0x0CE1, 1},
	{0x0D05, 0x0D0C, 1},
	{0x0D0E, 0x0D10, 1},
	{0x0D12, 0x0D28, 1},
	{0x0D2A, 0x0D39, 1},
	{0x0D60, 0x0D61, 1},
	{0x0E01, 0x0E2E, 1},
	{0x0E30, 0x0E30, 1},
	{0x0E32, 0x0E33, 1},
	{0x0E40, 0x0E45, 1},
	{0x0E81, 0x0E82, 1},
	{0x0E84, 0x0E84, 1},
	{0x0E87, 0x0E88, 1},
	{0x0E8A, 0x0E8D, 3},
	{0x0E94, 0x0E97, 1},
	{0x0E99, 0x0E9F, 1},
	{0x0EA1, 0x0EA3, 1},
	{0x0EA5, 0x0EA7, 2},
	{0x0EAA, 0x0EAB, 1},
	{0x0EAD, 0x0EAE, 1},
	{0x0EB0, 0x0EB0, 1},
	{0x0EB2, 0x0EB3, 1},
	{0x0EBD, 0x0EBD, 1},
	{0x0EC0, 0x0EC4, 1},
	{0x0F40, 0x0F47, 1},
	{0x0F49, 0x0F69, 1},
	{0x10A0, 0x10C5, 1},
	{0x10D0, 0x10F6, 1},
	{0x1100, 0x1100, 1},
	{0x1102, 0x1103, 1},
	{0x1105, 0x1107, 1},
	{0x1109, 0x1109, 1},
	{0x110B, 0x110C, 1},
	{0x110E, 0x1112, 1},
	{0x113C, 0x1140, 2},
	{0x114C, 0x1150, 2},
	{0x1154, 0x1155, 1},
	{0x1159, 0x1159, 1},
	{0x115F, 0x1161, 1},
	{0x1163, 0x1169, 2},
	{0x116D, 0x116E, 1},
	{0x1172, 0x1173, 1},
	{0x1175, 0x119E, 0x119E - 0x1175},
	{0x11A8, 0x11AB, 0x11AB - 0x11A8},
	{0x11AE, 0x11AF, 1},
	{0x11B7, 0x11B8, 1},
	{0x11BA, 0x11BA, 1},
	{0x11BC, 0x11C2, 1},
	{0x11EB, 0x11F0, 0x11F0 - 0x11EB},
	{0x11F9, 0x11F9, 1},
	{0x1E00, 0x1E9B, 1},
	{0x1EA0, 0x1EF9, 1},
	{0x1F00, 0x1F15, 1},
	{0x1F18, 0x1F1D, 1},
	{0x1F20, 0x1F45, 1},
	{0x1F48, 0x1F4D, 1},
	{0x1F50, 0x1F57, 1},
	{0x1F59, 0x1F5B, 0x1F5B - 0x1F59},
	{0x1F5D, 0x1F5D, 1},
	{0x1F5F, 0x1F7D, 1},
	{0x1F80, 0x1FB4, 1},
	{0x1FB6, 0x1FBC, 1},
	{0x1FBE, 0x1FBE, 1},
	{0x1FC2, 0x1FC4, 1},
	{0x1FC6, 0x1FCC, 1},
	{0x1FD0, 0x1FD3, 1},
	{0x1FD6, 0x1FDB, 1},
	{0x1FE0, 0x1FEC, 1},
	{0x1FF2, 0x1FF4, 1},
	{0x1FF6, 0x1FFC, 1},
	{0x2126, 0x2126, 1},
	{0x212A, 0x212B, 1},
	{0x212E, 0x212E, 1},
	{0x2180, 0x2182, 1},
	{0x3007, 0x3007, 1},
	{0x3021, 0x3029, 1},
	{0x3041, 0x3094, 1},
	{0x30A1, 0x30FA, 1},
	{0x3105, 0x312C, 1},
	{0x4E00, 0x9FA5, 1},
	{0xAC00, 0xD7A3, 1},
}

var second = []unicode.Range{
	{0x002D, 0x002E, 1},
	{0x0030, 0x0039, 1},
	{0x00B7, 0x00B7, 1},
	{0x02D0, 0x02D1, 1},
	{0x0300, 0x0345, 1},
	{0x0360, 0x0361, 1},
	{0x0387, 0x0387, 1},
	{0x0483, 0x0486, 1},
	{0x0591, 0x05A1, 1},
	{0x05A3, 0x05B9, 1},
	{0x05BB, 0x05BD, 1},
	{0x05BF, 0x05BF, 1},
	{0x05C1, 0x05C2, 1},
	{0x05C4, 0x0640, 0x0640 - 0x05C4},
	{0x064B, 0x0652, 1},
	{0x0660, 0x0669, 1},
	{0x0670, 0x0670, 1},
	{0x06D6, 0x06DC, 1},
	{0x06DD, 0x06DF, 1},
	{0x06E0, 0x06E4, 1},
	{0x06E7, 0x06E8, 1},
	{0x06EA, 0x06ED, 1},
	{0x06F0, 0x06F9, 1},
	{0x0901, 0x0903, 1},
	{0x093C, 0x093C, 1},
	{0x093E, 0x094C, 1},
	{0x094D, 0x094D, 1},
	{0x0951, 0x0954, 1},
	{0x0962, 0x0963, 1},
	{0x0966, 0x096F, 1},
	{0x0981, 0x0983, 1},
	{0x09BC, 0x09BC, 1},
	{0x09BE, 0x09BF, 1},
	{0x09C0, 0x09C4, 1},
	{0x09C7, 0x09C8, 1},
	{0x09CB, 0x09CD, 1},
	{0x09D7, 0x09D7, 1},
	{0x09E2, 0x09E3, 1},
	{0x09E6, 0x09EF, 1},
	{0x0A02, 0x0A3C, 0x3A},
	{0x0A3E, 0x0A3F, 1},
	{0x0A40, 0x0A42, 1},
	{0x0A47, 0x0A48, 1},
	{0x0A4B, 0x0A4D, 1},
	{0x0A66, 0x0A6F, 1},
	{0x0A70, 0x0A71, 1},
	{0x0A81, 0x0A83, 1},
	{0x0ABC, 0x0ABC, 1},
	{0x0ABE, 0x0AC5, 1},
	{0x0AC7, 0x0AC9, 1},
	{0x0ACB, 0x0ACD, 1},
	{0x0AE6, 0x0AEF, 1},
	{0x0B01, 0x0B03, 1},
	{0x0B3C, 0x0B3C, 1},
	{0x0B3E, 0x0B43, 1},
	{0x0B47, 0x0B48, 1},
	{0x0B4B, 0x0B4D, 1},
	{0x0B56, 0x0B57, 1},
	{0x0B66, 0x0B6F, 1},
	{0x0B82, 0x0B83, 1},
	{0x0BBE, 0x0BC2, 1},
	{0x0BC6, 0x0BC8, 1},
	{0x0BCA, 0x0BCD, 1},
	{0x0BD7, 0x0BD7, 1},
	{0x0BE7, 0x0BEF, 1},
	{0x0C01, 0x0C03, 1},
	{0x0C3E, 0x0C44, 1},
	{0x0C46, 0x0C48, 1},
	{0x0C4A, 0x0C4D, 1},
	{0x0C55, 0x0C56, 1},
	{0x0C66, 0x0C6F, 1},
	{0x0C82, 0x0C83, 1},
	{0x0CBE, 0x0CC4, 1},
	{0x0CC6, 0x0CC8, 1},
	{0x0CCA, 0x0CCD, 1},
	{0x0CD5, 0x0CD6, 1},
	{0x0CE6, 0x0CEF, 1},
	{0x0D02, 0x0D03, 1},
	{0x0D3E, 0x0D43, 1},
	{0x0D46, 0x0D48, 1},
	{0x0D4A, 0x0D4D, 1},
	{0x0D57, 0x0D57, 1},
	{0x0D66, 0x0D6F, 1},
	{0x0E31, 0x0E31, 1},
	{0x0E34, 0x0E3A, 1},
	{0x0E46, 0x0E46, 1},
	{0x0E47, 0x0E4E, 1},
	{0x0E50, 0x0E59, 1},
	{0x0EB1, 0x0EB1, 1},
	{0x0EB4, 0x0EB9, 1},
	{0x0EBB, 0x0EBC, 1},
	{0x0EC6, 0x0EC6, 1},
	{0x0EC8, 0x0ECD, 1},
	{0x0ED0, 0x0ED9, 1},
	{0x0F18, 0x0F19, 1},
	{0x0F20, 0x0F29, 1},
	{0x0F35, 0x0F39, 2},
	{0x0F3E, 0x0F3F, 1},
	{0x0F71, 0x0F84, 1},
	{0x0F86, 0x0F8B, 1},
	{0x0F90, 0x0F95, 1},
	{0x0F97, 0x0F97, 1},
	{0x0F99, 0x0FAD, 1},
	{0x0FB1, 0x0FB7, 1},
	{0x0FB9, 0x0FB9, 1},
	{0x20D0, 0x20DC, 1},
	{0x20E1, 0x3005, 0x3005 - 0x20E1},
	{0x302A, 0x302F, 1},
	{0x3031, 0x3035, 1},
	{0x3099, 0x309A, 1},
	{0x309D, 0x309E, 1},
	{0x30FC, 0x30FE, 1},
}

// HTMLEntity is an entity map containing translations for the
// standard HTML entity characters.
var HTMLEntity = htmlEntity

var htmlEntity = map[string]string{
	/*
		hget http://www.w3.org/TR/html4/sgml/entities.html |
		ssam '
			,y /\&gt;/ x/\&lt;(.|\n)+/ s/\n/ /g
			,x v/^\&lt;!ENTITY/d
			,s/\&lt;!ENTITY ([^ ]+) .*U\+([0-9A-F][0-9A-F][0-9A-F][0-9A-F]) .+/	"\1": "\\u\2",/g
		'
	*/
	"nbsp":     "\u00A0",
	"iexcl":    "\u00A1",
	"cent":     "\u00A2",
	"pound":    "\u00A3",
	"curren":   "\u00A4",
	"yen":      "\u00A5",
	"brvbar":   "\u00A6",
	"sect":     "\u00A7",
	"uml":      "\u00A8",
	"copy":     "\u00A9",
	"ordf":     "\u00AA",
	"laquo":    "\u00AB",
	"not":      "\u00AC",
	"shy":      "\u00AD",
	"reg":      "\u00AE",
	"macr":     "\u00AF",
	"deg":      "\u00B0",
	"plusmn":   "\u00B1",
	"sup2":     "\u00B2",
	"sup3":     "\u00B3",
	"acute":    "\u00B4",
	"micro":    "\u00B5",
	"para":     "\u00B6",
	"middot":   "\u00B7",
	"cedil":    "\u00B8",
	"sup1":     "\u00B9",
	"ordm":     "\u00BA",
	"raquo":    "\u00BB",
	"frac14":   "\u00BC",
	"frac12":   "\u00BD",
	"frac34":   "\u00BE",
	"iquest":   "\u00BF",
	"Agrave":   "\u00C0",
	"Aacute":   "\u00C1",
	"Acirc":    "\u00C2",
	"Atilde":   "\u00C3",
	"Auml":     "\u00C4",
	"Aring":    "\u00C5",
	"AElig":    "\u00C6",
	"Ccedil":   "\u00C7",
	"Egrave":   "\u00C8",
	"Eacute":   "\u00C9",
	"Ecirc":    "\u00CA",
	"Euml":     "\u00CB",
	"Igrave":   "\u00CC",
	"Iacute":   "\u00CD",
	"Icirc":    "\u00CE",
	"Iuml":     "\u00CF",
	"ETH":      "\u00D0",
	"Ntilde":   "\u00D1",
	"Ograve":   "\u00D2",
	"Oacute":   "\u00D3",
	"Ocirc":    "\u00D4",
	"Otilde":   "\u00D5",
	"Ouml":     "\u00D6",
	"times":    "\u00D7",
	"Oslash":   "\u00D8",
	"Ugrave":   "\u00D9",
	"Uacute":   "\u00DA",
	"Ucirc":    "\u00DB",
	"Uuml":     "\u00DC",
	"Yacute":   "\u00DD",
	"THORN":    "\u00DE",
	"szlig":    "\u00DF",
	"agrave":   "\u00E0",
	"aacute":   "\u00E1",
	"acirc":    "\u00E2",
	"atilde":   "\u00E3",
	"auml":     "\u00E4",
	"aring":    "\u00E5",
	"aelig":    "\u00E6",
	"ccedil":   "\u00E7",
	"egrave":   "\u00E8",
	"eacute":   "\u00E9",
	"ecirc":    "\u00EA",
	"euml":     "\u00EB",
	"igrave":   "\u00EC",
	"iacute":   "\u00ED",
	"icirc":    "\u00EE",
	"iuml":     "\u00EF",
	"eth":      "\u00F0",
	"ntilde":   "\u00F1",
	"ograve":   "\u00F2",
	"oacute":   "\u00F3",
	"ocirc":    "\u00F4",
	"otilde":   "\u00F5",
	"ouml":     "\u00F6",
	"divide":   "\u00F7",
	"oslash":   "\u00F8",
	"ugrave":   "\u00F9",
	"uacute":   "\u00FA",
	"ucirc":    "\u00FB",
	"uuml":     "\u00FC",
	"yacute":   "\u00FD",
	"thorn":    "\u00FE",
	"yuml":     "\u00FF",
	"fnof":     "\u0192",
	"Alpha":    "\u0391",
	"Beta":     "\u0392",
	"Gamma":    "\u0393",
	"Delta":    "\u0394",
	"Epsilon":  "\u0395",
	"Zeta":     "\u0396",
	"Eta":      "\u0397",
	"Theta":    "\u0398",
	"Iota":     "\u0399",
	"Kappa":    "\u039A",
	"Lambda":   "\u039B",
	"Mu":       "\u039C",
	"Nu":       "\u039D",
	"Xi":       "\u039E",
	"Omicron":  "\u039F",
	"Pi":       "\u03A0",
	"Rho":      "\u03A1",
	"Sigma":    "\u03A3",
	"Tau":      "\u03A4",
	"Upsilon":  "\u03A5",
	"Phi":      "\u03A6",
	"Chi":      "\u03A7",
	"Psi":      "\u03A8",
	"Omega":    "\u03A9",
	"alpha":    "\u03B1",
	"beta":     "\u03B2",
	"gamma":    "\u03B3",
	"delta":    "\u03B4",
	"epsilon":  "\u03B5",
	"zeta":     "\u03B6",
	"eta":      "\u03B7",
	"theta":    "\u03B8",
	"iota":     "\u03B9",
	"kappa":    "\u03BA",
	"lambda":   "\u03BB",
	"mu":       "\u03BC",
	"nu":       "\u03BD",
	"xi":       "\u03BE",
	"omicron":  "\u03BF",
	"pi":       "\u03C0",
	"rho":      "\u03C1",
	"sigmaf":   "\u03C2",
	"sigma":    "\u03C3",
	"tau":      "\u03C4",
	"upsilon":  "\u03C5",
	"phi":      "\u03C6",
	"chi":      "\u03C7",
	"psi":      "\u03C8",
	"omega":    "\u03C9",
	"thetasym": "\u03D1",
	"upsih":    "\u03D2",
	"piv":      "\u03D6",
	"bull":     "\u2022",
	"hellip":   "\u2026",
	"prime":    "\u2032",
	"Prime":    "\u2033",
	"oline":    "\u203E",
	"frasl":    "\u2044",
	"weierp":   "\u2118",
	"image":    "\u2111",
	"real":     "\u211C",
	"trade":    "\u2122",
	"alefsym":  "\u2135",
	"larr":     "\u2190",
	"uarr":     "\u2191",
	"rarr":     "\u2192",
	"darr":     "\u2193",
	"harr":     "\u2194",
	"crarr":    "\u21B5",
	"lArr":     "\u21D0",
	"uArr":     "\u21D1",
	"rArr":     "\u21D2",
	"dArr":     "\u21D3",
	"hArr":     "\u21D4",
	"forall":   "\u2200",
	"part":     "\u2202",
	"exist":    "\u2203",
	"empty":    "\u2205",
	"nabla":    "\u2207",
	"isin":     "\u2208",
	"notin":    "\u2209",
	"ni":       "\u220B",
	"prod":     "\u220F",
	"sum":      "\u2211",
	"minus":    "\u2212",
	"lowast":   "\u2217",
	"radic":    "\u221A",
	"prop":     "\u221D",
	"infin":    "\u221E",
	"ang":      "\u2220",
	"and":      "\u2227",
	"or":       "\u2228",
	"cap":      "\u2229",
	"cup":      "\u222A",
	"int":      "\u222B",
	"there4":   "\u2234",
	"sim":      "\u223C",
	"cong":     "\u2245",
	"asymp":    "\u2248",
	"ne":       "\u2260",
	"equiv":    "\u2261",
	"le":       "\u2264",
	"ge":       "\u2265",
	"sub":      "\u2282",
	"sup":      "\u2283",
	"nsub":     "\u2284",
	"sube":     "\u2286",
	"supe":     "\u2287",
	"oplus":    "\u2295",
	"otimes":   "\u2297",
	"perp":     "\u22A5",
	"sdot":     "\u22C5",
	"lceil":    "\u2308",
	"rceil":    "\u2309",
	"lfloor":   "\u230A",
	"rfloor":   "\u230B",
	"lang":     "\u2329",
	"rang":     "\u232A",
	"loz":      "\u25CA",
	"spades":   "\u2660",
	"clubs":    "\u2663",
	"hearts":   "\u2665",
	"diams":    "\u2666",
	"quot":     "\u0022",
	"amp":      "\u0026",
	"lt":       "\u003C",
	"gt":       "\u003E",
	"OElig":    "\u0152",
	"oelig":    "\u0153",
	"Scaron":   "\u0160",
	"scaron":   "\u0161",
	"Yuml":     "\u0178",
	"circ":     "\u02C6",
	"tilde":    "\u02DC",
	"ensp":     "\u2002",
	"emsp":     "\u2003",
	"thinsp":   "\u2009",
	"zwnj":     "\u200C",
	"zwj":      "\u200D",
	"lrm":      "\u200E",
	"rlm":      "\u200F",
	"ndash":    "\u2013",
	"mdash":    "\u2014",
	"lsquo":    "\u2018",
	"rsquo":    "\u2019",
	"sbquo":    "\u201A",
	"ldquo":    "\u201C",
	"rdquo":    "\u201D",
	"bdquo":    "\u201E",
	"dagger":   "\u2020",
	"Dagger":   "\u2021",
	"permil":   "\u2030",
	"lsaquo":   "\u2039",
	"rsaquo":   "\u203A",
	"euro":     "\u20AC",
}

// HTMLAutoClose is the set of HTML elements that
// should be considered to close automatically.
var HTMLAutoClose = htmlAutoClose

var htmlAutoClose = []string{
	/*
		hget http://www.w3.org/TR/html4/loose.dtd |
		9 sed -n 's/<!ELEMENT (.*) - O EMPTY.+/	"\1",/p' | tr A-Z a-z
	*/
	"basefont",
	"br",
	"area",
	"link",
	"img",
	"param",
	"hr",
	"input",
	"col     ",
	"frame",
	"isindex",
	"base",
	"meta",
}

var (
	esc_quot = []byte("&#34;") // shorter than "&quot;"
	esc_apos = []byte("&#39;") // shorter than "&apos;"
	esc_amp  = []byte("&amp;")
	esc_lt   = []byte("&lt;")
	esc_gt   = []byte("&gt;")
)

// Escape writes to w the properly escaped XML equivalent
// of the plain text data s.
func Escape(w io.Writer, s []byte) {
	var esc []byte
	last := 0
	for i, c := range s {
		switch c {
		case '"':
			esc = esc_quot
		case '\'':
			esc = esc_apos
		case '&':
			esc = esc_amp
		case '<':
			esc = esc_lt
		case '>':
			esc = esc_gt
		default:
			continue
		}
		w.Write(s[last:i])
		w.Write(esc)
		last = i + 1
	}
	w.Write(s[last:])
}
