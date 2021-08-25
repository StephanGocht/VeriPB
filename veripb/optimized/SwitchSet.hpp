#pragma once

#include <unordered_set>
#include <vector>
#include <algorithm>

template<typename T>
class SwitchSet {
    std::vector<T> vec;
    std::unordered_set<T> set;
    bool inVec = true;
    static const size_t upperThreshold = 60;
    static const size_t lowerThreshold = 30;

private:
    void toVec() {
        if (!inVec) {
            inVec = true;
            vec = std::vector<T>(set.begin(), set.end());
            set.clear();
        }
    }

    void toSet () {
        if (inVec) {
            inVec = false;
            set = std::unordered_set<T>(vec.begin(), vec.end());
            vec.clear();
        }
    }

public:

    void insert(T elem) {
        if (inVec) {
            vec.push_back(elem);
        } else {
            set.insert(elem);
        }
    }

    void erase(T elem) {
        if (vec.size() > upperThreshold) {
            toSet();
        }

        if (inVec) {
            auto it = std::find(vec.rbegin(), vec.rend(), elem);
            if (it != vec.rend()) {
                std::swap(*it, vec.back());
                vec.pop_back();
            }
        } else {
            set.erase(elem);
            if (set.size() < lowerThreshold) {
                toVec();
            }
        }
    }

    auto begin() {
        toVec();
        return vec.begin();
    }

    auto end() {
        toVec();
        return vec.end();
    }

    void clear() {
        vec.clear();
        set.clear();
        inVec = true;
    }

    size_t size() {
        if (inVec) {
            return vec.size();
        } else {
            return set.size();
        }
    }
};