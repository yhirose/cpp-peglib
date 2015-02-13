
#ifndef _MMAP_H_
#define _MMAP_H_

#if defined(_MSC_VER)
#include <windows.h>
#else
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#endif

class MemoryMappedFile
{
public:
    MemoryMappedFile(const char* path);
    ~MemoryMappedFile();

    bool is_open() const;
    size_t size() const;
    const char* data() const;

private:
    void cleanup();

#if defined(_MSC_VER)
    HANDLE hFile_;
    HANDLE hMapping_;
#else
    int    fd_;
#endif
    size_t size_;
    void*  addr_;
};

#if defined(_MSC_VER)
#define MAP_FAILED NULL
#endif

inline MemoryMappedFile::MemoryMappedFile(const char* path)
#if defined(_MSC_VER)
    : hFile_(NULL)
    , hMapping_(NULL)
#else
    : fd_(-1)
#endif
    , size_(0)
    , addr_(MAP_FAILED)
{
#if defined(_MSC_VER)
    hFile_ = ::CreateFileA(
        path,
        GENERIC_READ,
        FILE_SHARE_READ,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL);

    if (hFile_ == INVALID_HANDLE_VALUE) {
        std::runtime_error("");
    }

    size_ = ::GetFileSize(hFile_, NULL);

    hMapping_ = ::CreateFileMapping(hFile_, NULL, PAGE_READONLY, 0, 0, NULL);

    if (hMapping_ == NULL) {
        cleanup();
        std::runtime_error("");
    }

    addr_ = ::MapViewOfFile(hMapping_, FILE_MAP_READ, 0, 0, 0);
#else
    fd_ = open(path, O_RDONLY);
    if (fd_ == -1) {
        std::runtime_error("");
    }

    struct stat sb;
    if (fstat(fd_, &sb) == -1) {
        cleanup();
        std::runtime_error("");
    }
    size_ = sb.st_size;

    addr_ = mmap(NULL, size_, PROT_READ, MAP_PRIVATE, fd_, 0);
#endif

    if (addr_ == MAP_FAILED) {
        cleanup();
        std::runtime_error("");
    }
}

inline MemoryMappedFile::~MemoryMappedFile()
{
    cleanup();
}

inline bool MemoryMappedFile::is_open() const
{
    return addr_ != MAP_FAILED;
}

inline size_t MemoryMappedFile::size() const
{
    return size_;
}

inline const char* MemoryMappedFile::data() const
{
    return (const char*)addr_;
}

inline void MemoryMappedFile::cleanup()
{
#if defined(_MSC_VER)
    if (addr_) {
        ::UnmapViewOfFile(addr_);
        addr_ = MAP_FAILED;
    }

    if (hMapping_) {
        ::CloseHandle(hMapping_);
        hMapping_ = NULL;
    }

    if (hFile_ != INVALID_HANDLE_VALUE) {
        ::CloseHandle(hFile_);
        hFile_ = INVALID_HANDLE_VALUE;
    }
#else
    if (addr_ != MAP_FAILED) {
        munmap(addr_, size_);
        addr_ = MAP_FAILED;
    }

    if (fd_ != -1) {
        close(fd_);
        fd_ = -1;
    }
#endif
}

#endif // _MMAP_H_

// vim: et ts=4 sw=4 cin cino={1s ff=unix
