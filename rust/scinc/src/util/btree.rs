use std::borrow::Borrow;

use slab::Slab;
use smallvec::SmallVec;

use crate::util::ivec::IVec;

const NODE_SIZE: usize = 16;

struct ParentRef {
    parent: usize,
    parent_idx: u8,
}

struct NodeEntryPair<T, E> {
    data: T,
    edge: E,
}

enum NodeSearchResult {
    Value(usize),
    Edge(usize),
}

struct NodeImpl<T, E> {
    parent: Option<ParentRef>,
    last_edge: E,
    data: IVec<NODE_SIZE, NodeEntryPair<T, E>>,
}

impl<T, E> NodeImpl<T, E> {
    fn find_index(&self, cmp_fn: &mut impl FnMut(&T) -> std::cmp::Ordering) -> NodeSearchResult {
        match self.data.binary_search_by(|pair| cmp_fn(&pair.data)) {
            Ok(i) => NodeSearchResult::Value(i),
            Err(i) => NodeSearchResult::Edge(i),
        }
    }

    fn get_edge(&self, index: usize) -> &E {
        self.data
            .get(index)
            .map(|pair| &pair.edge)
            .unwrap_or(&self.last_edge)
    }

    fn get_value(&self, index: usize) -> Option<&T> {
        self.data.get(index).map(|pair| &pair.data)
    }

    fn get_value_mut(&mut self, index: usize) -> Option<&mut T> {
        self.data.get_mut(index).map(|pair| &mut pair.data)
    }

    fn try_insert(&mut self, index: usize, value: T, edge: E) -> Result<(), (T, E)> {
        match self
            .data
            .try_insert(index, NodeEntryPair { data: value, edge })
        {
            Ok(()) => Ok(()),
            Err(NodeEntryPair { data, edge }) => Err((data, edge)),
        }
    }
}

impl<T> NodeImpl<T, ()> {
    fn new() -> Self {
        Self {
            parent: None,
            last_edge: (),
            data: IVec::new(),
        }
    }
}

pub(crate) trait Comparator {
    type V;
    fn compare(&self, a: &Self::V, b: &Self::V) -> std::cmp::Ordering;
}

enum Node<T> {
    Leaf(NodeImpl<T, ()>),
    Internal(NodeImpl<T, usize>),
}

impl<T> Node<T> {
    fn find_index(&self, cmp_fn: &mut impl FnMut(&T) -> std::cmp::Ordering) -> NodeSearchResult {
        match self {
            Node::Leaf(node_impl) => node_impl.find_index(cmp_fn),
            Node::Internal(node_impl) => node_impl.find_index(cmp_fn),
        }
    }

    fn get_value(&self, index: usize) -> Option<&T> {
        match self {
            Node::Leaf(node_impl) => node_impl.get_value(index),
            Node::Internal(node_impl) => node_impl.get_value(index),
        }
    }

    fn get_value_mut(&mut self, index: usize) -> Option<&mut T> {
        match self {
            Node::Leaf(node_impl) => node_impl.get_value_mut(index),
            Node::Internal(node_impl) => node_impl.get_value_mut(index),
        }
    }

    fn try_insert(&mut self, index: usize, value: T) {}
}

pub(crate) struct BTree<T> {
    nodes: Slab<Node<T>>,
    root: usize,
}

impl<T> BTree<T> {
    pub(crate) fn new() -> Self {
        let mut nodes = Slab::new();
        let root = nodes.insert(Node::Leaf(NodeImpl::new()));
        Self { nodes, root }
    }

    fn search(
        &self,
        cmp_fn: &mut impl FnMut(&T) -> std::cmp::Ordering,
    ) -> (usize, NodeSearchResult) {
        let mut curr_node_index = self.root;
        loop {
            let node = self.nodes.get(curr_node_index).unwrap();
            match node.find_index(cmp_fn) {
                r @ NodeSearchResult::Value(_) => return (curr_node_index, r),
                NodeSearchResult::Edge(i) => match node {
                    Node::Internal(node_impl) => {
                        let next_node_index = node_impl
                            .data
                            .get(i)
                            .map(|pair| pair.edge)
                            .unwrap_or(node_impl.last_edge);
                        curr_node_index = next_node_index;
                    }
                    Node::Leaf(_) => return (curr_node_index, NodeSearchResult::Edge(i)),
                },
            }
        }
    }

    pub(crate) fn get<C>(&self, comparator: &C, value: &C::V) -> Option<&T>
    where
        C: Comparator,
        T: Borrow<C::V>,
    {
        let (node_index, search_result) =
            self.search(&mut |x| comparator.compare(x.borrow(), value));
        match search_result {
            NodeSearchResult::Value(i) => self.nodes.get(node_index).unwrap().get_value(i),
            NodeSearchResult::Edge(_) => None,
        }
    }

    pub(crate) fn get_mut<C>(&mut self, comparator: &C, value: &C::V) -> Option<&mut T>
    where
        C: Comparator,
        T: Borrow<C::V>,
    {
        let (node_index, search_result) =
            self.search(&mut |x| comparator.compare(x.borrow(), value));
        match search_result {
            NodeSearchResult::Value(i) => self.nodes.get_mut(node_index).unwrap().get_value_mut(i),
            NodeSearchResult::Edge(_) => None,
        }
    }

    pub(crate) fn insert<C>(&mut self, comparator: &C, value: T) -> Option<T>
    where
        C: Comparator,
        T: Borrow<C::V>,
    {
        let (node_index, search_result) =
            self.search(&mut |x| comparator.compare(x.borrow(), value.borrow()));

        let node = self.nodes.get_mut(node_index).unwrap();

        match search_result {
            NodeSearchResult::Value(i) => {
                let old_value = node.get_value_mut(i).unwrap();
                Some(std::mem::replace(old_value, value))
            }
            NodeSearchResult::Edge(i) => {
                node.try_insert(i, value);
                todo!()
            }
        }
    }
}
