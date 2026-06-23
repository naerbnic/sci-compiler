mod list_ptr;
mod node_ptr;

use std::{cell::RefCell, ops::Deref};

use list_ptr::ListPtr;
use node_ptr::{NodePtr, NodePtrKey, WeakNodePtr};

#[derive(Debug)]
pub(super) struct List<T>(ListPtr<T>);

impl<T> List<T> {
    pub(super) fn new() -> Self {
        List(ListPtr::new())
    }

    fn insert_head(&self, node: &ListNode<T>) {
        self.0.insert_head(&node.ptr);
    }

    fn insert_tail(&self, node: &ListNode<T>) {
        self.0.insert_tail(&node.ptr);
    }

    fn head(&self) -> Option<ListNode<T>> {
        self.0.head().map(|ptr| ListNode::from_ptr(ptr))
    }

    fn tail(&self) -> Option<ListNode<T>> {
        self.0.tail().map(|ptr| ListNode::from_ptr(ptr))
    }

    fn iter(&self) -> Iter<T> {
        Iter {
            list: self.0.clone(),
            front: self.0.head(),
            back: None,
        }
    }
}

impl<A> FromIterator<ListNode<A>> for List<A> {
    fn from_iter<T: IntoIterator<Item = ListNode<A>>>(iter: T) -> Self {
        let list = List::new();
        for item in iter {
            list.insert_tail(&item);
        }
        list
    }
}

impl<T> IntoIterator for List<T> {
    type Item = ListNode<T>;
    type IntoIter = Iter<T>;

    fn into_iter(self) -> Self::IntoIter {
        Iter {
            list: self.0.clone(),
            front: self.0.head(),
            back: None,
        }
    }
}

#[derive(Debug)]
pub(super) struct ListNode<T> {
    parent: RefCell<Option<ListPtr<T>>>,
    ptr: NodePtr<T>,
}

impl<T> ListNode<T> {
    fn from_ptr(ptr: NodePtr<T>) -> Self {
        let parent = ptr
            .parent()
            .map(|weak| weak.upgrade().expect("Parent list should be alive"));
        ListNode {
            parent: RefCell::new(parent),
            ptr,
        }
    }

    fn refresh_parent(&self) {
        let parent = self
            .ptr
            .parent()
            .map(|weak| weak.upgrade().expect("Parent list should be alive"));
        self.parent.replace(parent);
    }

    pub(super) fn new(value: T) -> Self {
        Self::from_ptr(NodePtr::new(value))
    }

    pub(super) fn next(&self) -> Option<ListNode<T>> {
        Some(Self::from_ptr(self.ptr.next()?))
    }

    pub(super) fn prev(&self) -> Option<ListNode<T>> {
        Some(Self::from_ptr(self.ptr.prev()?))
    }

    pub(super) fn insert_after(&self, node: &ListNode<T>) {
        self.ptr.insert_after(&node.ptr);
        node.refresh_parent();
    }

    pub(super) fn insert_before(&self, node: &ListNode<T>) {
        self.ptr.insert_before(&node.ptr);
        node.refresh_parent();
    }

    pub(super) fn replace_with(&self, node: &ListNode<T>) {
        self.ptr.replace_with(&node.ptr);
        self.refresh_parent();
        node.refresh_parent();
    }

    pub(super) fn remove(&self) {
        self.ptr.remove();
        self.refresh_parent();
    }

    pub(super) fn parent(&self) -> Option<List<T>> {
        Some(List(self.parent.borrow().as_ref()?.clone()))
    }

    pub(super) fn get(&self) -> &T {
        self.ptr.get()
    }
}

impl<T> Deref for ListNode<T> {
    type Target = T;

    fn deref(&self) -> &Self::Target {
        self.ptr.get()
    }
}

impl<T> Clone for ListNode<T> {
    fn clone(&self) -> Self {
        let parent = self.parent.take();
        self.parent.replace(parent.clone());
        ListNode {
            parent: RefCell::new(parent),
            ptr: self.ptr.clone(),
        }
    }
}

