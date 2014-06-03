enum {
    OPEN_READ  =1,
    OPEN_WRITE =2,
    OPEN_CREATE=4
};

enum {
    ATTR_ISREADONLY=1,
    ATTR_ISARCHIVE =0x100,
    ATTR_ISHIDDEN  =0x10000,
    ATTR_ISDIR     =0x1000000
};

enum {
    PATH_INVALID,
    PATH_EMPTY,
    PATH_BINARY,
    PATH_CHAR,
    PATH_WCHAR
};

typedef struct {
    u32 type;
    u32 size;
    u32 ptr;
} file_path;

typedef struct {
    bool (*fnFileExists)  (file_path path);
    u32  (*fnOpenFile)    (file_path path, u32 flags, u32 attr);
    void (*fnDeinitialize)();
} archive;

typedef struct {
    const char* name;
    u32 id;
    archive* (*fnOpenArchive)(file_path path);
} archive_type;

typedef struct {
    u32 (*fnRead) (u32 ptr, u32 sz, u64 off, u32* read_out);
    u32 (*fnWrite)(u32 ptr, u32 sz, u64 off, u32* written_out);
    u64 (*fnGetSize)();
    u32 (*fnClose)();
} file_type;


// archives/romfs.c
archive* romfs_OpenArchive(file_path path);
void romfs_Setup(FILE* fd, u32 off, u32 sz);

static archive_type archive_types[] =  {
    { "RomFS",
      3,
      romfs_OpenArchive
    },
    { "SaveData",
      4,
      NULL
    },
    { "ExtData",
      6,
      NULL
    },
    { "SharedExtData",
      7,
      NULL
    },
    { "SysData",
      8,
      NULL
    },
    { "SDMC",
      9,
      NULL
    }
};

// archives/fs_util.c
archive_type* fs_GetArchiveTypeById(u32 id);
const char* fs_FlagsToString(u32 flags, char* buf_out);
const char* fs_AttrToString(u32 attr, char* buf_out);
const char* fs_PathTypeToString(u32 type);
const char* fs_PathToString(u32 type, u32 ptr, u32 size,
    char* buf_out, size_t size_out);

