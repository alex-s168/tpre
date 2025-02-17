#include "tpre.h"
#include "vxcc_v3/ir/ir.h"
#include "vxcc_v3/ir/passes.h"
#include <assert.h>

#define NODE_DONE ((tpre_nodeid_t) -2)
#define NODE_ERR  ((tpre_nodeid_t) -1)

#define SPECIAL_ANY   (0)
#define SPECIAL_SPACE (1)
#define SPECIAL_END   (2)

static void gen_nd_jmp(vx_IrBlock* block, vx_IrVar temp, vx_IrType* u8, tpre_nodeid_t target_nd, tpre_re_t const* re, vx_IrVar v_retp)
{
    size_t dest;
    if (target_nd < 0) {
        int status = target_nd == NODE_DONE ? 0 : 1;

        {
        vx_IrOp* op = fastalloc(sizeof(vx_IrOp));
        vx_IrOp_init(op, VX_IR_OP_IMM, block);
        vx_IrOp_addOut(op, temp, u8);
        vx_IrOp_addParam_s(op, VX_IR_NAME_VALUE, VX_IR_VALUE_IMM_INT(status));
        vx_IrBlock_addOp(block, op);
        }

        {
        vx_IrOp* op = fastalloc(sizeof(vx_IrOp));
        vx_IrOp_init(op, VX_IR_OP_STORE, block);
        vx_IrOp_addParam_s(op, VX_IR_NAME_ADDR, VX_IR_VALUE_VAR(v_retp));
        vx_IrOp_addParam_s(op, VX_IR_NAME_VALUE, VX_IR_VALUE_VAR(temp));
        vx_IrBlock_addOp(block, op);
        }

        dest = re->num_nodes + re->num_nodes; // end of fn
    } else {
        dest = target_nd;
    }

    vx_IrOp* op = fastalloc(sizeof(vx_IrOp));
    vx_IrOp_init(op, VX_IR_OP_GOTO, block);
    vx_IrOp_addParam_s(op, VX_IR_NAME_ID, VX_IR_VALUE_ID(dest));
    vx_IrBlock_addOp(block, op);
}

