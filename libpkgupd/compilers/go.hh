#ifndef LIBPKGUPD_GO
#define LIBPKGUPD_GO

#include "../builder.hh"

namespace rlxos::libpkgupd {
class Go : public Compiler {
 protected:
  bool compile(Recipe const& recipe, std::string dir, std::string destdir,
               std::vector<std::string>& environ);
};
}  // namespace rlxos::libpkgupd

#endif