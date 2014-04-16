// Copyright 2010 The Go Authors.  All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

package math

// Make sqrtGo available for testing.

func SqrtGo(x float64) float64 { return sqrtGo(x) }
