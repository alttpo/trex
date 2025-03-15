# Summary

Trex is a service for SNES devices to allow applications to upload small programs which can perform very low-latency memory access against SNES memory chips controlled via logic.

This approach is in contrast to the state-of-the-art approach of USB2SNES where applications make direct memory requests on a higher-latency USB connection and need to wait for the results before sending the next request.

The Trex approach allows applications to perform reads, react quickly to changes, and write back to SNES memory chips much faster than a USB connection allows with its 1-2ms polling interval. Trex also allows applications to synchronize timing with the SNES device using tight loops to read flags and send messages back. Trex also allows applications to scatter/gather memory reads and report data back to applications with greater efficiency.

Trex can be built into the firmware of the SNES FXPak Pro flash cart and also embedded into SNES emulators alike.

# Design Goals

Design Goals:

  * single-threaded execution
  * must run in memory constrained environment; 24KiB max memory usage at runtime (the FXPak Pro uses an ARM Cortex-M3 with 32KiB memory, the 1.11 firmware uses about 9KiB)
  * must compile with arm-none-eabi-gcc
  * must compile with clang
  * portable to embedded microcontrollers
  * portable to embed in SNES emulators
  * allow multiple applications to multiplex over a single USB connection in the case of FXPak Pro, fronted by a PC-side application which manages the USB connection and multiplexes it over multiple TCP or IPC communication channels to applications.
  * programs must execute concurrently within Trex

To allow multiple programs to execute concurrently, we'll need to ensure the programs satisfy several criteria:

  * no backwards branching to guarantee that programs always terminate.
  * allow small sections of programs to act as critical sections which are not interruptible.
  * be interruptible at known points of execution to allow cooperative scheduling with other programs.

State machines satisfy all of these criteria, so we'll define the programs that applications can upload in terms of state machines.

# State Machines

Each state machine is created with a unique "name" (a 32-bit value) and a priority value between 1 and 8 where 8 is the highest priority.

Each state machine has a current state number. The state number determines which "state handler" is executed next. A state handler is simply a sequence of instructions to execute. State handlers are never interrupted and always run to completion. State handlers must return the new state number to execute next for the state machine.

## Scheduling

Trex handles scheduling of state machines using scheduling iterations.

A scheduling iteration is an ordered sequence of execution slots where a state machine may execute a single state handler per slot.

When an iteration begins it is assigned a number of execution slots equal to the sum of the running state machines' priority values. Stopped state machines cannot be executed in the current iteration and if they are changed to a running state they will be executed in the next iteration.

To determine which state machines to execute in each slot, Trex uses a round-robin scheduling algorithm and orders state machines by descending priority.

Each state of a state machine can specify the minimum number of times the state should be scheduled consecutively. This is important for tight-loop states that are polling for a status change in SNES memory. These states can be scheduled to run for longer bursts than other states so there's a greater chance of detecting the status change sooner.

As an example we have two state machines A and B where A has priority=3 and B has priority=2, they would execute in an iteration of 5 slots like this:

```
ABABA
```

If state machine A's current state specified that it must run 2 times consecutively:

```
AABAB
```

# Messages

State machines can deliver messages back to applications. A message is an arbitrary binary payload tagged with the name of the state machine that produced it.

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

# Example
Example interactive Trex session:

```lisp
> (state-machine-create
    (name     6F32)
    (priority 8)
    (memory   4)
)
; creates a new state machine with a 32-bit "name" of $6F32. all of its state handlers are cleared and it is set to the stopped status.
< (ack)

> (state-machine-define-state
    (name  6F32)
    (state 0)
    (burst 0)
    (handler
        ; we write all the asm for 2C00 handler except the first byte and then enable it with the final write to 2C00:
        (chip-use nmix)
        (chip-address-set 1)
        ; write this 65816 asm to fxpak's NMI handler:
        ; 2C00   9C 00 2C   STZ   $2C00
        ; 2C03   6C EA FF   JMP   ($FFEA)
        (chip-write-dword 002C6CEA)
        (chip-write-advance-byte     FF)
        ; move to next state:
        (set-state 1)
        (return)
    ))
< (ack)

> (state-machine-define-state
    (name 6F32)
    (state 1)
    (burst 0)
    (handler
        ; write the first byte of the asm routine to enable it:
        (chip-use nmix)
        (chip-address-set 0)
        (chip-write-no-advance-byte 9C)
        ; move to next state:
        (set-state 2)
        (return)
    ))
< (ack)

> (state-machine-define-state
    (name  6F32)
    (state 2)
    (burst 8) ; execute this state up to 8 times consecutively
    (handler
        (chip-read-no-advance-byte)
        (pop)
        (bz nmi)    ; branch if A is zero to "nmi" label
        (return)    ; else return
    (label nmi)
        ; NMI has fired! read 4 bytes from WRAM at $0010:
        (chip-use wram)
        (chip-address-set 10)
        (chip-read-dword)
        ; append that data to a message and send it:
        (message-append-dword)
        (message-send)
        ; set-state to 1 and return:
        (set-state 1)
        (return)
    ))
< (ack)

> (state-machine-run (name 6F32))
< (ack)

; message-receive polls if a message is available from any state-machine
> (message-receive)
; it returns (message name data...) if a message is available
< (message 6F32 00000014)
; else it returns (nak) if no message available

> (state-machine-stop (name 6F32))
< (ack)
```
