use itertools::Itertools;

use crate::{
    codegen::repr::opcodes::{
        ArgType, ArgWidth, BranchOp, LoadOffsetOp, MemOp, Op, Opcode, PropOp, SimpleOp, SlotAccess,
        VarArgType,
    },
    int::{IntRepr, MWord, SByte, SWord, UByte, UWord},
};

pub(crate) trait ComputedArg {
    type Context;

    fn compute_value(&self, ctxt: &Self::Context) -> ArgValue;
    fn try_to_mword(&self) -> Option<MWord>;
}

pub(crate) trait LayoutContext<L> {
    fn get_label_offset(&self, label: &L) -> UWord;
}

#[derive(Debug, Clone)]
pub(crate) enum ArgValue {
    UWord(UWord),
    SWord(SWord),
    UByte(UByte),
    SByte(SByte),
}

impl ArgValue {
    pub(crate) fn arg_type(&self) -> ArgType {
        match self {
            ArgValue::UWord(_) => ArgType::UWord,
            ArgValue::SWord(_) => ArgType::SWord,
            ArgValue::UByte(_) => ArgType::UByte,
            ArgValue::SByte(_) => ArgType::SByte,
        }
    }

    pub(crate) fn to_repr(&self) -> IntRepr {
        match self {
            ArgValue::UWord(val) => val.to_repr(),
            ArgValue::SWord(val) => val.to_repr(),
            ArgValue::UByte(val) => val.to_repr(),
            ArgValue::SByte(val) => val.to_repr(),
        }
    }
}

impl From<UWord> for ArgValue {
    fn from(val: UWord) -> Self {
        ArgValue::UWord(val)
    }
}

impl From<SWord> for ArgValue {
    fn from(val: SWord) -> Self {
        ArgValue::SWord(val)
    }
}

impl From<UByte> for ArgValue {
    fn from(val: UByte) -> Self {
        ArgValue::UByte(val)
    }
}

impl From<SByte> for ArgValue {
    fn from(val: SByte) -> Self {
        ArgValue::SByte(val)
    }
}

#[derive(Debug, Clone)]
pub(crate) struct ResolvedArg(ArgValue);

impl ComputedArg for ResolvedArg {
    type Context = ();

    fn compute_value(&self, _ctxt: &Self::Context) -> ArgValue {
        self.0.clone()
    }

    fn try_to_mword(&self) -> Option<MWord> {
        match &self.0 {
            ArgValue::UWord(val) => Some(val.to_machine()),
            ArgValue::SWord(val) => Some(val.to_machine()),
            ArgValue::UByte(_) | ArgValue::SByte(_) => None,
        }
    }
}

pub(crate) fn make_minimized_inst(op: Op, args: &[ArgValue]) -> RawInst {
    let mut minimized_args = Vec::new();
    let mut minimized_width = ArgWidth::Byte;
    let arg_types = op.args();
    for (arg_val, arg_type) in args.iter().zip_eq(arg_types.iter()) {
        let minimized_arg = match (arg_val, arg_type) {
            (ArgValue::UWord(val), VarArgType::UWord) => {
                if let Some(min_val) = val.try_to_byte() {
                    ArgValue::UByte(min_val)
                } else {
                    ArgValue::UWord(*val)
                }
            }
            (ArgValue::SWord(val), VarArgType::SWord) => {
                if let Some(min_val) = val.try_to_byte() {
                    ArgValue::SByte(min_val)
                } else {
                    ArgValue::SWord(*val)
                }
            }
            (ArgValue::UByte(val), VarArgType::UByte) => ArgValue::UByte(*val),
            (ArgValue::SByte(val), VarArgType::SByte) => ArgValue::SByte(*val),
            (ArgValue::UWord(val), VarArgType::UVar) => {
                if let Some(min_val) = val.try_to_byte() {
                    ArgValue::UByte(min_val)
                } else {
                    minimized_width = ArgWidth::Word;
                    ArgValue::UWord(*val)
                }
            }

            (ArgValue::SWord(val), VarArgType::SVar) => {
                if let Some(min_val) = val.try_to_byte() {
                    ArgValue::SByte(min_val)
                } else {
                    minimized_width = ArgWidth::Word;
                    ArgValue::SWord(*val)
                }
            }
            _ => panic!("Mismatched argument type"),
        };
        minimized_args.push(minimized_arg);
    }

    let opcode = op.to_opcode(minimized_width);

    match minimized_width {
        ArgWidth::Word => {
            // The args in minimized_args may only be partially minimized, so
            // use the original args.
            RawInst::from_op_args(opcode, args.to_vec())
        }
        ArgWidth::Byte => {
            // The minimized_args should match the minimized form of the
            // arg types. Use its values instead.
            RawInst::from_op_args(opcode, minimized_args)
        }
    }
}

