#pragma once
#include <string>
#include <sys/stat.h>
#include <limits.h>
#include <stdlib.h>
#include <unistd.h>

inline std::string to_abs_path(const std::string& path) {
	char buf[PATH_MAX];
	if (realpath(path.c_str(), buf)) return std::string(buf);
	return path;
}

inline bool file_exists(const std::string& p) {
	struct stat st{};
	return ::stat(p.c_str(), &st) == 0;
}

inline bool proc_pid_exists(pid_t pid) {
	if (pid <= 0) return false;
	std::string proc = "/proc/" + std::to_string(pid);
	return file_exists(proc);
}