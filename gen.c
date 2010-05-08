#include "gen.h"
#include "tok.h"

#define TMP_CONST	1
#define TMP_ADDR	2

#define R_RAX		0x00
#define R_RCX		0x01
#define R_RDX		0x02
#define R_RBX		0x03
#define R_RSP		0x04
#define R_RBP		0x05
#define R_RSI		0x06
#define R_RDI		0x07
#define R_R8		0x08
#define R_R9		0x09
#define R_R10		0x10
#define R_R11		0x11

#define MOV_M2R		0x8b
#define MOV_R2X		0x89
#define ADD_R2R		0x01
#define SUB_R2R		0x29
#define SHX_REG		0xd3
#define CMP_R2R		0x38
#define LEA_M2R		0x8d

#define TMP_BT(t)		((t)->type == TMP_ADDR ? 8 : (t)->bt)

static char buf[SECSIZE];
static char *cur;
static long sp;
static long spsub_addr;
static long maxsp;

static struct tmp {
	long addr;
	int type;
	unsigned bt;
} tmp[MAXTMP];
static int ntmp;

static void putint(char *s, long n, int l)
{
	while (l--) {
		*s++ = n;
		n >>= 8;
	}
}

static void os(char *s, int n)
{
	while (n--)
		*cur++ = *s++;
}

static void oi(long n, int l)
{
	while (l--) {
		*cur++ = n;
		n >>= 8;
	}
}

static void o_op(int op, int r1, int r2, unsigned bt)
{
	int rex = 0;
	if (r1 & 0x8)
		rex |= 4;
	if (r2 & 0x8)
		rex |= 1;
	if (rex || (bt & BT_SZMASK) == 8)
		oi(0x48 | rex, 1);
	if ((bt & BT_SZMASK) == 2)
		oi(0x66, 1);
	if ((bt & BT_SZMASK) == 1)
		op &= ~0x1;
	oi(op, 1);
}

static void memop(int op, int src, int base, int off, unsigned bt)
{
	int dis = off == (char) off ? 1 : 4;
	int mod = dis == 4 ? 2 : 1;
	o_op(op, src, base, bt);
	if (!off)
		mod = 0;
	oi((mod << 6) | ((src & 0x07) << 3) | (base & 0x07), 1);
	if (off)
		oi(off, dis);
}

static void regop(int op, int src, int dst, unsigned bt)
{
	o_op(op, src, dst, bt);
	oi((3 << 6) | (src << 3) | (dst & 0x07), 1);
}

static long sp_push(int size)
{
	sp += size;
	if (sp > maxsp)
		maxsp = sp;
	return sp;
}

static void deref(unsigned bt)
{
	memop(MOV_M2R, R_RAX, R_RAX, 0, bt);
}

static unsigned tmp_pop(int lval)
{
	struct tmp *t = &tmp[--ntmp];
	memop(MOV_M2R, R_RAX, R_RBP, -t->addr, TMP_BT(t));
	if (!lval && t->type == TMP_ADDR)
		deref(t->bt);
	return t->bt;
}

static void tmp_push(int type, unsigned bt)
{
	struct tmp *t = &tmp[ntmp++];
	t->addr = sp_push(8);
	t->bt = bt;
	t->type = type;
	memop(MOV_R2X, R_RAX, R_RBP, -t->addr, TMP_BT(t));
}

void o_tmpdrop(int n)
{
	if (n == -1 || n > ntmp)
		n = ntmp;
	ntmp -= n;
}

void o_tmpjoin(void)
{
	struct tmp t1 = tmp[ntmp - 1];
	struct tmp t2 = tmp[ntmp - 2];
	if (t1.addr == t2.addr) {
		o_tmpdrop(1);
		return;
	}
	tmp_pop(1);
	memop(MOV_R2X, R_RAX, R_RBP, -t2.addr, TMP_BT(&t1));
}

void o_tmpswap(void)
{
	struct tmp t = tmp[ntmp - 1];
	tmp[ntmp - 1] = tmp[ntmp - 2];
	tmp[ntmp - 2] = t;
}

void o_tmpcopy(void)
{
	int type = tmp[ntmp - 1].type;
	unsigned bt = tmp_pop(1);
	tmp_push(type, bt);
	tmp_push(type, bt);
}

static long codeaddr(void)
{
	return cur - buf;
}

void o_func_beg(char *name)
{
	out_func_beg(name);
	cur = buf;
	os("\x55", 1);			/* push %rbp */
	os("\x48\x89\xe5", 3);		/* mov %rsp, %rbp */
	sp = 0;
	maxsp = 0;
	ntmp = 0;
	os("\x48\x81\xec", 3);		/* sub $xxx, %rsp */
	spsub_addr = codeaddr();
	oi(0, 4);
}

void o_num(int n, unsigned bt)
{
	os("\xb8", 1);
	oi(n, 4);
	tmp_push(TMP_CONST, bt);
}

void o_deref(unsigned bt)
{
	tmp_pop(0);
	tmp_push(TMP_ADDR, bt);
}

void o_load(void)
{
	unsigned bt = tmp_pop(0);
	tmp_push(TMP_CONST, bt);
}

void o_shl(void)
{
	int bt1, bt2;
	bt1 = tmp_pop(0);
	regop(MOV_R2X, R_RAX, R_RCX, 1);
	bt2 = tmp_pop(0);
	regop(SHX_REG, 4, R_RAX, bt2);
	tmp_push(TMP_CONST, bt2);
}

