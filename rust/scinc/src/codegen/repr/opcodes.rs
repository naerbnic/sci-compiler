//! Representation of concrete opcodes. These represent concrete
//! bytecode instructions that will be emitted into the final object file.

/// Indicates the size of any variable width arguments to an instruction.
#[derive(Debug, Clone, Copy, PartialEq, Eq, PartialOrd, Ord, Hash)]
pub(crate) enum ArgWidth {
    /// Arguments are 1 byte wide.
    Byte,
    /// Arguments are 2 bytes wide.
    Word,
}

/// A single-byte opcode. Does not include any arguments.
///
/// This always represents a single byte value.
#[derive(Debug, Clone, Copy, PartialEq, Eq, PartialOrd, Ord, Hash)]
pub(crate) struct Opcode {
    op: Op,
    arg_width: ArgWidth,
}

impl Opcode {
    pub(crate) fn new(op: Op, arg_width: ArgWidth) -> Self {
        Self { op, arg_width }
    }

    pub(crate) fn to_u8(self) -> u8 {
        self.op.to_u8(self.arg_width)
    }

    pub(crate) fn args(&self) -> Vec<ArgType> {
        self.op
            .args()
            .iter()
            .map(|arg| match (self.arg_width, arg) {
                (_, VarArgType::UWord) => ArgType::UWord,
                (_, VarArgType::SWord) => ArgType::SWord,
                (_, VarArgType::UByte) => ArgType::UByte,
                (_, VarArgType::SByte) => ArgType::SByte,
                (ArgWidth::Byte, VarArgType::SVar) => ArgType::SByte,
                (ArgWidth::Word, VarArgType::SVar) => ArgType::SWord,
                (ArgWidth::Byte, VarArgType::UVar) => ArgType::UByte,
                (ArgWidth::Word, VarArgType::UVar) => ArgType::UWord,
            })
            .collect()
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, PartialOrd, Ord, Hash)]
pub(crate) enum Op {
    Simple(SimpleOp),
    Branch(BranchOp),
    Prop(PropOp),
    Mem(MemOp),
    LoadOffset(LoadOffsetOp),
}

impl Op {
    pub(super) fn inst_size(&self, width: ArgWidth) -> usize {
        let mut size = 1; // 1 byte for the opcode itself
        for arg in self.args() {
            size += match (width, arg) {
                (_, VarArgType::UWord) => 2,
                (_, VarArgType::SWord) => 2,
                (_, VarArgType::UByte) => 1,
                (_, VarArgType::SByte) => 1,
                (ArgWidth::Byte, VarArgType::SVar) => 1,
                (ArgWidth::Word, VarArgType::SVar) => 2,
                (ArgWidth::Byte, VarArgType::UVar) => 1,
                (ArgWidth::Word, VarArgType::UVar) => 2,
            };
        }
        size
    }

    pub(super) fn args(&self) -> &'static [VarArgType] {
        match self {
            Op::Simple(op) => op.args(),
            Op::Branch(op) => op.args(),
            Op::Prop(op) => op.args(),
            Op::Mem(op) => op.args(),
            Op::LoadOffset(op) => op.args(),
        }
    }

    pub(super) fn to_u8(self, arg_width: ArgWidth) -> u8 {
        match self {
            Op::Simple(op) => op.as_u8(arg_width),
            Op::Branch(op) => op.to_u8(arg_width),
            Op::Prop(op) => op.to_u8(arg_width),
            Op::Mem(op) => op.to_u8(arg_width),
            Op::LoadOffset(op) => op.to_u8(arg_width),
        }
    }

    pub(super) fn to_opcode(self, arg_width: ArgWidth) -> Opcode {
        Opcode::new(self, arg_width)
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, PartialOrd, Ord, Hash)]
pub(crate) enum ArgType {
    UWord,
    SWord,
    UByte,
    SByte,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, PartialOrd, Ord, Hash)]
pub(crate) enum VarArgType {
    UWord,
    SWord,
    UByte,
    SByte,
    SVar,
    UVar,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, PartialOrd, Ord, Hash)]
pub(crate) enum SimpleOp {
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
    Ldi,
    Push,
    Pushi,
    Toss,
    Dup,
    Link,
    Call,
    Callk,
    Callb,
    Calle,
    Ret,
    Send,
    Class,
    Self_,
    Super,
    Rest,
    Lea,
    SelfID,
    Pprev,
    Push0,
    Push1,
    Push2,
    PushSelf,
    DebugInfo,
}

