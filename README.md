# Summary

Trex is a simple instruction language expressed in s-expressions.

# Design Goals

Trex is designed to execute efficiently in a single-threaded environment on a memory-constrained embedded microcontroller such as the ARM Cortex-M3 with as little as 32KiB of memory.

# State Machines

State machines allow instructions to be executed in the idle time between interactive request-response commands. Each state machine is created with a unique "name" (a 32-bit value) and a priority value.

Each state machine has a current state number. The state number determines which "state handler" is executed next. A state handler is simply a sequence of instructions to execute to handle a specific state. State handlers are never interrupted and must run to completion either by an explicit (ret) instruction or after the last instruction of the handler is executed.

There are no backwards branches in Trex and control always proceeds forward through a state handler.

Only one state machine can execute at a time. When control flow leaves a state handler, the next state machine to execute is chosen by the scheduler, and the state handler for that state machine is selected by its state number.

The state machine scheduler uses a priority queue with ageing to ensure lower priority machines get a chance to execute.

# Interactive Sessions

Interactive Trex sessions strictly follow a request-response protocol. A single request must always generate a single response, no more, no less.

# Language Specification

Trex language is a simple language where all code is expressed using a custom variation of `s-expression`s borrowed from the LISP family of languages.

An `s-expression`, or `sexpr` for short, is an expression that can be one of several things:

  * an identifier, which must be expressed in all lowercased ASCII alphabetic characters. very short identifiers are encouraged given the tight memory constraints.
  * a number, which must be expressed in uppercased hexadecimal with no leading or trailing signifier. there are no decimal formatted numbers in Trex. numbers are always 32-bit integers, either unsigned or signed depending on the usage.
  * a list of one or more `sexpr`s surrounded by `(` and `)`

Comments begin with a `;` and continue until a `\n`.

Whitespace consists of ASCII spaces, `\t`, and `\n`.

Whitespace may be used to separate `sexpr`s where parsing ambiguities arise.

TODO:

* define register file and stack behavior
* define full instruction set - keep it very simple
* does each state machine get its own register file and stack?
* clarify state machine reset behavior between states - is stack cleared? are registers cleared?

# Example
Example interactive Trex session:

```lisp
> ; state-machine-create with name="o2" with priority=0
(smc 6F32 0)
; create a new state machine with a 32-bit "name". if the machine already exists, all of its state handlers are cleared.
< (ack)

> ; state-machine-define-state
(smds 6F32 0            ; state 0
    (cu   nmix)         ; chip-use NMIX
    (csa  2C00)         ; chip-set-address $2C00
    ; write this asm to NMIX handler:
    ; 2C00   9C 00 2C   STZ   $2C00
    ; 2C03   6C EA FF   JMP   ($FFEA)
    (cwbd 9C002C6C)     ; chip-write-big-endian-dword
    (cwbw EAFF)         ; chip-write-big-endian-word
    (csa  2C00)         ; chip-set-address $2C00
    (s 1)               ; set-state 1
)
< (ack)

> ; state-machine-define-state
(smds 6F32 1    ; state 1
    (crnb)      ; a = (chip-read-no-advance-byte)
    (jz 1)      ; jump if a is zero to +1 instructions from here
    (ret)       ; else return
    ; NMI has fired!
    (cu wram)   ; chip-use WRAM
    (csa 10)    ; chip-set-address 0x10
    (crld)      ; chip-read-little-endian-dword from WRAM
    (mald)      ; message-append-little-endian-dword
    (send)      ; send message
    (s 0)       ; set-state 0
)
< (ack)

> (smr 6F32) ; state-machine-run
< (ack)

; recv polls if a message is available from any state-machine
> (recv)
; it returns (m name data...) if a message is available
< (m 6F32 00000014)
; else it returns (nak) if no message available

> (sms) ; state-machine-stop
< (ack)
```
