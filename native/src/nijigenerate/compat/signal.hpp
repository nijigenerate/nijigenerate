#pragma once

// Adapted from Phobos std.signals.Signal.
// Copyright The D Language Foundation 2000 - 2009.
// Distributed under Boost Software License 1.0; see signal.LICENSE.
#include <algorithm>
#include <cstddef>
#include <limits>
#include <new>
#include <source_location>
#include <stdexcept>
#include <type_traits>
#include <vector>
#include "assert_error.hpp"
#include "out_of_memory_error.hpp"
#include "retained_string.hpp"

namespace nijigenerate::compat {
template<class... Args> class Signal;

// D object disposal notifications are synchronous and do not retain receivers.
class SignalReceiver {
    struct Disposal {
        void* signal;
        void (*unhook)(void*, SignalReceiver&);
    };
    std::vector<Disposal> disposals_;
    template<class...> friend class Signal;
    [[noreturn]] static void allocationFailure(
        std::source_location site = std::source_location::current()) {
        throw ngStaticError<OutOfMemoryError>(site);
    }
    void attach(void* signal, void (*unhook)(void*, SignalReceiver&)) {
        if (std::none_of(disposals_.begin(), disposals_.end(),
            [&](const auto& item) { return item.signal == signal; })) {
            try { disposals_.push_back({signal, unhook}); }
            catch (const std::bad_alloc&) { allocationFailure(); }
            catch (const std::length_error&) { allocationFailure(); }
        }
    }
    void detach(void* signal) {
        std::erase_if(disposals_, [&](const auto& item) { return item.signal == signal; });
    }
public:
    SignalReceiver() = default;
    SignalReceiver(const SignalReceiver&) = delete;
    SignalReceiver& operator=(const SignalReceiver&) = delete;
    virtual ~SignalReceiver() {
        while (!disposals_.empty()) {
            auto item = disposals_.back();
            disposals_.pop_back();
            item.unhook(item.signal, *this);
        }
    }
};

template<class... Args> class Signal {
public:
    class Slot {
        friend class Signal;
        SignalReceiver* receiver_ = nullptr;
        void* object_ = nullptr;
        void (*invoke_)(void*, Args...) = nullptr;
    public:
        Slot() = default;
        template<auto Method, class T> static Slot bind(T& receiver) {
            static_assert(std::is_base_of_v<SignalReceiver, T>);
            Slot result;
            result.receiver_ = &receiver;
            result.object_ = &receiver;
            result.invoke_ = [](void* object, Args... args) { (static_cast<T*>(object)->*Method)(args...); };
            return result;
        }
        explicit operator bool() const { return invoke_ != nullptr; }
        bool operator==(const Slot&) const = default;
    };
    using slot_t = Slot;
private:
    enum class Status { idle, emitting, disconnected };
    std::vector<Slot> slots_;
    std::size_t used_ = 0;
    Status status_ = Status::idle;
    static void unhookReceiver(void* signal, SignalReceiver& receiver) {
        static_cast<Signal*>(signal)->unhook(receiver);
    }
    void release() {
        for (std::size_t i = 0; i < used_; ++i) {
            if (slots_[i]) slots_[i].receiver_->detach(this);
        }
        // The source destructor frees the allocation, including unused slots.
        std::vector<Slot>{}.swap(slots_);
    }
public:
    Signal() = default;
    Signal(const Signal&) = delete;
    Signal& operator=(const Signal&) = delete;
    ~Signal() { release(); }

    void emit(Args... args) {
        if (status_ != Status::idle || slots_.empty()) return;
        status_ = Status::emitting;
        struct Reset { Status& status; ~Reset() { status = Status::idle; } } reset{status_};
        const auto snapshotLength = used_;
        for (std::size_t i = 0; i < snapshotLength; ++i) {
            const auto slot = slots_[i];
            if (slot) slot.invoke_(slot.object_, args...);
        }
#ifndef NDEBUG
        if (status_ == Status::idle) throw ngStaticError<AssertError>(
            RetainedString::fromStaticLiteral("Assertion failure"),
            RetainedString::fromStaticLiteral("std/signals.d"), 119);
#endif
        if (status_ == Status::disconnected) {
            for (std::size_t i = 0; i < used_;) {
                if (!slots_[i]) slots_[i] = slots_[--used_];
                else ++i;
            }
        }
    }
    void connect(Slot slot) {
        if (!slot) throw std::invalid_argument("Signal slots must refer to a receiver member");
        if (used_ == slots_.size()) {
            std::size_t length = 4;
            if (!slots_.empty()) {
                const auto limit = std::numeric_limits<std::size_t>::max();
                if (slots_.size() > (limit - 4) / 2)
                    ::nicxlive::core::d_runtime::ngAssertUnreachable(
                        "Assertion failure", "std/signals.d", 161);
                length = slots_.size() * 2 + 4;
                // The original slot_t is a context/code pointer delegate.
                if (length > limit / (2 * sizeof(void*)))
                    ::nicxlive::core::d_runtime::ngAssertUnreachable(
                        "Assertion failure", "std/signals.d", 161);
            }
            try { slots_.resize(length); }
            catch (const std::bad_alloc&) { SignalReceiver::allocationFailure(); }
            catch (const std::length_error&) { SignalReceiver::allocationFailure(); }
        }
        slots_[used_++] = slot;
        slot.receiver_->attach(this, &unhookReceiver);
    }
    template<auto Method, class T> void connect(T& receiver) { connect(Slot::template bind<Method>(receiver)); }

    void disconnect(Slot slot) {
        std::size_t disconnected = 0, previous = 0;
        if (status_ != Status::idle) {
            for (std::size_t i = 0; i < used_; ++i) {
                if (slots_[i].object_ == slot.object_ && ++previous && slots_[i] == slot) {
                    ++disconnected;
                    slots_[i] = {};
                    status_ = Status::disconnected;
                }
            }
        } else {
            for (std::size_t i = 0; i < used_;) {
                if (slots_[i].object_ == slot.object_ && ++previous && slots_[i] == slot) {
                    ++disconnected;
                    slots_[i] = slots_[--used_];
                    slots_[used_] = {};
                } else ++i;
            }
        }
        if (previous == disconnected && slot.receiver_) slot.receiver_->detach(this);
    }
    template<auto Method, class T> void disconnect(T& receiver) { disconnect(Slot::template bind<Method>(receiver)); }

    void disconnectAll() {
        release();
        used_ = 0;
        status_ = Status::idle;
    }
    void unhook(SignalReceiver& receiver) {
        // D's precondition also forbids destroying a receiver during emission.
#ifndef NDEBUG
        if (status_ != Status::idle) throw ngStaticError<AssertError>(
            RetainedString::fromStaticLiteral("Assertion failure"),
            RetainedString::fromStaticLiteral("std/signals.d"), 242);
#endif
        for (std::size_t i = 0; i < used_;) {
            if (slots_[i].receiver_ == &receiver) {
                slots_[i] = slots_[--used_];
                slots_[used_] = {};
            } else ++i;
        }
    }
};
}
