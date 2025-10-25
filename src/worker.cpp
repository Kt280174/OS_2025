#include "worker.h"
#include <filesystem>
#include <syslog.h>

namespace fs = std::filesystem;

void Worker::clear_folder(const std::string& path) {
    std::error_code ec;
    for (auto& p : fs::directory_iterator(path, fs::directory_options::skip_permission_denied, ec)) {
        fs::remove_all(p, ec);
    }
}

void Worker::process_task(const Task& t) {
    std::error_code ec;

    if (!fs::exists(t.src, ec)) {
        syslog(LOG_ERR, "Source folder '%s' not found", t.src.c_str());
        return;
    }
    fs::create_directories(t.dst, ec);
    if (ec) {
        syslog(LOG_ERR, "Cannot create destination '%s': %s", t.dst.c_str(), ec.message().c_str());
        return;
    }

    // dọn folder2 rồi tạo 2 thư mục con
    clear_folder(t.dst);
    fs::path ext_dir = fs::path(t.dst) / t.subfolder;
    fs::path oth_dir = fs::path(t.dst) / "OTHERS";
    fs::create_directories(ext_dir, ec);
    fs::create_directories(oth_dir, ec);

    size_t n_ext = 0, n_oth = 0;

    for (auto it = fs::recursive_directory_iterator(
        t.src, fs::directory_options::skip_permission_denied, ec);
        it != fs::recursive_directory_iterator(); ++it)
    {
        if (ec) break;
        if (!it->is_regular_file()) continue;

        const fs::path& f = it->path();
        fs::path dst =
            (f.extension() == t.ext ? ext_dir : oth_dir) / f.filename();

        fs::copy_file(f, dst, fs::copy_options::overwrite_existing, ec);
        if (!ec) {
            if (f.extension() == t.ext) ++n_ext;
            else ++n_oth;
        }
    }

    syslog(LOG_INFO, "Copied from '%s' -> '%s' | %zu *%s + %zu others",
        t.src.c_str(), t.dst.c_str(), n_ext, t.ext.c_str(), n_oth);
}

void Worker::do_work() {
    for (const auto& t : cfg_.tasks) process_task(t);
}