impl SimpleOp {
    pub(crate) fn args(self) -> &'static [VarArgType] {
        use SimpleOp as O;
        match self {
            O::Bnot
            | O::Add
            | O::Sub
            | O::Mul
            | O::Div
            | O::Mod
            | O::Shr
            | O::Shl
            | O::Xor
            | O::And
            | O::Or
            | O::Neg
            | O::Not
            | O::Eq
            | O::Ne
            | O::Gt
            | O::Ge
            | O::Lt
            | O::Le
            | O::Ugt
            | O::Uge
            | O::Ult
            | O::Ule
            | O::Push
            | O::Toss
            | O::Dup
            | O::Pprev
            | O::Push0
            | O::Push1
            | O::Push2
            | O::PushSelf
            | O::Ret
            | O::SelfID => &[],
            O::Ldi | O::Pushi | O::Link | O::Class | O::Rest => &[VarArgType::SVar],
            O::Call | O::Callk | O::Callb => &[VarArgType::SVar, VarArgType::UByte],
            O::Calle => &[VarArgType::UVar, VarArgType::SVar, VarArgType::UByte],
            O::Send | O::Self_ => &[VarArgType::UByte],
            O::Super => &[VarArgType::UVar, VarArgType::UByte],
            O::Lea => &[VarArgType::SVar, VarArgType::UVar],
            O::DebugInfo => &[VarArgType::UWord],
        }
    }

    pub(crate) fn as_u8(self, arg_width: ArgWidth) -> u8 {
        use SimpleOp as O;
        let prefix: u8 = match self {
            O::Bnot => 0x00,
            O::Add => 0x01,
            O::Sub => 0x02,
            O::Mul => 0x03,

            O::Div => 0x04,
            O::Mod => 0x05,
            O::Shr => 0x06,
            O::Shl => 0x07,

            O::Xor => 0x08,
            O::And => 0x09,
            O::Or => 0x0A,
            O::Neg => 0x0B,

            O::Not => 0x0C,
            O::Eq => 0x0D,
            O::Ne => 0x0E,
            O::Gt => 0x0F,

            O::Ge => 0x10,
            O::Lt => 0x11,
            O::Le => 0x12,
            O::Ugt => 0x13,

            O::Uge => 0x14,
            O::Ult => 0x15,
            O::Ule => 0x16,
            O::Ldi => 0x1A,
            O::Push => 0x1B,

            O::Pushi => 0x1C,
            O::Toss => 0x1D,
            O::Dup => 0x1E,
            O::Link => 0x1F,

            O::Call => 0x20,
            O::Callk => 0x21,
            O::Callb => 0x22,
            O::Calle => 0x23,

            O::Ret => 0x24,
            O::Send => 0x25,
            // Invalid => 0x26,
            // Invalid => 0x27,
            O::Class => 0x28,
            // Invalid => 0x29,
            O::Self_ => 0x2A,
            O::Super => 0x2B,
            O::Rest => 0x2C,
            O::Lea => 0x2D,
            O::SelfID => 0x2E,
            // Invalid => 0x2F,
            O::Pprev => 0x30,
            // Property Accessors (0x31 - 0x38)
            // load offset (0x39, 0x3A)
            O::Push0 => 0x3B,
            O::Push1 => 0x3C,
            O::Push2 => 0x3D,
            O::PushSelf => 0x3E,
            O::DebugInfo => 0x3F,
        };

        let byte_bit: u8 = match arg_width {
            ArgWidth::Byte => 0x01,
            ArgWidth::Word => 0x00,
        };

        (prefix << 1) | byte_bit
    }

    pub(super) fn to_op(self) -> Op {
        Op::Simple(self)
    }

    pub(super) fn to_byte_opcode(self) -> Opcode {
        Opcode::new(Op::Simple(self), ArgWidth::Byte)
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, PartialOrd, Ord, Hash)]
pub(crate) enum BranchOp {
    Bt,
    Bnt,
    Jmp,
}

impl BranchOp {
    pub(crate) fn args(self) -> &'static [VarArgType] {
        &[VarArgType::SVar]
    }

    pub(crate) fn to_u8(self, arg_width: ArgWidth) -> u8 {
        let prefix: u8 = match self {
            BranchOp::Bt => 0x17,
            BranchOp::Bnt => 0x18,
            BranchOp::Jmp => 0x19,
        };

        let byte_bit: u8 = match arg_width {
            ArgWidth::Byte => 0x01,
            ArgWidth::Word => 0x00,
        };

        (prefix << 1) | byte_bit
    }

    pub(crate) fn to_op(self) -> Op {
        Op::Branch(self)
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, PartialOrd, Ord, Hash)]
pub(crate) enum MemAccess {
    Load,
    Store,
    Inc,
    Dec,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, PartialOrd, Ord, Hash)]
pub(crate) enum SlotAccess {
    Global,
    Local,
    Param,
    Tmp,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, PartialOrd, Ord, Hash)]
