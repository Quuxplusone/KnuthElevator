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
