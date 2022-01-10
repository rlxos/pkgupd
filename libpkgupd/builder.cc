#include "builder.hh"

#include <iostream>

#include "archive.hh"
#include "compiler.hh"
#include "downloader.hh"
#include "exec.hh"
#include "image.hh"
#include "stripper.hh"
#include "tar.hh"

namespace rlxos::libpkgupd {

bool Builder::_prepare(std::vector<std::string> const &sources,
                       std::string const &srcdir) {
  for (auto const &i : sources) {
    std::string pkgfile = std::filesystem::path(i).filename().string();
    std::string url = i;

    size_t idx = i.rfind("::");
    if (idx != std::string::npos) {
      pkgfile = i.substr(0, idx);
      url = i.substr(idx + 2, i.length() - (idx + 2));
    }

    std::string outfile = _src_dir + "/" + pkgfile;

    auto downloader_ = Downloader();

    if (!std::filesystem::exists(outfile)) {
      if (!downloader_.download(url, outfile)) {
        _error = downloader_.error();
        return false;
      }
    }

    auto endswith = [](std::string const &fullstr, std::string const &ending) {
      if (fullstr.length() >= ending.length())
        return (0 == fullstr.compare(fullstr.length() - ending.length(),
                                     ending.length(), ending));
      else
        return false;
    };

    bool extracted = false;

    for (auto const &i : {".tar", ".gz", ".tgz", ".xz", ".txz", ".bzip2", ".bz",
                          ".bz2", ".lzma"}) {
      if (endswith(outfile, i)) {
        if (int status =
                Executor().execute("tar -xPf " + outfile + " -C " + srcdir);
            status != 0) {
          _error = "failed to extract " + outfile +
                   " with tar, exit status: " + std::to_string(status);
          return false;
        }
        extracted = true;
        break;
      }
    }

    if (endswith(outfile, "zip")) {
      if (int status = Executor().execute("unzip " + outfile + " -d " + srcdir);
          status != 0) {
        _error = "failed to extract " + outfile +
                 " with unzip, exit status:  " + std::to_string(status);
        return false;
      }
      extracted = true;
    }

    if (!extracted) {
      std::error_code err;
      std::filesystem::copy(
          outfile,
          srcdir + "/" + std::filesystem::path(outfile).filename().string(),
          err);
      if (err) {
        _error = err.message();
        return false;
      }
    }
  }

  return true;
}

bool Builder::_compile(std::string const &srcdir, std::string const &destdir,
                       std::shared_ptr<Recipe::Package> package) {
  auto compiler_ = Compiler(package);
  if (!compiler_.compile(srcdir, destdir)) {
    _error = compiler_.error();
    return false;
  }

  return true;
}

bool Builder::_build(std::shared_ptr<Recipe::Package> package) {
  std::string pkg_work_dir = _work_dir + "/" + package->id();
  std::string pkg_src_dir = pkg_work_dir + "/src";
  std::string pkg_pkg_dir = pkg_work_dir + "/pkg";

  std::string pkgfile = _pkgs_dir + "/" + package->packagefile();
  if (std::filesystem::exists(pkgfile) && !_force) {
    std::cout << "Found in cache, skipping" << std::endl;
    if (package->pack() != "none") {
      _archive_list.push_back(pkgfile);
    }
    return true;
  }

  for (auto const &dir : {pkg_src_dir}) {
    std::error_code err;
    std::filesystem::create_directories(dir, err);
    if (err) {
      _error = err.message();
      return false;
    }
  }

  auto allSources = package->sources();

  if (!_prepare(allSources, pkg_src_dir)) {
    return false;
  }

  // Inserting required environment variables
  package->prepand_environ("PKGUPD_SRCDIR=" + _src_dir);
  package->prepand_environ("PKGUPD_PKGDIR=" + _pkgs_dir);
  package->prepand_environ("pkgupd_srcdir=" + pkg_src_dir);
  package->prepand_environ("pkgupd_pkgdir=" + pkg_pkg_dir);

  if (package->prescript().size()) {
    if (int status = Executor().execute(package->prescript(),
                                    pkg_src_dir + "/" + package->dir(),
                                    package->environ());
        status != 0) {
      _error = "prescript failed to exit code: " + std::to_string(status);
      return false;
    }
  }

  PROCESS("compiling source code")

  if (!_compile(pkg_src_dir + "/" + package->dir(), pkg_pkg_dir, package)) {
    return false;
  }

  if (package->postscript().size()) {
    if (int status = Executor().execute(package->postscript(),
                                    pkg_src_dir + "/" + package->dir(),
                                    package->environ());
        status != 0) {
      _error = "postscript failed to exit code: " + std::to_string(status);
      return false;
    }
  }

  for (auto const &i :
       std::filesystem::recursive_directory_iterator(pkg_pkg_dir)) {
    if (i.is_regular_file() && i.path().filename().extension() == ".la") {
      DEBUG("removing " + i.path().string());
      std::filesystem::remove(i);
    }
  }

  if (package->strip()) {
    Stripper stripper_(package->skipstrip());

    PROCESS("stripping " + package->id());
    if (!stripper_.strip(pkg_pkg_dir)) {
      ERROR(stripper_.error());
    }
  }

  if (package->pack() == "none") {
    INFO("No packaging done");
  } else {
    std::shared_ptr<Archive> archive_;
    if (package->pack() == "rlx") {
      PROCESS("packaging rlx archive");
      archive_ = std::make_shared<Tar>(pkgfile);
    } else if (package->pack() == "app") {
      PROCESS("packaging app image");
      archive_ = std::make_shared<Image>(pkgfile);
    } else {
      _error = "unsupport packaging format specified '" + package->pack() + "'";
      return false;
    }

    if (!archive_->compress(pkg_pkg_dir, package)) {
      _error = archive_->error();
      return false;
    }

    INFO("Package generated at " + pkgfile)

    if (package->parent()->split()) {
      if (!_installer.install({pkgfile}, _root_dir, _skip_triggers, _force)) {
        _error = _installer.error();
        return false;
      }
    } else {
      _archive_list.push_back(pkgfile);
    }
  }

  return true;
}
}  // namespace rlxos::libpkgupd