pub(crate) enum IndexMode {
    Indexed,
    NotIndexed,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, PartialOrd, Ord, Hash)]
pub(crate) enum ValueLocation {
    Stack,
    Accum,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, PartialOrd, Ord, Hash)]
pub(crate) struct MemOp {
    access: MemAccess,
    slot: SlotAccess,
    index_mode: IndexMode,
    value_location: ValueLocation,
}

impl MemOp {
    pub(crate) fn access(self) -> MemAccess {
        self.access
    }

    pub(crate) fn slot(self) -> SlotAccess {
        self.slot
    }

    pub(crate) fn index(self) -> IndexMode {
        self.index_mode
    }

    pub(crate) fn location(self) -> ValueLocation {
        self.value_location
    }

    pub(crate) fn args(self) -> &'static [VarArgType] {
        &[VarArgType::SVar]
    }

    pub(crate) fn to_u8(self, arg_width: ArgWidth) -> u8 {
        let slot_bits: u8 = match self.slot {
            SlotAccess::Global => 0b00,
            SlotAccess::Local => 0b01,
            SlotAccess::Param => 0b10,
            SlotAccess::Tmp => 0b11,
        };

        let access_bits: u8 = match self.access {
            MemAccess::Load => 0b00,
            MemAccess::Store => 0b01,
            MemAccess::Inc => 0b10,
            MemAccess::Dec => 0b11,
        };

        let index_bit: u8 = match self.index_mode {
            IndexMode::Indexed => 0b1,
            IndexMode::NotIndexed => 0b0,
        };

        let value_location_bit: u8 = match self.value_location {
            ValueLocation::Stack => 0b0,
            ValueLocation::Accum => 0b1,
        };

        let byte_bit: u8 = match arg_width {
            ArgWidth::Byte => 0x01,
            ArgWidth::Word => 0x00,
        };
        let high_bit: u8 = 0x80; // High bit set for memory opcodes
        high_bit
            | (slot_bits << 6)
            | (access_bits << 4)
            | (index_bit << 3)
            | (value_location_bit << 1)
            | byte_bit
    }

    pub(crate) fn to_op(self) -> Op {
        Op::Mem(self)
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, PartialOrd, Ord, Hash)]
pub(crate) struct PropOp {
    access: MemAccess,
    value_location: ValueLocation,
}

impl PropOp {
    pub(crate) fn access(self) -> MemAccess {
        self.access
    }

    pub(crate) fn value_location(self) -> ValueLocation {
        self.value_location
    }
}

impl PropOp {
    pub(crate) fn args(self) -> &'static [VarArgType] {
        &[VarArgType::UVar]
    }

    pub(crate) fn to_u8(self, arg_width: ArgWidth) -> u8 {
        // [load, accum] pToa == 0x31,
        // [store, accum] aTop,
        // [load, stack] pTos,
        // [store, stack] sTop,
        // [inc, accum] ipToa,
        // [dec, accum] dpToa,
        // [inc, stack] ipTos,
        // [dec, stack] dpTos
        let access_bits: u8 = match self.access {
            MemAccess::Load => 0b000,
            MemAccess::Store => 0b001,
            MemAccess::Inc => 0b100,
            MemAccess::Dec => 0b101,
        };

        let value_location_bit: u8 = match self.value_location {
            ValueLocation::Stack => 0b10,
            ValueLocation::Accum => 0b00,
        };

        let byte_bit: u8 = match arg_width {
            ArgWidth::Byte => 0x01,
            ArgWidth::Word => 0x00,
        };
        let base_pattern: u8 = access_bits | value_location_bit;
        let prefix = base_pattern + 0x31;

        (prefix << 1) | byte_bit
    }

    pub(crate) fn to_op(self) -> Op {
        Op::Prop(self)
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, PartialOrd, Ord, Hash)]
pub(crate) struct LoadOffsetOp {
    value_location: ValueLocation,
}

impl LoadOffsetOp {
    pub(crate) fn value_location(self) -> ValueLocation {
        self.value_location
    }
    pub(crate) fn args(self) -> &'static [VarArgType] {
        &[VarArgType::SVar]
    }

    pub(crate) fn to_u8(self, arg_width: ArgWidth) -> u8 {
        let value_location_bit: u8 = match self.value_location {
            ValueLocation::Stack => 0b1,
            ValueLocation::Accum => 0b0,
        };

        let byte_bit: u8 = match arg_width {
            ArgWidth::Byte => 0x01,
            ArgWidth::Word => 0x00,
        };
        let prefix = 0x39 | (value_location_bit << 1);

        (prefix << 1) | byte_bit
    }

    pub(crate) fn to_op(self) -> Op {
        Op::LoadOffset(self)
    }
}
