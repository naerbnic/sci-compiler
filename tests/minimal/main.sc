; Testing in-file includes
(include "test.sh")
(script# 0)
(global
  foo 0 = 1)

(public
  bar 0)

(procedure (bar x y) (+ foo BLAH))