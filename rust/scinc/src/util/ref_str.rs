use std::{
    alloc::Layout, borrow::Borrow, fmt::Debug, hash::Hash, num::NonZeroU16, ops::Deref,
    sync::atomic::AtomicU32,
};

struct HeaderBlock {
    ref_count: AtomicU32,
    // Strings can be a maximum of u16 bytes long.
    strlen: NonZeroU16,
}

struct RefStrLayout {
    full_layout: Layout,
    string_offset: usize,
}

const fn ref_str_layout(len: usize) -> RefStrLayout {
    let header_layout = Layout::new::<HeaderBlock>();
    let Ok((string_layout, _)) = Layout::new::<u8>().repeat(len) else {
        panic!("Failed to create string layout")
    };
    let Ok((full_layout, string_offset)) = header_layout.extend(string_layout) else {
        panic!("Failed to create full layout")
    };
    RefStrLayout {
        full_layout,
        string_offset,
    }
}

struct AllocRefStr(*const HeaderBlock);

impl AllocRefStr {
    pub(crate) fn new(str: &str) -> Self {
        assert!(!str.is_empty());
        assert!(str.len() <= u16::MAX as usize);
        // We create a layout that starts with the header block, followed by the string data.
        let layout = ref_str_layout(str.len());
        let full_layout = layout.full_layout;
        let string_offset = layout.string_offset;
        let mem = unsafe { std::alloc::alloc(full_layout) };
        let header_ptr: *mut HeaderBlock = mem.cast::<HeaderBlock>();
        let header_block = HeaderBlock {
            ref_count: AtomicU32::new(1),
            strlen: NonZeroU16::new(str.len() as u16).unwrap(),
        };
        unsafe { header_ptr.write(header_block) };
        let string_data = unsafe { mem.add(string_offset).cast::<u8>() };
        unsafe { string_data.copy_from_nonoverlapping(str.as_ptr(), str.len()) };
        Self(header_ptr)
    }

    fn as_parts(&self) -> (&HeaderBlock, &str) {
        let header = unsafe { self.0.as_ref().unwrap() };
        let len: usize = header.strlen.get().into();
        let string_data = unsafe {
            let str_ptr = self.0.cast::<u8>().add(std::mem::size_of::<HeaderBlock>());
            std::slice::from_raw_parts(str_ptr, len)
        };
        (header, std::str::from_utf8(string_data).unwrap())
    }

    fn as_str(&self) -> &str {
        let (_, s) = self.as_parts();
        s
    }
}

impl Clone for AllocRefStr {
    fn clone(&self) -> Self {
        let parts = self.as_parts();
        parts
            .0
            .ref_count
            .fetch_add(1, std::sync::atomic::Ordering::SeqCst);
        Self(self.0)
    }
}

impl Drop for AllocRefStr {
    fn drop(&mut self) {
        let parts = self.as_parts();
        let prev_value = parts
            .0
            .ref_count
            .fetch_sub(1, std::sync::atomic::Ordering::SeqCst);
        if prev_value == 1 {
            let layout = ref_str_layout(parts.1.len());
            unsafe { std::alloc::dealloc(self.0.cast::<u8>() as *mut u8, layout.full_layout) };
        }
    }
}

impl Debug for AllocRefStr {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.debug_struct("AllocRefStr")
            .field("str", &self.as_str())
            .finish()
    }
}

#[derive(Debug, Clone)]
enum Inner {
    Static(&'static str),
    Allocated(AllocRefStr),
}

#[derive(Debug, Clone)]
pub(crate) struct RefStr(Inner);

impl RefStr {
    pub(crate) fn new<T: AsRef<str>>(s: T) -> Self {
        let s = s.as_ref();
        RefStr(if s.is_empty() {
            Inner::Static("")
        } else {
            Inner::Allocated(AllocRefStr::new(s))
        })
    }

    pub(crate) fn new_static(s: &'static str) -> Self {
        RefStr(Inner::Static(s))
    }

    pub(crate) fn as_str(&self) -> &str {
        match &self.0 {
            Inner::Static(s) => s,
            Inner::Allocated(alloc) => alloc.as_str(),
        }
    }
}

impl Deref for RefStr {
    type Target = str;

    fn deref(&self) -> &Self::Target {
        self.as_str()
    }
}

impl PartialEq for RefStr {
    fn eq(&self, other: &Self) -> bool {
        self.as_str() == other.as_str()
    }
}

impl Eq for RefStr {}

impl Ord for RefStr {
    fn cmp(&self, other: &Self) -> std::cmp::Ordering {
        self.as_str().cmp(other.as_str())
    }
}

impl PartialOrd for RefStr {
    fn partial_cmp(&self, other: &Self) -> Option<std::cmp::Ordering> {
        Some(self.cmp(other))
    }
}

impl Hash for RefStr {
    fn hash<H: std::hash::Hasher>(&self, state: &mut H) {
        self.as_str().hash(state);
    }
}

impl Default for RefStr {
    fn default() -> Self {
        RefStr(Inner::Static(""))
    }
}

impl AsRef<str> for RefStr {
    fn as_ref(&self) -> &str {
        self.as_str()
    }
}

impl Borrow<str> for RefStr {
    fn borrow(&self) -> &str {
        self.as_str()
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_ref_str() {
        let s = RefStr::new("hello");
        assert_eq!(s.as_str(), "hello");
        let s2 = RefStr::new_static("world");
        assert_eq!(s2.as_str(), "world");
    }
}