#[derive(Debug, thiserror::Error)]
#[error("Failed to create a new RawInst: {0}")]
pub(crate) struct NewRawInstError(String);

/// A raw instruction, including the opcode and any arguments.
///
/// This represents a concrete representation of an instruction, and cannot be
/// minimized.
pub(crate) struct RawInst {
    op: Opcode,
    args: Vec<ArgValue>,
}

impl RawInst {
    pub(crate) fn from_op_args(op: Opcode, args: Vec<ArgValue>) -> Self {
        Self::try_from_op_args(op, args)
            .expect("Failed to create RawInst from opcode and arguments")
    }

    pub(crate) fn try_from_op_args(
        op: Opcode,
        args: Vec<ArgValue>,
    ) -> Result<Self, NewRawInstError> {
        let arg_types = op.args();
        if arg_types.len() != args.len() {
            return Err(NewRawInstError(format!(
                "Opcode {:?} expects {} arguments, but got {}",
                op,
                arg_types.len(),
                args.len()
            )));
        }

        let mut errors = Vec::new();
        for (i, (arg_type, arg_value)) in arg_types.iter().zip(args.iter()).enumerate() {
            if arg_type != &arg_value.arg_type() {
                errors.push(format!(
                    "Argument {} for opcode {:?} expects type {:?}, but got {:?}",
                    i,
                    op,
                    arg_type,
                    arg_value.arg_type()
                ));
            }
        }

        if !errors.is_empty() {
            return Err(NewRawInstError(errors.join(", ")));
        }

        Ok(Self { op, args })
    }

    pub(crate) fn op(&self) -> Opcode {
        self.op
    }

    pub(crate) fn args(&self) -> &[ArgValue] {
        &self.args
    }

    pub(crate) fn to_bytes(&self) -> Vec<u8> {
        let mut bytes = vec![self.op.to_u8()];
        for arg in &self.args {
            bytes.extend_from_slice(&arg.to_repr());
        }
        bytes
    }
}

/// A full instruction, including the opcode and any arguments.
///
/// When serialized, instructions are minimized to their smallest possible
/// representation.
pub(crate) enum Inst<A> {
    Control(ControlInst<A>),
    Seq(SeqInst<A>),
}

// A small aside: The size of opcodes can vary based on their contents, which in
// theory could cause a circular dependency between the sizes of two instructions.
// For instance, if when instruction A is small, instruction B must be large,
// and vice versa, then it's possible for a size optimization pass to
// oscillate. This is not likely, as generally shrinking one instruction does
// not cause another instruction to grow. I _think_ this is a general rule, so
// a simple iterative optimization pass should always work, but when we
// implement this, we should have a failsafe to prevent infinite loops,
// just in case.
//
// To provide for various ways of accessing possible represenations, we provide methods
// that can both provide min sizes, and max sizes.

impl<A> Inst<A>
where
    A: ComputedArg,
{
    /// Returns a raw inst of the maximum size of this instruction
    ///
    /// If minimize is true, it will attempt to create the minimal representation of the instruction.
    fn to_raw(&self, minimize: bool, ctxt: &A::Context) -> RawInst {
        let op = self.get_op();
        let args = self.get_args(ctxt);
        if minimize {
            make_minimized_inst(op, &args)
        } else {
            RawInst::from_op_args(op.to_opcode(ArgWidth::Word), args)
        }
    }

    fn to_bytes(&self, minimize: bool, ctxt: &A::Context) -> Vec<u8> {
        self.to_raw(minimize, ctxt).to_bytes()
    }

    fn get_op(&self) -> Op {
        use Inst as I;
        match self {
            I::Control(control_inst) => control_inst.get_op(),
            I::Seq(seq_inst) => seq_inst.get_op(),
        }
    }

    fn get_args(&self, ctxt: &A::Context) -> Vec<ArgValue> {
        use Inst as I;
        match self {
            I::Control(control_inst) => control_inst.get_args(ctxt),
            I::Seq(seq_inst) => seq_inst.get_args(ctxt),
        }
    }
}

