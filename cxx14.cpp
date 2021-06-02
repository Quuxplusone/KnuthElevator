#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <deque>
#include <memory>
#include <string>

#include "xoshiro256ss.h"

template<class T>
void std_erase(std::deque<T>& ctr, const T& value)
{
    assert(std::count(ctr.begin(), ctr.end(), value) <= 1);
    auto it = std::find(ctr.begin(), ctr.end(), value);
    if (it != ctr.end()) {
        ctr.erase(it);
    }
}

struct ElevatorSimulation;

using Floor = int;     // 0 through 4
using Time = int;      // timestamp, in tenths of seconds
using Duration = int;  // duration, in tenths of seconds

enum Direction { GoingUp, GoingDown, Neutral };

struct Task : public std::enable_shared_from_this<Task> {
    int nextinst_ = 1;
    int nexttime_ = -1;

    struct ByNextTime {
        template<class T>
        bool operator()(const std::shared_ptr<T>& p, const std::shared_ptr<T>& q) const {
            return p->nexttime_ < q->nexttime_;
        }
    };

    virtual std::string stateStr() const = 0;
    virtual void resume(ElevatorSimulation& sim) = 0;
    virtual ~Task() = default;
};

struct ElevatorTask : public Task {
    std::string stateStr() const override { return "E" + std::to_string(nextinst_); }
    void resume(ElevatorSimulation& sim) override;
};

struct E5Task : public Task {
    std::string stateStr() const override { return "E5"; }
    void resume(ElevatorSimulation& sim) override;
};

struct E9Task : public Task {
    std::string stateStr() const override { return "E9"; }
    void resume(ElevatorSimulation& sim) override;
};

struct UserTask : public Task {
    Floor in_;
    Floor out_;

#if PRINT_STATISTICS
    static int nextCounter() { static int c = 0; return ++c; }
    int userNumber_ = nextCounter();
    Time enteredQueueAt_;
    Time enteredCarAt_;
    int maxOccupancy_ = 0;
    std::deque<Floor> stoppedAt_;
#endif

    std::shared_ptr<UserTask> shared_user_from_this() {
        return std::static_pointer_cast<UserTask>(this->shared_from_this());
    }

    std::string stateStr() const override { return "U" + std::to_string(nextinst_); }
    void resume(ElevatorSimulation& sim) override;
};

struct ElevatorSimulation {
public:
    Duration durationBeforeRapidDoorClose = 25;
    Duration durationBeforeInactivity = 300;
    Duration durationBeforeDoorClose = 76;
    Duration durationOfDoorOpen = 20;
    Duration durationOfLeaving = 25;
    Duration durationOfEntering = 25;
    Duration delayAfterDoorFlutter = 40;
    Duration durationOfDoorClose = 20;
    Duration durationOfUpwardAcceleration = 15;
    Duration durationOfDownwardAcceleration = 15;
    Duration durationOfDoorOpenFromDecisionSubroutine = 20;
    Duration delayBeforeHoming = 20;
    Duration durationOfUpwardTravel = 51;
    Duration durationOfUpwardDeceleration = 14;
    Duration durationOfDownwardTravel = 61;
    Duration durationOfDownwardDeceleration = 23;

public:
    xoshiro256ss rand_;

    Floor floor_ = 2;
    bool d1_ = false;  // Are the doors open AND people are getting in or out?
    bool d2_ = false;  // Has the elevator been active within the last 30 seconds?
    bool d3_ = false;  // Are the doors open BUT nobody is getting in or out?
    Direction state_ = Neutral;

    bool callup_[5] = {};
    bool calldown_[5] = {};
    bool callcar_[5] = {};

    std::deque<std::shared_ptr<Task>> wait_;
    std::deque<std::shared_ptr<UserTask>> queue_[5];
    std::deque<std::shared_ptr<UserTask>> elevator_;

