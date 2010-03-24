/*
    CPU features detection
    Copyright (C) 2010 by Andrew Zabolotny
*/

#include "config.h"
#include "lensfun.h"
#include "lensfunprv.h"

#if defined (__i386__) || defined (__x86_64__)

#if defined (__i386__)
#  define R_AX	"eax"
#  define R_BX	"ebx"
#  define R_CX	"ecx"
#  define R_DX	"edx"
#elif defined (__x86_64__)
#  define R_AX	"rax"
#  define R_BX	"rbx"
#  define R_CX	"rcx"
#  define R_DX	"rdx"
#endif

// Borrowed from RawStudio
guint _lf_detect_cpu_features ()
{
#define cpuid(cmd) \
    asm ( \
        "push %%"R_BX"\n" \
        "cpuid\n" \
        "pop %%"R_BX"\n" \
       : "=a" (ax), "=c" (cx),  "=d" (dx) \
       : "0" (cmd))

    register __SIZE_TYPE__ ax asm (R_AX);
    register __SIZE_TYPE__ bx asm (R_BX);
    register __SIZE_TYPE__ dx asm (R_DX);
    register __SIZE_TYPE__ cx asm (R_CX);
    static GStaticMutex lock = G_STATIC_MUTEX_INIT;
    static guint cpuflags = -1;

    g_static_mutex_lock (&lock);
    if (cpuflags == (guint)-1)
    {
        cpuflags = 0;

        /* Test cpuid presence by checking bit 21 of eflags */
        asm (
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
            : "=r" (ax), "=r" (bx));

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

            /* Is there extensions */
            cpuid (0x80000000);

            if (ax)
            {
                /* Request for extensions */
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
    g_static_mutex_unlock (&lock);

    return cpuflags;

#undef cpuid
}

#else

guint
rs_detect_cpu_features()
{
    return 0;
}

#endif /* __i386__ || __x86_64__ */
