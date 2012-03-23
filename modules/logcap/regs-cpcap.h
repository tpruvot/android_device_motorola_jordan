#ifndef _LOGCAP_REGS_CPCAP_H_
#define _LOGCAP_REGS_CPCAP_H_

const char* capcap_regname(short reg);
void capcap_dumpnames(int min, int max);

typedef struct _register_info_tbl {
        unsigned short address;         /* Address of the register */
        unsigned short constant_mask;   /* Constant modifiability mask */
        unsigned short rbw_mask;        /* Read-before-write mask */
} st_register_info_tbl;

// in cpcap-regacc.c
extern const st_register_info_tbl logcap_register_info_tbl[CPCAP_NUM_REG_CPCAP];

#endif