/// Instructions where all of the opcodes have no arguments
#[derive(Debug, Clone)]
pub(crate) enum SimpleInst {
    Bnot,
    Add,
    Sub,
    Mul,
    Div,
    Mod,
    Shr,
    Shl,
    Xor,
    And,
    Or,
    Neg,
    Not,
    Eq,
    Ne,
    Gt,
    Ge,
    Lt,
    Le,
    Ugt,
    Uge,
    Ult,
    Ule,
    Push,
    Toss,
    Dup,
    SelfID,
    Pprev,
    PushSelf,
}

impl SimpleInst {
    pub(crate) fn get_op(&self) -> Op {
        use SimpleInst as I;
        use SimpleOp as O;
        let simple_op = match self {
            I::Bnot => O::Bnot,
            I::Add => O::Add,
            I::Sub => O::Sub,
            I::Mul => O::Mul,
            I::Div => O::Div,
            I::Mod => O::Mod,
            I::Shr => O::Shr,
            I::Shl => O::Shl,
            I::Xor => O::Xor,
            I::And => O::And,
            I::Or => O::Or,
            I::Neg => O::Neg,
            I::Not => O::Not,
            I::Eq => O::Eq,
            I::Ne => O::Ne,
            I::Gt => O::Gt,
            I::Ge => O::Ge,
            I::Lt => O::Lt,
            I::Le => O::Le,
            I::Ugt => O::Ugt,
            I::Uge => O::Uge,
            I::Ult => O::Ult,
            I::Ule => O::Ule,
            I::Push => O::Push,
            I::Toss => O::Toss,
            I::Dup => O::Dup,
            I::SelfID => O::SelfID,
            I::Pprev => O::Pprev,
            I::PushSelf => O::PushSelf,
        };
        simple_op.to_op()
    }
}

/// Instructions that modify the control flow of the program.
///
/// In a basic block, these would be modeled as the last instruction.
pub(crate) enum ControlInst<L> {
    Branch(BranchInst<L>),
    Return,
}

impl<A> ControlInst<A>
where
    A: ComputedArg,
{
    fn get_op(&self) -> Op {
        use ControlInst as I;
        match self {
            I::Branch(branch_inst) => branch_inst.get_op(),
            I::Return => SimpleOp::Ret.to_op(),
        }
    }

    fn get_args(&self, ctxt: &A::Context) -> Vec<ArgValue> {
        match self {
            ControlInst::Branch(branch_inst) => branch_inst.get_args(ctxt),
            ControlInst::Return => vec![],
        }
    }
}

/// Instructions that do not modify the control flow of the program.
#[derive(Debug, Clone)]
pub(crate) enum SeqInst<A> {
    Simple(SimpleInst),

    Ldi(A),
    // Includes push0, push1, and push2,
    Pushi(SWord),
    Link(UWord),

    Call(CallInst),
    Send(SendInst),

    Class(UWord),
    Rest(SWord),
    Lea(LeaInst),
    Prop(PropInst),
    LoadOffset(LoadOffsetInst),
    DebugInfo(A),
    Mem(MemInst),
}

