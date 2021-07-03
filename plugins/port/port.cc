#include "../../recipe.hh"
#include <tuple>
#include <string>

#include <io.hh>

using std::string;
using namespace rlx;
using color = rlx::io::color;
using level = rlx::io::debug_level;

extern "C" std::tuple<bool, string> pkgupd_build(
    pkgupd::recipe const &recipe,
    YAML::Node const &node,
    pkgupd::package *pkg,
    string src_dir,
    string pkg_dir)
{
    io::debug(level::trace, "port::pkgupd_build() start");
    auto env = recipe.environ(pkg);

    auto checkenv = [&](string i) -> void
    {
        if (node["compiler"] && node["compiler"][i])
            env.push_back(io::format(i, "=", node["compiler"][i].as<string>()));
    };

    string compiler = (node["compiler"] && node["compiler"]["recipe"] ? node["compiler"]["recipe"].as<string>() : "port-compiler");

    io::debug(level::trace, "Compiler: ", color::MAGENTA, compiler, color::RESET);
    if (rlx::utils::exec::command(
            compiler + " " + pkg->id(), src_dir,
            env))
    {
        return {false, "failed to execute recipe compiler"};
    }
    return {true, ""};
}