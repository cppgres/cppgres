#include <cppgres.h>

extern "C" {
  PG_MODULE_MAGIC;
}

postgres_function(demo_len, ([](std::string_view t) { return t.length(); }));

namespace primes {
// Simple function to check if n is prime.
static bool is_prime(int64_t n) {
    if(n < 2) return false;
    for (int64_t i = 2; i * i <= n; ++i)
        if(n % i == 0)
            return false;
    return true;
}

// Returns the next prime after n.
int64_t next_prime(int64_t n) {
    int64_t candidate = n + 1;
    while(!is_prime(candidate))
        ++candidate;
    return candidate;
}

// A minimal forward iterator that lazily generates prime numbers.
class prime_iterator {
public:
    using value_type = int64_t;
    using difference_type = std::ptrdiff_t;
    using pointer = const int64_t*;
    using reference = const int64_t&;
    using iterator_category = std::forward_iterator_tag;

    // Default constructor creates an end iterator.
    prime_iterator() : current_(0), remaining_(0) {}

    // Begin iterator: starts at 2 and will produce 'count' primes.
    explicit prime_iterator(std::size_t count)
        : current_(2), remaining_(count)
    {
        if(count == 0)
            current_ = 0;
    }

    value_type operator*() const { return current_; }

    // Pre-increment: generate next prime.
    prime_iterator& operator++() {
        if(remaining_ <= 1) {
            // No more primes to generate; mark as end.
            remaining_ = 0;
            current_ = 0;
        } else {
            current_ = next_prime(current_);
            --remaining_;
        }
        return *this;
    }

    // Post-increment.
    prime_iterator operator++(int) {
        prime_iterator tmp(*this);
        ++(*this);
        return tmp;
    }

    // If both iterators have 0 remaining primes, they are considered equal (end).
    bool operator==(const prime_iterator& other) const {
        if(remaining_ == 0 && other.remaining_ == 0)
            return true;
        return remaining_ == other.remaining_ && current_ == other.current_;
    }
    bool operator!=(const prime_iterator& other) const {
        return !(*this == other);
    }

private:
    int64_t current_;
    std::size_t remaining_;
};

// A simple range that yields the first N primes.
class prime_range {
public:
    explicit prime_range(std::size_t count) : count_(count) {}

    prime_iterator begin() const { return prime_iterator(count_); }
    prime_iterator end() const { return prime_iterator(); } // End iterator.

private:
    std::size_t count_;
};
}

postgres_function(demo_srf, ([](int64_t t) {
                    primes::prime_range primes(t);
                    return primes;
                  }));
