#include <coroutine>
#include <cstdio>
#include <deque>
#include <utility>
#include <vector>

struct Troll {
    struct promise_type;
    using handle_t = std::coroutine_handle<promise_type>;
    struct promise_type {
        auto get_return_object() { return Troll(handle_t::from_promise(*this)); }
        auto initial_suspend() { return std::suspend_always(); }
        auto final_suspend() noexcept { return std::suspend_never(); }
        auto yield_value(bool b) noexcept {
            value_ = b;
            return std::suspend_always();
        }
        void unhandled_exception() {}
        bool value_ = false;
    };

    explicit Troll() = default;
    Troll(Troll&& rhs) noexcept : coro_(std::exchange(rhs.coro_, nullptr)) {}
    Troll& operator=(Troll rhs) noexcept {
        std::swap(coro_, rhs.coro_);
        return *this;
    }
    ~Troll() { if (coro_) coro_.destroy(); }

    bool poke() {
        coro_.resume(); // should modify value_
        return coro_.promise().value_;
    }

private:
    explicit Troll(handle_t h) : coro_(h) {}

    handle_t coro_ = nullptr;
};

void unconstrained(int n) {
    // SPCL page 550
    auto lamps = std::deque<bool>(n, false);
    auto trolls = std::vector<Troll>(n);
    auto make_troll = [&](int k) {
        return [](Troll *next_troll, bool *lamp) -> Troll {
            while (true) {
                // We've been poked while awake!
                *lamp = !*lamp;
                co_yield true;  // and be asleep
                // We've been poked -- awake!
                co_yield (next_troll ? next_troll->poke() : false);
            }
        }(k >= 1 ? &trolls[k-1] : nullptr, &lamps[k]);
    };
    for (int i=0; i < n; ++i) {
        trolls[i] = make_troll(i);
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
    auto lamps = std::deque<bool>(n, false);
    auto trolls = std::vector<Troll>(n);
    auto make_troll = [&](int k) {
        return [](Troll *next_troll, bool *lamp) -> Troll {
            while (true) {
                // awake0
                if (next_troll) {
                    while (next_troll->poke()) co_yield true;
                }
                *lamp = 1;
                co_yield true;
                // asleep1
                co_yield false;
                // awake1
                *lamp = 0;
                co_yield true;
                // asleep0
                if (next_troll) {
                    while (next_troll->poke()) co_yield true;
                }
                co_yield false;
            }
        }(k >= 1 ? &trolls[k-1] : nullptr, &lamps[k]);
    };
    for (int i=0; i < n; ++i) {
        trolls[i] = make_troll(i);
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
    auto lamps = std::deque<bool>(n, false);
    auto trolls = std::vector<Troll>(n);
    auto make_troll = [&](int k) {
        int kp = k + 1 + (k % 2);
        int kpp = k + 2 - (k % 2);
        return [](Troll *trollp, Troll *trollpp, bool *lamp) -> Troll {
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
        }(kp < n ? &trolls[kp] : nullptr, kpp < n ? &trolls[kpp] : nullptr, &lamps[k]);
    };
    for (int i=0; i < n; ++i) {
        lamps[i] = (i / 3) % 2;
        trolls[i] = make_troll(i);
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
