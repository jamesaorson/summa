#include <template_cpp/identity.hpp>

#include <gtest/gtest.h>

namespace tmpl = template_cpp;

TEST(IdentityTest, call_with_int) {
    for (int i = -100; i < 100; ++i) {
        EXPECT_EQ(i, tmpl::identity()(i));
    }
}

TEST(IdentityTest, call_with_custom_type) {
    struct S {
        int i;
    };

    for (int i = -100; i < 100; ++i) {
        const S s{i};
        EXPECT_EQ(s.i, tmpl::identity()(s).i);
    }
}
