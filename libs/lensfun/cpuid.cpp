/*
    CPU features detection
    Copyright (C) 2010 by Andrew Zabolotny
*/

#include "config.h"
#include "lensfun.h"
#include "lensfunprv.h"

#if defined (_MSC_VER) && !defined(_M_ARM64)
#include <intrin.h>
guint _lf_detect_cpu_features ()
{
    static guint cpuflags = -1;
#if defined(GLIB_CHECK_VERSION) && GLIB_CHECK_VERSION(2,32,0)
    static GMutex lock;

    g_mutex_lock (&lock);
#else
    static GStaticMutex lock = G_STATIC_MUTEX_INIT;

    g_static_mutex_lock (&lock);
#endif
    if (cpuflags == (guint)-1)
    {
        cpuflags = 0;

        int CPUInfo [4] = {-1};
        __cpuid (CPUInfo, 0);
        /* Are there are standard features? */
        if (CPUInfo [0] >= 1)
        {
            /* Get the standard level */
            __cpuid (CPUInfo, 1);
            if (CPUInfo [3] & 0x800000)
                cpuflags |= LF_CPU_FLAG_MMX;
            if (CPUInfo [3] & 0x2000000)
                cpuflags |= LF_CPU_FLAG_SSE;
            if (CPUInfo [3] & 0x4000000)
                cpuflags |= LF_CPU_FLAG_SSE2;
            if (CPUInfo [3] & 0x8000)
                cpuflags |= LF_CPU_FLAG_CMOV;
            if (CPUInfo [2] & 0x1)
                cpuflags |= LF_CPU_FLAG_SSE3;
            if (CPUInfo [2] & 0x200)
                cpuflags |= LF_CPU_FLAG_SSSE3;
            if (CPUInfo [2] & 0x80000)
                cpuflags |= LF_CPU_FLAG_SSE4_1;
            if (CPUInfo [2] & 0x100000)
                cpuflags |= LF_CPU_FLAG_SSE4_2;

            /* Are there extensions? */
            __cpuid (CPUInfo, 0x80000000);
            if (CPUInfo [0] >= 1)
            {
                /* ask extensions */
                __cpuid (CPUInfo, 0x80000001);
                if (CPUInfo [3] & 0x80000000)
                    cpuflags |= LF_CPU_FLAG_3DNOW;
                if (CPUInfo [3] & 0x40000000)
                    cpuflags |= LF_CPU_FLAG_3DNOW_EXT;
                if (CPUInfo [3] & 0x00400000)
                    cpuflags |= LF_CPU_FLAG_AMD_ISSE;
            };
        };
    };
#if defined(GLIB_CHECK_VERSION) && GLIB_CHECK_VERSION(2,32,0)
    g_mutex_unlock (&lock);
#else
    g_static_mutex_unlock (&lock);
#endif

    return cpuflags;
};
#else
#if defined (__i386__) || defined (__x86_64__)

#ifdef __x86_64__
#  define R_AX	"rax"
#  define R_BX	"rbx"
#  define R_CX	"rcx"
#  define R_DX	"rdx"
#else
#  define R_AX	"eax"
#  define R_BX	"ebx"
#  define R_CX	"ecx"
#  define R_DX	"edx"
#endif

// Borrowed from RawStudio
guint _lf_detect_cpu_features ()
{
#define cpuid(cmd) \
    __asm volatile ( \
        "push %%" R_BX "\n" \
        "cpuid\n" \
        "pop %%" R_BX "\n" \
       : "=a" (ax), "=c" (cx),  "=d" (dx) \
       : "0" (cmd))

#ifdef __x86_64__
    guint64 ax, cx, dx, tmp;
#else
    guint32 ax, cx, dx, tmp;
#endif

    static guint cpuflags = -1;
#if defined(GLIB_CHECK_VERSION) && GLIB_CHECK_VERSION(2,32,0)
    static GMutex lock;

    g_mutex_lock (&lock);
#else
    static GStaticMutex lock = G_STATIC_MUTEX_INIT; 

    g_static_mutex_lock (&lock);
#endif
    if (cpuflags == (guint)-1)
    {
        cpuflags = 0;

        /* Test cpuid presence by checking bit 21 of eflags */
        __asm volatile (
            "pushf\n"
            "pop     %0\n"
            "mov     %0, %1\n"
            "xor     $0x00200000, %0\n"
            "push    %0\n"
            "popf\n"
            "pushf\n"
            "pop     %0\n"
            "cmp     %0, %1\n"
            "setne   %%al\n"
            "movzb   %%al, %0\n"
            : "=r" (ax), "=r" (tmp));

        if (ax)
        {
            /* Get the standard level */
            cpuid (0x00000000);

            if (ax)
            {
                /* Request for standard features */
                cpuid (0x00000001);

                if (dx & 0x00800000)
                    cpuflags |= LF_CPU_FLAG_MMX;
                if (dx & 0x02000000)
                    cpuflags |= LF_CPU_FLAG_SSE;
                if (dx & 0x04000000)
                    cpuflags |= LF_CPU_FLAG_SSE2;
                if (dx & 0x00008000)
                    cpuflags |= LF_CPU_FLAG_CMOV;

                if (cx & 0x00000001)
                    cpuflags |= LF_CPU_FLAG_SSE3;
                if (cx & 0x00000200)
                    cpuflags |= LF_CPU_FLAG_SSSE3;
                if (cx & 0x00040000)
                    cpuflags |= LF_CPU_FLAG_SSE4_1;
                if (cx & 0x00080000)
                    cpuflags |= LF_CPU_FLAG_SSE4_2;
            }

            /* Are there extensions? */
            cpuid (0x80000000);

            if (ax)
            {
                /* Ask extensions */
                cpuid (0x80000001);

                if (dx & 0x80000000)
                    cpuflags |= LF_CPU_FLAG_3DNOW;
                if (dx & 0x40000000)
                    cpuflags |= LF_CPU_FLAG_3DNOW_EXT;
                if (dx & 0x00400000)
                    cpuflags |= LF_CPU_FLAG_AMD_ISSE;
            }
        }
    }
#if defined(GLIB_CHECK_VERSION) && GLIB_CHECK_VERSION(2,32,0)
    g_mutex_unlock (&lock);
#else
    g_static_mutex_unlock (&lock);
#endif

    return cpuflags;

#undef cpuid
}

#endif /* __i386__ || __x86_64__ */
#endif
