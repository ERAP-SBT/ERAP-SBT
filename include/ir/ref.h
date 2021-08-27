#pragma once
#include <cstddef>
#include <type_traits>

struct Refable {
    size_t ref_count = 0;

    void inc_ref_count() noexcept { ref_count++; }

    void dec_ref_count() noexcept { ref_count--; }
};

template <typename T> struct RefPtr {
    static_assert(std::is_base_of_v<Refable, T>);

    RefPtr() = default;
    RefPtr(T *ptr) noexcept : _ptr(ptr) {
        if (_ptr) {
            _ptr->inc_ref_count();
        }
    }

    RefPtr(const RefPtr &other) noexcept : RefPtr{other._ptr} {}

    RefPtr(RefPtr &&other) noexcept : RefPtr{other.release()} {}

    ~RefPtr() noexcept {
        if (_ptr) {
            _ptr->dec_ref_count();
            _ptr = nullptr;
        }
    }

    RefPtr &operator=(const RefPtr &other) noexcept {
        if (this != &other) {
            reset(other._ptr);
        }
        return *this;
    }

    RefPtr &operator=(RefPtr &&other) noexcept {
        reset(other.release());
        return *this;
    }

    T *operator->() { return _ptr; }

    const T *operator->() const { return _ptr; }

    T *get() const { return _ptr; }

    operator T *() { return _ptr; }

    operator const T *() const { return _ptr; }

    operator bool() const { return _ptr != nullptr; }

    void swap(RefPtr &other) noexcept { std::swap(_ptr, other._ptr); }

    void reset(T *ptr) noexcept {
        if (_ptr) {
            _ptr->dec_ref_count();
        }
        _ptr = ptr;
        if (_ptr) {
            _ptr->inc_ref_count();
        }
    }

    T *release() noexcept {
        T *ptr = _ptr;
        _ptr = nullptr;
        if (ptr) {
            ptr->dec_ref_count();
        }
        return ptr;
    }

  protected:
    T *_ptr = nullptr;
};
