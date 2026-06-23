use std::{
    cell::RefCell,
    fmt::Debug,
    hash::Hash,
    rc::{Rc, Weak},
};

use super::list_ptr::{ListPtr, WeakListPtr};

#[derive(Debug)]
struct NodeInner<T> {
    /// The parent list of this node, if it exists.
    ///
    /// This is weak so that the list itself owns its child nodes, and
    /// otherwise prevents a cycle between the list and its nodes.
    ///
    /// Invariant: If parent is Some, then the parent list must contain this node in its elements map.
    parent: RefCell<Option<WeakListPtr<T>>>,

    /// The next node in the list, if it exists.
    ///
    /// If this exists, must share the same parent as this node.
    next: RefCell<Option<WeakNodePtr<T>>>,

    /// The next node in the list, if it exists.
    ///
    /// If this exists, must share the same parent as this node.
    prev: RefCell<Option<WeakNodePtr<T>>>,
    data: T,
}

pub(super) struct NodePtr<T>(Rc<NodeInner<T>>);

impl<T> NodePtr<T> {
    pub(super) fn new(value: T) -> Self {
        NodePtr(Rc::new(NodeInner {
            parent: RefCell::new(None),
            next: RefCell::new(None),
            prev: RefCell::new(None),
            data: value,
        }))
    }

    pub(super) fn set_parent(&self, parent: Option<&ListPtr<T>>) {
        *self.0.parent.borrow_mut() = parent.map(|list| list.downgrade());
    }

    pub(super) fn set_next(&self, next: Option<&NodePtr<T>>) {
        *self.0.next.borrow_mut() = next.map(|node| WeakNodePtr(Rc::downgrade(&node.0)));
    }

    pub(super) fn set_prev(&self, prev: Option<&NodePtr<T>>) {
        *self.0.prev.borrow_mut() = prev.map(|node| WeakNodePtr(Rc::downgrade(&node.0)));
    }

    pub(super) fn parent(&self) -> Option<WeakListPtr<T>> {
        self.0.parent.borrow().clone()
    }

    pub(super) fn next(&self) -> Option<NodePtr<T>> {
        let next = self.0.next.borrow();
        Some(next.as_ref()?.upgrade().expect("Next should be alive"))
    }

    pub(super) fn prev(&self) -> Option<NodePtr<T>> {
        let prev = self.0.prev.borrow();
        Some(prev.as_ref()?.upgrade().expect("Prev should be alive"))
    }

    pub(super) fn downgrade(&self) -> WeakNodePtr<T> {
        WeakNodePtr(Rc::downgrade(&self.0))
    }

    pub(super) fn insert_after(&self, node: &NodePtr<T>) {
        if node.parent().is_some() {
            panic!("Node is already attached to a list");
        }

        let parent = self
            .parent()
            .expect("Node must be attached to a list")
            .upgrade()
            .expect("Parent list should be alive");

        parent.acquire_node_ownership(node);

        let next = self.next();
        self.set_next(Some(node));
        node.set_prev(Some(self));

        if let Some(next) = next {
            node.set_next(Some(&next));
            next.set_prev(Some(node));
        } else {
            parent.set_tail(Some(node));
        }
    }

    pub(super) fn insert_before(&self, node: &NodePtr<T>) {
        let parent = self
            .parent()
            .expect("Node must be attached to a list")
            .upgrade()
            .expect("Parent list should be alive");
        match self.prev() {
            Some(prev) => prev.insert_after(node),
            None => {
                parent.insert_head(node);
            }
        }
    }

    pub(super) fn replace_with(&self, node: &NodePtr<T>) {
        if node.parent().is_some() {
            panic!("Node is already attached to a list");
        }

        let parent = self
            .parent()
            .expect("Node must be attached to a list")
            .upgrade()
            .expect("Parent list should be alive");

        let next = self.next();
        let prev = self.prev();

        if let Some(prev) = &prev {
            prev.set_next(Some(node));
        } else {
            parent.set_head(Some(node));
        }

        if let Some(next) = &next {
            next.set_prev(Some(node));
        } else {
            parent.set_tail(Some(node));
        }

        parent.acquire_node_ownership(node);
        parent.release_node_ownership(self);

        node.set_next(next.as_ref());
        node.set_prev(prev.as_ref());
        self.set_next(None);
        self.set_prev(None);
    }

