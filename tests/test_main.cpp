#include "test_framework.h"

namespace test {

std::vector<Case>& registry() {
    static std::vector<Case> cases;
    return cases;
}

int& failures() {
    static int count = 0;
    return count;
}

void fail(const char* file, int line, const std::string& message) {
    ++failures();
    std::printf("  FAIL %s:%d: %s\n", file, line, message.c_str());
}

}  // namespace test

int main() {
    int failed_cases = 0;
    int total_cases = 0;

    for (const test::Case& c : test::registry()) {
        const int before = test::failures();
        ++total_cases;
        c.fn();
        const bool ok = test::failures() == before;
        std::printf("%s %s\n", ok ? "ok  " : "FAIL", c.name);
        if (!ok) ++failed_cases;
    }

    std::printf("\n%d/%d tests passed, %d assertions failed\n", total_cases - failed_cases,
                total_cases, test::failures());
    return failed_cases == 0 ? 0 : 1;
}
