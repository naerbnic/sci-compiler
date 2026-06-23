use std::{
    cell::RefCell,
    collections::HashMap,
    fmt::Debug,
    rc::{Rc, Weak},
};

use super::{NodePtr, NodePtrKey, WeakNodePtr};

#[derive(Debug)]
struct ListInner<T> {
    head: RefCell<Option<WeakNodePtr<T>>>,
    tail: RefCell<Option<WeakNodePtr<T>>>,
    elements: RefCell<HashMap<NodePtrKey<T>, NodePtr<T>>>,
}

pub(super) struct ListPtr<T>(Rc<ListInner<T>>);

impl<T> Debug for ListPtr<T>
where
    T: Debug,
{
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.debug_struct("ListPtr")
            .field("ptr", &Rc::as_ptr(&self.0))
            .field("inner", &self.0)
            .finish()
    }
}

impl<T> Clone for ListPtr<T> {
    fn clone(&self) -> Self {
        ListPtr(Rc::clone(&self.0))
    }
}

impl<T> ListPtr<T> {
    pub(super) fn new() -> Self {
        ListPtr(Rc::new(ListInner {
            head: RefCell::new(None),
            tail: RefCell::new(None),
            elements: RefCell::new(HashMap::new()),
        }))
    }

    pub(super) fn acquire_node_ownership(&self, node: &NodePtr<T>) {
        node.set_parent(Some(self));
        self.0
            .elements
            .borrow_mut()
            .insert(node.get_ptr_key(), node.clone());
    }

    pub(super) fn release_node_ownership(&self, node: &NodePtr<T>) {
        node.set_parent(None);
        self.0.elements.borrow_mut().remove(&node.get_ptr_key());
    }

    pub(super) fn set_head(&self, id: Option<&NodePtr<T>>) {
        *self.0.head.borrow_mut() = id.map(|node| node.downgrade());
    }

    pub(super) fn set_tail(&self, id: Option<&NodePtr<T>>) {
        *self.0.tail.borrow_mut() = id.map(|node| node.downgrade());
    }

    pub(super) fn insert_head(&self, node: &NodePtr<T>) {
        if node.parent().is_some() {
            panic!("Node is already attached to a list");
        }

        self.acquire_node_ownership(node);

        node.set_prev(None);
        if let Some(old_head) = self.head() {
            node.set_next(Some(&old_head));
            old_head.set_prev(Some(node));
        } else {
            // List was empty, so this is also the tail.
            self.set_tail(Some(node));
        }
        self.set_head(Some(node));
    }

    pub(super) fn insert_tail(&self, node: &NodePtr<T>) {
        let tail = self.0.tail.borrow().clone();
        match tail {
            Some(last) => last
                .upgrade()
                .expect("Tail node should be alive")
                .insert_after(node),
            None => self.insert_head(node),
        }
    }

    pub(super) fn head(&self) -> Option<NodePtr<T>> {
        self.0
            .head
            .borrow()
            .as_ref()?
            .upgrade()
            .expect("Head node should be alive")
            .into()
    }

    pub(super) fn tail(&self) -> Option<NodePtr<T>> {
        self.0
            .tail
            .borrow()
            .as_ref()?
            .upgrade()
            .expect("Head node should be alive")
            .into()
    }

    pub(super) fn downgrade(&self) -> WeakListPtr<T> {
        WeakListPtr(Rc::downgrade(&self.0))
    }

    pub(super) fn contains_node(&self, node: &NodePtr<T>) -> bool {
        self.0.elements.borrow().contains_key(&node.get_ptr_key())
    }
}

pub(super) struct WeakListPtr<T>(Weak<ListInner<T>>);

impl<T> WeakListPtr<T> {
    pub(super) fn upgrade(&self) -> Option<ListPtr<T>> {
        self.0.upgrade().map(ListPtr)
    }
}

impl<T> Debug for WeakListPtr<T> {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.debug_struct("WeakListPtr")
            .field("ptr", &self.0.as_ptr())
            .finish()
    }
}

impl<T> Clone for WeakListPtr<T> {
    fn clone(&self) -> Self {
        WeakListPtr(self.0.clone())
    }
}
