use std::{
    borrow::Borrow,
    collections::{BTreeSet, btree_set},
    rc::Rc,
};

use crate::util::ref_str::RefStr;

const OBJ_ID_SEL_NAME: &str = "-objID-";
const SIZE_SEL_NAME: &str = "-size-";
const PROP_DICT_SEL_NAME: &str = "-propDict-";
const METH_DICT_SEL_NAME: &str = "-methDict-";
const CLASS_SCRIPT_SEL_NAME: &str = "-classScript-";
const SCRIPT_SEL_NAME: &str = "-script-";
const SUPER_SEL_NAME: &str = "-super-";
const INFO_SEL_NAME: &str = "-info-";
const NAME_SEL_NAME: &str = "-name-";

const SEL_OBJID: SelectorNum = SelectorNum(0x1000);
const SEL_SIZE: SelectorNum = SelectorNum(0x1001);
const SEL_PROP_DICT: SelectorNum = SelectorNum(0x1002);
const SEL_METH_DICT: SelectorNum = SelectorNum(0x1003);
const SEL_CLASS_SCRIPT: SelectorNum = SelectorNum(0x1004);
const SEL_SCRIPT: SelectorNum = SelectorNum(0x1005);
const SEL_SUPER: SelectorNum = SelectorNum(0x1006);
const SEL_INFO: SelectorNum = SelectorNum(0x1007);

struct StdEntry {
    num: SelectorNum,
    sym: &'static str,
}

const STD_ENTRIES: &[StdEntry] = &[
    StdEntry {
        num: SEL_OBJID,
        sym: OBJ_ID_SEL_NAME,
    },
    StdEntry {
        num: SEL_SIZE,
        sym: SIZE_SEL_NAME,
    },
    StdEntry {
        num: SEL_PROP_DICT,
        sym: PROP_DICT_SEL_NAME,
    },
    StdEntry {
        num: SEL_METH_DICT,
        sym: METH_DICT_SEL_NAME,
    },
    StdEntry {
        num: SEL_CLASS_SCRIPT,
        sym: CLASS_SCRIPT_SEL_NAME,
    },
    StdEntry {
        num: SEL_SCRIPT,
        sym: SCRIPT_SEL_NAME,
    },
    StdEntry {
        num: SEL_SUPER,
        sym: SUPER_SEL_NAME,
    },
    StdEntry {
        num: SEL_INFO,
        sym: INFO_SEL_NAME,
    },
];

#[derive(Debug, Clone, Copy, PartialEq, Eq, PartialOrd, Ord, Hash)]
pub(crate) struct SelectorNum(u16);

// The base struct for an entry.
struct InnerEntry {
    num: SelectorNum,
    sym: RefStr,
}

impl InnerEntry {
    fn as_entry(&self) -> Entry<'_> {
        Entry {
            num: self.num,
            sym: &self.sym,
        }
    }
}

// The set key types for selector names.

macro_rules! impl_entry_by_key {
    ($name:ident, $key_ty:ty) => {
        impl Borrow<$key_ty> for $name {
            fn borrow(&self) -> &$key_ty {
                &self.as_key()
            }
        }

        impl PartialEq for $name {
            fn eq(&self, other: &Self) -> bool {
                self.as_key() == other.as_key()
            }
        }

        impl Eq for $name {}

        impl PartialOrd for $name {
            fn partial_cmp(&self, other: &Self) -> Option<std::cmp::Ordering> {
                Some(self.cmp(other))
            }
        }

        impl Ord for $name {
            fn cmp(&self, other: &Self) -> std::cmp::Ordering {
                self.as_key().cmp(other.as_key())
            }
        }
    };
}

struct EntryByName(Rc<InnerEntry>);

impl EntryByName {
    fn as_key(&self) -> &str {
        self.0.sym.as_str()
    }
}

impl_entry_by_key!(EntryByName, str);

struct EntryByNum(Rc<InnerEntry>);

impl EntryByNum {
    fn as_key(&self) -> &SelectorNum {
        &self.0.num
    }
}

impl_entry_by_key!(EntryByNum, SelectorNum);

