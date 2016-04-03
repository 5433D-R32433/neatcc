/*
 * THE NEATCC C COMPILER
 *
 * This header file is organized as follows:
 *
 * 0. helper functions and data structures
 * 1. ncc.c -> tok.c: the interface for reading tokens
 * 2. ncc.c -> int.c: the interface for generating the intermediate code
 * 3. int.c -> gen.c: the intermediate code
 * 4. gen.c -> x64.c: the interface for generating the final code
 * 5. gen.c -> out.c: the interface for generating object files
 */

/* SECTION ZERO: Helper Functions */
/* predefined array limits; (p.f. means per function) */
#define NSYMS		4096*8		/* number of elf symbols */
#define NRELS		8192*8		/* number of elf relocations */
#define NGLOBALS	2048		/* number of global variables */
#define NLOCALS		1024		/* number of locals p.f. */
#define NARGS		32		/* number of function/macro arguments */
#define NTMPS		64		/* number of expression temporaries */
#define NNUMS		1024		/* number of integer constants p.f. (arm.c) */
#define NJMPS		4096		/* number of jmp instructions p.f. */
#define NFUNCS		1024		/* number of functions */
#define NENUMS		4096		/* number of enum constants */
#define NTYPEDEFS	1024		/* number of typedefs */
#define NSTRUCTS	512		/* number of structs */
#define NFIELDS		128		/* number of fields in structs */
#define NARRAYS		8192*8		/* number of arrays */
#define NLABELS		1024		/* number of labels p.f. */
#define NAMELEN		128		/* size of identifiers */
#define NDEFS		1024		/* number of macros */
#define MARGLEN		1024		/* size of macro arguments */
#define MDEFLEN		2048		/* size of macro definitions */
#define NBUFS		32		/* macro expansion stack depth */
#define NLOCS		1024		/* number of header search paths */

#define LEN(a)		(sizeof(a) / sizeof((a)[0]))
#define ALIGN(x, a)	(((x) + (a) - 1) & ~((a) - 1))
#define MIN(a, b)	((a) < (b) ? (a) : (b))
#define MAX(a, b)	((a) < (b) ? (b) : (a))

void *mextend(void *old, long oldsz, long newsz, long memsz);
void die(char *msg, ...);
void err(char *fmt, ...);

/* variable length buffer */
struct mem {
	char *s;		/* allocated buffer */
	long sz;		/* buffer size */
	long n;			/* length of data stored in s */
};

void mem_init(struct mem *mem);
void mem_done(struct mem *mem);
void mem_cut(struct mem *mem, long pos);
void *mem_buf(struct mem *mem);
void mem_put(struct mem *mem, void *buf, long len);
void mem_putc(struct mem *mem, int c);
void mem_putz(struct mem *mem, long sz);
void mem_cpy(struct mem *mem, long off, void *buf, long len);
long mem_len(struct mem *mem);
void *mem_get(struct mem *mem);

/* SECTION ONE: Tokenisation */
void tok_init(char *path);
void tok_done(void);
char *tok_see(void);		/* return the current token; a static buffer */
char *tok_get(void);		/* return and consume the current token */
long tok_len(void);		/* the length of the last token */
long tok_num(char *tok, long *n);
long tok_addr(void);
void tok_jump(long addr);

int cpp_init(char *path);
void cpp_path(char *s);
void cpp_define(char *name, char *def);
char *cpp_loc(long addr);
int cpp_read(char **buf, long *len);

/* SECTION TWO: Intermediate Code Generation */
/* basic type meaning */
#define T_MSIZE		0x000f
#define T_MSIGN		0x0010
#define T_SZ(bt)	((bt) & T_MSIZE)
#define T_MK(sign, size)	(((sign) & T_MSIGN) | ((size) & T_MSIZE))

/* number of bytes in basic types */
#define ULNG		(LONGSZ)
#define UINT		(4)
#define USHT		(2)
#define UCHR		(1)
/* basic types */
#define SLNG		(ULNG | T_MSIGN)
#define SINT		(UINT | T_MSIGN)
#define SSHT		(USHT | T_MSIGN)
#define SCHR		(UCHR | T_MSIGN)

