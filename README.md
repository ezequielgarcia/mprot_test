# Just a test with mprotect()

## How does it work?

Two processes share a shared-memory mmaped object. Each process
has an assigned ID, and each process knows each other's ID.

mprotect() is used to change access permissions at the process
level, this allows to catch a SEGV signal whenever the process
attempts to access (read ro write) the protected memory.

Ownership is tracked in the shared memory itself, by using
the process local ID as an index into an "ownership byte".
A synchronous msync() call is used as a memory barrier.

Proper ordering on how the ownership local and remote
bytes are used, allows to implement a race-free protocol.

The result is that the shared-memory is never accessed
concurrently (except the ownership bytes).

As a consistency test, we define a very simple rule,
which should be maintained at all times:

    buffer[RESULT_OFF] == buffer[ARG0] + buffer[ARG1_OFF]

## I want to give this a try

First build it.

    make
    
Then run it with recripocal local and remote IDs, like this:

    ./mprot 3 1 &
    ./mprot 1 3 &
    
    The processes will run while the rule is satisfied.
