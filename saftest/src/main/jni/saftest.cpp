// SAFFAL regression test: runs the same file operations through real libc on a normal folder and
// through SAFFAL's intercepts on a SAF folder (and on a normal folder, for pass-through), then
// reports every result that differs from libc's.

#include <jni.h>
#include <dlfcn.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include <android/log.h>

#include <algorithm>
#include <atomic>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#define LOG(...) ((void)__android_log_print(ANDROID_LOG_INFO, "SAFTEST", __VA_ARGS__))

namespace
{

// One set of file functions: libc's or SAFFAL's exports
struct Fs
{
	std::string root;

	FILE *(*fopen_)(const char *, const char *);
	int (*fclose_)(FILE *);
	int (*open_)(const char *, int, mode_t);
	int (*open2_)(const char *, int);
	int (*close_)(int);
	int (*stat_)(const char *, struct stat *);
	int (*access_)(const char *, int);
	int (*mkdir_)(const char *, mode_t);
	int (*remove_)(const char *);
	int (*rename_)(const char *, const char *);
	DIR *(*opendir_)(const char *);
	struct dirent *(*readdir_)(DIR *);
	int (*closedir_)(DIR *);
	int (*scandir_)(const char *, struct dirent ***, int (*)(const struct dirent *), int (*)(const struct dirent **, const struct dirent **));
	int (*chdir_)(const char *);
	char *(*getcwd_)(char *, size_t);

