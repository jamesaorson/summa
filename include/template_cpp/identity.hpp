#ifndef TEMPLATE_CPP_IDENTITY_HPP
#define TEMPLATE_CPP_IDENTITY_HPP

#include <utility>

namespace template_cpp {

struct identity {
    template <class T>
    constexpr T&& operator()(T&& t) const noexcept {
        return std::forward<T>(t);
    }

    using is_transparent = void;
};

} // namespace template_cpp

#endif // TEMPLATE_CPP_IDENTITY_HPP
