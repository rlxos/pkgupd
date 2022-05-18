#ifndef LIBPKGUPD_AUTOCONF
#define LIBPKGUPD_AUTOCON

#include "../builder.hh"
namespace rlxos::libpkgupd {
class AutoConf : public Compiler {
 protected:
  bool compile(Recipe const& recipe, std::string dir, std::string destdir, std::vector<std::string>& environ);
};
}  // namespace rlxos::libpkgupd

#endif