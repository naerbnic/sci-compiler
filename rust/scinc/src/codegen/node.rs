use std::rc::Rc;

trait FixupContext {}

trait NodeImpl {
    fn emit_size(&self) -> usize;
    /// Sets the offset of this node, and returns the offset of the next
    /// byte after this node (i.e. offset + emit_size()).
    fn set_offset(&mut self, offset: usize) -> usize;
    fn collect_fixups(&self, fixups: &mut dyn FixupContext);
}

trait LeafNodeImpl: NodeImpl {}

trait CompositeNodeImpl {}

enum Inner {
    Leaf(Rc<dyn LeafNodeImpl>),
    Composite(Rc<dyn CompositeNodeImpl>),
}

impl Clone for Inner {
    fn clone(&self) -> Self {
        match self {
            Inner::Leaf(leaf) => Inner::Leaf(Rc::clone(leaf)),
            Inner::Composite(comp) => Inner::Composite(Rc::clone(comp)),
        }
    }
}

#[derive(Clone)]
struct Node(Inner);

struct LeafNode(Rc<dyn LeafNodeImpl>);