    pub(super) fn remove(&self) {
        let parent = self
            .parent()
            .expect("Node must be attached to a list")
            .upgrade()
            .expect("Parent list should be alive");

        let next = self.next();
        let prev = self.prev();

        if let Some(prev) = &prev {
            prev.set_next(next.as_ref());
        } else {
            parent.set_head(next.as_ref());
        }

        if let Some(next) = &next {
            next.set_prev(prev.as_ref());
        } else {
            parent.set_tail(prev.as_ref());
        }

        parent.release_node_ownership(self);

        self.set_next(None);
        self.set_prev(None);
    }

    pub(super) fn get_ptr_key(&self) -> NodePtrKey<T> {
        NodePtrKey(Rc::as_ptr(&self.0))
    }

    fn ptr_eq(&self, other: &NodePtr<T>) -> bool {
        Rc::ptr_eq(&self.0, &other.0)
    }

    pub(super) fn get(&self) -> &T {
        &self.0.data
    }

    pub(super) fn validate_invariants(&self) {
        if let Some(parent) = self.parent() {
            let parent = parent
                .upgrade()
                .expect("Parent list should be alive for invariant check");
            assert!(
                parent.contains_node(self),
                "Node must be in the parent's elements map"
            );
        }

        if let Some(next) = self.next() {
            assert!(
                next.parent().is_some(),
                "Next node must have a parent for invariant check"
            );
            assert!(
                next.parent().as_ref().unwrap().upgrade().is_some(),
                "Next node's parent must be alive for invariant check"
            );
        }

        if let Some(prev) = self.prev() {
            assert!(
                prev.parent().is_some(),
                "Prev node must have a parent for invariant check"
            );
            assert!(
                prev.parent().as_ref().unwrap().upgrade().is_some(),
                "Prev node's parent must be alive for invariant check"
            );
        }
    }
}

impl<T> PartialEq for NodePtr<T> {
    fn eq(&self, other: &Self) -> bool {
        self.ptr_eq(other)
    }
}

impl<T> Eq for NodePtr<T> {}

impl<T> Debug for NodePtr<T>
where
    T: Debug,
{
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.debug_struct("NodePtr")
            .field("ptr", &Rc::as_ptr(&self.0))
            .field("inner", &self.0)
            .finish()
    }
}

impl<T> Clone for NodePtr<T> {
    fn clone(&self) -> Self {
        NodePtr(Rc::clone(&self.0))
    }
}

pub(super) struct WeakNodePtr<T>(Weak<NodeInner<T>>);

impl<T> WeakNodePtr<T> {
    pub(super) fn upgrade(&self) -> Option<NodePtr<T>> {
        self.0.upgrade().map(NodePtr)
    }
}

impl<T> Debug for WeakNodePtr<T> {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.debug_struct("WeakNodePtr")
            .field("ptr", &self.0.as_ptr())
            .finish()
    }
}

impl<T> Clone for WeakNodePtr<T> {
    fn clone(&self) -> Self {
        WeakNodePtr(self.0.clone())
    }
}

#[derive(Debug)]
pub(super) struct NodePtrKey<T>(*const NodeInner<T>);

impl<T> Clone for NodePtrKey<T> {
    fn clone(&self) -> Self {
        NodePtrKey(self.0)
    }
}

impl<T> PartialEq for NodePtrKey<T> {
    fn eq(&self, other: &Self) -> bool {
        self.0 == other.0
    }
}

impl<T> Eq for NodePtrKey<T> {}

impl<T> PartialOrd for NodePtrKey<T> {
    fn partial_cmp(&self, other: &Self) -> Option<std::cmp::Ordering> {
        Some(self.cmp(other))
    }
}

impl<T> Ord for NodePtrKey<T> {
    fn cmp(&self, other: &Self) -> std::cmp::Ordering {
        self.0.cmp(&other.0)
    }
}

impl<T> Hash for NodePtrKey<T> {
    fn hash<H: std::hash::Hasher>(&self, state: &mut H) {
        self.0.hash(state);
    }
}