/* instruction flags */
#define O_FADD		0x000010	/* addition group */
#define O_FSHT		0x000020	/* shift group */
#define O_FMUL		0x000040	/* multiplication group */
#define O_FCMP		0x000080	/* comparison group */
#define O_FUOP		0x000100	/* jump group */
#define O_FCALL		0x000200	/* call group */
#define O_FRET		0x000400	/* return group */
#define O_FJMP		0x000800	/* jump group */
#define O_FIO		0x001000	/* load/store */
#define O_FMEM		0x002000	/* memory operation */
#define O_FMOV		0x004000	/* mov or cast */
#define O_FSYM		0x010000	/* symbol address */
#define O_FLOC		0x020000	/* local address */
#define O_FNUM		0x040000	/* number */
#define O_FVAL		0x080000	/* load a value */
#define O_FJIF		0x100000	/* conditional jump */
#define O_FJX		0x200000	/* jump if zero or nonzero */
#define O_FJCMP		0x400000	/* conditional jump with comparison */
#define O_C(op)		((op) & 0xffffff)	/* operation code */
#define O_T(op)		((op) >> 24)	/* instruction operand type */
#define O_MK(op, bt)	((op) | ((bt) << 24))
/* instruction masks */
#define O_MBOP		(O_FADD | O_FMUL | O_FCMP | O_FSHT)
#define O_MUOP		(O_FUOP)
#define O_MOUT		(O_MBOP | O_MUOP | O_FCALL | O_FMOV | O_FVAL)

/* binary instructions for o_bop() */
#define O_ADD		(0 | O_FADD)
#define O_SUB		(1 | O_FADD)
#define O_AND		(2 | O_FADD)
#define O_OR		(3 | O_FADD)
#define O_XOR		(4 | O_FADD)
#define O_SHL		(0 | O_FSHT)
#define O_SHR		(1 | O_FSHT)
#define O_MUL		(0 | O_FMUL)
#define O_DIV		(1 | O_FMUL)
#define O_MOD		(2 | O_FMUL)
#define O_LT		(0 | O_FCMP)
#define O_GE		(1 | O_FCMP)
#define O_EQ		(2 | O_FCMP)
#define O_NE		(3 | O_FCMP)
#define O_LE		(4 | O_FCMP)
#define O_GT		(5 | O_FCMP)
/* unary instructions for o_uop() */
#define O_NEG		(0 | O_FUOP)
#define O_NOT		(1 | O_FUOP)
#define O_LNOT		(2 | O_FUOP)
/* other instructions */
#define O_CALL		(0 | O_FCALL)
#define O_JMP		(0 | O_FJMP)
#define O_JZ		(1 | O_FJMP | O_FJIF | O_FJX)
#define O_JN		(2 | O_FJMP | O_FJIF | O_FJX)
#define O_RET		(0 | O_FRET)
#define O_MSET		(0 | O_FMEM)
#define O_MCPY		(1 | O_FMEM)
#define O_LOAD		(0 | O_FIO | O_FVAL)
#define O_SAVE		(1 | O_FIO)
#define O_MOV		(0 | O_FMOV)
/* loading values */
#define O_NUM		(0 | O_FNUM | O_FVAL)
#define O_LOC		(0 | O_FLOC | O_FVAL)
#define O_SYM		(0 | O_FSYM | O_FVAL)

/* operations on the stack */
void o_bop(long op);		/* binary operation */
void o_uop(long op);		/* unary operation */
void o_cast(long bt);
void o_memcpy(void);
void o_memset(void);
void o_call(int argc, int ret);
void o_ret(int ret);
void o_assign(long bt);
void o_deref(long bt);
void o_load(void);
int o_popnum(long *num);
int o_popsym(long *sym, long *off);
/* pushing values to the stack */
void o_num(long n);
void o_local(long addr);
void o_sym(char *sym);
void o_tmpdrop(int n);
void o_tmpswap(void);
void o_tmpcopy(void);
/* handling locals */
long o_mklocal(long size);
void o_rmlocal(long addr, long sz);
long o_arg2loc(int i);
/* branches */
void o_label(long id);
void o_jmp(long id);
void o_jz(long id);
long o_mark(void);
void o_back(long mark);
/* data/bss sections */
long o_dsnew(char *name, long size, int global);
void o_dscpy(long addr, void *buf, long len);
void o_dsset(char *name, long off, long bt);
void o_bsnew(char *name, long size, int global);
/* functions */
void o_func_beg(char *name, int argc, int global, int vararg);
void o_func_end(void);
/* output */
void o_write(int fd);

