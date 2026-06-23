//! After assembly and optimization (i.e. the order of instructions in the
//! program is finalized), we need to do an iterative shrinking of the layout of
//! the program. Some instructions, when their arguments are small enough, can be
//! encoded using smaller formats, but this has an effect on the layout of a
//! script memory space. This module provides code and types needed to perform
//! this iterative shrinking.
//!
//! This relies on an assumption that the layout of the program is monotonic.
//! If an entry gives a size in one state, all future sizes provided for
//! iterated states will be equal or smaller.

use std::{borrow::Borrow, cell::Cell, collections::BTreeMap, num::NonZeroUsize};

/// A value that produces a sequence of bytes, based on the current layout state.
trait Atom {
    fn data(&self, offset: usize, state: &LayoutState) -> Vec<u8>;
    fn size(&self, offset: usize, state: &LayoutState) -> usize {
        self.data(offset, state).len()
    }
}

struct LayoutEntry {
    atom: AtomEntry,
    offset: Cell<Option<usize>>,
}

impl LayoutEntry {
    fn predicted_size(&self, offset: usize, state: &LayoutState) -> usize {
        self.atom.predicted_size(offset, state)
    }
}

struct LayoutState {
    labels: BTreeMap<LabelId, usize>,
    entries: Vec<LayoutEntry>,
    // This is usize, in case the total size is u16::MAX.
    curr_size: Option<usize>,
}

impl LayoutState {
    fn iterate_once(&mut self) -> bool {
        let mut was_updated = false;
        let mut curr_offset = 0usize;

        for entry in self.entries.iter() {
            if let Some(old_offset) = entry.offset.get() {
                if curr_offset != old_offset {
                    entry.offset.set(Some(curr_offset));
                    was_updated = true;
                }
                if curr_offset > old_offset {
                    panic!(
                        "Layout is not monotonic: new offset {} is greater than old offset {}",
                        curr_offset, old_offset
                    );
                }
            } else {
                entry.offset.set(Some(curr_offset));
                was_updated = true;
            }
            let predicted_size = entry.predicted_size(curr_offset, self);
            let new_offset = curr_offset.checked_add(predicted_size).unwrap();
            curr_offset = new_offset;
        }

        if self.curr_size != Some(curr_offset) {
            self.curr_size = Some(curr_offset);
            was_updated = true;
        }

        was_updated
    }

    /// Returns the offset of the given label, if it is currently resolved. When
    /// None is returned, if used in predicted_size, it should be treated as
    /// unknown.
    pub(crate) fn offset_of(&self, label: LabelId) -> Option<usize> {
        let entry_index = self.labels[&label];
        if entry_index == self.entries.len() {
            return self.curr_size;
        }
        self.entries[entry_index].offset.get()
    }

    fn iterate_to_fixed_point(&mut self) {
        while self.iterate_once() {}
    }

    fn into_data(self) -> DataLayout {
        let mut bytes = Vec::new();
        let mut label_offsets = BTreeMap::new();
        for entry in self.entries.iter() {
            let offset = entry.offset.get().unwrap();
            let entry_bytes = entry.atom.to_bytes(offset, &self);
            bytes.extend(entry_bytes);
        }

        for label in self.labels.keys() {
            label_offsets.insert(*label, self.offset_of(*label).unwrap());
        }
        DataLayout {
            data: bytes,
            label_offsets,
        }
    }
}

type DynamicAtom = Box<dyn Atom>;

enum AtomEntry {
    /// Raw bytes that will be emitted into the output buffer.
    Bytes(Vec<u8>),

    /// Additional bytes needed to align the next entry to the given alignment.
    AlignmentPadding(usize),

    /// A block that has dynmaic content, based on existing layout.
    Dynamic(DynamicAtom),
}

impl AtomEntry {
    fn predicted_size(&self, offset: usize, state: &LayoutState) -> usize {
        match self {
            AtomEntry::Bytes(bytes) => bytes.len(),
            AtomEntry::AlignmentPadding(pad) => offset.next_multiple_of(*pad),
            AtomEntry::Dynamic(atom) => atom.size(offset, state),
        }
    }

    fn to_bytes(&self, offset: usize, state: &LayoutState) -> Vec<u8> {
        match self {
            AtomEntry::Bytes(bytes) => bytes.clone(),
            AtomEntry::AlignmentPadding(pad) => {
                let next_offset = offset.next_multiple_of(*pad);
                let padding_size = next_offset - offset;
                vec![0u8; padding_size]
            }
            AtomEntry::Dynamic(atom) => atom.data(offset, state),
        }
    }
}

struct LabelInner {
    index: Option<usize>,
}

pub(crate) struct Builder {
    labels: BTreeMap<LabelId, Option<usize>>,
    atoms: Vec<AtomEntry>,
    next_label: NonZeroUsize,
}

impl Builder {
    pub(crate) fn new() -> Self {
        Builder {
            labels: BTreeMap::new(),
            atoms: Vec::new(),
            next_label: NonZeroUsize::new(1).unwrap(),
        }
    }

    pub(crate) fn new_label(&mut self) -> LabelId {
        let label_id = LabelId(self.next_label);
        self.next_label = self.next_label.checked_add(1).expect("Too many labels");
        if self.labels.insert(label_id, None).is_some() {
            panic!("Internal error: label {:?} already exists", label_id);
        }
        label_id
    }

