#ifndef TC_PLATFORM_INFO_H
#define TC_PLATFORM_INFO_H 1

//=====================//
//=== Platform info ===//
//=====================//
#include "../defines/tc_defines.h"
#include "../time/tc_timer.h"

#include <cpuid.h>
#include <x86intrin.h>
#include <iostream>
#include <cstring>

//FTZ & DAZ - in a block scope!:
//_MM_SET_FLUSH_ZERO_MODE(_MM_FLUSH_ZERO_ON); //FTZ - Sets denormal results from floating-point calculations to zero.
//_MM_SET_DENORMALS_ZERO_MODE(_MM_DENORMALS_ZERO_ON); //DAZ - Treats denormal values used as input to floating-point instructions as zero.
//_mm_setcsr(_mm_getcsr() | 0x8040);

//Check if std::vector can go down fast path for items of type MyClass.
//static_assert(std::is_nothrow_constructible<MyClass>::value, "MyClass should be noexcept Constructible");
//static_assert(std::is_nothrow_copy_constructible<MyClass>::value, "MyClass should be noexcept CopyConstructible");
//static_assert(std::is_nothrow_move_constructible<MyClass>::value, "MyClass should be noexcept MoveConstructible");

namespace platform_info {
    typedef struct cpuid_struct {
        uint32_t eax;
        uint32_t ebx;
        uint32_t ecx;
        uint32_t edx;
    } cpuid_t;
    
    //! Measure the CPU performance. ASSSUMES that the timer is initialised!
    double get_cpu_sips() {
        const double spin_start_time = TCTimer::get_tsc_time();
        const int spin_count = 16777215;// max 16777215
        __m128 x = _mm_setzero_ps();
        for(int i=0; i<spin_count; i++) x = _mm_add_ps(x,_mm_set1_ps(1.0f));
        const int spin_count_result = _mm_cvt_ss2si(x);
        const double spin_end_time = TCTimer::get_tsc_time();
        const double sips = spin_count_result / ((spin_end_time - spin_start_time) * 1000000.0);
        return sips;
    }
    
    std::string get_cpu_brand_string() {
        // std::cerr << "get_cpu_info(): \n";
        char CPUBrandString[64];
        std::memset(CPUBrandString, 0, sizeof(CPUBrandString));
        uint32_t CPUInfo[4] = {0,0,0,0};
        __cpuid(0x80000000, CPUInfo[0], CPUInfo[1], CPUInfo[2], CPUInfo[3]);
        uint32_t nExIds = CPUInfo[0];
        
        for (uint32_t i = 0x80000000; i <= nExIds; ++i) {
            __cpuid(i, CPUInfo[0], CPUInfo[1], CPUInfo[2], CPUInfo[3]);
            
            if (i == 0x80000002) std::memcpy(CPUBrandString, CPUInfo, sizeof(CPUInfo));
            else if (i == 0x80000003) std::memcpy(CPUBrandString + 16, CPUInfo, sizeof(CPUInfo));
            else if (i == 0x80000004) std::memcpy(CPUBrandString + 32, CPUInfo, sizeof(CPUInfo));
            
            //if (i == 0x80000016) {
            //    std::cerr << "  Processor Base Frequency:  " << CPUInfo[0] << " MHz" << "\n";
            //    std::cerr << "  Maximum Frequency:         " << CPUInfo[1] << " MHz" << "\n";
            //    std::cerr << "  Bus (Reference) Frequency: " << CPUInfo[2] << " MHz" << "\n";
            //    std::cerr.flush();
            //}
        }
        
        return CPUBrandString;
    }
    
    std::string get_compiler() {
        char compiler_info[1024];
#if defined(__clang__) && defined(__APPLE__)
        sprintf(compiler_info, "Apple LLVM (clang) %d.%d.%d", __clang_major__, __clang_minor__, __clang_patchlevel__);
#elif defined(__clang__)
        sprintf(compiler_info, "clang %d.%d.%d", __clang_major__, __clang_minor__, __clang_patchlevel__);
#elif defined(__GNUC__)
        sprintf(compiler_info, "gcc %d.%d.%d", __GNUC__, __GNUC_MINOR__, __GNUC_PATCHLEVEL__);
#else
        sprintf(compiler_info, "Unknown compiler.");
#endif
        return compiler_info;
    }
    
    void get_cpuid(cpuid_t *info, unsigned int leaf, unsigned int subleaf) {
        asm volatile("cpuid"
                     : "=a" (info->eax), "=b" (info->ebx), "=c" (info->ecx), "=d" (info->edx)
                     : "a" (leaf), "c" (subleaf));
    }
    
    int get_cpu_display_model()
    {
        cpuid_t info;
        get_cpuid(&info, 1, 0);
        const uint32_t familiy_ID = (info.eax >> 8) & 15;
        const uint32_t model_ID = (info.eax >> 4) & 15;
        const uint32_t ext_model_ID = (info.eax >> 16) & 15;
        const uint32_t display_model_ID = ((familiy_ID==0x06) || (familiy_ID==0x0F)) ? (ext_model_ID << 4) + model_ID : model_ID;
        
        return display_model_ID;
    }
    
    int get_cpu_display_family()
    {
        cpuid_t info;
        get_cpuid(&info, 1, 0);
        const uint32_t familiy_ID = (info.eax >> 8) & 15;
        const uint32_t ext_familiy_ID = (info.eax >> 20) & 255;
        const uint32_t display_family_ID = (familiy_ID==0x0F) ? ext_familiy_ID + familiy_ID : familiy_ID;
        
        return display_family_ID;
    }

    bool is_tsc_invariant() {
        cpuid_t info;
        get_cpuid(&info, 0x80000007, 0);
        return (info.edx & 256);
    }
    
    double get_TSC_freq() {
        double tsc_freq = 0.0;
        if (is_tsc_invariant()) {
            cpuid_t info;
            get_cpuid(&info, 0x15, 0);
            if ((info.ebx != 0) && (info.eax != 0))
            { //It should be possible to calculate the TSC freq from the ART freq.
                //TSC freq = ART_freq * CPUID.15H:EBX[31:0] / CPUID.15H:EAX[31:0];
                
                // ToDo: Some of the special case enumerations aren't handled yet!!!
                
                const double TSC_ART_ratio = info.ebx / double(info.eax);
                const double ART_freq = double(info.ecx);
                tsc_freq = ART_freq * TSC_ART_ratio;
            }
        }
        return tsc_freq;
    }
    
    bool is_intel_cpu() {
        cpuid_t info;
        get_cpuid(&info, 0, 0);
        
        if (std::memcmp((char *) &info.ebx, "Genu", 4) ||
            std::memcmp((char *) &info.edx, "ineI", 4) ||
            std::memcmp((char *) &info.ecx, "ntel", 4)) {
            return false;
        } else {
            return true;
        }
    }
    
    bool is_drng_supported() {
        bool rdrand_supported = false;
        bool rdseed_supported = false;
        
        if (is_intel_cpu()) {
            cpuid_t info;
            get_cpuid(&info, 1, 0);
            
            if ((info.ecx & 0x40000000) == 0x40000000) {
                rdrand_supported = true;
            }
            
            get_cpuid(&info, 7, 0);
            
            if ( (info.ebx & 0x40000) == 0x40000 ) {
                rdseed_supported = true;
            }
        }
        
        return rdrand_supported & rdseed_supported;
    }
}

#endif //TC_PLATFORM_INFO_H
