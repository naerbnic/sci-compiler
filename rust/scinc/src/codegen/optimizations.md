# Optimization

## Peephole Optimizations

Optimizations done by C++ code

## PushI

- If ACC contains immediate value, change to Push
- If head of stack already has immediate value, change to dup
- If immediate in 0, 1, 2, change to single instruction

## Return

- [ret, ret] => \[ret]

## LoadI

- [loadi, push] => \[pushi]

## Branch

- [bX => bX A] => \[bX A]
- [bX => jmp A] => \[bX A]

## pToA

- [pToA, push] && clobbered(acc) => \[pTos]
- [pToA N] w/ ACC == prop N => \[]

## pTos

- [pToS N] w/ ACC == prop N => \[push]
- [pToS N] w/ STACK[0] == prop N => \[dup

## selfID

- [selfID, push] => \[pushSelf]
- [selfID, send] => \[self]

## Mem ops

- [push, mem<-stack] => \[mem<-acc]
- \[mem<-stack] w/ ACC == data at mem => \[]
- [mem->acc, push] w/ clobbered(ACC) => \[mem->stack]
- \[mem->acc\[i], push] w/ clobbered(ACC) => \[mem->stack\[i]]
- \[mem->acc] /w ACC == read value => \[]
