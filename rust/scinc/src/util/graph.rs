//! Generic graph representations and algorithms.

use std::{
    collections::{HashMap, HashSet},
    hash::Hash,
};

pub(crate) trait Graph {
    type NodeId: Clone + Hash + Eq + 'static;
    type Node;

    fn node_ids(&self) -> Vec<Self::NodeId>;
    fn get_node(&self, id: &Self::NodeId) -> &Self::Node;
    fn get_node_mut(&mut self, id: &Self::NodeId) -> &mut Self::Node;
    fn outgoing_edges(&self, id: &Self::NodeId) -> Vec<Self::NodeId>;
}

pub(crate) fn reachable_ids<T>(
    start_set: impl IntoIterator<Item = T>,
    mut outgoing_edge_fn: impl FnMut(&T) -> Vec<T>,
) -> HashSet<T>
where
    T: Clone + Hash + Eq,
{
    let mut seen = HashSet::new();
    let mut to_process: Vec<_> = start_set.into_iter().collect();

    while let Some(id) = to_process.pop() {
        if !seen.insert(id.clone()) {
            continue;
        }

        for target in outgoing_edge_fn(&id) {
            to_process.push(target);
        }
    }

    seen
}

pub(crate) fn reachable_nodes<G>(
    graph: &G,
    start_set: impl IntoIterator<Item = G::NodeId>,
) -> HashSet<G::NodeId>
where
    G: Graph,
{
    let mut seen = HashSet::new();
    let mut to_process: Vec<_> = start_set.into_iter().collect();

    while let Some(id) = to_process.pop() {
        if !seen.insert(id.clone()) {
            continue;
        }

        for target in graph.outgoing_edges(&id) {
            to_process.push(target);
        }
    }

    seen
}

pub(crate) fn reverse_edges<G>(graph: &G) -> HashMap<G::NodeId, HashSet<G::NodeId>>
where
    G: Graph,
{
    let mut reversed_edges = HashMap::new();

    for node_id in graph.node_ids() {
        for target in graph.outgoing_edges(&node_id) {
            reversed_edges
                .entry(target)
                .or_insert_with(HashSet::new)
                .insert(node_id.clone());
        }
    }

    reversed_edges
}

pub(crate) struct Subgraph<G>
where
    G: Graph,
{
    outer_graph: G,
    node_subset: HashSet<G::NodeId>,
}

impl<G> Subgraph<G>
where
    G: Graph,
{
    pub(crate) fn new(outer_graph: G, node_subset: HashSet<G::NodeId>) -> Self {
        Subgraph {
            outer_graph,
            node_subset,
        }
    }
}

impl<G> Graph for Subgraph<G>
where
    G: Graph,
{
    type NodeId = G::NodeId;

    type Node = G::Node;

    fn node_ids(&self) -> Vec<Self::NodeId> {
        self.node_subset.iter().cloned().collect()
    }

    fn get_node(&self, id: &Self::NodeId) -> &Self::Node {
        assert!(self.node_subset.contains(id));
        self.outer_graph.get_node(id)
    }

    fn get_node_mut(&mut self, id: &Self::NodeId) -> &mut Self::Node {
        assert!(self.node_subset.contains(id));
        self.outer_graph.get_node_mut(id)
    }

    fn outgoing_edges(&self, id: &Self::NodeId) -> Vec<Self::NodeId> {
        assert!(self.node_subset.contains(id));
        self.outer_graph
            .outgoing_edges(id)
            .into_iter()
            .filter(|id| self.node_subset.contains(id))
            .collect()
    }
}
