Trex is a simple instruction language expressed in s-expressions.

All identifiers are lowercased ASCII.
All numbers are uppercased hexadecimal.
All values are 32-bit integers.

There is one 32-bit accumulator register and a small stack of 32-bit values.

Interactive Trex sessions strictly follow a request-response protocol. A single request must always generate a single response, no more, no less.

State machines allow instructions to be executed in the background. Each state machine is created with a unique "name" (a 32-bit value) and a priority value.

Each state machine has a current state number. The state number determines which "state handler" is executed next. A state handler is simply a sequence of instructions to execute to handle a specific state. State handlers are never interrupted and must run to completion either by an explicit (ret) instruction or after the last instruction of the handler is executed.

There are no backwards branches in Trex and control always proceeds forward through a state handler.

Only one state machine can execute at a time. When control flow leaves a state handler, the next state machine to execute is chosen by the scheduler, and the state handler for that state machine is selected by its state number.

The state machine scheduler uses a priority queue with ageing to ensure lower priority machines get a chance to execute.

TODO:

* define register file and stack behavior
* define full instruction set - keep it very simple
* does each state machine get its own register file and stack?
* clarify state machine reset behavior between states - is stack cleared? are registers cleared?

Example interactive Trex session:

```lisp
> ; state-machine-create name "o2\0\0" priority 0
(smc 6F320000 0)
; this should create a new state machine with a 32-bit "name". if the machine already exists, all of its state handlers are cleared.
< (ack)

> ; state-machine-define-state
(smds 6F320000 0        ; state 0
    (cu   @NMIX)        ; chip-use NMIX
    (csa  0)            ; chip-set-address 0
    ; write this asm to NMIX handler:
    ; 2C00   9C 00 2C               STZ   $2C00
    ; 2C03   6C EA FF               JMP   ($FFEA)
    (cwbd 9C002C6C)     ; chip-write-big-endian-dword
    (cwbw EAFF)         ; chip-write-big-endian-word
    (csa  0)            ; chip-set-address 0
    (smgs 1)            ; state-machine-goto-state 1
)
< (ack)

> ; state-machine-define-state
(smds 6F320000 1        ; state 1
    ; if (chip-read-no-advance-byte) != 0 then (return)
    (crnb)      ; a = (chip-read-no-advance-byte)
    (jz 1)      ; jump if a is zero to +1 instructions from here
    (ret)       ; return
    ; NMI has fired!
    (cu @WRAM)  ; chip-use WRAM
    (csa 10)    ; chip-set-address 0x10
    (crld)      ; chip-read-little-endian-dword from WRAM
    (mald)      ; message-append-little-endian-dword
    (send)      ; send message
    (smgs 0)    ; state-machine-goto-state 0
)
< (ack)

> (smr 6F320000) ; state-machine-run
< (ack)

; recv polls if a message is available from any state-machine
> (recv)
; it returns (m name data...) if a message is available
< (m 6F320000 00000014)
; else it returns (nak) if no message available

> (sms) ; state-machine-stop
```