pub(crate) struct Entry<'a> {
    pub(crate) num: SelectorNum,
    pub(crate) sym: &'a RefStr,
}

impl Entry<'_> {
    pub(crate) fn name(&self) -> &str {
        self.sym
    }
}

pub(crate) struct Entries<'a> {
    iter: btree_set::Iter<'a, EntryByNum>,
}

impl<'a> Iterator for Entries<'a> {
    type Item = Entry<'a>;

    fn next(&mut self) -> Option<Self::Item> {
        let entry_by_num = self.iter.next()?;
        Some(Entry {
            num: entry_by_num.0.num,
            sym: &entry_by_num.0.sym,
        })
    }
}

pub(crate) struct SelectorTable {
    entry_by_num: BTreeSet<EntryByNum>,
    entry_by_name: BTreeSet<EntryByName>,
}

impl SelectorTable {
    pub(crate) fn builder() -> Builder {
        // Create a new builder and prepopulate it with the standard entries.
        let mut builder = Builder::new();
        for std_entry in STD_ENTRIES {
            builder.insert_entry(InnerEntry {
                num: std_entry.num,
                sym: RefStr::new(std_entry.sym),
            });
        }
        builder
    }

    pub(crate) fn iter(&self) -> Entries<'_> {
        Entries {
            iter: self.entry_by_num.iter(),
        }
    }

    pub(crate) fn get_by_num(&self, selector_num: &SelectorNum) -> Option<Entry<'_>> {
        Some(self.entry_by_num.get(selector_num)?.0.as_entry())
    }

    pub(crate) fn get_by_name(&self, sym: &str) -> Option<Entry<'_>> {
        Some(self.entry_by_name.get(sym)?.0.as_entry())
    }
}

pub(crate) struct Builder {
    entry_by_num: BTreeSet<EntryByNum>,
    entry_by_name: BTreeSet<EntryByName>,
    min_next_selector_num: u16,
}

impl Builder {
    fn new() -> Self {
        Builder {
            entry_by_num: BTreeSet::new(),
            entry_by_name: BTreeSet::new(),
            min_next_selector_num: 0,
        }
    }

    fn insert_entry(&mut self, entry: InnerEntry) {
        let name = entry.sym.clone();
        let entry = Rc::new(entry);
        if !self.entry_by_num.insert(EntryByNum(entry.clone())) {
            panic!("Selector number {:?} is already declared", entry.num);
        }
        if !self.entry_by_name.insert(EntryByName(entry)) {
            panic!("Selector symbol {:?} is already declared", name);
        }
    }

    /// Declare a selector with the given name and number.
    ///
    /// This will panic if the selector number or name is already declared.
    pub(crate) fn declare(&mut self, sym: &RefStr, selector_num: SelectorNum) {
        let entry = Rc::new(InnerEntry {
            num: selector_num,
            sym: sym.clone(),
        });
        if !self.entry_by_num.insert(EntryByNum(entry.clone())) {
            panic!("Selector number {:?} is already declared", selector_num);
        }
        if !self.entry_by_name.insert(EntryByName(entry)) {
            panic!("Selector symbol {:?} is already declared", sym);
        }
    }

    /// Add a selector with the given name and allocate a free selector number for it.
    ///
    /// The next available selector number will be allocated for the selector. This will
    /// respect any previous declared selectors.
    ///
    /// This will panic if the selector name is already declared or if there are no more
    /// available selector numbers.
    pub(crate) fn add(&mut self, sym: &RefStr) -> SelectorNum {
        loop {
            let next_selector_num = self
                .min_next_selector_num
                .checked_add(1)
                .expect("Selector number overflow");
            self.min_next_selector_num = next_selector_num;
            let selector_num = SelectorNum(next_selector_num);
            if self.entry_by_name.contains(sym.as_str()) {
                continue;
            }
            self.insert_entry(InnerEntry {
                num: selector_num,
                sym: sym.clone(),
            });
            return selector_num;
        }
    }

    pub(crate) fn build(self) -> SelectorTable {
        SelectorTable {
            entry_by_num: self.entry_by_num,
            entry_by_name: self.entry_by_name,
        }
    }
}
