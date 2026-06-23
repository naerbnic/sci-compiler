#![expect(dead_code, reason = "Work in progress")]

use crate::codegen::{LabelRef, LiteralValue, ProcedureName, PtrRef};

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct ScriptNum(u16);

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct ScriptVarIndex(u16);

/// A reference to a script variable.
pub struct ScriptVarRef {}

pub struct ProcRef {}

pub struct ParamRef {}

pub struct TempRef {}

pub(super) struct EnvBuilder {}
impl EnvBuilder {
    /// Create a variable for a given script, with the given index and optional name.
    ///
    /// If you are compiling this script, this variable must be defined in the
    /// compilation scope.
    pub fn make_global(
        &self,
        script: ScriptNum,
        index: ScriptVarIndex,
        name: Option<&str>,
    ) -> ScriptVarRef {
        todo!()
    }

    /// Create an external procedure for a given script, with the given index and optional name.
    ///
    /// If you are compiling this script, this procedure must be defined in the
    /// compilation scope.
    pub fn create_script_proc(
        &self,
        script: ScriptNum,
        proc_index: u16,
        name: Option<&str>,
    ) -> ProcRef {
        todo!()
    }

    /// Create a kernel procedure with the given entry number and optional name.
    pub fn create_kernel_proc(&self, entry_num: u16, name: Option<&str>) -> ProcRef {
        todo!()
    }
}

pub(super) struct ScriptBuilder {}

impl ScriptBuilder {
    pub(super) fn num(&self) -> ScriptNum {
        todo!()
    }

    /// Declares a procedure that can be called from other scripts.
    ///
    /// This does not define the procedure body; it only declares that the
    /// procedure exists and can be called.
    ///
    /// This procedure must be defined before this script is compiled, or else
    /// the compiler will error.
    pub(super) fn declare_pub_proc(&mut self, proc_index: u16, name: Option<&str>) -> ProcRef {
        todo!()
    }

    pub(super) fn declare_proc(&mut self, name: Option<&str>) -> ProcRef {
        todo!()
    }

    pub(super) fn declare_var(&mut self, name: Option<&str>) -> ScriptVarRef {
        todo!()
    }

    pub(super) fn define_array_var(&mut self, name: Option<&str>, len: u16) -> ScriptVarRef {
        todo!()
    }

    pub(super) fn declare_class(&mut self) -> ClassId {
        todo!()
    }

    pub(super) fn define_proc(&mut self, proc: ProcRef) -> ProcBuilder {
        todo!()
    }

    pub(super) fn define_var(
        &mut self,
        var: ScriptVarRef,
        initial_value: LiteralValue,
    ) -> &mut Self {
        todo!()
    }
}

pub(super) struct ProcBuilder {}

impl ProcBuilder {
    pub(super) fn define_param(&mut self, name: Option<&str>) -> ParamRef {
        todo!()
    }

    pub(super) fn define_array_param(&mut self, name: Option<&str>, len: u16) -> ParamRef {
        todo!()
    }

    pub(super) fn define_temp(&mut self, name: Option<&str>) -> TempRef {
        todo!()
    }

    pub(super) fn define_array_temp(&mut self, name: Option<&str>, len: u16) -> TempRef {
        todo!()
    }

    pub(super) fn define_body(self) -> BodyBuilder {
        todo!()
    }
}

// Idea: Simplify build instruction set, relying on the optimizer to avoid
// excess pushes/pops.

pub(super) struct BodyBuilder {}

impl BodyBuilder {
    pub(super) fn create_label(&mut self) -> LabelRef {
        todo!()
    }

    pub(super) fn insert_label(&mut self, label: LabelRef) -> &mut Self {
        todo!()
    }

    pub(super) fn unary_op(&mut self, op: UnOp) -> &mut Self {
        todo!()
    }

    pub(super) fn binary_op(&mut self, op: BinOp) -> &mut Self {
        todo!()
    }

    pub(super) fn push_imm(&mut self, value: LiteralValue) -> &mut Self {
        todo!()
    }

    pub(super) fn toss(&mut self) -> &mut Self {
        todo!()
    }

