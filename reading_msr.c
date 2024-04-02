#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#define PERF_SEL_START 0x186U
#define PMC_START 0x0C1U

#define NUM_CPUS 1

int fds[NUM_CPUS];

struct cpu_perf_info{
    
    uint8_t version;
    uint8_t avl_ev_cnt;
    uint32_t unavl_ev_mask;
    uint8_t counter_width;
    uint8_t msr_cnt;

};

struct arch_perf_ev{
    uint8_t umask;
    uint8_t ev_select;
};

const struct arch_perf_ev events[8] = {
                               {.umask      = 0x00U,
                                .ev_select  = 0x3CU},
                               {.umask      = 0x00U,
                                .ev_select  = 0xC0U},
                               {.umask      = 0x01U,
                                .ev_select  = 0x3CU},
                               {.umask      = 0x4FU,
                                .ev_select  = 0x2EU},
                               {.umask      = 0x41U,
                                .ev_select  = 0x2EU},
                               {.umask      = 0x00U,
                                .ev_select  = 0xC4U},
                               {.umask      = 0x00,
                                .ev_select  = 0xC5U},
                               {.umask      = 0x01,
                                .ev_select  = 0xA4U},
                                }; 

void printCpuPerfInfo(struct cpu_perf_info *cpu){
    printf("Version\t\t\t: %u\n",cpu->version); 
    printf("Available Event\t\t: %u\n",cpu->avl_ev_cnt); 
    printf("Counter Bit Width\t: %u\n",cpu->counter_width); 
    printf("MSR Count\t\t: %u\n",cpu->msr_cnt); 
    printf("Unavailable Event Mask\t: %x\n",cpu->unavl_ev_mask);

}


void getCpuPerfInfo(struct cpu_perf_info *cpu){
    uint32_t eax;
    uint32_t mode = 0x0aU;
    uint32_t ebx;

    asm volatile (    "mov %2, %%eax\n\t"
            "CPUID\n\t"
            "mov %%eax, %0\n\t"
            "mov %%ebx, %1"
            : "=r"(eax), "=r"(ebx)
            : "r"(mode));

    cpu->version = eax & 0x000000FF; 
    cpu->avl_ev_cnt = (eax & 0xFF000000) >> 24; 
    cpu->counter_width = (eax & 0x00FF0000) >> 16; 
    cpu->msr_cnt = (eax & 0x0000FF00) >> 8; 
    cpu->unavl_ev_mask = ebx;
}

void setMSR(struct cpu_perf_info *cpu, int cpu_idx, int msr_idx, int ev_idx){
    
    if(msr_idx > cpu->msr_cnt || ev_idx > cpu->avl_ev_cnt || cpu_idx >= NUM_CPUS)
        return;

    if(!fds[cpu_idx]){
        //[TODO] open the fd, not supported yet
        return;
    }

    struct arch_perf_ev event = events[ev_idx];

    uint32_t lower, upper, addr;

    lower = (((uint32_t)event.umask) << 8) | ((uint32_t) event.ev_select);
    upper = 0x000041U;
    addr = PERF_SEL_START + msr_idx * 0x40; 

    uint64_t data = ((uint64_t)upper) << 16 | ((uint64_t)lower);

    if(pwrite(fds[cpu_idx], &data, sizeof(uint64_t), addr) != sizeof(uint64_t)){
        perror("pwrite failed");
        return;
    }

}

uint64_t readCnt(struct cpu_perf_info *cpu, int cpu_idx, int msr_idx, int ev_idx){
    if(msr_idx > cpu->msr_cnt || ev_idx > cpu->avl_ev_cnt || cpu_idx >= NUM_CPUS)
        return 0;

    if(!fds[cpu_idx]){
        //[TODO] open the fd, not supported yet
        return 0;
    }
    
    uint32_t addr = PMC_START + msr_idx * 0x40;

    uint64_t data;

    if(pread(fds[cpu_idx], &data, sizeof(uint64_t), addr) != sizeof(uint64_t)){
        perror("pwread failed");
        return 0;
    }

    return data;
 

}




int main(int argc, char *argv[]){

    struct cpu_perf_info cpu;

    fds[0]= open("/dev/cpu/0/msr", O_WRONLY);

    if(fds[0] < 0){
        perror("Failed to open file");
        return 1;
    }


    getCpuPerfInfo(&cpu);
    printCpuPerfInfo(&cpu);

    setMSR(&cpu, 0, 0, 0);

    while(1){
        sleep(1);
        printf("Clock Cycles: %lu\n", readCnt(&cpu, 0, 0, 0));
    }

    return 0;
}