/** 0 = ok */
int tpre_compile_jit(char const * str, tpre_errs_t * errs_out)
{
    tpre_re_t re;
    tpre_compile(&re, str, errs_out);

    vx_IrBlock* block = vx_IrBlock_initHeap(NULL, NULL);
    block->name = "matches";

    vx_IrType* u8 = fastalloc(sizeof(vx_IrType));
    u8->debugName = "u8";
    u8->kind = VX_IR_TYPE_KIND_BASE;
    u8->base = (vx_IrTypeBase) { .size = 1, .align = 2, .isfloat = false };

    vx_IrType* u16 = fastalloc(sizeof(vx_IrType));
    u16->debugName = "u16";
    u16->kind = VX_IR_TYPE_KIND_BASE;
    u16->base = (vx_IrTypeBase) { .size = 2, .align = 2, .isfloat = false };

    vx_IrType* ptr = fastalloc(sizeof(vx_IrType));
    ptr->debugName = "ptr";
    ptr->kind = VX_IR_TYPE_KIND_BASE;
    ptr->base = (vx_IrTypeBase) { .size = 8, .align = 4, .isfloat = false };

    vx_IrVar nextVar = 0;

    vx_IrVar v_strp = nextVar++;
    vx_IrVar v_retp = nextVar++; // u8*
    vx_IrVar v_curr_idx = nextVar++;

    vx_IrBlock_addIn(block, v_strp, ptr);
    vx_IrBlock_addIn(block, v_retp, ptr);

    {
        vx_IrOp* op = fastalloc(sizeof(vx_IrOp));
        vx_IrOp_init(op, VX_IR_OP_IMM, block);
        vx_IrOp_addOut(op, v_curr_idx, u16);
        vx_IrOp_addParam_s(op, VX_IR_NAME_VALUE, VX_IR_VALUE_IMM_INT((size_t) 0));
        vx_IrBlock_addOp(block, op);
    }

    for (tpre_nodeid_t nd = 0; nd < re.num_nodes; nd ++)
    {
        {
            vx_IrOp* op = fastalloc(sizeof(vx_IrOp));
            vx_IrOp_init(op, VX_IR_OP_LABEL, block);
            vx_IrOp_addParam_s(op, VX_IR_NAME_ID, VX_IR_VALUE_ID((size_t) nd));
            vx_IrBlock_addOp(block, op);
        }

        vx_IrVar load_addr = nextVar ++;
        {
            vx_IrOp* op = fastalloc(sizeof(vx_IrOp));
            vx_IrOp_init(op, VX_IR_OP_ADD, block);
            vx_IrOp_addOut(op, load_addr, ptr);
            vx_IrOp_addParam_s(op, VX_IR_NAME_OPERAND_A, VX_IR_VALUE_VAR(v_strp));
            vx_IrOp_addParam_s(op, VX_IR_NAME_OPERAND_B, VX_IR_VALUE_VAR(v_curr_idx));
            vx_IrBlock_addOp(block, op);
        }

        vx_IrVar loaded = nextVar ++;
        {
            vx_IrOp* op = fastalloc(sizeof(vx_IrOp));
            vx_IrOp_init(op, VX_IR_OP_LOAD, block);
            vx_IrOp_addOut(op, loaded, u8);
            vx_IrOp_addParam_s(op, VX_IR_NAME_ADDR, VX_IR_VALUE_VAR(load_addr));
            vx_IrBlock_addOp(block, op);
        }

        vx_IrVar iszero = nextVar++;
        {
            vx_IrOp* op = fastalloc(sizeof(vx_IrOp));
            vx_IrOp_init(op, VX_IR_OP_EQ, block);
            vx_IrOp_addOut(op, iszero, u8);
            vx_IrOp_addParam_s(op, VX_IR_NAME_OPERAND_A, VX_IR_VALUE_VAR(loaded));
            vx_IrOp_addParam_s(op, VX_IR_NAME_OPERAND_B, VX_IR_VALUE_IMM_INT(0));
            vx_IrBlock_addOp(block, op);
        }

        vx_IrVar notmatches;
        if (re.i_pat[nd].is_special) {
            switch (re.i_pat[nd].val) {
                case SPECIAL_ANY: {
                    notmatches = iszero;
                } break;

                case SPECIAL_SPACE:
                case SPECIAL_END:
                default:
                    assert(false);
            }
        } else {
            notmatches = nextVar ++;
            vx_IrValue cmp = VX_IR_VALUE_IMM_INT((long long) re.i_pat[nd].val);

            vx_IrOp* op = fastalloc(sizeof(vx_IrOp));
            vx_IrOp_init(op, VX_IR_OP_NEQ, block);
            vx_IrOp_addOut(op, notmatches, u8);
            vx_IrOp_addParam_s(op, VX_IR_NAME_OPERAND_A, VX_IR_VALUE_VAR(loaded));
            vx_IrOp_addParam_s(op, VX_IR_NAME_OPERAND_B, cmp);
            vx_IrBlock_addOp(block, op);
        }

        {
            vx_IrOp* op = fastalloc(sizeof(vx_IrOp));
            vx_IrOp_init(op, VX_IR_OP_COND, block);
            vx_IrOp_addParam_s(op, VX_IR_NAME_COND, VX_IR_VALUE_VAR(notmatches));
            vx_IrOp_addParam_s(op, VX_IR_NAME_ID, VX_IR_VALUE_ID(re.num_nodes + nd));
            vx_IrBlock_addOp(block, op);
        }

        vx_IrVar addedOne = nextVar ++;
        {
            vx_IrOp* op = fastalloc(sizeof(vx_IrOp));
            vx_IrOp_init(op, VX_IR_OP_ADD, block);
            vx_IrOp_addOut(op, addedOne, u16);
            vx_IrOp_addParam_s(op, VX_IR_NAME_OPERAND_A, VX_IR_VALUE_VAR(v_curr_idx));
            vx_IrOp_addParam_s(op, VX_IR_NAME_OPERAND_B, VX_IR_VALUE_IMM_INT(1));
            vx_IrBlock_addOp(block, op);
        }

        {
            vx_IrOp* op = fastalloc(sizeof(vx_IrOp));
            vx_IrOp_init(op, VX_IR_OP_CMOV, block);
            vx_IrOp_addOut(op, v_curr_idx, u16);
            vx_IrOp_addParam_s(op, VX_IR_NAME_COND, VX_IR_VALUE_VAR(iszero));
            vx_IrOp_addParam_s(op, VX_IR_NAME_COND_THEN, VX_IR_VALUE_VAR(v_curr_idx));
            vx_IrOp_addParam_s(op, VX_IR_NAME_COND_ELSE, VX_IR_VALUE_VAR(addedOne));
            vx_IrBlock_addOp(block, op);
        }

        gen_nd_jmp(block, nextVar++, u8, re.i_ok[nd], &re, v_retp);
    }

    for (tpre_nodeid_t nd = 0; nd < re.num_nodes; nd ++)
    {
        {
            vx_IrOp* op = fastalloc(sizeof(vx_IrOp));
            vx_IrOp_init(op, VX_IR_OP_LABEL, block);
            vx_IrOp_addParam_s(op, VX_IR_NAME_ID, VX_IR_VALUE_ID(re.num_nodes + nd));
            vx_IrBlock_addOp(block, op);
        }

        if (re.i_backtrack[nd] > 0)
        {
            vx_IrOp* op = fastalloc(sizeof(vx_IrOp));
            vx_IrOp_init(op, VX_IR_OP_SUB, block);
            vx_IrOp_addOut(op, v_curr_idx, u16);
            vx_IrOp_addParam_s(op, VX_IR_NAME_OPERAND_A, VX_IR_VALUE_VAR(v_curr_idx));
            vx_IrOp_addParam_s(op, VX_IR_NAME_OPERAND_B, VX_IR_VALUE_IMM_INT(re.i_backtrack[nd]));
            vx_IrBlock_addOp(block, op);
        }

        gen_nd_jmp(block, nextVar++, u8, re.i_err[nd], &re, v_retp);
    }

    // end of fn
    {
        vx_IrOp* op = fastalloc(sizeof(vx_IrOp));
        vx_IrOp_init(op, VX_IR_OP_LABEL, block);
        vx_IrOp_addParam_s(op, VX_IR_NAME_ID, VX_IR_VALUE_ID(re.num_nodes + re.num_nodes));
        vx_IrBlock_addOp(block, op);
    }

    vx_CU* cu = fastalloc(sizeof(vx_CU));
    vx_CU_init(cu, "amd64:cmov");
    vx_CU_addType(cu, u8);
    vx_CU_addType(cu, u16);
    vx_CU_addType(cu, ptr);
    vx_CU_addIrBlock(cu, block, true);

    block->ll_out_types_len = 0;
    vx_IrBlock_putLabel(block, re.num_nodes + re.num_nodes + 1, NULL);
    vx_CIrBlock_fix(cu, block);
    vx_IrBlock_llir_fix_decl(cu, block);

    vx_CU_compile(cu, NULL, NULL, stdout, 0, NULL, VX_CU_COMPILE_MODE_FROM_LLIR);

    tpre_free(re);

    return 0;
}

int main() {
    assert(tpre_compile_jit("\\s*?(red|blue)?\\s*?car\\s*?", NULL) == 0);
}
