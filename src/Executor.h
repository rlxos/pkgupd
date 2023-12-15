#ifndef PKGUPD_EXEC_H
#define PKGUPD_EXEC_H

#include <array>
#include <string>
#include <utility>
#include <vector>
#include <ostream>
#include <optional>

#include <sys/wait.h>
#include "Colors.h"
#include "Container.h"


class Executor {

    std::vector<std::string> args_;
    std::optional<std::string> path_{};
    std::vector<std::string> environ_;
    pid_t pid = -1;
    int pipe_fd[2]{};

public:
    explicit Executor(const std::string &binary) {
        args_.push_back(binary);
    }

    Executor &container(const std::optional<Container> &container) {
        if (container) {
            std::string path = path_ ? *path_ : "/";
            auto a = container->args();
            a.insert(a.end(), {"--chdir", path});
            args_.insert(args_.begin(), a.begin(), a.end());
            path_.reset();
        }
        return *this;
    }

    Executor &arg(const std::string &a) {
        args_.emplace_back(a);
        return *this;
    }

    Executor &path(const std::string &p) {
        path_ = p;
        return *this;
    }

    Executor &environ(const std::string &env) {
        environ_.push_back(env);
        return *this;
    }

    Executor &environ(const std::vector<std::string> &env) {
        environ_.insert(environ_.end(), env.begin(), env.end());
        return *this;
    }

    Executor &start();

    int wait(std::ostream *out = nullptr);

    int run();

    void execute();

    std::tuple<int, std::string> output();
};


#endif