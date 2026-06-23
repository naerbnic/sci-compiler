mod builder;

use std::collections::HashSet;

use crate::codegen::repr::inst::{ResolvedArg, SeqInst};

#[derive(Debug, Clone, PartialEq, Eq, Hash)]
pub(crate) struct BlockId(usize);

#[derive(Debug, Clone, PartialEq, Eq, Hash)]
pub(crate) enum BranchOp {
    BranchIfTrue,
    BranchIfFalse,
}

#[derive(Debug, Clone, PartialEq, Eq, Hash)]
enum BlockSource {
    Entry,
    Block(BlockId),
}

pub(crate) struct BranchTerm {
    op: BranchOp,
    taken_target: BlockId,
    not_taken_target: BlockId,
}

pub(crate) enum Terminator {
    Branch(BranchTerm),
    Jump(BlockId),
    Return,
}

pub(crate) struct BasicBlock {
    sources: HashSet<BlockSource>,
    seq: Vec<SeqInst<ResolvedArg>>,
    // This is an option only for the purposes of building a graph. Uses with
    // a built graph should always have a terminator (or error during building).
    terminator: Option<Terminator>,
}

pub(crate) struct DataFlowGraph {
    entrance: BlockId,
    blocks: slab::Slab<BasicBlock>,
}

impl DataFlowGraph {
    pub(crate) fn builder() -> builder::Builder {
        builder::Builder::new()
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_build_infinite_loop() {
        let mut builder = DataFlowGraph::builder();
        let start = builder.new_label();
        builder.insert_label(&start);
        builder.jump(&start);
        let _graph = builder.build();
    }
}