    pub(super) fn dup(&mut self) -> &mut Self {
        todo!()
    }

    pub(super) fn push_rest(&mut self, value: u32) -> &mut Self {
        todo!()
    }

    pub(super) fn load_imm(&mut self, value: LiteralValue) -> &mut Self {
        todo!()
    }

    pub(super) fn load_ptr_offset(&mut self, ptr: PtrRef) -> &mut Self {
        todo!()
    }

    pub(super) fn load_slot_addr(&mut self, slot: Slot, indexed: bool) -> &mut Self {
        todo!()
    }

    pub(super) fn var_access(&mut self, op: SlotOp, slot: Slot, indexed: bool) -> &mut Self {
        todo!()
    }

    pub(super) fn prop_access(&mut self, op: SlotOp, indexed: bool) -> &mut Self {
        todo!()
    }

    pub(super) fn load_class(&mut self, class_id: ClassId) -> &mut Self {
        todo!()
    }

    pub(super) fn load_self(&mut self) -> &mut Self {
        todo!()
    }

    pub(super) fn branch(&mut self, op: BranchOp, target: LabelRef) -> &mut Self {
        todo!()
    }

    pub(super) fn call(&mut self, proc: ProcRef, num_args: u32) -> &mut Self {
        todo!()
    }

    pub(super) fn send(&mut self, num_args: u32) -> &mut Self {
        todo!()
    }

    pub(super) fn send_self(&mut self, num_args: u32) -> &mut Self {
        todo!()
    }

    pub(super) fn ret(&mut self) -> &mut Self {
        todo!()
    }
}

pub(super) enum Op {
    Unary(UnOp),
    Binary(BinOp),
    Cmp {
        op: CmpOp,
        push_acc: bool,
    },
    /// Push accumulator onto stack,
    Push,
    /// Push Immediate value onto the stack.
    PushImm(LiteralValue),
    /// Pop the top of the stack. Does not affect the accumulator.
    Toss,
    /// Push the top of the stack onto the stack, duplicating the top value.
    Dup,
    /// Push the rest of the parameters onto the stack, starting from the given argument index.
    PushRest(u32),
    /// Load the given immediate value into the accumulator.
    LoadImm(LiteralValue),
    /// Load the offset of the given pointer into the accumulator.
    LoadPtrOffset(PtrRef),
    /// Load the address of the given slot into the accumulator.
    LoadSlotAddr {
        slot: Slot,
        add_acc_index: bool,
    },
    VarAccess {
        op: SlotOp,
        slot: Slot,
        add_acc_index: bool,
    },
    PropAccess {
        op: SlotOp,
        prop: PropIndex,
    },
    LoadClass(ClassId),
    LoadSelf,
    Branch {
        op: BranchOp,
        target: LabelRef,
    },
    CallProc {
        proc: ProcRef,
        num_args: u32,
    },
    Send {
        num_args: u32,
    },
    SelfSend {
        num_args: u32,
        super_type: Option<ClassId>,
    },
    Return,
}

pub(super) enum UnOp {
    Negate,
    Not,
    BinaryNot,
}

pub(super) enum Slot {
    Global(GlobalSlot),
    Local(LocalSlot),
    Param(ParamSlot),
    Temp(TempSlot),
}

pub(super) enum SlotOp {
    Load,
    Store,
    Inc,
    Dec,
}

pub(super) struct GlobalSlot(u16);
pub(super) struct LocalSlot(u16);
pub(super) struct ParamSlot(u16);
pub(super) struct TempSlot(u16);
pub(super) struct PropIndex(u16);
pub(super) struct ClassId(u16);

pub(super) enum BinOp {
    // Math Operations
    Add,
    Sub,
    Mul,
    Div,
    Shl,
    Shr,
    Mod,
    And,
    Or,
    Xor,
}

pub(super) enum BranchOp {
    Bnt,
    Bt,
    Jmp,
}

pub(super) enum CmpOp {
    Gt,
    Ge,
    Lt,
    Le,
    Eq,
    Ne,
    Ugt,
    Uge,
    Ult,
    Ule,
}
