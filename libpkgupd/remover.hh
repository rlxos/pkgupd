#ifndef _LIBPKGUPD_REMOVER_HH_
#define _LIBPKGUPD_REMOVER_HH_

#include "defines.hh"
#include "sysdb.hh"
#include "triggerer.hh"

namespace rlxos::libpkgupd {
class Remover : public Object {
 private:
  std::string _root_dir;
  SystemDatabase _sys_db;
  Triggerer _triggerer;

  std::vector<std::vector<std::string>> _files_list;

  bool _skip_trigger = false;

 public:
  Remover(SystemDatabase &sdb, std::string const &rootdir)
      : _sys_db{sdb}, _root_dir{rootdir} {}

  bool remove(std::shared_ptr<SystemDatabase::package> pkginfo_);

  bool remove(std::vector<std::string> const &pkgs, bool skip_triggers = false);
};
}  // namespace rlxos::libpkgupd

#endif