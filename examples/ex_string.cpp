// example: String (basic:: and threadSafe::)
// Compile: g++ -std=c++17 -DidxDEBUG -I../containers -pthread ex_string.cpp -o ex_string

#include <iostream>
#include <thread>
#include <vector>
#include "string.hpp"
#include <atomic>

using namespace idx;

void basic_example() {
    std::cout << "=== basic::String ===\n";
    std::cout << "    SSO: strings <= 14 chars stored inline (no heap allocation)\n";

    // Small String Optimisation — inline storage, no malloc
    basic::String small("hello");
    std::cout << "small: \"" << small.cString() << "\"  length=" << small.length() << "\n";

    // Heap path — triggered above 14 characters
    basic::String large("this exceeds 14 chars");
    std::cout << "large: length=" << large.length() << "\n";

    // Empty string
    basic::String empty;
    std::cout << "empty: length=" << empty.length() << "  empty()=" << empty.empty() << "\n";

    // Copy and move
    basic::String copy(small);
    std::cout << "copy: \"" << copy.cString() << "\"\n";
    basic::String moved(std::move(copy));
    std::cout << "moved: \"" << moved.cString() << "\"  copy now empty=" << copy.empty() << "\n";

    // Equality
    basic::String a("hello"), b("hello"), c("world");
    std::cout << "a==b: " << (a == b) << "  a==c: " << (a == c) << "\n";

    // operator+= (in-place append)
    basic::String s("hi");
    s += basic::String(" there");
    std::cout << "after +=: \"" << s.cString() << "\"  length=" << s.length() << "\n";

    // SSO -> heap transition when appending crosses 14-char boundary
    basic::String crossing("hello world!");  // 12 chars, SSO
    crossing += basic::String(" ok!");        // 16 total, now heap
    std::cout << "crossing SSO: \"" << crossing.cString() << "\"\n";

    // Self-append
    basic::String self("abc");
    self += self;
    std::cout << "self-append: \"" << self.cString() << "\"\n";  // "abcabc"

    // operator+ (concatenation, returns new String)
    basic::String result = basic::String("foo") + basic::String("bar");
    std::cout << "foo+bar: \"" << result.cString() << "\"\n";

    // C-string prepend via free function
    basic::String suffix("world");
    basic::String full = "hello " + suffix;
    std::cout << "prepend: \"" << full.cString() << "\"\n";

    // Character access
    std::cout << "full[0]='" << full[0] << "'  full[6]='" << full[6] << "'\n";

    // resize
    basic::String r("hello");
    r.resize(10);
    std::cout << "resize(10): length=" << r.length() << "\n";  // 10, extra bytes zeroed
    r.resize(3);
    std::cout << "resize(3):  \"" << r.cString() << "\"\n";    // "hel"

    // clear
    basic::String cl("test");
    cl.clear();
    std::cout << "after clear: empty=" << cl.empty() << "\n";

    // Free functions
    basic::String n  = toString1((sint64)-42);
    basic::String z  = toString1((sint64)0);
    basic::String p  = toString1((sint64)99999);
    std::cout << "toString1(-42): \"" << n.cString() << "\"\n";
    std::cout << "toString1(0):   \"" << z.cString() << "\"\n";
    std::cout << "toString1(99999): \"" << p.cString() << "\"\n";

    basic::String f = toString2(3.14159265358979);
    std::cout << "toString2(pi): \"" << f.cString() << "\"\n";

    int x = 42;
    basic::String hex = toString3((void*)&x);
    std::cout << "toString3(&x): \"" << hex.cString() << "\"  (length=" << hex.length() << ")\n";
}

void threadsafe_example() {
    std::cout << "\n=== threadSafe::String ===\n";

    threadSafe::String shared("base");

    // One writer appending, three readers reading length concurrently
    std::vector<std::thread> threads;
    std::atomic<int> reads{0};
    threads.emplace_back([&]{
        for (int i = 0; i < 100; ++i) shared += threadSafe::String("x");
    });
    for (int t = 0; t < 3; ++t)
        threads.emplace_back([&]{ for (int i = 0; i < 100; ++i) reads += shared.length(); });
    for (auto& th : threads) th.join();

    std::cout << "final length: " << shared.length() << "\n";  // 4 + 100 = 104
    std::cout << "reader operations completed: " << reads.load() << "\n";

    // Copy and move under lock
    threadSafe::String copy(shared);
    std::cout << "copy length: " << copy.length() << "\n";

    threadSafe::String moved(std::move(copy));
    std::cout << "moved length: " << moved.length() << "  copy now empty: " << copy.empty() << "\n";
}

int main() {
    basic_example();
    threadsafe_example();
}