	std::string p(const std::string &rel) const
	{
		return rel.empty() ? root : root + "/" + rel;
	}
};

template <typename T>
bool bindFn(T &fn, void *lib, const char *name)
{
	fn = reinterpret_cast<T>(dlsym(lib, name));
	if (!fn)
		LOG("dlsym(%s) failed", name);
	return fn != nullptr;
}

bool bindAll(Fs &fs, void *lib)
{
	bool ok = true;
	ok &= bindFn(fs.fopen_, lib, "fopen");
	ok &= bindFn(fs.fclose_, lib, "fclose");
	ok &= bindFn(fs.open_, lib, "open");
	ok &= bindFn(fs.open2_, lib, "__open_2");
	ok &= bindFn(fs.close_, lib, "close");
	ok &= bindFn(fs.stat_, lib, "stat");
	ok &= bindFn(fs.access_, lib, "access");
	ok &= bindFn(fs.mkdir_, lib, "mkdir");
	ok &= bindFn(fs.remove_, lib, "remove");
	ok &= bindFn(fs.rename_, lib, "rename");
	ok &= bindFn(fs.opendir_, lib, "opendir");
	ok &= bindFn(fs.readdir_, lib, "readdir");
	ok &= bindFn(fs.closedir_, lib, "closedir");
	ok &= bindFn(fs.scandir_, lib, "scandir");
	ok &= bindFn(fs.chdir_, lib, "chdir");
	ok &= bindFn(fs.getcwd_, lib, "getcwd");
	return ok;
}

std::string errName(int e)
{
	switch (e)
	{
		case 0: return "errno=0";
		case ENOENT: return "ENOENT";
		case EEXIST: return "EEXIST";
		case ENOTDIR: return "ENOTDIR";
		case EISDIR: return "EISDIR";
		case ENOTEMPTY: return "ENOTEMPTY";
		case EACCES: return "EACCES";
		case EPERM: return "EPERM";
		case EBADF: return "EBADF";
		case EINVAL: return "EINVAL";
		case EBUSY: return "EBUSY";
		case EXDEV: return "EXDEV";
		case EROFS: return "EROFS";
		case ENAMETOOLONG: return "ENAMETOOLONG";
		default: return "errno=" + std::to_string(e);
	}
}

std::string failed(int e)
{
	return "fail " + errName(e);
}

// 0 is success; anything else is shown with errno so a non -1 failure (Java's 1) stands out
std::string rc(int r, int e)
{
	if (r == 0)
		return "ok";
	if (r == -1)
		return failed(e);
	return "returned " + std::to_string(r) + " " + errName(e);
}

std::string quote(const std::string &s)
{
	std::string out = "\"";
	for (char c : s)
		out += c == '\n' ? std::string("\\n") : std::string(1, c);
	return out + "\"";
}

std::string readAll(const Fs &fs, const std::string &path, const char *mode = "r")
{
	errno = 0;
	FILE *f = fs.fopen_(path.c_str(), mode);
	if (!f)
		return failed(errno);

	std::string data;
	char buf[1024];
	size_t n;
	while ((n = fread(buf, 1, sizeof(buf), f)) > 0)
		data.append(buf, n);
	bool err = ferror(f) != 0;
	fs.fclose_(f);
	return (err ? "read error " : "") + quote(data);
}

std::string writeFile(const Fs &fs, const std::string &path, const char *mode, const std::string &data)
{
	errno = 0;
	FILE *f = fs.fopen_(path.c_str(), mode);
	if (!f)
		return failed(errno);

	size_t n = fwrite(data.data(), 1, data.size(), f);
	errno = 0;
	int c = fs.fclose_(f);
	if (n != data.size())
		return "short write " + std::to_string(n);
	if (c != 0)
		return "fclose " + failed(errno);
	return "ok";
}

// Mode with read and write: write, rewind, read everything back
std::string writeThenRead(const Fs &fs, const std::string &path, const char *mode, const std::string &data)
{
	errno = 0;
	FILE *f = fs.fopen_(path.c_str(), mode);
	if (!f)
		return failed(errno);

	size_t n = fwrite(data.data(), 1, data.size(), f);
	fflush(f);
	fseek(f, 0, SEEK_SET);
	std::string back;
	char buf[256];
	size_t got;
	while ((got = fread(buf, 1, sizeof(buf), f)) > 0)
		back.append(buf, got);
	fs.fclose_(f);
	return "wrote " + std::to_string(n) + " read " + quote(back);
}

std::string statStr(const Fs &fs, const std::string &path)
{
	struct stat st;
	errno = 0;
	if (fs.stat_(path.c_str(), &st) != 0)
		return failed(errno);
	if (S_ISDIR(st.st_mode))
		return "dir";
	if (S_ISREG(st.st_mode))
		return "file size=" + std::to_string((long long) st.st_size);
	return "other type=" + std::to_string(st.st_mode & S_IFMT);
}

// Distinct files must have distinct inodes, the same file the same one (engines compare dev/ino)
std::string inodeStr(const Fs &fs, const std::string &a, const std::string &b)
{
	struct stat sa, sb, sa2;
	if (fs.stat_(a.c_str(), &sa) || fs.stat_(b.c_str(), &sb) || fs.stat_(a.c_str(), &sa2))
		return failed(errno);
	bool same = sa.st_dev == sa2.st_dev && sa.st_ino == sa2.st_ino;
	bool distinct = sa.st_dev != sb.st_dev || sa.st_ino != sb.st_ino;
	return std::string(same ? "same file same ino" : "same file DIFFERENT ino") + ", " +
		(distinct ? "two files distinct ino" : "two files SAME ino");
}

std::string accessStr(const Fs &fs, const std::string &path, int mode)
{
	errno = 0;
	int r = fs.access_(path.c_str(), mode);
	return rc(r, errno);
}

std::string mkdirStr(const Fs &fs, const std::string &path)
{
	errno = 0;
	int r = fs.mkdir_(path.c_str(), 0770);
	return rc(r, errno);
}

std::string removeStr(const Fs &fs, const std::string &path)
{
	errno = 0;
	int r = fs.remove_(path.c_str());
	return rc(r, errno);
}

std::string renameStr(const Fs &fs, const std::string &from, const std::string &to)
{
	errno = 0;
	int r = fs.rename_(from.c_str(), to.c_str());
	return rc(r, errno);
}

std::string chdirStr(const Fs &fs, const std::string &path)
{
	errno = 0;
	int r = fs.chdir_(path.c_str());
	return rc(r, errno);
}

// A working directory shown relative to the test root, so the libc and SAF runs compare equal
std::string relToRoot(const Fs &fs, const std::string &cwd)
{
	if (cwd.compare(0, fs.root.size(), fs.root) == 0)
		return "<root>" + cwd.substr(fs.root.size());
	return "outside root";
}

std::string cwdStr(const Fs &fs)
{
	char buf[PATH_MAX];
	errno = 0;
	if (!fs.getcwd_(buf, sizeof(buf)))
		return failed(errno);
	return relToRoot(fs, buf);
}

std::string cwdMallocStr(const Fs &fs)
{
	errno = 0;
	char *buf = fs.getcwd_(nullptr, 0);
	if (!buf)
		return failed(errno);
	std::string out = relToRoot(fs, buf);
	free(buf);
	return out;
}

std::string cwdSmallBufferStr(const Fs &fs)
{
	char buf[4];
	errno = 0;
	if (!fs.getcwd_(buf, sizeof(buf)))
		return failed(errno);
	return "returned " + std::string(buf, strnlen(buf, sizeof(buf)));
}

// Q2PRO-style lookup: try each folder in turn, keep going only when the error is ENOENT
std::string searchPaths(const Fs &fs, const std::vector<std::string> &dirs, const std::string &file)
{
	for (const std::string &dir : dirs)
	{
		errno = 0;
		FILE *f = fs.fopen_((dir + "/" + file).c_str(), "rb");
		if (f)
		{
			fs.fclose_(f);
			return "found in " + std::string(dir == dirs.back() ? "last" : "earlier") + " folder";
		}
		if (errno != ENOENT)
			return "stopped by " + errName(errno);
	}
	return "not found";
}

// Ironwail's Sys_fopen: mkdir every folder from "/" (EEXIST is fine if stat says folder), then write
std::string createPathWrite(const Fs &fs, const std::string &path, const std::string &data)
{
	for (size_t i = 1; i < path.size(); i++)
	{
		if (path[i] != '/')
			continue;
		std::string dir = path.substr(0, i);
		errno = 0;
		int r = fs.mkdir_(dir.c_str(), 0777);
		if (r != 0 && errno == EEXIST)
		{
			struct stat st;
			if (fs.stat_(dir.c_str(), &st) == 0 && S_ISDIR(st.st_mode))
				r = 0;
		}
		if (r != 0)
			return "mkdir " + (dir.size() >= fs.root.size() ? relToRoot(fs, dir) : std::string("above root")) + " " + failed(errno);
	}
	return writeFile(fs, path, "w", data);
}

std::string listDir(const Fs &fs, const std::string &path)
{
	errno = 0;
	DIR *d = fs.opendir_(path.c_str());
	if (!d)
		return failed(errno);

	std::vector<std::string> names;
	struct dirent *e;
	while ((e = fs.readdir_(d)) != nullptr)
	{
		std::string n = e->d_name;
		if (n == "." || n == "..")
			continue;
		const char *type = e->d_type == DT_DIR ? "/" : e->d_type == DT_REG ? "" : "?";
		names.push_back(n + type);
	}
	fs.closedir_(d);

	std::sort(names.begin(), names.end());
	std::string out = "[";
	for (size_t i = 0; i < names.size(); i++)
		out += (i ? " " : "") + names[i];
	return out + "]";
}

int txtFilter(const struct dirent *e)
{
	size_t n = strlen(e->d_name);
	return n > 4 && strcmp(e->d_name + n - 4, ".txt") == 0;
}

std::string scanDir(const Fs &fs, const std::string &path)
{
	struct dirent **list = nullptr;
	errno = 0;
	int n = fs.scandir_(path.c_str(), &list, txtFilter, nullptr);
	if (n < 0)
		return failed(errno);

	std::vector<std::string> names;
	for (int i = 0; i < n; i++)
	{
		names.push_back(list[i]->d_name);
		free(list[i]);
	}
	free(list);

	std::sort(names.begin(), names.end());
	std::string out = std::to_string(n) + " [";
	for (size_t i = 0; i < names.size(); i++)
		out += (i ? " " : "") + names[i];
	return out + "]";
}

std::string openWrite(const Fs &fs, const std::string &path, int flags, const std::string &data)
{
	errno = 0;
	int fd = fs.open_(path.c_str(), flags, 0660);
	if (fd < 0)
		return failed(errno);

	ssize_t n = write(fd, data.data(), data.size());
	int e = errno;
	fs.close_(fd);
	if (n != (ssize_t) data.size())
		return "write returned " + std::to_string(n) + " " + errName(e);
	return "ok";
}

std::string openRead(const Fs &fs, const std::string &path, bool viaOpen2 = false)
{
	errno = 0;
	int fd = viaOpen2 ? fs.open2_(path.c_str(), O_RDONLY) : fs.open_(path.c_str(), O_RDONLY, 0);
	if (fd < 0)
		return failed(errno);

	std::string data;
	char buf[1024];
	ssize_t n;
	while ((n = read(fd, buf, sizeof(buf))) > 0)
		data.append(buf, n);
	int e = errno;
	off_t end = lseek(fd, 0, SEEK_END);
	fs.close_(fd);
	return (n < 0 ? "read " + failed(e) + " " : "") + quote(data) + " end=" + std::to_string((long long) end);
}

// Two FILEs on one file need their own positions (unbuffered, so the fd offset is what is read)
std::string twoReaders(const Fs &fs, const std::string &path)
{
	errno = 0;
	FILE *a = fs.fopen_(path.c_str(), "r");
	FILE *b = fs.fopen_(path.c_str(), "r");
	if (!a || !b)
	{
		int e = errno;
		if (a)
			fs.fclose_(a);
		if (b)
			fs.fclose_(b);
		return failed(e);
	}
	setvbuf(a, nullptr, _IONBF, 0);
	setvbuf(b, nullptr, _IONBF, 0);

	char x[8] = {}, y[8] = {}, z[8] = {};
	fread(x, 1, 2, a);
	fread(y, 1, 3, b);
	fread(z, 1, 2, a);
	fs.fclose_(a);
	fs.fclose_(b);
	return std::string(x) + "|" + y + "|" + z;
}

std::string seekTest(const Fs &fs, const std::string &path)
{
	errno = 0;
	FILE *f = fs.fopen_(path.c_str(), "r");
	if (!f)
		return failed(errno);

	fseek(f, 0, SEEK_END);
	long size = ftell(f);
	fseek(f, 1, SEEK_SET);
	char buf[4] = {};
	fread(buf, 1, 3, f);
	fs.fclose_(f);
	return "size=" + std::to_string(size) + " at1=" + quote(buf);
}

// Overwrite the first byte in place
std::string rplusTest(const Fs &fs, const std::string &path)
{
	errno = 0;
	FILE *f = fs.fopen_(path.c_str(), "r+");
	if (!f)
		return failed(errno);

	size_t n = fwrite("Z", 1, 1, f);
	int c = fs.fclose_(f);
	return "wrote " + std::to_string(n) + (c ? " fclose failed" : "");
}

// A reader open while the file is rewritten sees the new contents from its own offset
std::string readerSeesRewrite(const Fs &fs, const std::string &path)
{
	std::string w = writeFile(fs, path, "w", "hello");
	if (w != "ok")
		return "setup " + w;

	errno = 0;
	FILE *f = fs.fopen_(path.c_str(), "r");
	if (!f)
		return failed(errno);
	setvbuf(f, nullptr, _IONBF, 0);

	char a[8] = {}, b[8] = {};
	fread(a, 1, 1, f);
	std::string w2 = writeFile(fs, path, "w", "new");
	fread(b, 1, 7, f);
	fs.fclose_(f);
	return std::string(a) + "|" + b + " (rewrite " + w2 + ")";
}

struct Results
{
	std::vector<std::pair<std::string, std::string>> items;

