/*
 * LogDuration.h
 *
 *  Created on: Dec 20, 2022
 *      Author: sergio
 */

#ifndef LOG_DURATION_H_
#define LOG_DURATION_H_

#include <chrono>
#include <iostream>


#define PROFILE_CONCAT_INTERNAL(X, Y) X##Y
#define PROFILE_CONCAT(X, Y) PROFILE_CONCAT_INTERNAL(X, Y)
#define UNIQUE_VAR_NAME_PROFILE PROFILE_CONCAT(profileGuard, __LINE__)
#define LOG_DURATION(x) LogDuration UNIQUE_VAR_NAME_PROFILE(x)
#define LOG_DURATION_STREAM(x, y) LogDuration UNIQUE_VAR_NAME_PROFILE(x, y)

class LogDuration {
public:
    // заменим имя типа std::chrono::steady_clock
    // с помощью using для удобства
    using Clock = std::chrono::steady_clock;

    LogDuration(const std::string& id, std::ostream& os = std::cerr) : id_(id), os_(os) {
    }

    ~LogDuration() {
        using namespace std::chrono;
        using namespace std::literals;

        const auto end_time = Clock::now();
        const auto dur = end_time - start_time_;
        os_ << id_ << ": "s << duration_cast<microseconds>(dur).count() << " µs"s << std::endl;
    }

private:
    const std::string id_;
    std::ostream& os_;
    const Clock::time_point start_time_ = Clock::now();
};

#endif /* LOG_DURATION_H_ */
