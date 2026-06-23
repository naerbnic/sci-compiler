use std::collections::{HashMap, HashSet};

use slab::Slab;

use crate::{
    codegen::{
        dataflow::{BasicBlock, BlockId, BlockSource},
        repr::inst::{ResolvedArg, SeqInst},
    },
    util::graph::reachable_ids,
};

#[derive(Debug, Clone, PartialEq, Eq, Hash)]
pub(crate) struct Label(usize);

#[derive(Debug, Clone)]

enum Terminator {
    Branch {
        op: super::BranchOp,
        taken: Label,
        not_taken: Label,
    },
    Jump(Label),
    Return,
}

impl Terminator {
    fn get_targets(&self) -> Vec<Label> {
        match self {
            Terminator::Branch {
                taken, not_taken, ..
            } => vec![taken.clone(), not_taken.clone()],
            Terminator::Jump(target) => vec![target.clone()],
            Terminator::Return => vec![],
        }
    }

    fn build_with(
        &self,
        label_to_block_index: &HashMap<Label, usize>,
        old_index_to_new_index: &HashMap<usize, BlockId>,
    ) -> super::Terminator {
        match self {
            Terminator::Branch {
                op,
                taken,
                not_taken,
            } => super::Terminator::Branch(super::BranchTerm {
                op: op.clone(),
                taken_target: old_index_to_new_index
                    .get(label_to_block_index.get(taken).unwrap())
                    .unwrap()
                    .clone(),
                not_taken_target: old_index_to_new_index
                    .get(label_to_block_index.get(not_taken).unwrap())
                    .unwrap()
                    .clone(),
            }),
            Terminator::Jump(target) => super::Terminator::Jump(
                old_index_to_new_index
                    .get(label_to_block_index.get(target).unwrap())
                    .expect("Label target not found")
                    .clone(),
            ),
            Terminator::Return => super::Terminator::Return,
        }
    }
}

struct LabelInner {
    block_index: Option<usize>,
}

struct BlockInner {
    seq: Vec<SeqInst<ResolvedArg>>,
    terminator: Option<Terminator>,
}

impl BlockInner {
    fn get_labels(&self) -> impl IntoIterator<Item = Label> + '_ {
        self.terminator.as_ref().unwrap().get_targets()
    }
}

pub(crate) struct Builder {
    pending_labels: slab::Slab<LabelInner>,
    entry_block: usize,
    curr_block: usize,
    blocks: slab::Slab<BlockInner>,
}

impl Builder {
    pub(crate) fn new() -> Self {
        let mut blocks = slab::Slab::new();
        let entry_block = blocks.insert(BlockInner {
            seq: Vec::new(),
            terminator: None,
        });
        Self {
            pending_labels: slab::Slab::new(),
            entry_block,
            curr_block: entry_block,
            blocks,
        }
    }

    pub(crate) fn new_label(&mut self) -> Label {
        let label_index = self.pending_labels.insert(LabelInner { block_index: None });
        Label(label_index)
    }

    fn get_block(&self, block_index: usize) -> &BlockInner {
        self.blocks.get(block_index).unwrap()
    }

    fn get_block_mut(&mut self, block_index: usize) -> &mut BlockInner {
        self.blocks.get_mut(block_index).unwrap()
    }

    fn get_curr_block_mut(&mut self) -> &mut BlockInner {
        self.get_block_mut(self.curr_block)
    }

    fn get_curr_block(&self) -> &BlockInner {
        self.blocks.get(self.curr_block).unwrap()
    }

    fn finish_block_with(&mut self, terminator: Terminator) {
        let block = self.get_curr_block_mut();
        assert!(
            block.terminator.is_none(),
            "Cannot add instructions after a terminator"
        );
        block.terminator = Some(terminator);
        self.curr_block = self.blocks.insert(BlockInner {
            seq: Vec::new(),
            terminator: None,
        });
    }

    fn set_label_target(&mut self, label: &Label, block_index: usize) {
        let label_inner = self.pending_labels.get_mut(label.0).unwrap();
        assert!(
            label_inner.block_index.is_none(),
            "Label target already set"
        );
        label_inner.block_index = Some(block_index);
    }

    fn get_label_target(&self, label: &Label) -> usize {
        self.pending_labels
            .get(label.0)
            .unwrap()
            .block_index
            .unwrap()
    }

