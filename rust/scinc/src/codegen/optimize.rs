// Optimizations done by C++ code:

use crate::{
    codegen::repr::{
        inst::{ComputedArg, ResolvedArg, SeqInst, SimpleInst},
        opcodes::{IndexMode, MemAccess, SlotAccess, ValueLocation},
    },
    int::{MWord, UWord},
};

#[derive(Copy, Clone, Debug, PartialEq, Eq)]
enum VarLocation {
    Global,
    Local,
    Param,
    Tmp,
    Prop,
}

/// The abstrat value found in the state.
///
/// These provide guarantees about the given value.
#[derive(Copy, Clone, Debug)]
enum AbstractValue {
    /// The given value is unknown
    Unknown,
    /// The given value is exactly known to be a specific value.
    Immediate(MWord),
    /// The given value is known to be the current value of the current object
    /// property.
    ///
    /// Note that this is invalidated if either the value is modified, or if
    /// the property is modified.
    Var { loc: VarLocation, index: UWord },
    /// The given value is known to be the pointer to the current object.
    Self_,
}

impl AbstractValue {
    fn maybe_invalidate_var(self, loc: VarLocation, index: Option<UWord>) -> Self {
        if let AbstractValue::Var {
            loc: var_loc,
            index: var_index,
        } = self
        {
            if var_loc != loc {
                return self;
            }

            let invalidate = if let Some(index) = index {
                var_index == index
            } else {
                // None indicates that we don't know the index.
                true
            };

            if invalidate {
                return AbstractValue::Unknown;
            }
        };
        self
    }

    fn invalidate_var(self) -> Self {
        if let AbstractValue::Var { .. } = self {
            AbstractValue::Unknown
        } else {
            self
        }
    }

    fn merge(self, other: Self) -> Self {
        match (self, other) {
            (AbstractValue::Unknown, _) | (_, AbstractValue::Unknown) => AbstractValue::Unknown,
            (AbstractValue::Immediate(v1), AbstractValue::Immediate(v2)) if v1 == v2 => {
                AbstractValue::Immediate(v1)
            }
            (
                AbstractValue::Var {
                    loc: loc1,
                    index: index1,
                },
                AbstractValue::Var {
                    loc: loc2,
                    index: index2,
                },
            ) if loc1 == loc2 && index1 == index2 => AbstractValue::Var {
                loc: loc1,
                index: index1,
            },
            (AbstractValue::Self_, AbstractValue::Self_) => AbstractValue::Self_,
            _ => AbstractValue::Unknown,
        }
    }
}

#[derive(Clone, Debug)]
struct AbstractState {
    acc: AbstractValue,
    stack_top: AbstractValue,
}

impl AbstractState {
    fn maybe_invalidate_var(&self, loc: VarLocation, index: Option<UWord>) -> Self {
        Self {
            acc: self.acc.maybe_invalidate_var(loc, index),
            stack_top: self.stack_top.maybe_invalidate_var(loc, index),
        }
    }

    fn invalidate_var(&self) -> Self {
        Self {
            acc: self.acc.invalidate_var(),
            stack_top: self.stack_top.invalidate_var(),
        }
    }
}

