; Testing in-file includes
(include "test.sh")
(script# 0)
(global
  foo 0)

(procedure (bar x y) (+ foo BLAH))