	void add(const std::string &label, const std::string &result)
	{
		items.emplace_back(label, result);
	}
};

void missingTests(const Fs &fs, Results &r)
{
	r.add("missing: fopen r", readAll(fs, fs.p("missing.txt")));
	r.add("missing: fopen r, missing parent", readAll(fs, fs.p("nodir/missing.txt")));
	r.add("missing: fopen rb, missing grandparent", readAll(fs, fs.p("nodir/sub/missing.txt"), "rb"));
	r.add("missing: fopen r, missing parent again", readAll(fs, fs.p("nodir/missing.txt")));
	r.add("missing: open", openRead(fs, fs.p("missing.txt")));
	r.add("missing: open, missing parent", openRead(fs, fs.p("nodir/missing.txt")));
	r.add("missing: __open_2", openRead(fs, fs.p("missing.txt"), true));
	r.add("missing: stat", statStr(fs, fs.p("missing.txt")));
	r.add("missing: stat, missing parent", statStr(fs, fs.p("nodir/missing.txt")));
	r.add("missing: access F_OK", accessStr(fs, fs.p("missing.txt"), F_OK));
	r.add("missing: access R_OK, missing parent", accessStr(fs, fs.p("nodir/missing.txt"), R_OK));
	r.add("missing: opendir", listDir(fs, fs.p("nodir")));
	r.add("missing: scandir", scanDir(fs, fs.p("nodir")));
	r.add("missing: remove", removeStr(fs, fs.p("missing.txt")));
	r.add("missing: rename", renameStr(fs, fs.p("missing.txt"), fs.p("other.txt")));
	r.add("missing: rename onto existing", writeFile(fs, fs.p("keep.txt"), "w", "keep") + " " + renameStr(fs, fs.p("missing.txt"), fs.p("keep.txt")));
	r.add("missing: target after failed rename", readAll(fs, fs.p("keep.txt")));
	r.add("missing: chdir", chdirStr(fs, fs.p("nodir")));

	// The Q2REPRO failure: a user folder without the file searched before the folder that has it
	r.add("search: setup", mkdirStr(fs, fs.p("search")) + " " + mkdirStr(fs, fs.p("search/user")) + " " + mkdirStr(fs, fs.p("search/data")) + " " +
		mkdirStr(fs, fs.p("search/data/pics")) + " " + writeFile(fs, fs.p("search/data/pics/colormap.pcx"), "w", "pcx"));
	r.add("search: user folder first", searchPaths(fs, {fs.p("search/user"), fs.p("search/data")}, "pics/colormap.pcx"));
	r.add("search: missing folder first", searchPaths(fs, {fs.p("search/nouser"), fs.p("search/data")}, "pics/colormap.pcx"));
	r.add("search: user folder first again", searchPaths(fs, {fs.p("search/user"), fs.p("search/data")}, "pics/colormap.pcx"));
}

// Run twice: in a normal folder and under user_files, which SAFFAL's fd cache treats differently
void fileTests(const Fs &fs, Results &r, const std::string &dir)
{
	auto P = [&](const char *name) { return fs.p(dir + "/" + name); };
	std::string l = dir + ": ";

	r.add(l + "mkdir", mkdirStr(fs, fs.p(dir)));
	r.add(l + "write w", writeFile(fs, P("a.txt"), "w", "hello\n"));
	r.add(l + "stat", statStr(fs, P("a.txt")));
	r.add(l + "access F_OK", accessStr(fs, P("a.txt"), F_OK));
	r.add(l + "access R_OK|W_OK", accessStr(fs, P("a.txt"), R_OK | W_OK));
	r.add(l + "read r", readAll(fs, P("a.txt"), "r"));
	r.add(l + "read rb", readAll(fs, P("a.txt"), "rb"));
	r.add(l + "read r again", readAll(fs, P("a.txt"), "r"));
	r.add(l + "two readers", twoReaders(fs, P("a.txt")));
	r.add(l + "seek", seekTest(fs, P("a.txt")));
	r.add(l + "append a", writeFile(fs, P("a.txt"), "a", "more\n"));
	r.add(l + "read after append", readAll(fs, P("a.txt")));
	r.add(l + "stat after append", statStr(fs, P("a.txt")));
	r.add(l + "truncate w", writeFile(fs, P("a.txt"), "w", "x"));
	r.add(l + "read after truncate", readAll(fs, P("a.txt")));
	r.add(l + "stat after truncate", statStr(fs, P("a.txt")));
	// Each check below has its own file, so one difference doesn't change what later checks read
	r.add(l + "w+ write then read", writeThenRead(fs, P("wp.txt"), "w+", "abc"));
	r.add(l + "a+ append then read", writeFile(fs, P("ap.txt"), "w", "abc") + " " + writeThenRead(fs, P("ap.txt"), "a+", "def"));
	r.add(l + "read after a+", readAll(fs, P("ap.txt")));
	r.add(l + "r+ overwrite first byte", writeFile(fs, P("rp.txt"), "w", "abc") + " " + rplusTest(fs, P("rp.txt")));
	r.add(l + "read after r+", readAll(fs, P("rp.txt")));
	r.add(l + "read other case", writeFile(fs, P("case.txt"), "w", "case") + " " + readAll(fs, P("CASE.TXT")));
	r.add(l + "reader during rewrite", readerSeesRewrite(fs, P("c.txt")));
	r.add(l + "inodes", inodeStr(fs, P("wp.txt"), P("c.txt")));
	r.add(l + "create path from / then write", createPathWrite(fs, P("cp/sub/cp.txt"), "cp") + " " + readAll(fs, P("cp/sub/cp.txt")));
	r.add(l + "create path again", createPathWrite(fs, P("cp/sub/cp.txt"), "cp2") + " " + readAll(fs, P("cp/sub/cp.txt")));
	r.add(l + "read other file afterwards", readAll(fs, P("wp.txt")));
	r.add(l + "remove", removeStr(fs, P("a.txt")));
	r.add(l + "read after remove", readAll(fs, P("a.txt")));
	r.add(l + "stat after remove", statStr(fs, P("a.txt")));
	r.add(l + "access after remove", accessStr(fs, P("a.txt"), F_OK));
	r.add(l + "rename", writeFile(fs, P("r1.txt"), "w", "one") + " " + readAll(fs, P("r1.txt")) + " " + renameStr(fs, P("r1.txt"), P("r2.txt")));
	r.add(l + "read old name", readAll(fs, P("r1.txt")));
	r.add(l + "read new name", readAll(fs, P("r2.txt")));
	r.add(l + "stat new name", statStr(fs, P("r2.txt")));
	r.add(l + "rename over existing", writeFile(fs, P("r3.txt"), "w", "three") + " " + writeFile(fs, P("r4.txt"), "w", "old") + " " +
		readAll(fs, P("r4.txt")) + " " + renameStr(fs, P("r3.txt"), P("r4.txt")));
	r.add(l + "read renamed over", readAll(fs, P("r4.txt")));
	r.add(l + "list", listDir(fs, fs.p(dir)));
}

void fdTests(const Fs &fs, Results &r)
{
	r.add("fd: create O_TRUNC", openWrite(fs, fs.p("o.txt"), O_WRONLY | O_CREAT | O_TRUNC, "abc"));
	r.add("fd: read", openRead(fs, fs.p("o.txt")));
	r.add("fd: O_APPEND", openWrite(fs, fs.p("o.txt"), O_WRONLY | O_APPEND, "def"));
	r.add("fd: read after O_APPEND", openRead(fs, fs.p("o.txt")));
	r.add("fd: O_RDWR overwrite first byte", openWrite(fs, fs.p("rw.txt"), O_WRONLY | O_CREAT | O_TRUNC, "abcdef") + " " + openWrite(fs, fs.p("rw.txt"), O_RDWR, "X"));
	r.add("fd: read after O_RDWR", openRead(fs, fs.p("rw.txt")));
	r.add("fd: O_CREAT|O_EXCL on existing", openWrite(fs, fs.p("ex.txt"), O_WRONLY | O_CREAT | O_TRUNC, "abc") + " " + openWrite(fs, fs.p("ex.txt"), O_WRONLY | O_CREAT | O_EXCL, "E"));
	r.add("fd: read after O_EXCL", openRead(fs, fs.p("ex.txt")));
	r.add("fd: O_CREAT|O_EXCL new file", openWrite(fs, fs.p("ex2.txt"), O_WRONLY | O_CREAT | O_EXCL, "N"));
	r.add("fd: read after O_EXCL create", openRead(fs, fs.p("ex2.txt")));
	r.add("fd: create without O_TRUNC", openWrite(fs, fs.p("n.txt"), O_WRONLY | O_CREAT, "n"));
	r.add("fd: read new", openRead(fs, fs.p("n.txt")));
	r.add("fd: __open_2 read", openRead(fs, fs.p("n.txt"), true));
	r.add("fd: stat", statStr(fs, fs.p("o.txt")));
}

void dirTests(const Fs &fs, Results &r)
{
	r.add("dir: mkdir", mkdirStr(fs, fs.p("d")));
	r.add("dir: mkdir existing", mkdirStr(fs, fs.p("d")));
	r.add("dir: mkdir, missing parent", mkdirStr(fs, fs.p("nodir2/sub")));
	r.add("dir: stat parent of failed mkdir", statStr(fs, fs.p("nodir2")));
	r.add("dir: mkdir on a file", mkdirStr(fs, fs.p("o.txt")));
	r.add("dir: opendir on a file", listDir(fs, fs.p("o.txt")));
	r.add("dir: scandir on a file", scanDir(fs, fs.p("o.txt")));
	r.add("dir: stat", statStr(fs, fs.p("d")));
	r.add("dir: access", accessStr(fs, fs.p("d"), F_OK));
	r.add("dir: fill", writeFile(fs, fs.p("d/1.txt"), "w", "1") + " " + writeFile(fs, fs.p("d/2.txt"), "w", "22") + " " +
		mkdirStr(fs, fs.p("d/s")) + " " + writeFile(fs, fs.p("d/s/3.txt"), "w", "333"));
	r.add("dir: list", listDir(fs, fs.p("d")));
	r.add("dir: scandir *.txt", scanDir(fs, fs.p("d")));
	r.add("dir: list subfolder", listDir(fs, fs.p("d/s")));
	r.add("dir: mkdir empty", mkdirStr(fs, fs.p("empty")));
	r.add("dir: list empty", listDir(fs, fs.p("empty")));
	r.add("dir: scandir empty", scanDir(fs, fs.p("empty")));
	r.add("dir: remove file", removeStr(fs, fs.p("d/1.txt")));
	r.add("dir: list after remove", listDir(fs, fs.p("d")));
	r.add("dir: remove empty folder", removeStr(fs, fs.p("empty")));
	r.add("dir: stat removed folder", statStr(fs, fs.p("empty")));
	r.add("dir: rename file", renameStr(fs, fs.p("d/2.txt"), fs.p("d/two.txt")));
	r.add("dir: list after rename", listDir(fs, fs.p("d")));
	r.add("dir: rename folder", renameStr(fs, fs.p("d/s"), fs.p("d/t")));
	r.add("dir: read in renamed folder", readAll(fs, fs.p("d/t/3.txt")));
	r.add("dir: read old folder path", readAll(fs, fs.p("d/s/3.txt")));
	r.add("dir: move file to other folder", renameStr(fs, fs.p("d/two.txt"), fs.p("d/t/two.txt")));
	r.add("dir: read moved file", readAll(fs, fs.p("d/t/two.txt")));
	r.add("dir: list after move", listDir(fs, fs.p("d/t")));

	// A folder first seen missing, then created
	r.add("dir: read in missing folder", readAll(fs, fs.p("late/f.txt")));
	r.add("dir: mkdir that folder", mkdirStr(fs, fs.p("late")));
	r.add("dir: write in it", writeFile(fs, fs.p("late/f.txt"), "w", "L"));
	r.add("dir: read in it", readAll(fs, fs.p("late/f.txt")));
	r.add("dir: stat in missing folder", statStr(fs, fs.p("late2/g.txt")));
	r.add("dir: mkdir, open O_CREAT in it", mkdirStr(fs, fs.p("late2")) + " " + openWrite(fs, fs.p("late2/g.txt"), O_WRONLY | O_CREAT | O_TRUNC, "G"));
	r.add("dir: stat in it", statStr(fs, fs.p("late2/g.txt")));

	// Own folder: on SAF this may delete the whole tree
	r.add("dir: remove non-empty folder", mkdirStr(fs, fs.p("full")) + " " + writeFile(fs, fs.p("full/x.txt"), "w", "x") + " " + removeStr(fs, fs.p("full")));
	r.add("dir: file in it afterwards", statStr(fs, fs.p("full/x.txt")));
}

void pathTests(const Fs &fs, Results &r)
{
	r.add("path: dot-dot", statStr(fs, fs.p("d/../o.txt")));
	r.add("path: dot", readAll(fs, fs.p("./n.txt")));
	r.add("path: double slash", readAll(fs, fs.p("d//t/3.txt")));
	r.add("path: trailing slash on folder", statStr(fs, fs.p("d/")));
	r.add("path: trailing slash on file", statStr(fs, fs.p("o.txt/")));
	r.add("path: root folder", statStr(fs, fs.p("")));
}

// Relative paths after chdir into the test folder; restores the real working directory
void cwdTests(const Fs &fs, Results &r, const std::string &savedCwd)
{
	r.add("cwd: chdir", chdirStr(fs, fs.p("d")));
	r.add("cwd: getcwd", cwdStr(fs));
	r.add("cwd: read relative", readAll(fs, "t/3.txt"));
	r.add("cwd: stat dot-dot", statStr(fs, "../o.txt"));
	r.add("cwd: write relative", writeFile(fs, "rel.txt", "w", "R"));
	r.add("cwd: read it by full path", readAll(fs, fs.p("d/rel.txt")));
	r.add("cwd: open relative", openRead(fs, "rel.txt"));
	r.add("cwd: access relative", accessStr(fs, "rel.txt", F_OK));
	r.add("cwd: opendir relative", listDir(fs, "t"));
	r.add("cwd: rename relative", renameStr(fs, "rel.txt", "rel2.txt"));
	r.add("cwd: remove relative", removeStr(fs, "rel2.txt"));

	r.add("cwd: fopen relative, missing file", readAll(fs, "nofile.txt"));
	r.add("cwd: fopen relative, missing folder", readAll(fs, "nodir/x.txt"));
	r.add("cwd: open relative, missing folder", openRead(fs, "nodir/x.txt"));
	r.add("cwd: stat relative, missing folder", statStr(fs, "nodir/x.txt"));
	r.add("cwd: access relative, missing file", accessStr(fs, "nofile.txt", F_OK));
	r.add("cwd: opendir relative, missing folder", listDir(fs, "nodir"));
	r.add("cwd: remove relative, missing file", removeStr(fs, "nofile.txt"));
	r.add("cwd: mkdir relative", mkdirStr(fs, "m"));
	r.add("cwd: stat relative new folder", statStr(fs, "m"));
	r.add("cwd: scandir relative", scanDir(fs, "t"));
	r.add("cwd: read ./ path", readAll(fs, "./t/3.txt"));
	r.add("cwd: write ./ path", writeFile(fs, "./m/w.txt", "w", "W"));
	r.add("cwd: read it by full path", readAll(fs, fs.p("d/m/w.txt")));
	r.add("cwd: read relative with ..", readAll(fs, "../d/t/3.txt"));

	r.add("cwd: getcwd NULL buffer", cwdMallocStr(fs));
	r.add("cwd: getcwd too small buffer", cwdSmallBufferStr(fs));

	r.add("cwd: chdir relative", chdirStr(fs, "t"));
	r.add("cwd: getcwd after relative chdir", cwdStr(fs));
	r.add("cwd: read in relative chdir", readAll(fs, "3.txt"));
	r.add("cwd: chdir ..", chdirStr(fs, ".."));
	r.add("cwd: getcwd after ..", cwdStr(fs));
	r.add("cwd: chdir .", chdirStr(fs, "."));
	r.add("cwd: getcwd after .", cwdStr(fs));
	r.add("cwd: chdir relative, missing folder", chdirStr(fs, "nodir"));
	r.add("cwd: getcwd after failed chdir", cwdStr(fs));

	// The working directory is per process, so another thread sees it too
	std::string threadRead, threadMissing;
	std::thread th([&]() {
		threadRead = readAll(fs, "t/3.txt");
		threadMissing = readAll(fs, "nodir/x.txt");
	});
	th.join();
	r.add("cwd: read relative on another thread", threadRead);
	r.add("cwd: missing relative on another thread", threadMissing);

	r.add("cwd: chdir back", chdirStr(fs, savedCwd));

	char buf[PATH_MAX];
	r.add("cwd: getcwd after chdir back", fs.getcwd_(buf, sizeof(buf)) && savedCwd == buf ? "restored" : "not restored");
}

// What an engine sees at launch: Clibs chdir()s into the game folder, then the engine uses ./ paths
void launchTests(const Fs &fs, Results &r, const std::string &savedCwd)
{
	r.add("launch: setup", mkdirStr(fs, fs.p("game")) + " " + mkdirStr(fs, fs.p("game/baseq2")) + " " + mkdirStr(fs, fs.p("game/baseq2/pics")) + " " +
		writeFile(fs, fs.p("game/baseq2/pak0.pak"), "w", "PACK") + " " + writeFile(fs, fs.p("game/baseq2/pics/colormap.pcx"), "w", "pcx") + " " +
		mkdirStr(fs, fs.p("game/user")) + " " + mkdirStr(fs, fs.p("game/user/baseq2")));
	r.add("launch: chdir game folder", chdirStr(fs, fs.p("game")));
	r.add("launch: getcwd", cwdStr(fs));
	r.add("launch: read ./baseq2/pak0.pak", readAll(fs, "./baseq2/pak0.pak", "rb"));
	r.add("launch: stat ./baseq2/pak0.pak", statStr(fs, "./baseq2/pak0.pak"));
	r.add("launch: stat ./baseq2", statStr(fs, "./baseq2"));
	r.add("launch: opendir ./baseq2", listDir(fs, "./baseq2"));
	r.add("launch: open baseq2/pak0.pak", openRead(fs, "baseq2/pak0.pak"));
	r.add("launch: search ./user/baseq2 then ./baseq2", searchPaths(fs, {"./user/baseq2", "./baseq2"}, "pics/colormap.pcx"));
	r.add("launch: search ./nouser/baseq2 then ./baseq2", searchPaths(fs, {"./nouser/baseq2", "./baseq2"}, "pics/colormap.pcx"));
	r.add("launch: search, file nowhere", searchPaths(fs, {"./user/baseq2", "./baseq2"}, "pics/missing.pcx"));
	r.add("launch: stat ../game/baseq2/pak0.pak", statStr(fs, "../game/baseq2/pak0.pak"));
	r.add("launch: write ./user/baseq2/config.cfg", writeFile(fs, "./user/baseq2/config.cfg", "w", "cfg"));
	r.add("launch: read ./user/baseq2/config.cfg", readAll(fs, "./user/baseq2/config.cfg"));
	r.add("launch: chdir back", chdirStr(fs, savedCwd));
}

// More files than the fd cache holds (256, or 1024 with prefetch), then threads reading them
void cacheTests(const Fs &fs, Results &r)
{
	const int count = 270;
	char name[64];

	r.add("cache: mkdir", mkdirStr(fs, fs.p("c")));

	int writeErrors = 0, readErrors = 0;
	for (int i = 0; i < count; i++)
	{
		snprintf(name, sizeof(name), "c/f%03d.txt", i);
		writeErrors += writeFile(fs, fs.p(name), "w", std::to_string(i)) != "ok";
	}
	for (int pass = 0; pass < 2; pass++)
	{
		for (int i = 0; i < count; i++)
		{
			snprintf(name, sizeof(name), "c/f%03d.txt", i);
			readErrors += readAll(fs, fs.p(name)) != quote(std::to_string(i));
		}
	}
	r.add("cache: write 270, read twice", "write errors " + std::to_string(writeErrors) + ", read errors " + std::to_string(readErrors));

	r.add("cache: rewrite after cached read", writeFile(fs, fs.p("c/f000.txt"), "w", "changed") + " " + readAll(fs, fs.p("c/f000.txt")));

	std::atomic<int> errors(0);
	std::vector<std::thread> threads;
	for (int t = 0; t < 4; t++)
	{
		threads.emplace_back([&fs, &errors, t]() {
			char n[64];
			for (int i = 0; i < 40; i++)
			{
				int k = 1 + (t * 7 + i) % 12;
				snprintf(n, sizeof(n), "c/f%03d.txt", k);
				if (readAll(fs, fs.p(n)) != quote(std::to_string(k)))
					errors++;
			}
		});
	}
	for (auto &th : threads)
		th.join();
	r.add("cache: 4 threads x 40 reads", "errors " + std::to_string(errors.load()));
}

Results runAll(const Fs &fs, const std::string &savedCwd)
{
	Results r;
	r.add("setup: mkdir root", mkdirStr(fs, fs.root));
	missingTests(fs, r);
	fileTests(fs, r, "files");
	r.add("setup: mkdir user_files", mkdirStr(fs, fs.p("user_files")));
	fileTests(fs, r, "user_files/test");
	fdTests(fs, r);
	dirTests(fs, r);
	pathTests(fs, r);
	cwdTests(fs, r, savedCwd);
	launchTests(fs, r, savedCwd);
	cacheTests(fs, r);
	return r;
}

long long nowMs()
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return ts.tv_sec * 1000LL + ts.tv_nsec / 1000000;
}