/* SECTION THREE: The Intermediate Code */
/* intermediate code instructions */
struct ic {
	long op;		/* instruction opcode */
	long arg0;		/* first argument; usually destination */
	long arg1;		/* second argument */
	long arg2;		/* more information, like jump target */
	long *args;		/* call arguments */
};

/* get the generated intermediate code */
void ic_get(struct ic **c, long *n);
int ic_num(struct ic *ic, long iv, long *num);
int ic_sym(struct ic *ic, long iv, long *sym, long *off);

/* SECTION FOUR: Final Code Generation */
/*
 * To make maintaining different architectures easier and to unify the
 * optimizations, I have merged the code generation for different
 * architectures.  The i_*() functions are now the low level
 * architecture-specific code generation entry points.  The
 * differences between RISC and CISC architectures, actually the
 * annoying asymmetry in CISC architecture, has made this interface
 * more complex than it could have ideally been.  Nevertheless,
 * the benefits of extracting gen.c and the cleaner design,
 * especially with the presence of the optimizations, outweighs the
 * added complexity.  Overall, there were many challenges for
 * extracting gen.c including:
 * + Different register sets; caller/callee saved and argument registers
 * + CISC-style instructions that work on limited registers and parameters
 * + Different instruction formats and immediate value limitations
 * + Generating epilog, prolog, and local variable addresses when optimizing
 *
 * I tried to make this interface as small as possible.  The key
 * functions and macros described next.
 *
 * i_reg() returns the mask of allowed registers for each
 * operand of an instruction.  The first argument op, specifies
 * the instruction (O_* macros); i_reg() sets the value r0, r1,
 * and r2 to indicate the mask of acceptable registers for the
 * first, second, and third operands of the instruction.
 * The value of these masks may be changed to zero to indicate
 * fewer than three operands.  If md is zero while m1 is not,
 * the destination register should be equal to the first register,
 * as in CISC architectures.  mt denotes the mask of registers
 * that may lose their contents after the instruction.
 *
 * i_ins() generates code for the given instruction.  The arguments
 * indicate the instruction and its operands.  The code is generated
 * by calling os() and oi() functions and the current position in
 * the code segment is obtained by calling opos().  For branch
 * instructions, i_ins() returns the position of branch offset in
 * code segment, to be filled later with i_fill().
 *
 * Some macros should be defined in architecture-dependent headers
 * and a few variables should be defined for each architecture,
 * such as tmpregs, which is an array of register numbers that
 * can be used for holding temporaries and argregs, which is an
 * array of register numbers for holding the first N_ARGS arguments.
 * Consult x64.h as an example, for the macros defined for each
 * architecture.
 *
 */
#ifdef NEATCC_ARM
#include "arm.h"
#endif
#ifdef NEATCC_X64
#include "x64.h"
#endif
#ifdef NEATCC_X86
#include "x86.h"
#endif

/* architecture-specific operations */
long i_reg(long op, long *r0, long *r1, long *r2, long *mt);
long i_ins(long op, long r0, long r1, long r2);
void i_label(long id);
void i_wrap(int argc, int varg, int sargs, int sregs, int initfp, int subsp);
void i_code(char **c, long *c_len, long **rsym, long **rflg, long **roff, long *rcnt);
void i_done(void);

extern int tmpregs[];
extern int argregs[];

/* SECTION FIVE: Object File Generation */
#define OUT_CS		0x0001		/* code segment symbol */
#define OUT_DS		0x0002		/* data segment symbol */
#define OUT_BSS		0x0004		/* bss segment symbol */

#define OUT_GLOB	0x0010		/* global symbol */

#define OUT_RLREL	0x0020		/* relative relocation */
#define OUT_RLSX	0x0040		/* sign extend relocation */
#define OUT_RL24	0x0400		/* 3-byte relocation */
#define OUT_RL32	0x0800		/* 4-byte relocation */

#define OUT_ALIGNMENT	16		/* section alignment */

void out_init(long flags);

long out_sym(char *name);
void out_def(char *name, long flags, long off, long len);
void out_rel(long id, long flags, long off);

void out_write(int fd, char *cs, long cslen, char *ds, long dslen);
