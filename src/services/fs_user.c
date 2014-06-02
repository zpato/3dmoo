#include "util.h"
#include "handles.h"
#include "archives.h"
#include "mem.h"

#include "service_macros.h"

/* _____ File Server Service _____ */

static u32 priority;


SERVICE_START(fs_user);

SERVICE_CMD(0x08010002) { // Initialize
    DEBUG("Initialize\n");

    RESP(1, 0); // Result
    return 0;

}

SERVICE_CMD(0x080201C2) { // OpenFile
    u32 transaction       = CMD(1);
    u32 handle_arch       = CMD(2);
    u32 handle_arch_hi    = CMD(3);
    u32 file_lowpath_type = CMD(4);
    u32 file_lowpath_sz   = CMD(5);
    u32 flags             = CMD(6);
    u32 attr              = CMD(7);
    u32 file_lowpath_desc = CMD(8);
    u32 file_lowpath_ptr  = CMD(9);

    char tmp[256];

    DEBUG("OpenFile\n");
    DEBUG("   archive_handle=%08x\n",
          handle_arch);
    DEBUG("   flags=%s\n",
          fs_FlagsToString(flags, tmp));
    DEBUG("   file_lowpath_type=%s\n",
          fs_PathTypeToString(file_lowpath_type));
    DEBUG("   file_lowpath=%s\n",
          fs_PathToString(file_lowpath_type, file_lowpath_ptr, file_lowpath_sz, tmp, sizeof(tmp)));
    DEBUG("   attr=%s\n",
          fs_AttrToString(flags, tmp));

    handleinfo* arch_hi = handle_Get(handle_arch);

    if(arch_hi == NULL) {
        ERROR("Invalid handle.\n");
        RESP(1, -1);
        return 0;
    }

    archive* arch = (archive*) arch_hi->subtype;
    u32 file_handle = arch->fnOpenFile(
        (file_path) { file_lowpath_type, file_lowpath_sz, file_lowpath_ptr },
        flags, attr);

    RESP(1, 0); // Result
    RESP(2, file_handle); // File handle
    return 0;
}

SERVICE_CMD(0x08030204) { // OpenFileDirectly
    u32 transaction       = CMD(1);
    u32 arch_id           = CMD(2);
    u32 arch_lowpath_type = CMD(3);
    u32 arch_lowpath_sz   = CMD(4);
    u32 file_lowpath_type = CMD(5);
    u32 file_lowpath_sz   = CMD(6);
    u32 flags             = CMD(7);
    u32 attr              = CMD(8);
    u32 arch_lowpath_desc = CMD(9);
    u32 arch_lowpath_ptr  = CMD(10);
    u32 file_lowpath_desc = CMD(11);
    u32 file_lowpath_ptr  = CMD(12);

    char tmp[256];
    archive_type* arch_type = fs_GetArchiveTypeById(arch_id);

    DEBUG("OpenFileDirectly\n");
    DEBUG("   archive_id=%x (%s)\n",
          arch_id, arch_type == NULL ? "unknown" : arch_type->name);
    DEBUG("   flags=%s\n",
          fs_FlagsToString(flags, tmp));
    DEBUG("   arch_lowpath_type=%s\n",
          fs_PathTypeToString(arch_lowpath_type));
    DEBUG("   arch_lowpath=%s\n",
          fs_PathToString(arch_lowpath_type, arch_lowpath_ptr, arch_lowpath_sz, tmp, sizeof(tmp)));
    DEBUG("   file_lowpath_type=%s\n",
          fs_PathTypeToString(file_lowpath_type));
    DEBUG("   file_lowpath=%s\n",
          fs_PathToString(file_lowpath_type, file_lowpath_ptr, file_lowpath_sz, tmp, sizeof(tmp)));
    DEBUG("   attr=%s\n",
          fs_AttrToString(flags, tmp));

    if(arch_type != NULL && arch_type->fnOpenArchive != NULL) {
        archive* arch = arch_type->fnOpenArchive(
            (file_path) { arch_lowpath_type, arch_lowpath_sz, arch_lowpath_ptr});

        if(arch != NULL) {
            u32 file_handle = arch->fnOpenFile(
                (file_path) { file_lowpath_type, file_lowpath_sz, file_lowpath_ptr },
                flags, attr);

            RESP(1, 0); // Result
            RESP(3, file_handle); // File handle

            arch->fnDeinitialize();
        }
        else {
            ERROR("OpenArchive failed\n");
            RESP(1, -1);
        }
    }
    else {
        ERROR("Archive type %x not fully implemented\n", arch_id)
        RESP(1, -1);
    }

    return 0;
}

