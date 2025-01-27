#ifndef INTERVAL_H
#define INTERVAL_H
//==============================================================================================
// To the extent possible under law, the author(s) have dedicated all copyright and related and
// neighboring rights to this software to the public domain worldwide. This software is
// distributed without any warranty.
//
// You should have received a copy (see file COPYING.txt) of the CC0 Public Domain Dedication
// along with this software. If not, see <http://creativecommons.org/publicdomain/zero/1.0/>.
//==============================================================================================


class interval {
  public:
    double mMin, mMax;

    interval() : mMin(+infinity), mMax(-infinity) {} // Default interval is empty

    interval(double mMin, double mMax) : mMin(mMin), mMax(mMax) {}

    interval(const interval& a, const interval& b) {
        // Create the interval tightly enclosing the two input intervals.
        mMin = a.mMin <= b.mMin ? a.mMin : b.mMin;
        mMax = a.mMax >= b.mMax ? a.mMax : b.mMax;
    }

    double size() const {
        return mMax - mMin;
    }

    bool contains(double x) const {
        return mMin <= x && x <= mMax;
    }

    bool surrounds(double x) const {
        return mMin < x && x < mMax;
    }

    double clamp(double x) const {
        if (x < mMin) return mMin;
        if (x > mMax) return mMax;
        return x;
    }

    interval expand(double delta) const {
        auto padding = delta/2;
        return interval(mMin - padding, mMax + padding);
    }

    static const interval empty, universe;
};

const interval interval::empty    = interval(+infinity, -infinity);
const interval interval::universe = interval(-infinity, +infinity);

interval operator+(const interval& ival, double displacement) {
    return interval(ival.mMin + displacement, ival.mMax + displacement);
}

interval operator+(double displacement, const interval& ival) {
    return ival + displacement;
}


#endif
