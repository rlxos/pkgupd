#include "sysdb.hh"

#include <string.h>
#include <sys/stat.h>

#include <ctime>
#include <fstream>

namespace rlxos::libpkgupd {

sysdb::package::package(YAML::Node const &data, std::string const &file) {
  READ_VALUE(std::string, id);
  READ_VALUE(std::string, version);
  READ_VALUE(std::string, about);

  READ_VALUE(std::string, installed_on);

  READ_LIST(std::string, depends);
  READ_LIST(std::string, files);

  READ_OBJECT_LIST(pkginfo::user, users);
  READ_OBJECT_LIST(pkginfo::group, groups);

  OPTIONAL_VALUE(std::string, install_script, "");
}

std::shared_ptr<pkginfo> sysdb::operator[](std::string const &pkgid) {
  auto datafile = _data_dir + "/" + pkgid;
  if (!std::filesystem::exists(datafile)) return nullptr;

  DEBUG("Found at: " << datafile);

  return std::make_shared<sysdb::package>(YAML::LoadFile(datafile), datafile);
}

bool sysdb::is_installed(std::shared_ptr<pkginfo> const &pkginfo) {
  if ((*this)[pkginfo->id()] == nullptr) return false;

  return true;
}

bool sysdb::outdated(std::shared_ptr<pkginfo> const &pkginfo) {
  auto installedPackage = (*this)[pkginfo->id()];
  if (installedPackage == nullptr)
    throw std::runtime_error(pkginfo->id() + " is missing in sysdb");

  return (installedPackage->version() != pkginfo->version());
}

bool sysdb::remove(std::shared_ptr<pkginfo> const &pkginfo) {
  auto installedPackage = (*this)[pkginfo->id()];
  if (installedPackage == nullptr) {
    _error = "no package with id " + pkginfo->id() + " is installed";
    return false;
  }

  std::string datafile = _data_dir + "/" + pkginfo->id();
  std::error_code err;
  std::filesystem::remove(datafile, err);
  if (err) {
    _error = err.message();
    return false;
  }

  return true;
}

std::vector<std::shared_ptr<pkginfo>> sysdb::all() {
  if (!std::filesystem::exists(_data_dir)) {
    _error = "no packages database found";
    return {};
  }

  std::vector<std::shared_ptr<pkginfo>> pkgs;
  for (auto const &i : std::filesystem::directory_iterator(_data_dir)) {
    YAML::Node data = YAML::LoadFile(i.path().string());
    pkgs.push_back(std::make_shared<sysdb::package>(data, i.path().string()));
  }

  return pkgs;
}

bool sysdb::add(std::shared_ptr<pkginfo> const &pkginfo,
                std::vector<std::string> const &files, std::string root,
                bool toupdate) {
  try {
    if (is_installed(pkginfo) && !outdated(pkginfo) && !toupdate) {
      _error = pkginfo->id() + " " + pkginfo->version() +
               " is already registered in the system";
      return false;
    }

    toupdate = outdated(pkginfo);
  } catch (...) {
  }

  auto datafile = _data_dir + "/" + pkginfo->id();

  std::ofstream fileptr(datafile);
  if (!fileptr.is_open()) {
    _error = "failed to open sysdb to register " + pkginfo->id();
    return false;
  }

  fileptr << "id: " << pkginfo->id() << std::endl
          << "version: " << pkginfo->version() << std::endl
          << "about: " << pkginfo->about() << std::endl;

  if (pkginfo->depends(false).size()) {
    fileptr << "depends:" << std::endl;
    for (auto const &i : pkginfo->depends(false))
      fileptr << " - " << i << std::endl;
  }

  if (pkginfo->users().size()) {
    fileptr << "users: " << std::endl;
    for (auto const &i : pkginfo->users()) {
      i->print(fileptr);
    }
  }

  if (pkginfo->groups().size()) {
    fileptr << "groups: " << std::endl;
    for (auto const &i : pkginfo->groups()) {
      i->print(fileptr);
    }
  }

  if (pkginfo->install_script().size()) {
    fileptr << "install_script: | " << std::endl;
    std::stringstream ss(pkginfo->install_script());
    std::string line;
    while (std::getline(ss, line, '\n')) fileptr << "  " << line << std::endl;
  }

  std::time_t t = std::time(0);
  std::tm *now = std::localtime(&t);

  fileptr << "installed_on: " << (now->tm_year + 1900) << "/"
          << (now->tm_mon + 1) << "/" << (now->tm_mday) << " " << (now->tm_hour)
          << ":" << (now->tm_min) << std::endl;

  fileptr << "files:" << std::endl;
  for (auto i : files) fileptr << "  - " << i << std::endl;

  fileptr.close();

  return true;
}
}  // namespace rlxos::libpkgupd