// Appends one line per mismatch; returns the number of mismatches
int compare(const std::string &what, const Results &expected, const Results &got, std::string &report)
{
	int fails = 0;
	for (size_t i = 0; i < expected.items.size(); i++)
	{
		const std::string &label = expected.items[i].first;
		const std::string &want = expected.items[i].second;
		const std::string have = i < got.items.size() ? got.items[i].second : "(missing)";
		if (want != have)
		{
			std::string line = "FAIL " + what + " | " + label + " | libc: " + want + " | saffal: " + have;
			LOG("%s", line.c_str());
			report += line + "\n";
			fails++;
		}
	}
	return fails;
}

std::string jstr(JNIEnv *env, jstring s)
{
	const char *c = env->GetStringUTFChars(s, nullptr);
	std::string out = c;
	env->ReleaseStringUTFChars(s, c);
	return out;
}

} // namespace

extern "C" JNIEXPORT jstring JNICALL
Java_com_opentouchgaming_saftest_MainActivity_runSuite(JNIEnv *env, jclass, jstring jRealBase, jstring jSafBase, jstring jMode)
{
	const std::string realBase = jstr(env, jRealBase);
	const std::string safBase = jstr(env, jSafBase);
	const std::string mode = jstr(env, jMode);

	void *libc = dlopen("libc.so", RTLD_NOW);
	void *saffal = dlopen("libsaffal.so", RTLD_NOW);

	Fs real, saf, pass;
	real.root = realBase + "/libc";
	saf.root = safBase + "/saffal";
	pass.root = realBase + "/passthrough";

	if (!libc || !saffal || !bindAll(real, libc) || !bindAll(saf, saffal) || !bindAll(pass, saffal))
		return env->NewStringUTF("SAFTEST ERROR could not load libc or libsaffal functions");

	char cwd[PATH_MAX];
	std::string savedCwd = real.getcwd_(cwd, sizeof(cwd)) ? cwd : "/";

	LOG("mode %s: libc in %s", mode.c_str(), real.root.c_str());
	long long t0 = nowMs();
	Results expected = runAll(real, savedCwd);
	long long t1 = nowMs();
	LOG("mode %s: saffal in %s", mode.c_str(), saf.root.c_str());
	Results onSaf = runAll(saf, savedCwd);
	long long t2 = nowMs();
	LOG("mode %s: saffal pass-through in %s", mode.c_str(), pass.root.c_str());
	Results passThrough = runAll(pass, savedCwd);
	long long t3 = nowMs();

	std::string fails;
	int safFails = compare(mode + " SAF", expected, onSaf, fails);
	int passFails = compare(mode + " pass-through", expected, passThrough, fails);
	int total = (int) expected.items.size();

	char summary[512];
	snprintf(summary, sizeof(summary),
		"SAFTEST mode=%s checks=%d SAF: %d differ (%lld ms), pass-through: %d differ (%lld ms), libc %lld ms",
		mode.c_str(), total, safFails, t2 - t1, passFails, t3 - t2, t1 - t0);
	LOG("%s", summary);

	return env->NewStringUTF((std::string(summary) + "\n" + fails).c_str());
}