    std::shared_ptr<ElevatorTask> elevatortask_ = std::make_shared<ElevatorTask>();
    std::shared_ptr<E5Task> e5task_ = std::make_shared<E5Task>();
    std::shared_ptr<E9Task> e9task_ = std::make_shared<E9Task>();

public:
    ElevatorSimulation() {
        auto t = std::make_shared<UserTask>();
        Time time_zero = 0;
        this->schedule(t, 1, time_zero);  // The first user enters at time zero.
    }

    void runUntil(Time deadline) {
        while (true) {
            assert(!wait_.empty());
            std::shared_ptr<Task> t = wait_.front();
            if (t->nexttime_ >= deadline) {
                return;
            }
            wait_.pop_front();
            printf("%04d %c %d %c %c %c %s\n",
                t->nexttime_, (state_ == Neutral ? 'N' : state_ == GoingUp ? 'U' : 'D'),
                floor_, "0X"[int(d1_)], "0X"[int(d2_)], "0X"[int(d3_)], t->stateStr().c_str());
#if 0
            for (int i=0; i < 5; ++i) {
                if (!queue_[i].empty()) printf("Queued on floor %d: %zu users\n", i, queue_[i].size());
            }
            if (!elevator_.empty()) printf("In the elevator: %zu users\n", elevator_.size());
            printf("Tasks in the wait queue: ");
            for (const auto& tt : wait_) {
                printf("%s/%d ", tt->stateStr().c_str(), tt->nexttime_);
            }
            printf("\n");
#endif

            t->resume(*this);
        }
    }

    struct NewUserInfo {
        Floor in_;             // floor on which this user enters
        Floor out_;            // this user's destination floor
        Duration giveuptime_;  // amount of time this user will wait
        Duration intertime_;   // amount of time before next user arrives
    };

    NewUserInfo createNewUser() {
#if USE_KNUTH_DATA
        static const NewUserInfo data[] = {
            { 0, 2, 152-0,     38 -    0 },
            { 4, 1, 36000,    136 -   38 },
            { 2, 1, 36000,    141 -  136 },
            { 2, 1, 36000,    291 -  141 },
            { 3, 1, 36000,    364 -  291 },
            { 2, 1, 540-364,  602 -  364 },
            { 1, 2, 36000,    827 -  602 },
            { 1, 0, 36000,    876 -  827 },
            { 1, 3, 36000,   1048 -  876 },
            { 0, 4, 36000,   4384 - 1048 },
            { 2, 3, 36000,   4845 - 4384 },  // Knuth's "User 17"
        };
        static int i = 0;
        if (i < 11) return data[i++];
#endif
        auto random_between = [&](int lo, int hi) {
            return lo + (rand_() % (1 + hi - lo));
        };
        Floor in = random_between(0, 4);
        Floor out = (in + random_between(1, 4)) % 5;
        Duration giveup = random_between(300, 1200);
        Duration intertime = random_between(10, 900);
        return NewUserInfo{ in, out, giveup, intertime };
    }

    void schedule(std::shared_ptr<Task> t, int step, Time when) {
        t->nextinst_ = step;
        t->nexttime_ = when;
        std_erase(wait_, t);
        wait_.push_back(t);
        std::stable_sort(wait_.begin(), wait_.end(), Task::ByNextTime());
    }

    void schedule_immediately(std::shared_ptr<Task> t, int step, Time when) {
        t->nextinst_ = step;
        t->nexttime_ = when;
        std_erase(wait_, t);
        wait_.push_front(t);
        assert(std::is_sorted(wait_.begin(), wait_.end(), Task::ByNextTime()));
    }

    void cancel(std::shared_ptr<Task> t) {
        std_erase(wait_, t);
    }