impl<A> SeqInst<A>
where
    A: ComputedArg,
{
    fn get_op(&self) -> Op {
        use SeqInst as I;
        match self {
            // These are always single byte opcodes, and have no arguments.
            I::Simple(simple_inst) => simple_inst.get_op(),
            I::Ldi(_) => SimpleOp::Ldi.to_op(),
            I::Pushi(imm) => match imm.as_i16() {
                0 => SimpleOp::Push0.to_op(),
                1 => SimpleOp::Push1.to_op(),
                2 => SimpleOp::Push2.to_op(),
                _ => SimpleOp::Pushi.to_op(),
            },
            I::Link(_) => SimpleOp::Link.to_op(),
            I::Class(_) => SimpleOp::Class.to_op(),
            I::Rest(_) => SimpleOp::Rest.to_op(),
            I::DebugInfo(_) => SimpleOp::DebugInfo.to_op(),
            I::Lea { .. } => SimpleOp::Lea.to_op(),
            I::Call(call) => call.get_op(),
            I::Send(send) => send.get_op(),
            I::Prop(prop) => prop.get_op(),
            I::LoadOffset(load_offset) => load_offset.get_op(),
            I::Mem(mem) => mem.get_op(),
        }
    }

    fn get_args(&self, ctxt: &A::Context) -> Vec<ArgValue> {
        match self {
            SeqInst::Simple(_) => vec![],
            SeqInst::Ldi(imm) => vec![imm.compute_value(ctxt)],
            SeqInst::Pushi(imm) => match imm.as_i16() {
                0 => vec![],
                1 => vec![],
                2 => vec![],
                _ => vec![(*imm).into()],
            },
            SeqInst::Link(uword) => vec![(*uword).into()],
            SeqInst::Call(call_inst) => call_inst.get_args(),
            SeqInst::Send(send_inst) => send_inst.get_args(),
            SeqInst::Class(uword) => vec![(*uword).into()],
            SeqInst::Rest(sword) => vec![(*sword).into()],
            SeqInst::Lea(lea_inst) => lea_inst.get_args(),
            SeqInst::Prop(prop_inst) => prop_inst.get_args(),
            SeqInst::LoadOffset(load_offset_inst) => load_offset_inst.get_args(),
            SeqInst::DebugInfo(uword) => vec![uword.compute_value(ctxt)],
            SeqInst::Mem(mem_inst) => mem_inst.get_args(),
        }
    }
}

#[derive(Debug, Clone)]
pub(crate) struct LeaInst {
    slot: SlotAccess,
    offset: UWord,
    use_index: bool,
}

impl LeaInst {
    fn type_arg(&self) -> SWord {
        let slot_bits = match self.slot {
            SlotAccess::Global => 0b00,
            SlotAccess::Local => 0b01,
            SlotAccess::Param => 0b10,
            SlotAccess::Tmp => 0b11,
        };
        let index_bit = if self.use_index { 0b1 } else { 0b0 };
        SWord::from_i16((index_bit << 4) | (slot_bits << 1))
    }
    fn get_args(&self) -> Vec<ArgValue> {
        vec![self.type_arg().into(), self.offset.into()]
    }
}

pub(crate) struct BranchInst<A> {
    op: BranchOp,
    target: A,
}

impl<A> BranchInst<A>
where
    A: ComputedArg,
{
    pub(crate) fn get_op(&self) -> Op {
        self.op.to_op()
    }

    fn get_args(&self, ctxt: &A::Context) -> Vec<ArgValue> {
        vec![self.target.compute_value(ctxt)]
    }
}

#[derive(Debug, Clone)]
pub(crate) struct SendInst {
    num_params: UByte,
    op: SendOp,
}

impl SendInst {
    pub(crate) fn get_op(&self) -> Op {
        match self.op {
            SendOp::Accum => SimpleOp::Send.to_op(),
            SendOp::Self_ => SimpleOp::Self_.to_op(),
            SendOp::Super { .. } => SimpleOp::Super.to_op(),
        }
    }

    fn get_args(&self) -> Vec<ArgValue> {
        let mut prefix_args = self.op.get_prefix_args();
        prefix_args.push(ArgValue::UByte(self.num_params));
        prefix_args
    }
}

#[derive(Debug, Clone)]
pub(crate) enum SendOp {
    Accum,
    Self_,
    Super { class_id: UWord },
}

impl SendOp {
    pub(crate) fn to_op(&self) -> Op {
        match self {
            SendOp::Accum => SimpleOp::Send.to_op(),
            SendOp::Self_ => SimpleOp::Self_.to_op(),
            SendOp::Super { .. } => SimpleOp::Super.to_op(),
        }
    }