void o_shr(void)
{
	int bt1, bt2;
	bt1 = tmp_pop(0);
	regop(MOV_R2X, R_RAX, R_RCX, 1);
	bt2 = tmp_pop(0);
	regop(SHX_REG, bt2 & BT_SIGNED ? 5 : 7, R_RAX, bt2);
	tmp_push(TMP_CONST, bt2);
}

void o_mul(void)
{
	int bt1, bt2;
	bt1 = tmp_pop(0);
	regop(MOV_R2X, R_RAX, R_RBX, 8);
	bt2 = tmp_pop(0);
	os("\xf7\xeb", 2);	/* mul %ebx */
	tmp_push(TMP_CONST, bt2);
}

void o_arrayderef(unsigned bt)
{
	tmp_pop(0);
	if (BT_SZ(bt) > 1) {
		os("\xbb", 1);		/* mov $x, %ebx */
		oi(BT_SZ(bt), 4);
		os("\xf7\xeb", 2);	/* mul %ebx */
	}
	regop(MOV_R2X, R_RAX, R_RBX, 4);
	tmp_pop(0);
	regop(ADD_R2R, R_RBX, R_RAX, 8);
	tmp_push(TMP_ADDR, bt);
}

void o_addr(void)
{
	tmp[ntmp - 1].type = TMP_CONST;
	tmp[ntmp - 1].bt = 8;
}

void o_ret(unsigned bt)
{
	if (bt)
		tmp_pop(0);
	else
		os("\x31\xc0", 2);	/* xor %eax, %eax */
	os("\xc9\xc3", 2);		/* leave; ret; */
}

static int binop(void)
{
	unsigned bt1, bt2;
	bt1 = tmp_pop(0);
	regop(MOV_R2X, R_RAX, R_RBX, bt1);
	bt2 = tmp_pop(0);
	return BT_SZ(bt1) > BT_SZ(bt2) ? bt1 : bt2;
}

void o_add(void)
{
	int bt = binop();
	regop(ADD_R2R, R_RBX, R_RAX, bt);
	tmp_push(TMP_CONST, bt);
}

void o_sub(void)
{
	int bt = binop();
	regop(SUB_R2R, R_RBX, R_RAX, bt);
	tmp_push(TMP_CONST, bt);
}

static void o_cmp(int uop, int sop)
{
	char set[] = "\x0f\x00\xc0";
	unsigned bt = binop();
	regop(CMP_R2R, R_RBX, R_RAX, bt);
	set[1] = bt & BT_SIGNED ? sop : uop;
	os(set, 3);			/* setl %al */
	os("\x0f\xb6\xc0", 3);		/* movzbl %al, %eax */
	tmp_push(TMP_CONST, 4 | BT_SIGNED);
}

void o_lt(void)
{
	o_cmp(0x92, 0x9c);
}

void o_func_end(void)
{
	os("\xc9\xc3", 2);		/* leave; ret; */
	putint(buf + spsub_addr, (maxsp + 7) & ~0x07, 4);
	out_func_end(buf, cur - buf);
}

void o_local(long addr, unsigned bt)
{
	memop(LEA_M2R, R_RAX, R_RBP, -addr, 8);
	tmp_push(TMP_ADDR, bt);
}

long o_mklocal(int size)
{
	return sp_push((size + 7) & ~0x07);
}

static int arg_regs[] = {R_RDI, R_RSI, R_RDX, R_RCX, R_R8, R_R9};

long o_arg(int i, unsigned bt)
{
	long addr = o_mklocal(BT_SZ(bt));
	memop(MOV_R2X, arg_regs[i], R_RBP, -addr, bt);
	return addr;
}

void o_assign(unsigned bt)
{
	unsigned bt2 = tmp_pop(0);
	regop(MOV_R2X, R_RAX, R_RBX, bt2);
	tmp_pop(1);
	memop(MOV_R2X, R_RBX, R_RAX, 0, bt);
	regop(MOV_R2X, R_RBX, R_RAX, bt2);
	tmp_push(TMP_CONST, bt);
}

long o_mklabel(void)
{
	return codeaddr();
}

long o_jz(long addr)
{
	tmp_pop(0);
	os("\x48\x85\xc0", 3);		/* test %rax, %rax */
	os("\x0f\x84", 2);		/* jz $addr */
	oi(addr - codeaddr() - 4, 4);
	return codeaddr() - 4;
}

long o_jmp(long addr)
{
	os("\xe9", 1);			/* jmp $addr */
	oi(addr - codeaddr() - 4, 4);
	return codeaddr() - 4;
}

void o_filljmp(long addr)
{
	putint(buf + addr, codeaddr() - addr - 4, 4);
}

void o_symaddr(char *name, unsigned bt)
{
	os("\x48\xc7\xc0", 3);		/* mov $addr, %rax */
	out_rela(name, codeaddr());
	oi(0, 4);
	tmp_push(TMP_ADDR, bt);
}

void o_call(int argc, unsigned *bt, unsigned ret_bt)
{
	int i;
	for (i = 0; i < argc; i++) {
		tmp_pop(0);
		regop(MOV_R2X, R_RAX, arg_regs[i], bt[i]);
	}
	tmp_pop(1);
	os("\xff\xd0", 2);		/* callq *%rax */
	if (ret_bt)
		tmp_push(TMP_CONST, ret_bt);
}
