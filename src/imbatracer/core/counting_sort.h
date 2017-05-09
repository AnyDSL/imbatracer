#ifndef IMBA_COUNTING_SORT_H
#define IMBA_COUNTING_SORT_H

#include <tbb/tbb.h>
#include <vector>
#include <atomic>

namespace imba {

template <int N>
struct Counter {
    Counter(int n) {
        assert(n <= N);
        std::fill(counts, counts + n, 0);
    }

    Counter(Counter& c, tbb::split) {
        std::fill(counts, counts + N, 0);
    }

    template <typename Iterator>
    void operator () (const tbb::blocked_range<Iterator>& range) {
        for (auto it = range.begin(); it != range.end(); ++it) {
            counts[*it]++;
        }
    }

    void join(const Counter& c) {
        for (int i = 0; i < N; i++) counts[i] += c.counts[i];
    }

    int counts[N];
};

template <>
struct Counter<-1> {
    Counter(int n) : counts(n, 0) {}

    Counter(Counter& c, tbb::split)
        : counts(c.counts.size(), 0)
    {}

    template <typename Iterator>
    void operator () (const tbb::blocked_range<Iterator>& range) {
        for (auto it = range.begin(); it != range.end(); ++it) {
            counts[*it]++;
        }
    }

    void join(const Counter& c) {
        for (int i = 0, n = counts.size(); i < n; i++) counts[i] += c.counts[i];
    }

    std::vector<int> counts;
};

template <int N>
struct CountingSort {
    template <typename Iterator>
    static void sort(Iterator begin, Iterator end, int n, std::atomic_int* offs, int* ids) {
        Counter<N> c(n);
        tbb::parallel_reduce(tbb::blocked_range<Iterator>(begin, end), c);
        std::partial_sum(&c.counts[0], &c.counts[0] + n, &c.counts[0]);     
        std::copy(&c.counts[0], &c.counts[0] + n, offs);
        tbb::parallel_for(tbb::blocked_range<int>(0, end - begin),
            [=] (const tbb::blocked_range<int>& range)
        {
            for (auto i = range.begin(); i != range.end(); ++i)
                ids[--offs[*(begin + i)]] = i;
        });
    }
};

template <typename Iterator>
inline void counting_sort(Iterator begin, Iterator end, int n, int* ids) {
    if (n <= 1024) {
        std::atomic_int offs[1024];
        CountingSort<1024>::sort(begin, end, n, offs, ids);
    } else {
        static thread_local std::vector<std::atomic_int> offs;
        if (offs.size() < n) offs = std::vector<std::atomic_int>(n);
        CountingSort<-1>::sort(begin, end, n, offs.data(), ids);
    }
}

} // namespace imba

#endif // IMBA_COUNTING_SORT_H