fn interpret_inst(state: &AbstractState, inst: &SeqInst<ResolvedArg>) -> AbstractState {
    use AbstractValue as V;
    match inst {
        SeqInst::Simple(simple_inst) => match simple_inst {
            // Unary operations only use the accumulator.
            SimpleInst::Neg | SimpleInst::Bnot | SimpleInst::Not => AbstractState {
                acc: AbstractValue::Unknown,
                ..*state
            },
            // Binary operations use both the accumulator and the stack top.
            SimpleInst::Add
            | SimpleInst::Sub
            | SimpleInst::Mul
            | SimpleInst::Div
            | SimpleInst::Mod
            | SimpleInst::Shr
            | SimpleInst::Shl
            | SimpleInst::Xor
            | SimpleInst::And
            | SimpleInst::Or
            | SimpleInst::Eq => AbstractState {
                acc: V::Unknown,
                stack_top: V::Unknown,
            },

            // Comparison operations use both the accumulator and the stack top.
            //
            // They also store into the prev register.
            SimpleInst::Ne
            | SimpleInst::Gt
            | SimpleInst::Ge
            | SimpleInst::Lt
            | SimpleInst::Le
            | SimpleInst::Ugt
            | SimpleInst::Uge
            | SimpleInst::Ult
            | SimpleInst::Ule => AbstractState {
                acc: V::Unknown,
                stack_top: V::Unknown,
            },

            SimpleInst::Push => AbstractState {
                stack_top: state.acc,
                ..*state
            },
            SimpleInst::Toss => AbstractState {
                stack_top: V::Unknown,
                ..*state
            },
            SimpleInst::Dup => state.clone(),
            SimpleInst::SelfID => AbstractState {
                acc: V::Self_,
                ..*state
            },
            SimpleInst::Pprev => AbstractState {
                acc: V::Unknown,
                ..*state
            },
            SimpleInst::PushSelf => todo!(),
        },
        SeqInst::Ldi(arg) => AbstractState {
            acc: if let Some(word) = arg.try_to_mword() {
                V::Immediate(word)
            } else {
                V::Unknown
            },
            ..*state
        },
        SeqInst::Pushi(sword) => AbstractState {
            stack_top: V::Immediate(sword.to_machine()),
            ..*state
        },
        SeqInst::Link(_) => AbstractState {
            stack_top: V::Unknown,
            ..*state
        },
        SeqInst::Call(_) | SeqInst::Send(_) => {
            // Any values tracking the global state must be invalidated, since
            // we don't know what the function call may have done.
            state.invalidate_var()
        }
        SeqInst::Lea { .. } => AbstractState {
            acc: V::Unknown,
            stack_top: V::Unknown,
        },
        SeqInst::Class(_) => AbstractState {
            acc: V::Unknown,
            ..*state
        },
        SeqInst::Rest(_) => AbstractState {
            stack_top: V::Unknown,
            ..*state
        },
        SeqInst::Prop(prop_inst) => {
            match (prop_inst.op().access(), prop_inst.op().value_location()) {
                (MemAccess::Store, ValueLocation::Stack) => AbstractState {
                    stack_top: V::Var {
                        loc: VarLocation::Prop,
                        index: prop_inst.index(),
                    },
                    ..*state
                },
                (MemAccess::Store, ValueLocation::Accum) => state.clone(),
                (_, ValueLocation::Stack) => AbstractState {
                    stack_top: V::Unknown,
                    ..*state
                },
                (_, ValueLocation::Accum) => AbstractState {
                    acc: V::Unknown,
                    ..*state
                },
            }
        }
        SeqInst::LoadOffset(load_offset_inst) => match load_offset_inst.op().value_location() {
            ValueLocation::Stack => AbstractState {
                stack_top: V::Unknown,
                ..*state
            },
            ValueLocation::Accum => AbstractState {
                acc: V::Unknown,
                ..*state
            },
        },
        SeqInst::DebugInfo(_) => state.clone(),
        SeqInst::Mem(mem_inst) => {
            let loc = match mem_inst.op().slot() {
                SlotAccess::Global => VarLocation::Global,
                SlotAccess::Local => VarLocation::Local,
                SlotAccess::Param => VarLocation::Param,
                SlotAccess::Tmp => VarLocation::Tmp,
            };
            match mem_inst.op().access() {
                MemAccess::Store => {
                    let inval_slot = if let IndexMode::NotIndexed = mem_inst.op().index() {
                        Some(mem_inst.slot())
                    } else {
                        None
                    };
                    let next_state = state.maybe_invalidate_var(loc, inval_slot);
                    match mem_inst.op().location() {
                        // If we're storing fromt the stack, then the top of the stack
                        // is popped off, so we no longer know what value is there.
                        ValueLocation::Stack => AbstractState {
                            stack_top: V::Unknown,
                            ..next_state
                        },
                        // Storing from the accumulator doesn't change the accumulator.
                        ValueLocation::Accum => next_state,
                    }
                }
                _ => {
                    let value = match mem_inst.op().index() {
                        // If indexed, we can't know where the value is read from, so
                        // we treat it as unknown.
                        IndexMode::Indexed => V::Unknown,
                        IndexMode::NotIndexed => V::Var {
                            loc,
                            index: mem_inst.slot(),
                        },
                    };
                    match mem_inst.op().location() {
                        ValueLocation::Stack => AbstractState {
                            stack_top: value,
                            ..*state
                        },
                        ValueLocation::Accum => AbstractState {
                            acc: value,
                            ..*state
                        },
                    }
                }
            }
        }
    }
}