    pub(crate) fn seq_inst(&mut self, inst: SeqInst<ResolvedArg>) {
        let block = self.blocks.get_mut(self.curr_block).unwrap();
        assert!(
            block.terminator.is_none(),
            "Cannot add instructions after a terminator"
        );
        block.seq.push(inst);
    }

    pub(crate) fn branch(&mut self, op: super::BranchOp, taken: Label) {
        let not_taken = self.new_label();
        self.finish_block_with(Terminator::Branch {
            op,
            taken,
            not_taken: not_taken.clone(),
        });
        self.set_label_target(&not_taken, self.curr_block);
    }

    pub(crate) fn jump(&mut self, target: &Label) {
        self.finish_block_with(Terminator::Jump(target.clone()));
    }

    pub(crate) fn ret(&mut self) {
        self.finish_block_with(Terminator::Return);
    }

    pub(crate) fn insert_label(&mut self, label: &Label) {
        if !self.get_curr_block().seq.is_empty() {
            // We have to create a new block for the label if we're currently partway
            // through creating a block.
            self.finish_block_with(Terminator::Jump(label.clone()));
        }
        self.set_label_target(label, self.curr_block);
    }

    pub(crate) fn build(self) -> super::DataFlowGraph {
        // Sanity checks:
        //
        // - All used labels must have been assigned a target block.
        // - All blocks reachable from the entry block must have a terminator.
        //
        // Warnings:
        //
        // - A label was created and inserted, but never used as a target.
        // - There exist non-empty blocks that are unreachable from the entry
        //   block.
        let reachable_blocks = reachable_ids([self.entry_block], |block_index| {
            self.get_block(*block_index)
                .get_labels()
                .into_iter()
                .map(|label| self.get_label_target(&label))
                .collect()
        });

        // Compute a map between reachable labels and their target blocks, to
        // be able to derive branch_ids for each block.
        let mut label_to_block_index: HashMap<Label, usize> = HashMap::new();
        for reachable_block_index in &reachable_blocks {
            let block = self.get_block(*reachable_block_index);
            for label in block.get_labels() {
                let target_block_index = self.get_label_target(&label);
                label_to_block_index.insert(label, target_block_index);
            }
        }

        let mut old_index_to_new_index: HashMap<usize, BlockId> = HashMap::new();
        let mut new_block_slab = Slab::new();
        let mut reverse_edges: HashMap<usize, HashSet<usize>> = HashMap::new();

        // We need to do this in two passes, because we have to have all of the
        // built IDs before wiring up the terminator targets.

        // By the end here, the old_index_to_new_index map and reverse edges
        // will be populated. The blocks will not have the sources or terminators set yet.
        for reachable_block_index in &reachable_blocks {
            let block = self.get_block(*reachable_block_index);
            let new_block_index = new_block_slab.insert(BasicBlock {
                sources: HashSet::new(),
                seq: block.seq.clone(),
                terminator: None,
            });
            old_index_to_new_index.insert(*reachable_block_index, BlockId(new_block_index));

            for target_label in block.get_labels() {
                let target_block_index = self.get_label_target(&target_label);
                reverse_edges
                    .entry(target_block_index)
                    .or_default()
                    .insert(*reachable_block_index);
            }
        }

        for reachable_block_index in &reachable_blocks {
            let block = self.get_block(*reachable_block_index);
            let new_block_index = old_index_to_new_index.get(reachable_block_index).unwrap();
            let new_block = new_block_slab.get_mut(new_block_index.0).unwrap();

            // Set the sources for this block.
            if let Some(sources) = reverse_edges.get(reachable_block_index) {
                for source in sources {
                    let source_new_index = old_index_to_new_index.get(source).unwrap();
                    new_block
                        .sources
                        .insert(BlockSource::Block(source_new_index.clone()));
                }
            }

            // Set the terminator for this block.
            new_block.terminator = Some(
                block
                    .terminator
                    .as_ref()
                    .unwrap()
                    .build_with(&label_to_block_index, &old_index_to_new_index),
            );
        }

        let entry_id = old_index_to_new_index
            .get(&self.entry_block)
            .expect("Entry block not found");
        let new_entry_block = new_block_slab.get_mut(entry_id.0).unwrap();
        new_entry_block.sources.insert(BlockSource::Entry);

        super::DataFlowGraph {
            entrance: entry_id.clone(),
            blocks: new_block_slab,
        }
    }
}
