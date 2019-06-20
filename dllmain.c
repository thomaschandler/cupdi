// dllmain.c : Defines the entry point for the DLL application.

#include "os/include/platform.h"
#include "device/device.h"
#include "updi/nvm.h"
#include "dllmain.h"

HANDLE g_hUpdi = NULL;
bool APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
                     )
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
        break;
    case DLL_THREAD_ATTACH:
        break;
    case DLL_THREAD_DETACH:
        break;
    case DLL_PROCESS_DETACH:
        break;
    }
    return true;
}

DLLENTRY int dev_open(const char *port, int baud, const char *devname)
{
    void *nvm_ptr;

    if (!port || !devname)
        return ERROR_PTR;

    dev_close();
    nvm_ptr = (HANDLE)updi_nvm_init(port, baud, devname);
    if (!nvm_ptr)
        return -2;
    
    g_hUpdi = nvm_ptr;
    return 0;
}

DLLENTRY void dev_close(void)
{
    void *nvm_ptr = g_hUpdi;
    if (!nvm_ptr)
        return;

    updi_nvm_deinit(nvm_ptr);
    g_hUpdi = NULL;
}

DLLENTRY int dev_get_device_info(void)
{
    void *nvm_ptr = g_hUpdi;
    if (!nvm_ptr)
        return ERROR_PTR;

    return nvm_get_device_info(nvm_ptr);
}

DLLENTRY int dev_enter_progmode(void)
{
    void *nvm_ptr = g_hUpdi;
    if (!nvm_ptr)
        return ERROR_PTR;

    return nvm_enter_progmode(nvm_ptr);
}

DLLENTRY int dev_leave_progmode(void)
{
    void *nvm_ptr = g_hUpdi;
    if (!nvm_ptr)
        return ERROR_PTR;

    return nvm_leave_progmode(nvm_ptr);
}

DLLENTRY int dev_unlock_device(void)
{
    void *nvm_ptr = g_hUpdi;
    if (!nvm_ptr)
        return ERROR_PTR;

    return nvm_unlock_device(nvm_ptr);
}

DLLENTRY int dev_chip_erase(void)
{
    void *nvm_ptr = g_hUpdi;
    if (!nvm_ptr)
        return ERROR_PTR;

    return nvm_chip_erase(nvm_ptr);
}

DLLENTRY int dev_read_flash(u16 address, u8 *data, int len)
{
    void *nvm_ptr = g_hUpdi;
    if (!nvm_ptr)
        return ERROR_PTR;

    return nvm_read_flash(nvm_ptr, address, data, len);
}

DLLENTRY int dev_write_flash(u16 address, const u8 *data, int len)
{
    void *nvm_ptr = g_hUpdi;
    if (!nvm_ptr)
        return ERROR_PTR;

    return nvm_write_flash(nvm_ptr, address, data, len);
}

DLLENTRY int dev_write_fuse(int fusenum, u8 fuseval)
{
    void *nvm_ptr = g_hUpdi;
    if (!nvm_ptr)
        return ERROR_PTR;

    return nvm_write_fuse(nvm_ptr, fusenum, fuseval);
}

DLLENTRY int dev_read_mem(u16 address, u8 *data, int len)
{
    void *nvm_ptr = g_hUpdi;
    if (!nvm_ptr)
        return ERROR_PTR;

    return nvm_read_mem(nvm_ptr, address, data, len);
}

DLLENTRY int dev_write_mem(u16 address, const u8 *data, int len)
{
    void *nvm_ptr = g_hUpdi;
    if (!nvm_ptr)
        return ERROR_PTR;

    return nvm_write_mem(nvm_ptr, address, data, len);
}

DLLENTRY int dev_get_flash_info(void *info_ptr)
{
    flash_info_t *info = (flash_info_t *)info_ptr;
    void *nvm_ptr = g_hUpdi;
    if (!nvm_ptr)
        return ERROR_PTR;

    return nvm_get_flash_info(nvm_ptr, info);
}

DLLENTRY int dev_set_verbose_level(verbose_t level)
{
    return set_verbose_level(level);
}