## Knuth Elevator Simulator

This is a C++14 implementation of the elevator simulator
from Donald Knuth's _Art of Computer Programming_, volume 1,
section 2.2.5 ("Doubly Linked Lists").

Knuth expresses the program as a pair of coroutines,
"Coroutine E" (for the elevator itself)
and "Coroutine U" (for the users of the elevator).

These coroutines don't seem to map well onto C++20's
"coroutines" feature: the problem is that sometimes the
user's behavior depends on observing whether the elevator
is in state E6, and sometimes the elevator needs to "kick"
a user from state U4 to state U5. In other words, the
state of the coroutine is both visible and modifiable;
it's not just an opaque "handle" to be "resumed" from
exactly where it left off.

These problems are solvable, but I haven't solved them yet.

### Running the simulator

    make 'CXXFLAGS=-DPRINT_STATISTICS -DUSE_KNUTH_DATA'
    ./go 4841

will produce a table similar to — in fact, a superset of —
Knuth's Table 1 (_TAOCP_ volume 1, third edition, page 286).

Add `-DEXERCISE_SIX` to see what happens if we install lights
outside the elevator to indicate which direction it's moving,
so that users can avoid getting in when it's moving in the
wrong direction for them.