    pub(crate) fn add_label(&mut self, label: LabelId) {
        let Some(index) = self.labels.get_mut(&label) else {
            panic!("Label {:?} does not exist", label);
        };
        if index.replace(self.atoms.len()).is_some() {
            panic!("Label {:?} is already defined", label);
        }
    }

    fn pad_to_alignment(&mut self, alignment: usize) {
        assert!(alignment.is_power_of_two());
        if alignment > 1 {
            self.atoms.push(AtomEntry::AlignmentPadding(alignment));
        }
    }

    /// Add a sequence of bytes to the end of the layout. The bytes will be
    /// aligned to the given alignment, from the beginning of the output buffer.
    fn add_bytes(&mut self, bytes: impl IntoIterator<Item = impl Borrow<u8>>) {
        self.atoms.push(AtomEntry::Bytes(
            bytes.into_iter().map(|b| *b.borrow()).collect(),
        ));
    }

    fn add_fn_atom<F>(&mut self, atom: F)
    where
        F: Fn(usize, &LayoutState) -> Vec<u8> + 'static,
    {
        struct FnAtom<F>(F);

        impl<F> Atom for FnAtom<F>
        where
            F: Fn(usize, &LayoutState) -> Vec<u8> + 'static,
        {
            fn data(&self, offset: usize, state: &LayoutState) -> Vec<u8> {
                (self.0)(offset, state)
            }
        }

        self.atoms.push(AtomEntry::Dynamic(Box::new(FnAtom(atom))));
    }

    fn add_sized_atom<F>(&mut self, size: usize, atom: F)
    where
        F: Fn(usize, &LayoutState) -> Vec<u8> + 'static,
    {
        struct SizedAtom<F> {
            size: usize,
            data_fn: F,
        }

        impl<F> Atom for SizedAtom<F>
        where
            F: Fn(usize, &LayoutState) -> Vec<u8> + 'static,
        {
            fn data(&self, offset: usize, state: &LayoutState) -> Vec<u8> {
                let data = (self.data_fn)(offset, state);
                assert_eq!(data.len(), self.size);
                data
            }

            fn size(&self, _offset: usize, _state: &LayoutState) -> usize {
                self.size
            }
        }

        self.atoms.push(AtomEntry::Dynamic(Box::new(SizedAtom {
            size,
            data_fn: atom,
        })));
    }

    fn into_data(self) -> DataLayout {
        let mut entries = Vec::new();

        for entry in self.atoms.into_iter() {
            entries.push(LayoutEntry {
                atom: entry,
                offset: Cell::new(None),
            });
        }

        let new_labels = self
            .labels
            .into_iter()
            .map(|(label, index)| {
                if let Some(index) = index {
                    Ok((label, index))
                } else {
                    Err(format!("label {:?} was not defined", label))
                }
            })
            .collect::<Result<_, _>>();

        let mut state = LayoutState {
            labels: new_labels.unwrap(),
            entries,
            curr_size: None,
        };
        state.iterate_to_fixed_point();
        state.into_data()
    }
}

#[derive(Clone, Copy, Debug, PartialEq, Eq, PartialOrd, Ord, Hash)]
pub(crate) struct LabelId(NonZeroUsize);

pub(crate) struct DataLayout {
    data: Vec<u8>,
    label_offsets: BTreeMap<LabelId, usize>,
}

impl DataLayout {
    pub(crate) fn data(&self) -> &[u8] {
        &self.data
    }

    pub(crate) fn offset_of(&self, label: LabelId) -> usize {
        *self.label_offsets.get(&label).unwrap()
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_bytes_layout() {
        let mut builder = Builder::new();
        builder.add_bytes(b"abcde");
        assert_eq!(builder.into_data().data(), b"abcde");
    }

    #[test]
    fn test_alignment_padding() {
        let mut builder = Builder::new();
        builder.add_bytes(b"abc");
        builder.pad_to_alignment(4);
        builder.add_bytes(b"def");
        assert_eq!(builder.into_data().data(), b"abc\0def");
    }

    #[test]
    fn test_forward_reference() {
        let mut builder = Builder::new();
        let label = builder.new_label();
        builder.add_fn_atom({
            move |_offset, state| {
                let base_offset = state.offset_of(label).unwrap_or(0xFF) as u8;
                vec![base_offset]
            }
        });
        builder.add_bytes(b"abcdef");
        builder.add_label(label);
        assert_eq!(builder.into_data().data(), b"\x07abcdef");
    }

    #[test]
    fn test_iteration() {
        let mut builder = Builder::new();
        let label = builder.new_label();
        builder.add_fn_atom({
            move |_offset, state| {
                // Shrinks its size by one for each value. Should shrink to 0
                let base_offset = state.offset_of(label);
                let size = base_offset.unwrap_or(255).saturating_sub(1);
                vec![0u8; size]
            }
        });
        builder.add_label(label);
        assert_eq!(builder.into_data().data(), b"");
    }

    #[test]
    fn test_output_offsets() {
        let mut builder = Builder::new();
        let la = builder.new_label();
        let lb = builder.new_label();
        let lc = builder.new_label();
        builder.add_bytes(b"abc");
        builder.add_label(la);
        builder.add_bytes(b"def");
        builder.add_label(lb);
        builder.add_bytes(b"ghi");
        builder.add_label(lc);
        let data = builder.into_data();
        assert_eq!(data.data(), b"abcdefghi");
        assert_eq!(data.offset_of(la), 3);
        assert_eq!(data.offset_of(lb), 6);
        assert_eq!(data.offset_of(lc), 9);
    }
}