    void decision(Time now, bool fromE6) {
        // D1. Decision necessary?
        if (state_ != Neutral) {
            return;
        }
        // D2. Should doors open?
        if (elevatortask_->nextinst_ == 1 && (callup_[2] || calldown_[2] || callcar_[2])) {
            this->schedule(elevatortask_, 3, now + durationOfDoorOpenFromDecisionSubroutine);
            return;
        }
        // D3. Any calls?
        int jj = (fromE6 ? 2 : -1);
        for (int j=0; j < 5; ++j) {
            if (j == floor_) {
                continue;
            }
            if (callup_[j] || calldown_[j] || callcar_[j]) {
                jj = j;
                break;
            }
        }
        if (jj != -1) {
            // D4. Set STATE.
            state_ = (jj < floor_) ? GoingDown : (jj > floor_) ? GoingUp : Neutral;
            // D5. Elevator dormant?
            if (elevatortask_->nextinst_ == 1 && jj != 2) {
                this->schedule(elevatortask_, 6, now + delayBeforeHoming);
            }
        }
    }
};


    void UserTask::resume(ElevatorSimulation& sim) {
        Time now = this->nexttime_;
        std::shared_ptr<UserTask> me = shared_user_from_this();
#if EXERCISE_SIX
        auto elevator_is_available = [&](Floor in, Floor out) {
            Direction avoid = (out < in) ? GoingUp : GoingDown;
            return (sim.floor_ == in) && (sim.state_ != avoid);
        };
#else
        auto elevator_is_available = [&](Floor in, Floor) {
            return (sim.floor_ == in);
        };
#endif
        switch (this->nextinst_) {
            case 1: {
                // U1. Enter, prepare for successor.
                auto info = sim.createNewUser();
                sim.schedule(std::make_shared<UserTask>(), 1, now + info.intertime_);
                // U2. Signal and wait.
                assert(info.in_ != info.out_);
                if (elevator_is_available(info.in_, info.out_) && sim.elevatortask_->nextinst_ == 6) {
                    sim.schedule_immediately(sim.elevatortask_, 3, now);
                } else if (elevator_is_available(info.in_, info.out_) && sim.d3_) {
                    sim.d3_ = false;
                    sim.d1_ = true;
                    sim.schedule_immediately(sim.elevatortask_, 4, now);
                } else {
                    if (info.in_ < info.out_) {
                        sim.callup_[info.in_] = true;
                    } else {
                        sim.calldown_[info.in_] = true;
                    }
                    if (!sim.d2_ || sim.elevatortask_->nextinst_ == 1) {
                        sim.decision(now, false);
                    }
                }
                // U3. Enter queue.
                this->in_ = info.in_;
                this->out_ = info.out_;
                sim.queue_[this->in_].push_back(me);
                sim.schedule(me, 4, now + info.giveuptime_);
#if PRINT_STATISTICS
                this->enteredQueueAt_ = now;
#endif
                return;
            }
            case 4: {
                // U4. Give up.
                if (!elevator_is_available(this->in_, this->out_) || !sim.d1_) {
                    std_erase(sim.queue_[this->in_], me);
#if PRINT_STATISTICS
                    Duration d = now - this->enteredQueueAt_;
                    printf("User %d walked after %d.%ds waiting in the queue on floor %d\n", this->userNumber_, d / 10, d % 10, this->in_);
#endif
                }
                return;
            }
            case 5: {
                // U5. Get in.
                std_erase(sim.queue_[this->in_], me);
                sim.elevator_.push_front(me);
                sim.callcar_[this->out_] = true;
                if (sim.state_ == Neutral) {
                    sim.state_ = (this->in_ < this->out_) ? GoingUp : GoingDown;
                    sim.schedule(sim.e5task_, 5, now + sim.durationBeforeRapidDoorClose);
                }
#if PRINT_STATISTICS
                this->enteredCarAt_ = now;
                for (auto& user : sim.elevator_) {
                    user->maxOccupancy_ = std::max(user->maxOccupancy_, int(sim.elevator_.size()));
                }
#endif
                return;
            }
            case 6: {
                // U6. Get out.
                std_erase(sim.elevator_, me);
#if PRINT_STATISTICS
                Duration d1 = this->enteredCarAt_ - this->enteredQueueAt_;
                Duration d2 = now - this->enteredCarAt_;
                printf("User %d arrived after %d.%ds waiting in the queue on floor %d followed by %d.%ds in the elevator. Max occupancy %d. Stopped at floors",
                    this->userNumber_, d1 / 10, d1 % 10, this->in_, d2 / 10, d2 % 10, this->maxOccupancy_);
                for (Floor f : this->stoppedAt_) {
                    printf(" %d", f);
                }
                printf(".\n");
#endif
                return;
            }
        }
    }

    void ElevatorTask::resume(ElevatorSimulation& sim) {
        Time now = this->nexttime_;
        std::shared_ptr<Task> me = shared_from_this();
        assert(me == sim.elevatortask_);
        switch (this->nextinst_) {
            case 1: {
                // E1. Wait for call.
                assert(sim.floor_ == 2);
                return;
            }
            case 2: {
                // E2. Change of state?
                bool passenger_wants_up = false;
                bool passenger_wants_down = false;
                bool waiter_wants_up = false;
                bool waiter_wants_down = false;
                for (int j=0; j < 5; ++j) {
                    if (j != sim.floor_) {
                        if (sim.callcar_[j]) {
                            ((j > sim.floor_) ? passenger_wants_up : passenger_wants_down) = true;
                        }
                        if (sim.callup_[j] || sim.calldown_[j]) {
                            ((j > sim.floor_) ? waiter_wants_up : waiter_wants_down) = true;
                        }
                    }
                }
                if (sim.state_ == GoingUp && !(passenger_wants_up || waiter_wants_up)) {
                    sim.state_ = (passenger_wants_down ? GoingDown : Neutral);
                } else if (sim.state_ == GoingDown && !(passenger_wants_down || waiter_wants_down)) {
                    sim.state_ = (passenger_wants_up ? GoingUp : Neutral);
                }
                goto caseE3;
            }
            case 3: caseE3: {
                // E3. Open doors.
                sim.d1_ = true;
                sim.d2_ = true;
                sim.schedule(sim.e9task_, 9, now + sim.durationBeforeInactivity);
                sim.schedule(sim.e5task_, 5, now + sim.durationBeforeDoorClose);
                sim.schedule(me, 4, now + sim.durationOfDoorOpen);
#if PRINT_STATISTICS
                for (const auto& user : sim.elevator_) {
                    user->stoppedAt_.push_back(sim.floor_);
                }
#endif
                return;
            }
            case 4: {
                // E4. Let people out, in.
                assert(sim.d1_);
                auto leaver = std::find_if(sim.elevator_.begin(), sim.elevator_.end(), [&](const auto& p) {
                    return p->out_ == sim.floor_;
                });
                auto enterer = std::find_if(sim.queue_[sim.floor_].begin(), sim.queue_[sim.floor_].end(), [&](const auto& p) {
#if EXERCISE_SIX
                    return (sim.state_ == Neutral) || ((p->out_ > sim.floor_) == (sim.state_ == GoingUp));
#else
                    (void)p;
                    return true;
#endif
                });
                if (leaver != sim.elevator_.end()) {
                    sim.schedule_immediately(*leaver, 6, now);
                    sim.schedule(me, 4, now + sim.durationOfLeaving);
                } else if (enterer != sim.queue_[sim.floor_].end()) {
                    assert((*enterer)->nextinst_ == 4);
                    sim.schedule_immediately(*enterer, 5, now);
                    sim.schedule(me, 4, now + sim.durationOfEntering);
                } else {
                    sim.d1_ = false;
                    sim.d3_ = true;
                }
                return;
            }
            case 6: {
                // E6. Prepare to move.
                assert(!sim.d1_);
                sim.callcar_[sim.floor_] = false;
                if (sim.state_ != GoingDown) {
                    sim.callup_[sim.floor_] = false;
                }
                if (sim.state_ != GoingUp) {
                    sim.calldown_[sim.floor_] = false;
                }
                sim.decision(now, true);
                if (sim.state_ == Neutral) {
                    assert(sim.floor_ == 2);
                    assert(std::find(sim.wait_.begin(), sim.wait_.end(), me) == sim.wait_.end());
                    sim.schedule_immediately(me, 1, now);
                } else {
                    if (sim.d2_) {
                        sim.cancel(sim.e9task_);
                    }
                    if (sim.state_ == GoingUp) {
                        sim.schedule(me, 7, now + sim.durationOfUpwardAcceleration);
                    } else {
                        sim.schedule(me, 8, now + sim.durationOfDownwardAcceleration);
                    }
                }
                return;
            }
            case 7: {
                // E7. Go up a floor.
                assert(!sim.d1_);
                assert(sim.floor_ < 4);
                sim.floor_ += 1;
                sim.schedule(me, 71, now + sim.durationOfUpwardTravel);
                return;
            }
            case 71: {
                // E7, continued.
                bool passenger_wants_up = false;
                bool passenger_wants_down = false;
                bool waiter_wants_up = false;
                bool waiter_wants_down = false;
                for (int j=0; j < 5; ++j) {
                    if (j != sim.floor_) {
                        if (sim.callcar_[j]) {
                            ((j > sim.floor_) ? passenger_wants_up : passenger_wants_down) = true;
                        }
                        if (sim.callup_[j] || sim.calldown_[j]) {
                            ((j > sim.floor_) ? waiter_wants_up : waiter_wants_down) = true;
                        }
                    }
                }
                bool should_stop_here = sim.callcar_[sim.floor_] ||
                    sim.callup_[sim.floor_] ||
                    ((sim.floor_ == 2 || sim.calldown_[sim.floor_]) && !(passenger_wants_up || waiter_wants_up));
                if (should_stop_here) {
                    sim.schedule(me, 2, now + sim.durationOfUpwardDeceleration);
                } else {
                    sim.schedule_immediately(me, 7, now);
                }
                return;
            }
            case 8: {
                // E8. Go down a floor.
                assert(!sim.d1_);
                assert(sim.floor_ > 0);
                sim.floor_ -= 1;
                sim.schedule(me, 81, now + sim.durationOfDownwardTravel);
                return;
            }
            case 81: {
                // E8, continued.
                bool passenger_wants_up = false;
                bool passenger_wants_down = false;
                bool waiter_wants_up = false;
                bool waiter_wants_down = false;
                for (int j=0; j < 5; ++j) {
                    if (j != sim.floor_) {
                        if (sim.callcar_[j]) {
                            ((j > sim.floor_) ? passenger_wants_up : passenger_wants_down) = true;
                        }
                        if (sim.callup_[j] || sim.calldown_[j]) {
                            ((j > sim.floor_) ? waiter_wants_up : waiter_wants_down) = true;
                        }
                    }
                }
                bool should_stop_here = sim.callcar_[sim.floor_] ||
                    sim.calldown_[sim.floor_] ||
                    ((sim.floor_ == 2 || sim.callup_[sim.floor_]) && !(passenger_wants_down || waiter_wants_down));
                if (should_stop_here) {
                    sim.schedule(me, 2, now + sim.durationOfDownwardDeceleration);
                } else {
                    sim.schedule_immediately(me, 8, now);
                }
                return;
            }
            default: {
                assert(false);
            }
        }
    }

    void E5Task::resume(ElevatorSimulation& sim) {
        Time now = this->nexttime_;
        std::shared_ptr<Task> me = shared_from_this();
        assert(me == sim.e5task_);
        assert(nextinst_ == 5);

        // E5. Close doors.
        if (sim.d1_) {
            sim.schedule(me, 5, now + sim.delayAfterDoorFlutter);
        } else {
            sim.d3_ = false;
            sim.schedule(sim.elevatortask_, 6, now + sim.durationOfDoorClose);
        }
    }

    void E9Task::resume(ElevatorSimulation& sim) {
        Time now = this->nexttime_;
        std::shared_ptr<Task> me = shared_from_this();
        assert(me == sim.e9task_);
        assert(nextinst_ == 9);

        // E9. Set inaction indicator.
        sim.d2_ = false;
        sim.decision(now, false);
    }


int main(int argc, char **argv)
{
    ElevatorSimulation sim;
    Time deadline = (argc >= 2) ? atoi(argv[1]) : 3600'0;
    sim.runUntil(deadline);
}