pub(super) struct Iter<T> {
    list: ListPtr<T>,
    /// The element at the front of the iterator. This points directly to the next element
    /// to be returned, as long as it's not equal to the back element.
    front: Option<NodePtr<T>>,
    /// The element after the back of the iterator. This points to the next element after
    /// the last element to be returned, as long as it's not equal to the front element.
    back: Option<NodePtr<T>>,
}

impl<T> Iter<T> {
    fn is_empty(&self) -> bool {
        match (self.front.as_ref(), self.back.as_ref()) {
            (Some(front), Some(back)) => front.get_ptr_key() == back.get_ptr_key(),
            (None, None) => true,
            _ => false,
        }
    }
}

impl<T> Iterator for Iter<T> {
    type Item = ListNode<T>;

    fn next(&mut self) -> Option<Self::Item> {
        if self.is_empty() {
            return None;
        }
        let current_node = self
            .front
            .take()
            .expect("front should not be None if front != back");
        self.front = current_node.next();
        Some(ListNode::from_ptr(current_node))
    }
}

impl<T> DoubleEndedIterator for Iter<T> {
    fn next_back(&mut self) -> Option<Self::Item> {
        if self.is_empty() {
            return None;
        }
        let current_node = match self.back.take() {
            Some(node) => node.prev(),
            None => self.list.tail(),
        }
        .expect("back should not be None if front != back");
        self.back = Some(current_node.clone());
        Some(ListNode::from_ptr(current_node))
    }
}

pub(super) struct ListNodeRef<T> {
    ptr: WeakNodePtr<T>,
}

impl<T> ListNodeRef<T> {
    pub(super) fn get(&self) -> ListNode<T> {
        ListNode::from_ptr(self.ptr.upgrade().expect("Node should be alive"))
    }

    pub(super) fn try_get(&self) -> Option<ListNode<T>> {
        self.ptr.upgrade().map(ListNode::from_ptr)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn empty_list_works() {
        let list = List::<i32>::new();
        let v: Vec<_> = list.iter().collect();
        assert_eq!(v.len(), 0);
    }

    #[test]
    fn singleton_list_works() {
        let list = List::<i32>::new();
        list.insert_tail(&ListNode::new(1));
        eprintln!("list: {:#?}", list);
        let v: Vec<_> = list.iter().collect();
        assert_eq!(v.len(), 1);
    }

    #[test]
    fn simple_list_works() {
        let list = List::new();
        list.insert_tail(&ListNode::new(1));
        list.insert_tail(&ListNode::new(2));
        list.insert_tail(&ListNode::new(3));

        let v: Vec<_> = list.iter().map(|node| *node.get()).collect();
        assert_eq!(v, vec![1, 2, 3]);
    }

    #[test]
    fn middle_removal_works() {
        let list = List::new();
        let middle = ListNode::new(2);
        list.insert_tail(&ListNode::new(1));
        list.insert_tail(&middle);
        list.insert_tail(&ListNode::new(3));

        middle.remove();

        let v: Vec<_> = list.iter().map(|node| *node.get()).collect();
        assert_eq!(v, vec![1, 3]);
    }

    #[test]
    fn head_removal_works() {
        let list = List::new();
        let head = ListNode::new(1);
        list.insert_tail(&head);
        list.insert_tail(&ListNode::new(2));
        list.insert_tail(&ListNode::new(3));

        head.remove();

        let v: Vec<_> = list.iter().map(|node| *node.get()).collect();
        assert_eq!(v, vec![2, 3]);
    }

    #[test]
    fn tail_removal_works() {
        let list = List::new();
        let tail = ListNode::new(3);
        list.insert_tail(&ListNode::new(1));
        list.insert_tail(&ListNode::new(2));
        list.insert_tail(&tail);

        tail.remove();

        let v: Vec<_> = list.iter().map(|node| *node.get()).collect();
        assert_eq!(v, vec![1, 2]);
    }
}