    fn get_prefix_args(&self) -> Vec<ArgValue> {
        match self {
            SendOp::Accum | SendOp::Self_ => vec![],
            SendOp::Super { class_id } => vec![(*class_id).into()],
        }
    }
}

#[derive(Debug, Clone)]
pub(crate) struct CallInst {
    num_params: UByte,
    op: CallOp,
}

impl CallInst {
    pub(crate) fn get_op(&self) -> Op {
        self.op.to_op()
    }

    fn get_args(&self) -> Vec<ArgValue> {
        let mut prefix_args = self.op.get_prefix_args();
        prefix_args.push(self.num_params.into());
        prefix_args
    }

    pub(crate) fn to_inst<A>(&self) -> Inst<A> {
        Inst::Seq(SeqInst::Call(self.clone()))
    }
}

#[derive(Debug, Clone)]
pub(crate) struct CallProc {
    offset: SWord,
}

impl CallProc {
    pub(crate) fn get_prefix_args(&self) -> Vec<ArgValue> {
        vec![self.offset.into()]
    }
}

#[derive(Debug, Clone)]
pub(crate) enum CallOp {
    Proc(CallProc),
    Kernel { kernel_id: UWord },
    Base { proc_id: UWord },
    External { script_num: UWord, prod_id: SWord },
}

impl CallOp {
    pub(crate) fn to_op(&self) -> Op {
        match self {
            CallOp::Proc(_) => SimpleOp::Call.to_op(),
            CallOp::Kernel { .. } => SimpleOp::Callk.to_op(),
            CallOp::Base { .. } => SimpleOp::Callb.to_op(),
            CallOp::External { .. } => SimpleOp::Calle.to_op(),
        }
    }

    fn get_prefix_args(&self) -> Vec<ArgValue> {
        match self {
            CallOp::Proc(proc) => proc.get_prefix_args(),
            CallOp::Kernel { kernel_id } => {
                vec![(*kernel_id).into()]
            }
            CallOp::Base { proc_id } => {
                vec![(*proc_id).into()]
            }
            CallOp::External {
                script_num,
                prod_id,
            } => {
                vec![(*script_num).into(), (*prod_id).into()]
            }
        }
    }
}

#[derive(Debug, Clone)]
pub(crate) struct PropInst {
    op: PropOp,
    index: UWord,
}

impl PropInst {
    pub(crate) fn op(&self) -> PropOp {
        self.op
    }

    pub(crate) fn index(&self) -> UWord {
        self.index
    }

    pub(crate) fn get_op(&self) -> Op {
        self.op.to_op()
    }

    fn get_args(&self) -> Vec<ArgValue> {
        vec![self.index.into()]
    }
}

#[derive(Debug, Clone)]
pub(crate) struct LoadOffsetInst {
    op: LoadOffsetOp,
    offset: UWord,
}

impl LoadOffsetInst {
    pub(crate) fn op(&self) -> LoadOffsetOp {
        self.op
    }

    pub(crate) fn offset(&self) -> UWord {
        self.offset
    }

    pub(crate) fn get_op(&self) -> Op {
        self.op.to_op()
    }

    fn get_args(&self) -> Vec<ArgValue> {
        vec![self.offset.into()]
    }
}

#[derive(Debug, Clone)]
pub(crate) struct MemInst {
    op: MemOp,
    slot: UWord,
}

impl MemInst {
    pub(crate) fn op(&self) -> MemOp {
        self.op
    }

    pub(crate) fn slot(&self) -> UWord {
        self.slot
    }

    pub(crate) fn get_op(&self) -> Op {
        self.op.to_op()
    }

    fn get_args(&self) -> Vec<ArgValue> {
        vec![self.slot.into()]
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_short_call_encoding() {
        let call_op: Inst<ResolvedArg> = CallInst {
            num_params: 1u8.into(),
            op: CallOp::Proc(CallProc {
                offset: (123i16).into(),
            }),
        }
        .to_inst();
        assert_eq!(call_op.to_bytes(true, &()), vec![0x41, 0x7b, 0x01]);
    }
}
