#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <algorithm>

namespace fs = std::filesystem;

struct TestResult
{
    std::string name;
    bool passed;
    std::string expected;
    std::string actual;
};

static std::string readFileContents(const std::string &path)
{
    std::ifstream file(path);
    if (!file.is_open())
        return "";
    std::stringstream ss;
    ss << file.rdbuf();
    return ss.str();
}

static std::string runClox(const std::string &cloxPath, const std::string &loxFile)
{
    std::string cmd = cloxPath + " " + loxFile + " 2>/dev/null";
    FILE *pipe = popen(cmd.c_str(), "r");
    if (!pipe)
        return "";

    std::string result;
    char buffer[256];
    while (fgets(buffer, sizeof(buffer), pipe))
        result += buffer;

    pclose(pipe);
    return result;
}

static std::vector<TestResult> runTestsInDir(const std::string &cloxPath, const fs::path &dir)
{
    std::vector<TestResult> results;

    for (const auto &entry : fs::directory_iterator(dir))
    {
        if (entry.path().extension() != ".lox")
            continue;

        std::string loxFile = entry.path().string();
        std::string expectedFile = entry.path().stem().string() + ".expected";
        fs::path expectedPath = dir / expectedFile;

        if (!fs::exists(expectedPath))
        {
            std::cerr << "  WARNING: no .expected file for " << entry.path().filename() << "\n";
            continue;
        }

        std::string expected = readFileContents(expectedPath.string());
        std::string actual = runClox(cloxPath, loxFile);

        TestResult r;
        r.name = dir.filename().string() + "/" + entry.path().stem().string();
        r.expected = expected;
        r.actual = actual;
        r.passed = (expected == actual);
        results.push_back(r);
    }

    return results;
}

int main(int argc, char *argv[])
{
    std::string cloxPath = "./clox";
    std::string testsDir = "tests";

    if (argc >= 2)
        cloxPath = argv[1];
    if (argc >= 3)
        testsDir = argv[2];

    if (!fs::exists(cloxPath))
    {
        std::cerr << "Error: clox binary not found at '" << cloxPath << "'\n";
        std::cerr << "Build it first with 'make'\n";
        return 1;
    }

    if (!fs::exists(testsDir))
    {
        std::cerr << "Error: tests directory not found at '" << testsDir << "'\n";
        return 1;
    }

    int totalPassed = 0;
    int totalFailed = 0;
    std::vector<TestResult> failures;

    std::vector<fs::path> categories;
    for (const auto &entry : fs::directory_iterator(testsDir))
    {
        if (entry.is_directory() && entry.path().filename() != "." && entry.path().filename().string()[0] != '.')
            categories.push_back(entry.path());
    }
    std::sort(categories.begin(), categories.end());

    for (const auto &category : categories)
    {
        std::cout << "=== " << category.filename().string() << " ===\n";
        auto results = runTestsInDir(cloxPath, category);

        std::sort(results.begin(), results.end(),
                  [](const TestResult &a, const TestResult &b)
                  { return a.name < b.name; });

        for (const auto &r : results)
        {
            if (r.passed)
            {
                std::cout << "  PASS: " << r.name << "\n";
                totalPassed++;
            }
            else
            {
                std::cout << "  FAIL: " << r.name << "\n";
                totalFailed++;
                failures.push_back(r);
            }
        }
    }

    std::cout << "\n--- Results ---\n";
    std::cout << "Passed: " << totalPassed << "\n";
    std::cout << "Failed: " << totalFailed << "\n";
    std::cout << "Total:  " << totalPassed + totalFailed << "\n";

    if (!failures.empty())
    {
        std::cout << "\n--- Failure Details ---\n";
        for (const auto &f : failures)
        {
            std::cout << f.name << ":\n";
            std::cout << "  expected: \"" << f.expected << "\"\n";
            std::cout << "  actual:   \"" << f.actual << "\"\n";
        }
    }

    return totalFailed > 0 ? 1 : 0;
}
