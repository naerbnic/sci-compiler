use std::collections::BTreeMap;

use crate::util::ref_str::RefStr;

mod dataflow;
mod layout;
mod list;
mod node;
mod opcodes;
mod optimize;
mod repr;
mod core;

#[derive(Clone, Debug, PartialEq, Eq)]
pub struct TextRef(());

#[derive(Clone, Debug, PartialEq, Eq)]
pub struct PtrRef(());

#[derive(Clone, Debug, PartialEq, Eq)]
pub struct LabelRef(());

pub enum LiteralValue {
    Int(i32),
    String(TextRef),
}

pub struct ProcedureName(());

pub struct MethodName(());

pub enum FuncName {
    Procedure(ProcedureName),
    Method(MethodName),
}