SERVICE_CMD(0x080C00C2) { // OpenArchive
    u32 arch_id           = CMD(1);
    u32 arch_lowpath_type = CMD(2);
    u32 arch_lowpath_sz   = CMD(3);
    u32 arch_lowpath_desc = CMD(4);
    u32 arch_lowpath_ptr  = CMD(5);

    char tmp[256];
    archive_type* arch_type = fs_GetArchiveTypeById(arch_id);

    DEBUG("OpenArchive\n");
    DEBUG("   archive_id=%x (%s)\n",
          arch_id, arch_type == NULL ? "unknown" : arch_type->name);
    DEBUG("   arch_lowpath_type=%s\n",
          fs_PathTypeToString(arch_lowpath_type));
    DEBUG("   arch_lowpath=%s\n",
          fs_PathToString(arch_lowpath_type, arch_lowpath_ptr, arch_lowpath_sz, tmp, sizeof(tmp)));

    if(arch_type == NULL) {
        ERROR("Unknown archive type: %x.\n", arch_id);
        RESP(1, -1);
        return 0;
    }

    if(arch_type->fnOpenArchive == NULL) {
        ERROR("Archive type %x has not implemented OpenArchive\n", arch_id);
        RESP(1, -1);
        return 0;
    }

    archive* arch = arch_type->fnOpenArchive(
        (file_path) { arch_lowpath_type, arch_lowpath_sz, arch_lowpath_ptr});

    if(arch == NULL) {
        ERROR("OpenArchive failed\n");
        RESP(1, -1);
        return 0;
    }

    u32 arch_handle = handle_New(HANDLE_TYPE_ARCHIVE, (uintptr_t) &arch);

    RESP(1, 0); // Result
    RESP(2, arch_handle); // Arch handle lo
    RESP(3, 0xDEADC0DE);  // Arch handle hi (not used)
    return 0;
}

SERVICE_CMD(0x080E0080) { // CloseArchive
    DEBUG("CloseArchive -- TODO --\n");
    return 0;
}

SERVICE_CMD(0x08610042) { // InitializeWithSdkVersion
    DEBUG("InitializeWithSdkVersion -- TODO --\n");

    RESP(1, 0); // Result
    return 0;
}

SERVICE_CMD(0x08620040) { // SetPriority
    priority = CMD(1);
    DEBUG("SetPriority, prio=%x\n", priority);

    RESP(1, 0);
    return 0;
}

SERVICE_CMD(0x08630000) { // GetPriority
    DEBUG("GetPriority\n");

    RESP(1, priority);
    return 0;
}

SERVICE_CMD(0x08170000) { // IsSdmcDetected
    DEBUG("IsSdmcDetected - STUBBED -\n");

    RESP(1, 0);
    return 0;
}

SERVICE_END();



/* ____ File Service ____ */

SERVICE_START(file);

SERVICE_CMD(0x080200C2) { // Read
    u32 rc, read;
    u64 off = CMD(1) | ((u64) CMD(2)) << 32;
    u64 sz  = CMD(3);
    u64 ptr = CMD(5);

    DEBUG("Read, off=%llx, len=%x\n", off, sz);

    file_type* type = (file_type*) h->subtype;

    if(type->fnRead != NULL) {
        rc = type->fnRead(ptr, sz, off, &read);
    }
    else {
        ERROR("Read() not implemented for this type.\n");
        rc = -1;
        read = 0;
    }

    RESP(1, rc); // Result
    RESP(2, read);
    return 0;
}

SERVICE_CMD(0x08030102) { // Write
    DEBUG("Write\n");
    return 0;
}

SERVICE_CMD(0x08040000) { // GetSize
    DEBUG("GetSize\n");
    return 0;
}

SERVICE_CMD(0x08080000) { // Close
    u32 rc = 0;
    file_type* type = (file_type*) h->subtype;

    DEBUG("Close\n");

    if(type->fnClose != NULL)
        rc = type->fnClose();

    RESP(1, rc);
    return 0;
}

SERVICE_END();

u32 file_CloseHandle(ARMul_State *state, handleinfo* h) {
    DEBUG("file_CloseHandle\n");
    return 0;
}
