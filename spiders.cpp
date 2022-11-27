/*
    "Efficient Coroutine Generation of Constrained Gray Sequences" (2001),
    reprinted in "Selected Papers on Computer Languages" pages 545–574.
    This doesn't include the actual arbitrary-digraph stuff, but does
    include the three preliminary approaches that solve subproblems.

    Our task is to produce all n-bit bitstrings satisfying a certain set
    of constraints; and furthermore, to produce those bitstrings in a
    "Gray sequence," in which only one bit changes at each step.
    (For example, 010 011 001 101 is a Gray sequence, but 010 100 is not.)

    Knuth narrates the algorithm in terms of a row of "trolls," each either
    awake or asleep, and each carrying a lamp which may be on or off. When a
    sleeping troll is poked, it awakes and pokes its neighbor. When an awake
    troll is poked, it toggles the state of its lamp and then goes to sleep
    again. By starting all the trolls awake and their lamps off, we can run
    through a Gray sequence of all possible bitstrings by repeatedly poking
    the endmost troll. Here 'w' means awake and 's' means asleep, and
    capitalization indicates "lamp on":
        wwww (0000)
        wwwS (0001)
        wwSW (0011)
        wwSs (0010)
        wSWw (0110)
        wSWS (0111)
        wSsW (0101)

    This protocol is implemented by `UnconstrainedTroll` in the function
    `unconstrained` below.

    But we don't want to produce _all_ bitstrings; we want only those
    satisfying a set of constraints. The simplest constraint to consider
    is where each bit's value must be less-than-or-equal-to the value of
    the bit to its left. (That is, the totally acyclic digraph of
    "bit X is constrained to be less-or-equal-to bit Y" will be a
    simple unbranching tree connecting all the bits in a chain.)
    Knuth gives a troll-based protocol for this constrained problem;
    it is implemented by `ChainTroll` in the function `chains` below.

    Another simple constraint is where the totally acyclic digraph forms
    a bipartite "fence": bit 0 must be less-or-equal-to bit 1, bit 1 must
    be greater-or-equal-to bits 0 and 2, bit 2 must be less-or-equal-to
    bits 1 and 3, bit 3 must be greater-or-equal-to bits 2 and 4, and so on.
    Knuth gives a troll-based protocol for this constrained problem; it
    is implemented by `FenceTroll` in the function `fence_digraph` below.

    Knuth's paper goes on to define a simple text-based serialization
    format for totally acyclic digraphs, and describe a troll-based
    protocol for the general case (for constraints corresponding to any
    arbitrary user-provided totally acyclic digraph). The general case
    solution involves composing coroutines together in ways I don't fully
    understand yet. These parts of Knuth's paper are not implemented here.
*/

#include <coroutine>
#include <cstdio>
#include <deque>
#include <utility>
#include <vector>

template<class Derived>
struct TrollBase {
    struct promise_type;
    using handle_t = std::coroutine_handle<promise_type>;
    struct promise_type {
        Derived get_return_object() { Derived d; d.coro_ = handle_t::from_promise(*this); return d; }
        auto initial_suspend() { return std::suspend_always(); }
        auto final_suspend() noexcept { return std::suspend_never(); }
        auto yield_value(bool b) noexcept {
            value_ = b;
            return std::suspend_always();
        }
        void unhandled_exception() {}
        bool value_ = false;
    };

    explicit TrollBase() = default;
    TrollBase(TrollBase&& rhs) noexcept : coro_(std::exchange(rhs.coro_, nullptr)) {}
    void operator=(TrollBase rhs) noexcept { std::swap(coro_, rhs.coro_); }
    ~TrollBase() { if (coro_) coro_.destroy(); }

    bool poke() {
        coro_.resume(); // should modify value_
        return coro_.promise().value_;
    }

private:
    handle_t coro_ = nullptr;
};

void unconstrained(int n) {
    // SPCL page 550
    struct UnconstrainedTroll : TrollBase<UnconstrainedTroll> {
        static UnconstrainedTroll make(UnconstrainedTroll *next_troll, bool *lamp) {
            while (true) {
                // We've been poked while awake!
                *lamp = !*lamp;
                co_yield true;  // and be asleep
                // We've been poked -- awake!
                co_yield (next_troll ? next_troll->poke() : false);
            }
        }
    };

    auto lamps = std::deque<bool>(n, false);
    auto trolls = std::vector<UnconstrainedTroll>(n);
    for (int i=0; i < n; ++i) {
        trolls[i] = UnconstrainedTroll::make(i > 0 ? &trolls[i-1] : nullptr, &lamps[i]);
    }
    while (trolls[n-1].poke()) {
        printf("Lamps are: ");
        for (bool b : lamps) {
            printf("%c", (b ? '1' : '0'));
        }
        printf("\n");
    }
}

void chains(int n) {
    // SPCL page 552
    struct ChainTroll : TrollBase<ChainTroll> {
        static ChainTroll make(ChainTroll *next_troll, bool *lamp) {
            while (true) {
                // awake0
                while (next_troll && next_troll->poke()) co_yield true;
                *lamp = 1;
                co_yield true;
                // asleep1
                co_yield false;
                // awake1
                *lamp = 0;
                co_yield true;
                // asleep0
                while (next_troll && next_troll->poke()) co_yield true;
                co_yield false;
            }
        }
    };
    auto lamps = std::deque<bool>(n, false);
    auto trolls = std::vector<ChainTroll>(n);
    for (int i=0; i < n; ++i) {
        trolls[i] = ChainTroll::make(i > 0 ? &trolls[i-1] : nullptr, &lamps[i]);
    }
    while (trolls[n-1].poke() || lamps[n-1]) {
        printf("Lamps are: ");
        for (bool b : lamps) {
            printf("%c", (b ? '1' : '0'));
        }
        printf("\n");
    }
}

void fence_digraph(int n) {
    // SPCL page 557
    struct FenceTroll : TrollBase<FenceTroll> {
        static FenceTroll make(FenceTroll *trollp, FenceTroll *trollpp, bool *lamp) {
            if (*lamp) goto awake1;
            while (true) {
                // awake0
                while (trollp && trollp->poke()) co_yield true;
                *lamp = 1;
                co_yield true;
                // asleep1
                while (trollpp && trollpp->poke()) co_yield true;
                co_yield false;
            awake1:
                while (trollpp && trollpp->poke()) co_yield true;
                *lamp = 0;
                co_yield true;
                // asleep0
                while (trollp && trollp->poke()) co_yield true;
                co_yield false;
            }
        }
    };
    auto lamps = std::deque<bool>(n, false);
    auto trolls = std::vector<FenceTroll>(n);
    for (int i=0; i < n; ++i) {
        int kp = i + 1 + (i % 2);
        int kpp = i + 2 - (i % 2);
        lamps[i] = (i / 3) % 2;
        trolls[i] = FenceTroll::make(kp < n ? &trolls[kp] : nullptr, kpp < n ? &trolls[kpp] : nullptr, &lamps[i]);
    }
    while (trolls[0].poke()) {
        printf("Lamps are: ");
        for (bool b : lamps) {
            printf("%c", (b ? '1' : '0'));
        }
        printf("\n");
    }
}

int main()
{
    puts("-----UNCONSTRAINED");
    unconstrained(4);

    puts("-----CHAINS");
    chains(4);

    puts("-----FENCE DIGRAPH");
    fence_digraph(4);
}
